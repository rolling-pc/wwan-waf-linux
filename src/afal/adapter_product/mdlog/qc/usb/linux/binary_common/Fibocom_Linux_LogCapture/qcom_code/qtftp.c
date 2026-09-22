#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <poll.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include "log_control.h"
#include "qlog.h"

extern unsigned qlog_msecs(void);

#define dbg(fmt, arg... ) do { unsigned msec = qlog_msecs();  printf("[%03d.%03d]" fmt,  msec/1000, msec%1000, ## arg);} while (0)
#define errno_dbg(fmt, ...) do {dbg(fmt ", errno: %d (%s) at File: %s, Line: %d\n", ##__VA_ARGS__, errno, strerror(errno), __func__, __LINE__);} while (0)
#define N_DATA 1 // 4 get 10MB/s, 8 get 20MB/s
#define CACHE_SIZE (5*1024*1024)
#define FIFO_NUM   4
#define TFTP_RRQ   1
#define TFTP_WRQ   2
#define TFTP_DATA  3
#define TFTP_ACK   4
#define TFTP_ERROR 5
#define TFTP_OACK  6
#define TFTP_MAX_RETRY  12
#define TFTP_TIME_RETRY 200

struct __kfifo 
{
    int fd;
    size_t in;
    size_t out;
    size_t size;
    void *data;
};

static struct sockaddr_in tftp_server;
static struct __kfifo qkfifo[FIFO_NUM] = {{-1, 0, 0, 0, NULL}, {-1, 0, 0, 0, NULL}, {-1, 0, 0, 0, NULL}, {-1, 0, 0, 0, NULL}};
static const size_t cache_step = (256*1024);

const char *q_tftp_server_ip = NULL;
int q_block_size = 16384;
struct q_tftp_request *q_cur_q_req = NULL;

static int __kfifo_write(struct __kfifo *fifo, const void *buf, size_t size)
{
    void *data;
    size_t unused, len;
    ssize_t nbytes;
    int fd = fifo->fd;

    if (fifo->out == fifo->in)
    {
        nbytes = log_poll_write(fd, buf, size);

        if(nbytes > 0)
        {
            if((size_t)nbytes == size)
            {
                return 1;
            }
            else
            {
                buf = (char *)buf + nbytes;
                size -= nbytes;
            }
        }
        else if (errno == ECONNRESET)
        {
            LogInfo("TODO: ECONNRESET\n");
            return 0;
        }
    }

    unused = fifo->size - fifo->in;
    if (unused < size && size < (unused + fifo->out))
    {
        memmove(fifo->data, (char *)fifo->data + fifo->out, fifo->in - fifo->out);
        fifo->in -=  fifo->out;
        fifo->out = 0;    
    }

    unused = fifo->size - fifo->in;
    if(unused < size && fifo->size < CACHE_SIZE)
    {
        data = malloc(fifo->size + cache_step);

        if (data)
        {
            LogInfo("cache[fd=%d] size %zd -> %zd KB\n", fd, fifo->size/1024, (fifo->size + cache_step)/1024);
            if(fifo->data)
            {
                len = fifo->in - fifo->out;
                if (len)
                    memcpy(data, (char *)fifo->data + fifo->out, len);
                free(fifo->data);
            }

            fifo->in -=  fifo->out;
            fifo->out = 0;
            fifo->size += cache_step;
            fifo->data = data;
        }
    }

    unused = fifo->size - fifo->in;
    if (unused < size)
    {
        static size_t drop = 0;
        static unsigned slient_msec = 0;
        unsigned now = qlog_msecs();

        drop += size;
        if ((slient_msec+5000) < now)
        {
            LogInfo("cache[fd=%d] full, total drop %zd\n", fd, drop);
            slient_msec = now;
        }
    }
    else
    {
        memcpy((char *)fifo->data + fifo->in, buf, size);
        fifo->in += size;
    }

    len = fifo->in - fifo->out;
    if (len)
    {
        nbytes = log_poll_write(fd, (char *)fifo->data + fifo->out, len);

        if (nbytes > 0)
        {
            fifo->out += nbytes;

            if (fifo->out == fifo->in)
            {
                fifo->in = 0;
                fifo->out = 0;
            }
        }
        else if(errno == ECONNRESET)
        {
            LogInfo("TODO: ECONNRESET\n");
            return 0;
        }
    }

    return 1;
}

int qkfifo_alloc(int fd)
{
    int idx = 0;
    int flags;

    if (fd == -1)
        return fd;

    for (idx = 0; idx < FIFO_NUM; idx++) {
        if (qkfifo[idx].fd == -1)
            break;
    }

    if (idx == FIFO_NUM) {
        LogInfo("No Free FIFO for fd = %d\n", fd);
        return -1;
    }

    qkfifo[idx].fd = fd;
    qkfifo[idx].in = qkfifo[idx].out = 0;

    flags = fcntl(fd, F_GETFL);
    if (flags != -1)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    LogInfo("%s [%d] = %d\n", __func__, idx, fd);
    return idx;
}

size_t qkfifo_write(int idx, const void *buf, size_t size)
{
    if (idx < 0 ||  idx >= FIFO_NUM)
        return 0;
    return __kfifo_write(&qkfifo[idx], buf, size) ? size : 0;
}

void qkfifo_free(int idx)
{
    if (idx < 0 || idx >= FIFO_NUM)
        return;
    LogInfo("%s [%d] = %d\n", __func__, idx, qkfifo[idx].fd);
    qkfifo[idx].fd = -1;
    qkfifo[idx].in = qkfifo[idx].out = 0;
}

int qkfifo_idx(int fd)
{
    int idx = 0;

    if (fd == -1)
        return fd;

    for (idx = 0; idx < FIFO_NUM; idx++) {
        if (qkfifo[idx].fd == fd)
            break;
    }

    if (idx == FIFO_NUM) {
        return -1;
    }

    return idx;
}
struct tftp_packet
{
	uint16_t cmd;
	union{
		uint16_t code;
		uint16_t block;
		// For a RRQ and WRQ TFTP packet
		char filename[2];
	};
	uint8_t data[512];
};
struct tftp_data_packet
{
    uint16_t cmd;
    uint16_t block;
    uint8_t data[16384];
};

struct q_tftp_request
{
    char *file;
    int sock;
    int pipe[2];
    pthread_t tid;
    int cur;
    //uint16_t block;
    struct sockaddr_in sender;
    socklen_t  addr_len;
    struct tftp_packet rx_pkt;
    struct tftp_data_packet tx_pkt[N_DATA];
};

static int tftp_send_data_pkt(int sock, struct tftp_data_packet *tx_pkt, uint16_t block, int size, struct sockaddr_in *to, int is_sync)
{
    struct tftp_packet rx_pkt;
    int wait_ack;
    int ret;
    struct sockaddr_in sender;
    socklen_t addr_len = sizeof(sender);
    int need_send = 1;

    //dbg("%s send = %ld!\n", __func__, size);
    tx_pkt->cmd = htons(TFTP_DATA);
    tx_pkt->block = htons(block);

    for(wait_ack = 0; wait_ack < TFTP_MAX_RETRY; wait_ack++) {
        struct pollfd pollfds[1] = {{sock, POLLIN, 0}};

        if (need_send) {
            ret = sendto(sock, tx_pkt, size + 4, 0, (struct sockaddr *)to, addr_len);
            if (ret == -1) {
                errno_dbg("sendto");
                return 0;
            }
        }

        if (!is_sync)
            return size;

        do  {
            ret = poll(pollfds, 1, TFTP_TIME_RETRY);
        } while ((ret < 0) && (errno == EINTR));

        if (ret <= 0) {
            dbg("wait ack timeout, ret=%d, block=%d\n", ret, block);
            need_send = 1;
            continue;
        }

        ret = recvfrom(sock, &rx_pkt, sizeof(struct tftp_packet), MSG_DONTWAIT, (struct sockaddr *)&sender, &addr_len);
        if(ret >= 4 && rx_pkt.cmd == htons(TFTP_ACK) && rx_pkt.block == htons(block)) {
            return size;
        }
        else if (ret >= 4 && rx_pkt.cmd == htons(TFTP_ACK)) {
            int i;

            //dbg("wait %u, but get %u\n", block, ntohs(rx_pkt.block));
            for (i = 1; i < N_DATA; i++) {
                if ((block + i) == ntohs(rx_pkt.block)) {
                    dbg("wait %u, but get %u\n", block, ntohs(rx_pkt.block));
                    return size;
                }
            }
            need_send = 0;
        }   
        else if (ret >= 4 && rx_pkt.cmd == htons(TFTP_ERROR)) {
            dbg("Error Code, Code: %d, Message: %s, block=%u\n", ntohs(rx_pkt.code), rx_pkt.data, block);
            assert(0);
        }         
        else {
            need_send = 1;
            errno_dbg("wait ack timeout, ret=%d, block=%d\n", ret, block);
        }     
    }

    assert(0);
    return 0;
}

static void * tftp_write_thread(void *arg)
{
    struct q_tftp_request *q_req = (struct q_tftp_request *)arg;
    q_req->cur = 0;
    //q_req->block = 1;
    int save_fd = -1;
    uint32_t tx_block = 1;
    uint16_t rx_block = 0;
    uint8_t state[N_DATA] = {0};
    int w_idx = 0;
    int re_send = 0;

    //save_fd = open(q_req->file, O_CREAT | O_WRONLY | O_TRUNC, 0444); //md5sum

    while (1) {
        int ret;
        struct pollfd pollfds[2] = {{q_req->sock, POLLIN, 0}, {q_req->pipe[1], POLLIN, 0}};
        int n = 1;
        int i, r_idx;
        int busy = 0;

        for (i = 0; i < N_DATA; i++)
            busy += state[i];

        if (q_req->pipe[1] == -1 && busy == 0)
                break;

        w_idx = tx_block%N_DATA;
        if (q_req->pipe[1] != -1 && state[w_idx] == 0)
            n = 2;

        //dbg("%s poll n = %d!\n", __func__, n);
        do  {
            ret = poll(pollfds, n, busy ? TFTP_TIME_RETRY : 12000); //if no data transfer in 1 second, tftp server will auto hangup connection
        } while ((ret < 0) && (errno == EINTR));

        if (ret <= 0) {
            dbg("poll=%d, rx_block=%u\n", ret, rx_block);
            if (re_send++ > TFTP_MAX_RETRY) {
                break;
            }
            else
            {
                //MCU -> PC: (= N_DATA) packet lost
                for (i = 0; i < N_DATA; i++) {
                    r_idx = (rx_block + 1)%N_DATA;

                    //dbg("state %d, block %u\n", state[r_idx], (rx_block + 1));
                    if (state[r_idx] == 0)
                        break;
                    //dbg("re-send %u\n", rx_block+1);
                    assert((rx_block + 1) == ntohs(q_req->tx_pkt[r_idx].block));
                    tftp_send_data_pkt(q_req->sock, &q_req->tx_pkt[r_idx],
                        ntohs(q_req->tx_pkt[r_idx].block), q_block_size, &q_req->sender, 1);
                    rx_block = ntohs(q_req->tx_pkt[r_idx].block);
                    state[r_idx] = 0;
                }
            }
            continue;
        }

        if (pollfds[0].revents & POLLIN) {
            ret = recv(q_req->sock, &q_req->rx_pkt, sizeof(struct tftp_packet), MSG_DONTWAIT);
            //dbg("%s recv = %d!\n", __func__, ret);
           if(ret >= 4 && q_req->rx_pkt.cmd == htons(TFTP_ACK)) {
                if (((rx_block + 1) == htons(q_req->rx_pkt.block))) {
                    rx_block = htons(q_req->rx_pkt.block);
                    assert(state[rx_block%N_DATA]);
                    state[rx_block%N_DATA] = 0;
                    re_send = 0;
                }
                else if (rx_block == htons(q_req->rx_pkt.block)) {
                    //MCU -> PC: (< N_DATA) packet lost
                    dbg("double ACK %u\n", rx_block);

                    //r_idx = (rx_block + 1)%N_DATA;
                    //assert(state[r_idx]);

                    for (i = 0; i < N_DATA; i++) {
                        r_idx = (rx_block + 1)%N_DATA;

                        //dbg("state %d, block %u\n", state[r_idx], (rx_block + 1));
                        if (state[r_idx] == 0)
                            break;
                        //dbg("re-send %u\n", rx_block+1);
                        assert((rx_block + 1) == ntohs(q_req->tx_pkt[r_idx].block));
                        tftp_send_data_pkt(q_req->sock, &q_req->tx_pkt[r_idx],
                            ntohs(q_req->tx_pkt[r_idx].block), q_block_size, &q_req->sender, 1);
                        rx_block = ntohs(q_req->tx_pkt[r_idx].block);
                        state[r_idx] = 0;
                        re_send = 0;
                    }
                    continue;
                }
                else {
                    //PC -> MCU: ACK packet lost
                    uint16_t num = (uint16_t)(htons(q_req->rx_pkt.block) - rx_block);
                    dbg("get %u, expect %u, num %u\n", htons(q_req->rx_pkt.block), (rx_block + 1), num);
                    if (num <= N_DATA) {
                        for (i = 0; i < num; i++) {
                            r_idx = (rx_block + 1 + i)%N_DATA;
                            assert(state[r_idx]);
                            state[r_idx] = 0;
                        }
                        re_send = 0;
                        rx_block = htons(q_req->rx_pkt.block);
                        continue;
                    }
                    assert(0);
                }
            }
        }

        if (n == 1)
            continue;

        if (pollfds[1].revents & POLLIN) {
            ret = read(q_req->pipe[1], q_req->tx_pkt[w_idx].data + q_req->cur, q_block_size - q_req->cur);
            //dbg("%s read = %d!\n", __func__, ret);
            if (ret > 0) {
                int is_sync = (tx_block == 1) || (N_DATA == 1);
                q_req->cur += ret;

                if (q_req->cur == q_block_size) {
                    if (save_fd != -1) {
                        assert(write(save_fd, q_req->tx_pkt[w_idx].data, q_req->cur) == q_req->cur);
                    }
                    tftp_send_data_pkt(q_req->sock, &q_req->tx_pkt[w_idx], tx_block&0xFFFF, q_req->cur, &q_req->sender, is_sync);
					if (is_sync) {
                        rx_block = tx_block&0xFFFF;
                        state[w_idx] = 0;
                    }
                    else {
                        state[w_idx] = 1;
                    }
                    tx_block++;
                    q_req->cur = 0;
                }

                if (pollfds[1].revents & (POLLERR | POLLHUP))
                {
                    dbg("read %d, wait for more\n",ret);
                    continue;
                }
            }
            else {
                if (ret < 0) errno_dbg("read: %d", ret);
                close(q_req->pipe[1]);
                q_req->pipe[1] = -1;
            }
        }

        if (pollfds[1].revents & (POLLERR | POLLHUP)) {
            //errno_dbg("revents: %x", pollfds[0].revents);
            close(q_req->pipe[1]);
            q_req->pipe[1] = -1;
            //dbg("file %s close\n", q_req->file);
        }
    }

    if (save_fd != -1)
    {
        if (q_req->cur)
            assert(write(save_fd, q_req->tx_pkt[w_idx].data, q_req->cur) == q_req->cur);
        close(save_fd);
    }

    tftp_send_data_pkt(q_req->sock, &q_req->tx_pkt[w_idx], tx_block++, q_req->cur, &q_req->sender, 1);

    close(q_req->sock);
    if (q_req->pipe[1] != -1)
        close(q_req->pipe[1]);
    //dbg("file %s exit\n", q_req->file);
    free(q_req->file);
    if (q_cur_q_req != q_req)
        free(q_req);
    return NULL;
}

static int tftp_socket(const char *serv_ip)
{
    int sock, reuse_addr = 1;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        errno_dbg("socket");
        return -1;
    }
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,sizeof(reuse_addr));
    dbg("serv_ip is %s\r\n", serv_ip);
    return sock;
}

int qtftp_write_request(const char *filename, long tsize)
{
    struct tftp_packet tx_pkt;
    int wait_ack;
    int ret, sock, size;
    char *charp;
    struct sockaddr_in sender;
    socklen_t  addr_len = sizeof(sender);

    if (q_cur_q_req) {
        pthread_join(q_cur_q_req->tid, NULL);
        free(q_cur_q_req);
        q_cur_q_req = NULL;
    }
    
    dbg("%s filename=%s, tsize=%ld\n", __func__, filename, tsize);

    sock = tftp_socket(q_tftp_server_ip);

    tx_pkt.cmd = htons(TFTP_WRQ);
    charp = (char *)tx_pkt.data; //tx_pkt->filename to avoid [-Wformat-overflow=]
    if (tsize)
        size = snprintf(charp, 512, "%.256s%c%s%c%s%c%d%c%s%c%ld%c", filename, 0, "octet", 0, "blksize", 0, q_block_size, 0, "tsize", 0, tsize, 0);
    else
        size = snprintf(charp, 512, "%.256s%c%s%c%s%c%d%c", filename, 0, "octet", 0, "blksize", 0, q_block_size, 0);
    memmove(&tx_pkt.cmd + 1, charp, size);

    for(wait_ack = 0; wait_ack < TFTP_MAX_RETRY; wait_ack++) {
        struct pollfd pollfds[1] = {{sock, POLLIN, 0}};

        ret = sendto(sock, &tx_pkt, size + 2, 0, (struct sockaddr*)&tftp_server, sizeof(tftp_server));
        if (ret == -1) {
            errno_dbg("sendto");
            return 0;
        }

        do  {
            ret = poll(pollfds, 1, TFTP_TIME_RETRY);
        } while ((ret < 0) && (errno == EINTR));

        if (ret <= 0) {
            errno_dbg("wait ack timeout, ret=%d", ret);
            continue;
        }

        ret = recvfrom(sock, &tx_pkt, sizeof(struct tftp_packet), MSG_DONTWAIT, (struct sockaddr *)&sender, &addr_len);
        if (ret >= 4 && tx_pkt.cmd == htons(TFTP_OACK)) {
            struct q_tftp_request *q_req = (struct q_tftp_request *)malloc(sizeof(struct q_tftp_request));
            pthread_attr_t attr;

            q_req->file = strdup(filename);
            q_req->sock = sock;
            q_req->sender = sender;
            q_req->addr_len = addr_len;
            if (socketpair(AF_LOCAL, SOCK_STREAM, 0, q_req->pipe)) errno_dbg("socketpair");
            
            pthread_attr_init (&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            q_cur_q_req = q_req;
            if (pthread_create(&q_req->tid, q_cur_q_req ? NULL : &attr, tftp_write_thread, q_req)) errno_dbg("pthread_create");
            pthread_attr_destroy(&attr);
            return q_req->pipe[0];
        }
        else {
             errno_dbg("wait ack timeout");
        }
    }

    return -1;
}

int qtftp_test_server(const char *serv_ip)
{
    int sock;

    tftp_server.sin_family = AF_INET;
    tftp_server.sin_port = htons(69);
    inet_pton(AF_INET, serv_ip, &(tftp_server.sin_addr.s_addr));

    sock = qtftp_write_request("qlog_tftp_test_blksize", 0);
    if (sock > 0) {
        int size = strlen(serv_ip);
        int retval = write(sock, (void *)serv_ip, size);

        close(sock);
        if (retval == size) {
           dbg("%s %s OK!\n", __func__, serv_ip);
           return 1;
        }
    }

    dbg("%s %s FAIL!\n", __func__, serv_ip);
    return 0;
}


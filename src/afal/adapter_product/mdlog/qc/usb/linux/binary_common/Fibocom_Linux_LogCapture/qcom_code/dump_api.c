#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <byteswap.h>
#include <endian.h>
#include "dloaddef.h"
#include "dump_api.h"
#include "boot_sahara.h"
#include <stdint.h>
#include <dirent.h>
#include <limits.h>

#define MIN(a,b) (((a)>(b))? (b):(a))
/* Match Sahara chunk / usbfs URB size for fewer read syscalls. */
#define SAHARA_DUMP_READ_BUF_SIZE    (64 * 1024)
#define SAHARA_DUMP_PROGRESS_STEP    (1024 * 1024)

FILE *g_dump_pull_log_fp = NULL;

static void dump_pull_log_open(void)
{
    char log_path[PATH_MAX] = {0};

    if (g_dump_pull_log_fp != NULL)
        return;
    snprintf(log_path, sizeof(log_path), "%s/dump_pull.log", sahara_dump_path);
    g_dump_pull_log_fp = fopen(log_path, "a");
}

static void dump_pull_log_close(void)
{
    if (g_dump_pull_log_fp != NULL) {
        fflush(g_dump_pull_log_fp);
        fclose(g_dump_pull_log_fp);
        g_dump_pull_log_fp = NULL;
    }
}
uint8 sahara_packet_buffer[SAHARA_MAX_PACKET_SIZE_IN_BYTES];
uint8 sahara_packet_rcv_buffer[SAHARA_MAX_PACKET_SIZE_IN_BYTES];
uint32 g_u32_memory_table_addr = 0;
uint32 g_u32_memory_table_length = 0;
uint64 g_u64_memory_table_addr = 0;
uint64 g_u64_memory_table_length = 0;
static dload_debug_type g_dload_debug_info[NUM_REGIONS];
static dload_debug_type_64 g_dload_debug64_info[NUM_REGIONS];
char sahara_dump_path[SAHARA_PACKET_LOG_LENGTH] = {0};
int ram_dump_64bit = 0;

#if 0
static void cprintf_hex(const uint8 *buf, int buf_size)
{  
    int i;
    uint32 count = 0;
    cprintf("buf_size: %d\n", buf_size);

    for(i=0; i<buf_size; i++) {
        cprintf("%02X ", buf[i]);
        count++;
        if ((count%4) == 0) {
            cprintf(" ");
        }
        if ((count%32) == 0) {
            cprintf("\n");
        }
    }
    cprintf("\n");
}
#endif

#if 0
static void SetTermios(struct termios *p,word uBaudRate)
{
    cfsetispeed(p, uBaudRate);
    cfsetospeed(p, uBaudRate);
    p->c_cflag &= ~CSIZE;
    p->c_cflag |= CS8;

    p->c_cflag |= ~CS8;
    p->c_cflag &= ~CSTOPB;  //stop bit 1
    p->c_cflag &= ~PARENB;  //no check
    p->c_iflag = 0; //(INPCK | ISTRIP | IGNPAR);//IGNPAR | IUCLC | IXON | IGNCR;p->c_oflag |= 0;
    p->c_lflag = 0; //&= ~(ICANON | ECHO | ECHOE | ISIG);//ICANON;

    p->c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /*Input*/
    p->c_oflag &= ~OPOST; /*Output*/

    p->c_cc[VINTR] = 0;
    p->c_cc[VQUIT] = 0;
    p->c_cc[VERASE] = 0;
    p->c_cc[VKILL] = 0;
    p->c_cc[VEOF] = 0;
    p->c_cc[VTIME] = 1;
    p->c_cc[VMIN] = 0;
    p->c_cc[VSWTC] = 0;
    p->c_cc[VSTART] = 0;
    p->c_cc[VSTOP] = 0;
    p->c_cc[VSUSP] = 0;
    p->c_cc[VEOL] = 0;
    p->c_cc[VREPRINT] = 0;
    p->c_cc[VDISCARD] = 0;
    p->c_cc[VWERASE] = 0;
    p->c_cc[VLNEXT] = 0;
    p->c_cc[VEOL2] = 0;
}

int ttycom_init(char *ttypath)
{
    struct termios newio;
    printf("ttypath=%s\n",ttypath);
    int edl_port = open(ttypath,O_RDWR | O_NOCTTY /*| O_NDELAY |O_NONBLOCK*/ );
    if (edl_port == -1) {
        cprintf("open %s failed.", ttypath);
        return edl_port;
    }

    memset(&newio, 0, sizeof(struct termios));
    tcgetattr(edl_port,&newio);
    tcflush(edl_port, TCIOFLUSH);
    SetTermios(&newio,B115200);
    tcflush(edl_port, TCIFLUSH);
    tcsetattr(edl_port,TCSANOW,&newio);
    printf("open sucess.\n");

    return edl_port;
}

#endif

uint64 qlog_le64_fibo(uint64 v64)
{
    const uint64 is_bigendian = 1;
    uint64 tmp = v64;

    if ((*(char*)&is_bigendian) == 0)
    {
        unsigned char *s = (unsigned char *)(&v64);
        unsigned char *d = (unsigned char *)(&tmp);
        d[0] = s[7];
        d[1] = s[6];
        d[2] = s[5];
        d[3] = s[4];
        d[4] = s[3];
        d[5] = s[2];
        d[6] = s[1];
        d[7] = s[0];
    }
    return tmp;
}
extern size_t log_poll_write(int fd, const void *buf, size_t size);

int WriteToComPort(int fd_tty, void *data, const uint32 size)
{
    size_t wc;

    /* usbfs path needs log_poll_write (URB out); tty path also works via write(). */
    wc = log_poll_write(fd_tty, data, size);
    if (wc != (size_t)size) {
        cprintf("[%s] failed: wrote %zu/%u bytes\n", __func__, wc, size);
        return -1;
    }
    return (int)wc;
}

static int ReadComPort(int fd_tty, uint8 *data, const uint32 size)
{
    uint32 total = 0;
    unsigned timeout_sec = 5 + (size / (64 * 1024));

    while (total < size) {
        int fs_sel;
        fd_set fs_read;
        struct timeval tv_timeout;

        tv_timeout.tv_sec = timeout_sec;
        tv_timeout.tv_usec = 0;
        FD_ZERO(&fs_read);
        FD_SET(fd_tty, &fs_read);

        fs_sel = select(fd_tty + 1, &fs_read, NULL, NULL, &tv_timeout);
        if (fs_sel == 0) {
            cprintf("[%s] time out, got %u/%u bytes.\n", __func__, total, size);
            return (int)total;
        }
        if (fs_sel < 0) {
            if (errno == EINTR)
                continue;
            cprintf("[%s] select failed: %s\n", __func__, strerror(errno));
            return (total > 0) ? (int)total : -1;
        }

        if (FD_ISSET(fd_tty, &fs_read)) {
            ssize_t n = read(fd_tty, data + total, size - total);
            if (n < 0) {
                if (errno == EAGAIN || errno == EINTR)
                    continue;
                cprintf("[%s] read failed: %s\n", __func__, strerror(errno));
                return (total > 0) ? (int)total : -1;
            }
            if (n == 0) {
                cprintf("[%s] EOF, got %u/%u bytes.\n", __func__, total, size);
                return (int)total;
            }
            total += (uint32)n;
        }
    }

    return (int)total;
}

int sahara_send_packet
(
  int fd_tty,
  const uint8* packet_buffer,
  const uint32 length
)
{
    int sendbytes = WriteToComPort(fd_tty, (uint8 *)packet_buffer, length);
    //cprintf("OUT\n");
    //cprintf_hex(packet_buffer, sendbytes);

    return sendbytes;
}

int sahara_read_packet
(
    int fd_tty,
    uint8 *packet_buffer,
    size_t length
)
{
    int readbytes = ReadComPort(fd_tty, packet_buffer, length);
    //cprintf("IN\n");
    //cprintf_hex(packet_buffer, readbytes);

    return readbytes;
}

int sahara_read_memory_debug(int fd_tty, uint8 *packet_buffer, size_t length)
{
    return ReadComPort(fd_tty, packet_buffer, (uint32)length);
}

int sahara_no_cmd_id(int fd_tty)
{
    int ret = -1;
    char buf[16] = {0};
    tcflush(fd_tty, TCIOFLUSH);
    ret = sahara_send_packet(fd_tty, (uint8 *)buf, sizeof(buf));
    cprintf("[%s] ret=%d\n", __func__, ret);

    return ret;
}

int sahara_read_hello(int fd_tty)
{
    int i;
    struct sahara_packet_hello *packet_hello = (struct sahara_packet_hello*)sahara_packet_rcv_buffer;
    int cmd_len = sizeof(struct sahara_packet_hello);

    for (i=0; i<2; i++)
    {
        memset(sahara_packet_rcv_buffer, 0, sizeof(sahara_packet_rcv_buffer));
        int ret = sahara_read_packet(fd_tty, sahara_packet_rcv_buffer, cmd_len);
        cprintf("[%s] ret=%d, packet_hello->command=0x%02X, packet_hello->mode=0x%02X\n",__func__, ret, packet_hello->command, packet_hello->mode);
        if (ret == cmd_len && packet_hello->command == SAHARA_HELLO_ID) {
            return packet_hello->mode;
        }
    }

    return -1;
}

int sahara_send_hello_resp(int fd_tty, int mode)
{
    int ret = -1;
    struct sahara_packet_hello_resp *packet_hello_resp =
            (struct sahara_packet_hello_resp*)sahara_packet_buffer;
    int cmd_len = sizeof(struct sahara_packet_hello_resp);

    packet_hello_resp->command = myntohl(SAHARA_HELLO_RESP_ID);
    packet_hello_resp->length = myntohl(cmd_len);
    packet_hello_resp->version = myntohl(SAHARA_VERSION_MAJOR);
    packet_hello_resp->version_supported = myntohl(SAHARA_VERSION_MAJOR_SUPPORTED);
    packet_hello_resp->status = myntohl(SAHARA_STATUS_SUCCESS);
    packet_hello_resp->mode = myntohl(mode);
    packet_hello_resp->reserved0 = 0;
    packet_hello_resp->reserved1 = 0;
    packet_hello_resp->reserved2 = 0;
    packet_hello_resp->reserved3 = 0;
    packet_hello_resp->reserved4 = 0;
    packet_hello_resp->reserved5 = 0;

    tcflush(fd_tty, TCIOFLUSH);
    ret = sahara_send_packet(fd_tty, (uint8 *)packet_hello_resp, cmd_len);
    cprintf("[%s] ret=%d, cmd_len=%d, command=%u, length=%u, mode=%u\n", __func__, ret, cmd_len,
        myntohl(packet_hello_resp->command), myntohl(packet_hello_resp->length), myntohl(packet_hello_resp->mode));

    return ret;
}

int sahara_reset_target(int fd_tty)
{
    struct sahara_packet_reset *packet_reset = (struct sahara_packet_reset*)sahara_packet_buffer;
    int cmd_len = sizeof(struct sahara_packet_reset);

    packet_reset->command = myntohl(SAHARA_RESET_ID);
    packet_reset->length = myntohl(cmd_len);

    int ret = sahara_send_packet(fd_tty, (uint8 *)packet_reset, cmd_len);
    cprintf("[%s] ret=%d, cmd_len=%d, length=%u\n", __func__, ret, cmd_len, packet_reset->length);
    return ret;
}

int sahara_reset_target_resp(int fd_tty)
{
    struct sahara_packet_reset_resp *packet_done_resp = (struct sahara_packet_reset_resp*)sahara_packet_buffer;
    int cmd_len = sizeof(struct sahara_packet_reset_resp);

    int ret = sahara_read_packet(fd_tty, sahara_packet_rcv_buffer, cmd_len);
    cprintf("[%s] ret=%d, cmd_len=%d, length=%u, command=%u\n", __func__, ret, cmd_len,
                    myntohl(packet_done_resp->length), myntohl(packet_done_resp->command));

    if ((ret == cmd_len) && myntohl(packet_done_resp->length) == cmd_len
            && myntohl(packet_done_resp->command) == SAHARA_RESET_RESP_ID) {
        cprintf("[%s] successful.\n", __func__);
    }
    return 0;
}

int sahara_read_memory_debug_info(int fd_tty)
{
    int i;
    struct sahara_packet_memory_debug *packet_memory_debug =
                (struct sahara_packet_memory_debug*)sahara_packet_rcv_buffer;
    struct sahara_packet_memory_64bits_debug *packet_memory64_debug =
                (struct sahara_packet_memory_64bits_debug*)sahara_packet_rcv_buffer;
    int cmd_len = sizeof(struct sahara_packet_memory_debug);
    int cmd64_len = sizeof(struct sahara_packet_memory_64bits_debug);

    for (i=0; i<5; i++)
    {
        memset(sahara_packet_rcv_buffer, 0, sizeof(sahara_packet_rcv_buffer));
        int ret = sahara_read_memory_debug(fd_tty, sahara_packet_rcv_buffer, cmd_len);
        cprintf("[%s] ret=%d ",__func__, ret);
        cprintf("command=0x%02X, length=0x%02X, memory_table_addr=0x%08X, memory_table_length=0x%08X\n",
                    packet_memory_debug->command, packet_memory_debug->length,
                    packet_memory_debug->memory_table_addr, packet_memory_debug->memory_table_length);


        if (ret == cmd_len && packet_memory_debug->command == SAHARA_MEMORY_DEBUG_ID) {
            cprintf("command = SAHARA_MEMORY_DEBUG_ID\n");

            g_u32_memory_table_addr = myntohl(packet_memory_debug->memory_table_addr);
            g_u32_memory_table_length = myntohl(packet_memory_debug->memory_table_length);
            cprintf("[%s] parsed table_addr=0x%08X table_len=%u (buffer_max=%zu)\n",
                    __func__, g_u32_memory_table_addr, g_u32_memory_table_length,
                    sizeof(g_dload_debug_info));
            return 0;
        }
        else if (ret == cmd64_len && packet_memory_debug->command == SAHARA_MEMORY_DEBUG_64BITS_ID){
            cprintf("command = SAHARA_MEMORY_DEBUG_64BITS_ID\n");

            ram_dump_64bit = 1;
            g_u64_memory_table_addr = qlog_le64_fibo(packet_memory64_debug->memory_table_addr);
            g_u64_memory_table_length = qlog_le64_fibo(packet_memory64_debug->memory_table_length);
            cprintf("[%s] parsed table_addr=0x%llX table_len=%llu\n",
                    __func__, (unsigned long long)g_u64_memory_table_addr,
                    (unsigned long long)g_u64_memory_table_length);
            return 0;
        }
        #if 0
        if(i==4)
        {
            if (sahara_reset_target(fd_tty) < 0) {
                return -1;
            }
        }
        #endif
    }
    return -1;
}

int sahara_memory_read_packet(int fd_tty)
{
    int ret = -1;
    struct sahara_packet_memory_read *packet_memory_read =
            (struct sahara_packet_memory_read*)sahara_packet_buffer;
    struct sahara_packet_memory_64bits_debug *packet_memory64_read =
            (struct sahara_packet_memory_64bits_debug*)sahara_packet_buffer;

    int cmd_len = 0;

    if(ram_dump_64bit == 1){
        cmd_len = sizeof(struct sahara_packet_memory_64bits_debug);
        packet_memory64_read->command = myntohl(SAHARA_MEMORY_READ_64BITS_ID);
        packet_memory64_read->length = myntohl(cmd_len);
        packet_memory64_read->memory_table_addr = qlog_le64_fibo(g_u64_memory_table_addr);
        packet_memory64_read->memory_table_length = qlog_le64_fibo(g_u64_memory_table_length);
    }
    else{
        cmd_len = sizeof(struct sahara_packet_memory_read);
        packet_memory_read->command = myntohl(SAHARA_MEMORY_READ_ID);
        packet_memory_read->length = myntohl(cmd_len);
        packet_memory_read->memory_addr = myntohl(g_u32_memory_table_addr);
        packet_memory_read->memory_length = myntohl(g_u32_memory_table_length);
    }

    tcflush(fd_tty, TCIFLUSH);
    ret = sahara_send_packet(fd_tty, (uint8 *)packet_memory_read, cmd_len);

    return ret;
}

int sahara_memory64_read_packet_resp(int fd_tty){
    int i;
    int readbytes = -1;
    uint64_t table_len = g_u64_memory_table_length;
    int num_entries = 0;

    memset(g_dload_debug64_info, 0, sizeof(g_dload_debug64_info));

    if (table_len == 0 || table_len > sizeof(g_dload_debug64_info)
            || table_len > sizeof(sahara_packet_rcv_buffer)
            || (table_len % sizeof(dload_debug_type_64)) != 0) {
        cprintf("[%s] invalid memory table length: 0x%llX\n", __func__, (unsigned long long)table_len);
        return -1;
    }

    num_entries = (int)(table_len / sizeof(dload_debug_type_64));

    cprintf("%s start, table_len=%llu, num_entries=%d, read_max=%zu.\n", __func__,
            (unsigned long long)table_len, num_entries, sizeof(sahara_packet_rcv_buffer));
    for (i=0; i<2; i++)
    {
        if (i == 0)
            memset(sahara_packet_rcv_buffer, 0, sizeof(sahara_packet_rcv_buffer));
        readbytes = sahara_read_packet(fd_tty, sahara_packet_rcv_buffer, (size_t)table_len);
        if (readbytes == (int)table_len)
        {
            memcpy(g_dload_debug64_info, sahara_packet_rcv_buffer, (size_t)table_len);
            break;
        }
        cprintf("[%s] retry %d, read %d/%llu bytes\n", __func__, i + 1, readbytes,
                (unsigned long long)table_len);
        if (readbytes > 0) {
            break;
        }
        readbytes = -1;
    }

    if (readbytes > 0 && (uint64_t)readbytes != table_len) {
        cprintf("[%s] short read %d/%llu, abort\n", __func__, readbytes,
                (unsigned long long)table_len);
        return -1;
    }

    if (readbytes == (int)table_len) {
        char path[PATH_MAX] = {0};
        snprintf(path, sizeof(path), "%s/dump_info.txt", sahara_dump_path);
        FILE *fp = fopen(path, "a");
        if (fp) {
            fprintf(fp, "---------------------------------------------------------------------\n");
            fprintf(fp, "num    pref    base        length                region    filename\n");
            fprintf(fp, "---------------------------------------------------------------------\n");
        }
        cprintf("---------------------------------------------------------------------\n");
        cprintf("num    pref    base        length                region    filename\n");
        cprintf("---------------------------------------------------------------------\n");
        for (i=0; i<num_entries; i++) {
            dload_debug_type_64 *dinfo = &g_dload_debug64_info[i];
            if (dinfo->save_pref) {
                cprintf("%3d %llu    %llu  %llu  %20s     %s\n", i, dinfo->save_pref,
                        dinfo->mem_base, dinfo->length, dinfo->desc, dinfo->filename);

                if (fp) {
                    fprintf(fp, "%3d %llu   %llu %llu  %20s     %s\n", i, dinfo->save_pref,
                        dinfo->mem_base, dinfo->length, dinfo->desc, dinfo->filename);
                }
            }
        }
        if (fp) {
            fclose(fp);
        }
        cprintf("---------------------------------------------------------------------\n");
    }
    return readbytes;
}

int sahara_memory_read_packet_resp(int fd_tty)
{
    int i;
    int readbytes = -1;
    uint32_t table_len = g_u32_memory_table_length;
    int num_entries = 0;

    memset(g_dload_debug_info, 0, sizeof(g_dload_debug_info));

    if (table_len == 0 || table_len > sizeof(g_dload_debug_info)
            || table_len > sizeof(sahara_packet_rcv_buffer)
            || (table_len % sizeof(dload_debug_type)) != 0) {
        cprintf("[%s] invalid memory table length: 0x%08X\n", __func__, table_len);
        return -1;
    }

    num_entries = (int)(table_len / sizeof(dload_debug_type));

    cprintf("%s start, table_len=%u, num_entries=%d, read_max=%zu.\n", __func__,
            table_len, num_entries, sizeof(sahara_packet_rcv_buffer));
    for (i=0; i<2; i++)
    {
        if (i == 0)
            memset(sahara_packet_rcv_buffer, 0, sizeof(sahara_packet_rcv_buffer));
        readbytes = sahara_read_packet(fd_tty, sahara_packet_rcv_buffer, table_len);
        if (readbytes == (int)table_len)
        {
            memcpy(g_dload_debug_info, sahara_packet_rcv_buffer, table_len);
            break;
        }
        cprintf("[%s] retry %d, read %d/%u bytes\n", __func__, i + 1, readbytes, table_len);
        if (readbytes > 0) {
            break;
        }
        readbytes = -1;
    }

    if (readbytes > 0 && (uint32_t)readbytes != table_len) {
        cprintf("[%s] short read %d/%u, abort (sync latest dump_api.c if expect %u)\n",
                __func__, readbytes, table_len, table_len);
        return -1;
    }

    if (readbytes == (int)table_len) {
        char path[PATH_MAX] = {0};
        snprintf(path, sizeof(path), "%s/dump_info.txt", sahara_dump_path);
        FILE *fp = fopen(path, "a");
        if (fp) {
            fprintf(fp, "---------------------------------------------------------------------\n");
            fprintf(fp, "num    pref    base        length                region    filename\n");
            fprintf(fp, "---------------------------------------------------------------------\n");
        }
        cprintf("---------------------------------------------------------------------\n");
        cprintf("num    pref    base        length                region    filename\n");
        cprintf("---------------------------------------------------------------------\n");
        for (i=0; i<num_entries; i++) {
            dload_debug_type *dinfo = &g_dload_debug_info[i];
            if (dinfo->save_pref) {
                cprintf("%3d %4d    0x%08X %8d  %20s     %s\n", i, dinfo->save_pref,
                        dinfo->mem_base, dinfo->length, dinfo->desc, dinfo->filename);

                if (fp) {
                    fprintf(fp, "%3d %4d    0x%08X %8d  %20s     %s\n", i, dinfo->save_pref,
                        dinfo->mem_base, dinfo->length, dinfo->desc, dinfo->filename);
                }
            }
        }
        if (fp) {
            fclose(fp);
        }
        cprintf("---------------------------------------------------------------------\n");
    }
    return readbytes;
}

static int dump_region_indices[NUM_REGIONS];

static int dump_build_region_order_32(int num_entries)
{
    int i;
    int count = 0;
    int max_idx = -1;
    unsigned int max_len = 0;

    for (i = 0; i < num_entries; i++) {
        dload_debug_type *dinfo = &g_dload_debug_info[i];
        if (dinfo->save_pref == 0)
            continue;
        if (max_idx < 0 || dinfo->length > max_len) {
            max_len = dinfo->length;
            max_idx = i;
        }
    }

    for (i = 0; i < num_entries; i++) {
        if (g_dload_debug_info[i].save_pref == 0)
            continue;
        if (i == max_idx)
            continue;
        dump_region_indices[count++] = i;
    }

    if (max_idx >= 0) {
        dump_region_indices[count++] = max_idx;
        cprintf("[%s] defer largest region [%d][%s] len=%u to end\n",
                __func__, max_idx, g_dload_debug_info[max_idx].filename, max_len);
    }

    return count;
}

static int dump_build_region_order_64(int num_entries)
{
    int i;
    int count = 0;
    int max_idx = -1;
    uint64_t max_len = 0;

    for (i = 0; i < num_entries; i++) {
        dload_debug_type_64 *dinfo = &g_dload_debug64_info[i];
        if (dinfo->save_pref == 0)
            continue;
        if (max_idx < 0 || dinfo->length > max_len) {
            max_len = dinfo->length;
            max_idx = i;
        }
    }

    for (i = 0; i < num_entries; i++) {
        if (g_dload_debug64_info[i].save_pref == 0)
            continue;
        if (i == max_idx)
            continue;
        dump_region_indices[count++] = i;
    }

    if (max_idx >= 0) {
        dump_region_indices[count++] = max_idx;
        cprintf("[%s] defer largest region [%d][%s] len=%llu to end\n",
                __func__, max_idx, g_dload_debug64_info[max_idx].filename,
                (unsigned long long)max_len);
    }

    return count;
}


/* Read one Sahara memory-read response and append raw bytes to an open file. */
static int sahara_dump_chunk_to_fp(int fd_tty, FILE *fp, int length, char *name)
{
#ifdef CONFIG_QCOM_DUMP
    uint8 readbuf[SAHARA_DUMP_READ_BUF_SIZE] = {0};
    int left_len = length;

    if (fp == NULL || length <= 0)
        return -1;

    while (left_len > 0) {
        int toread = MIN(left_len, (int)sizeof(readbuf));
        int readbytes = sahara_read_packet(fd_tty, readbuf, toread);
        if (readbytes != toread) {
            cprintf("[%s] [%s] read %d/%d bytes, remaining:%d\n",
                    __func__, name, readbytes, toread, left_len);
            return -1;
        }

        if (fwrite(readbuf, 1, (size_t)readbytes, fp) != (size_t)readbytes) {
            cprintf("[%s] fwrite failed for %s\n", __func__, name);
            return -1;
        }

        left_len -= readbytes;
    }

    /* Rely on setvbuf; caller fflush/closes after region or on retry. */
    return 0;
#else
    (void)fd_tty;
    (void)fp;
    (void)length;
    (void)name;
    printf("Error:dump function should define CONFIG_QCOM_DUMP Macro\r\n");
    return -1;
#endif
}

static int sahara_send_memory_read32(int fd_tty, uint32_t mem_addr, uint32_t mem_len)
{
    struct sahara_packet_memory_read *packet_memory_read =
            (struct sahara_packet_memory_read *)sahara_packet_buffer;
    int cmd_len = sizeof(struct sahara_packet_memory_read);

    packet_memory_read->command = myntohl(SAHARA_MEMORY_READ_ID);
    packet_memory_read->length = myntohl(cmd_len);
    packet_memory_read->memory_addr = myntohl(mem_addr);
    packet_memory_read->memory_length = myntohl(mem_len);

    tcflush(fd_tty, TCIFLUSH);
    return sahara_send_packet(fd_tty, (uint8 *)packet_memory_read, cmd_len);
}

static int sahara_dump_text_region(int fd_tty, dload_debug_type *dinfo, int region_idx)
{
    char path[PATH_MAX] = {0};
    uint8_t *buf = NULL;
    int attempt;
    int ret = -1;

    if (dinfo->length <= 0 || dinfo->length > sizeof(sahara_packet_rcv_buffer)) {
        cprintf("[%s] invalid text region length %u for %s\n",
                __func__, dinfo->length, dinfo->filename);
        return -1;
    }

    snprintf(path, sizeof(path), "%s/%.20s", sahara_dump_path, dinfo->filename);
    buf = (uint8_t *)malloc(dinfo->length);
    if (buf == NULL)
        return -1;

    for (attempt = 0; attempt < MAX_RETRY_TIMES; attempt++) {
        int readbytes;

        if (attempt > 0) {
            cprintf("[%s] retry %d for text region [%d][%s]\n",
                    __func__, attempt + 1, region_idx, dinfo->filename);
            tcflush(fd_tty, TCIOFLUSH);
            usleep(200000);
        }

        if (sahara_send_memory_read32(fd_tty, dinfo->mem_base, dinfo->length) < 0)
            continue;

        readbytes = sahara_read_packet(fd_tty, buf, dinfo->length);
        if (readbytes != (int)dinfo->length) {
            cprintf("[%s] [%s] text read %d/%u\n",
                    __func__, dinfo->filename, readbytes, dinfo->length);
            continue;
        }

        {
            FILE *fp = fopen(path, "wb");
            if (fp == NULL) {
                cprintf("[%s] open '%s' failed\n", __func__, path);
                break;
            }
            if (fwrite(buf, 1, (size_t)readbytes, fp) != (size_t)readbytes) {
                cprintf("[%s] fwrite '%s' failed\n", __func__, path);
                fclose(fp);
                unlink(path);
                continue;
            }
            fclose(fp);
        }

        cprintf("[%s] saved text region [%d][%s]\n", __func__, region_idx, dinfo->filename);
        ret = 0;
        break;
    }

    free(buf);
    return ret;
}

static int sahara_dump_binary_region(int fd_tty, dload_debug_type *dinfo, int region_idx)
{
    int chunk_size = packet_memory_read_memory_length;
    int left_len = dinfo->length;
    uint32_t offset = 0;
    char raw_path[PATH_MAX] = {0};
    FILE *fp = NULL;
    uint32_t last_log_offset = UINT32_MAX;

    snprintf(raw_path, sizeof(raw_path), "%s/%.20s", sahara_dump_path, dinfo->filename);

    fp = fopen(raw_path, "wb");
    if (fp == NULL) {
        cprintf("[%s] open '%s' failed: %s\n", __func__, raw_path, strerror(errno));
        return -1;
    }
    /* Larger stdio buffer: drain USB first, write disk in bigger batches. */
    setvbuf(fp, NULL, _IOFBF, 256 * 1024);

    while (left_len > 0) {
        int toread = MIN(left_len, chunk_size);

        if (offset == 0 || offset - last_log_offset >= SAHARA_DUMP_PROGRESS_STEP) {
            cprintf("[%d][%s] offset:%u/%u chunk:%d\n",
                    region_idx, dinfo->filename, offset, dinfo->length, toread);
            last_log_offset = offset;
        }

        /*
         * Once the USB/tty link drops, shrinking chunk + retry cannot recover.
         * Fail this region immediately and keep whatever was already written.
         */
        if (sahara_send_memory_read32(fd_tty, dinfo->mem_base + offset, (uint32_t)toread) < 0 ||
            sahara_dump_chunk_to_fp(fd_tty, fp, toread, dinfo->filename) != 0) {
            fclose(fp);
            if (offset > 0) {
                cprintf("[%s] region [%d][%s] link failed, keep partial dump %s (%u bytes)\n",
                        __func__, region_idx, dinfo->filename, raw_path, offset);
            } else {
                cprintf("[%s] region [%d][%s] link failed, no data saved\n",
                        __func__, region_idx, dinfo->filename);
                unlink(raw_path);
            }
            return -1;
        }

        offset += (uint32_t)toread;
        left_len -= toread;
    }

    fclose(fp);
    cprintf("[%s] saved binary region [%d][%s] (%u bytes)\n",
            __func__, region_idx, dinfo->filename, dinfo->length);
    return 0;
}

#if 0
static int sahara_dump_mem_resp(int fd_tty, char *filename, int length, int append, char *name)
{
    int ret = -1;
    uint8 readbuf[SAHARA_MAX_MEMORY_DATA_SIZE_IN_BYTES] = {0};
    char file_name[64] = {0};
    //gzFile gzfile = NULL;
    int left_len = length;

    sprintf(file_name, "%s/%s", sahara_dump_path, filename);

    //gzfile = gzopen(file_name, append? "ab6" : "wb6");
    //gzfile = gzopen(file_name, append? "ab6" : "wb6");
    FILE *fp = fopen(file_name, "ab+");
    if (fp == NULL) {
        cprintf("[%s] open '%s' failed\n", __func__, file_name);
        goto END;
    }

    while (left_len > 0)
    {
        int toread = MIN(left_len, sizeof(readbuf));
        int readbytes = sahara_read_packet(fd_tty, readbuf, toread);
        if (readbytes < 0) {
            cprintf("[%s] [%s] total:%d, remaining:%d, toread:%d\n", __func__, name, length, left_len, toread);
            cprintf("[%s] sahara_read_packet failed.\n", __func__);
            goto END;
        }

        if (readbytes == 0) {
            cprintf("ERROR: [%s] [%s] total:%d, remaining:%d, toread:%d\n", __func__, name, length, left_len, toread);
            goto END;
        }

        if (fwrite(readbuf, sizeof(readbuf),readbytes,fp)!= readbytes)
        
        //gzwrite(gzfile, readbuf, readbytes) != readbytes)
        {
            cprintf("[%s] Fwrite failed.\n", __func__);
            goto END;
        }
        left_len -= readbytes;
    }
    ret = 0;

END:
    if (fclose(fp) != 0)
        //gzclose(gzfile) != Z_OK) 
    {
        cprintf("[%s] fclose failed\n", __func__);
    }
    return ret;
}
#endif

int sahara_dump_mem(int fd_tty)
{
    int failed_regions = 0;
    uint32_t table_len = g_u32_memory_table_length;
    int num_entries = 0;

    if (table_len == 0 || (table_len % sizeof(dload_debug_type)) != 0) {
        cprintf("[%s] invalid memory table length: 0x%08X\n", __func__, table_len);
        return -1;
    }

    num_entries = (int)(table_len / sizeof(dload_debug_type));

    {
        int order_count = dump_build_region_order_32(num_entries);
        int o;

        for (o = 0; o < order_count; o++) {
            int idx = dump_region_indices[o];
            dload_debug_type *dinfo = &g_dload_debug_info[idx];
            int region_ret;

        cprintf("---------------------------------------------------------------------\n");
        cprintf("num  pref    base        length                region    filename\n");
        cprintf("---------------------------------------------------------------------\n");
        cprintf("%3d %4d    0x%08X %8d  %20s     %s\n", idx, dinfo->save_pref,
                    dinfo->mem_base, dinfo->length, dinfo->desc, dinfo->filename);
        cprintf("---------------------------------------------------------------------\n");

        if (dinfo->save_pref == 2) {
            region_ret = sahara_dump_text_region(fd_tty, dinfo, idx);
        } else if (dinfo->save_pref == 1) {
            region_ret = sahara_dump_binary_region(fd_tty, dinfo, idx);
        } else {
            cprintf("[%s] skip unsupported save_pref=%d for %s\n",
                    __func__, dinfo->save_pref, dinfo->filename);
            continue;
        }

        if (region_ret < 0) {
            failed_regions++;
            cprintf("[%s] WARNING region [%d][%s] skipped, continue next region\n",
                    __func__, idx, dinfo->filename);
        }
        }
    }

    if (failed_regions > 0) {
        cprintf("[%s] partial dump finished, %d region(s) failed/skipped\n",
                __func__, failed_regions);
    }

    return failed_regions;
}

static int sahara_send_memory_read64(int fd_tty, uint64_t mem_addr, uint64_t mem_len)
{
    struct sahara_packet_memory_64bits_read *packet_memory64_read =
            (struct sahara_packet_memory_64bits_read *)sahara_packet_buffer;
    int cmd_len = sizeof(struct sahara_packet_memory_64bits_read);

    packet_memory64_read->command = myntohl(SAHARA_MEMORY_READ_64BITS_ID);
    packet_memory64_read->length = myntohl(cmd_len);
    packet_memory64_read->memory_addr = qlog_le64_fibo(mem_addr);
    packet_memory64_read->memory_length = qlog_le64_fibo(mem_len);

    tcflush(fd_tty, TCIFLUSH);
    return sahara_send_packet(fd_tty, (uint8 *)packet_memory64_read, cmd_len);
}

static int sahara_dump64_text_region(int fd_tty, dload_debug_type_64 *dinfo, int region_idx)
{
    char path[PATH_MAX] = {0};
    uint8_t *buf = NULL;
    int attempt;
    int ret = -1;
    uint64_t text_len = dinfo->length;

    if (text_len == 0 || text_len > sizeof(sahara_packet_rcv_buffer)) {
        cprintf("[%s] invalid text region length %llu for %s\n",
                __func__, (unsigned long long)text_len, dinfo->filename);
        return -1;
    }

    snprintf(path, sizeof(path), "%s/%.20s", sahara_dump_path, dinfo->filename);
    buf = (uint8_t *)malloc((size_t)text_len);
    if (buf == NULL)
        return -1;

    for (attempt = 0; attempt < MAX_RETRY_TIMES; attempt++) {
        int readbytes;

        if (attempt > 0) {
            cprintf("[%s] retry %d for text region [%d][%s]\n",
                    __func__, attempt + 1, region_idx, dinfo->filename);
            tcflush(fd_tty, TCIOFLUSH);
            usleep(200000);
        }

        if (sahara_send_memory_read64(fd_tty, dinfo->mem_base, text_len) < 0)
            continue;

        readbytes = sahara_read_packet(fd_tty, buf, (int)text_len);
        if (readbytes != (int)text_len) {
            cprintf("[%s] [%s] text read %d/%llu\n",
                    __func__, dinfo->filename, readbytes, (unsigned long long)text_len);
            continue;
        }

        {
            FILE *fp = fopen(path, "wb");
            if (fp == NULL) {
                cprintf("[%s] open '%s' failed\n", __func__, path);
                break;
            }
            if (fwrite(buf, 1, (size_t)readbytes, fp) != (size_t)readbytes) {
                cprintf("[%s] fwrite '%s' failed\n", __func__, path);
                fclose(fp);
                unlink(path);
                continue;
            }
            fclose(fp);
        }

        cprintf("[%s] saved text region [%d][%s]\n", __func__, region_idx, dinfo->filename);
        ret = 0;
        break;
    }

    free(buf);
    return ret;
}

static int sahara_dump64_binary_region(int fd_tty, dload_debug_type_64 *dinfo, int region_idx)
{
    int chunk_size = packet_memory_read_memory_length;
    uint64_t left_len = dinfo->length;
    uint64_t offset = 0;
    char raw_path[PATH_MAX] = {0};
    FILE *fp = NULL;
    uint64_t last_log_offset = UINT64_MAX;

    snprintf(raw_path, sizeof(raw_path), "%s/%.20s", sahara_dump_path, dinfo->filename);

    fp = fopen(raw_path, "wb");
    if (fp == NULL) {
        cprintf("[%s] open '%s' failed: %s\n", __func__, raw_path, strerror(errno));
        return -1;
    }
    setvbuf(fp, NULL, _IOFBF, 256 * 1024);

    while (left_len > 0) {
        int toread = (int)MIN((uint64_t)left_len, (uint64_t)chunk_size);

        if (offset == 0 || offset - last_log_offset >= SAHARA_DUMP_PROGRESS_STEP) {
            cprintf("[%d][%s] offset:%llu/%llu chunk:%d\n",
                    region_idx, dinfo->filename,
                    (unsigned long long)offset,
                    (unsigned long long)dinfo->length, toread);
            last_log_offset = offset;
        }

        /* Same as 32-bit: no retry/shrink after link failure. */
        if (sahara_send_memory_read64(fd_tty, dinfo->mem_base + offset, (uint64_t)toread) < 0 ||
            sahara_dump_chunk_to_fp(fd_tty, fp, toread, dinfo->filename) != 0) {
            fclose(fp);
            if (offset > 0) {
                cprintf("[%s] region [%d][%s] link failed, keep partial dump %s (%llu bytes)\n",
                        __func__, region_idx, dinfo->filename, raw_path,
                        (unsigned long long)offset);
            } else {
                cprintf("[%s] region [%d][%s] link failed, no data saved\n",
                        __func__, region_idx, dinfo->filename);
                unlink(raw_path);
            }
            return -1;
        }

        offset += (uint64_t)toread;
        left_len -= (uint64_t)toread;
    }

    fclose(fp);
    cprintf("[%s] saved binary region [%d][%s] (%llu bytes)\n",
            __func__, region_idx, dinfo->filename,
            (unsigned long long)dinfo->length);
    return 0;
}

int sahara_dump64_mem(int fd_tty)
{
    int failed_regions = 0;
    uint64_t table_len = g_u64_memory_table_length;
    int num_entries = 0;

    if (table_len == 0 || (table_len % sizeof(dload_debug_type_64)) != 0) {
        cprintf("[%s] invalid memory table length: 0x%llX\n", __func__, (unsigned long long)table_len);
        return -1;
    }

    num_entries = (int)(table_len / sizeof(dload_debug_type_64));

    {
        int order_count = dump_build_region_order_64(num_entries);
        int o;

        for (o = 0; o < order_count; o++) {
            int idx = dump_region_indices[o];
            dload_debug_type_64 *dinfo = &g_dload_debug64_info[idx];
            int region_ret;

        cprintf("---------------------------------------------------------------------\n");
        cprintf("num  pref    base        length                region    filename\n");
        cprintf("---------------------------------------------------------------------\n");
        cprintf("%3d %llu    %llu %llu  %20s     %s\n", idx, dinfo->save_pref,
                    dinfo->mem_base, dinfo->length, dinfo->desc, dinfo->filename);
        cprintf("---------------------------------------------------------------------\n");

        if (dinfo->save_pref == 2) {
            region_ret = sahara_dump64_text_region(fd_tty, dinfo, idx);
        } else if (dinfo->save_pref == 1) {
            region_ret = sahara_dump64_binary_region(fd_tty, dinfo, idx);
        } else {
            cprintf("[%s] skip unsupported save_pref=%llu for %s\n",
                    __func__, (unsigned long long)dinfo->save_pref, dinfo->filename);
            continue;
        }

        if (region_ret < 0) {
            failed_regions++;
            cprintf("[%s] WARNING region [%d][%s] skipped, continue next region\n",
                    __func__, idx, dinfo->filename);
        }
        }
    }

    if (failed_regions > 0) {
        cprintf("[%s] partial dump finished, %d region(s) failed/skipped\n",
                __func__, failed_regions);
    }

    return failed_regions;
}

static void dump_remove_dir_if_only_log(void)
{
    DIR *dir;
    struct dirent *ent;
    int entry_count = 0;
    int only_dump_log = 0;
    char log_path[PATH_MAX] = {0};

    if (sahara_dump_path[0] == '\0')
        return;

    dir = opendir(sahara_dump_path);
    if (dir == NULL)
        return;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        entry_count++;
        if (entry_count == 1 && strcmp(ent->d_name, "dump_pull.log") == 0)
            only_dump_log = 1;
        else
            only_dump_log = 0;
    }
    closedir(dir);

    if (entry_count != 1 || !only_dump_log)
        return;

    snprintf(log_path, sizeof(log_path), "%s/dump_pull.log", sahara_dump_path);
    unlink(log_path);
    if (rmdir(sahara_dump_path) == 0) {
        printf("[%s] removed empty dump dir: %s\n", __func__, sahara_dump_path);
    } else {
        printf("[%s] failed to remove dump dir '%s': %s\n",
                __func__, sahara_dump_path, strerror(errno));
    }
}


int dump_log_collect(int edl_port)
{
    int ret = -1;
    int mode = 0;
    int regions_failed = 0;
    struct timeval startTime;
    struct timeval endTime;
    memset(&startTime, 0, sizeof(struct timeval));
    memset(&endTime, 0, sizeof(struct timeval));
    gettimeofday(&startTime, NULL); //get start time

    dump_pull_log_open();
    
    printf("[%s] start sahara_no_cmd_id (dump_api protocol-length build)\n", __func__);
    fflush(stdout);
    if (sahara_no_cmd_id(edl_port) < 0) {
        goto END;
    }
    mode = sahara_read_hello(edl_port);
    if (mode < 0) {
        goto END;
    }
    if (sahara_send_hello_resp(edl_port, mode) < 0) {
        goto END;
    }
    if (sahara_read_memory_debug_info(edl_port) < 0) {
        goto END;
    }
    if (sahara_memory_read_packet(edl_port) < 0) {
        goto END;
    }
    if(ram_dump_64bit){
        if (sahara_memory64_read_packet_resp(edl_port) < 0) {
            goto END;
        }
        regions_failed = sahara_dump64_mem(edl_port);
        if (regions_failed < 0) {
            goto END;
        }
        if (regions_failed > 0) {
            cprintf("[%s] %d region(s) skipped; continue reset for partial dump\n",
                    __func__, regions_failed);
        }
    }
    else{
        if (sahara_memory_read_packet_resp(edl_port) < 0) {
            goto END;
        }
        regions_failed = sahara_dump_mem(edl_port);
        if (regions_failed < 0) {
            goto END;
        }
        if (regions_failed > 0) {
            cprintf("[%s] %d region(s) skipped; continue reset for partial dump\n",
                    __func__, regions_failed);
        }
    }

    if (sahara_reset_target(edl_port) < 0) {
        cprintf("[%s] reset failed (port may be closed after large region), keep dump files\n",
                __func__);
    } else if (sahara_reset_target_resp(edl_port) < 0) {
        cprintf("[%s] reset resp failed, keep dump files\n", __func__);
    }
    if (regions_failed > 0) {
        cprintf("-------------- [Partial Finished] -----------------\n");
    } else {
        cprintf("-------------- [Finished] -----------------\n");
    }
    ret = 0;
    gettimeofday(&endTime, NULL); //get end time
    show_used_time(&startTime, &endTime);
    dump_pull_log_close();
    return ret;
END:
    cprintf("-------------- [excepted] -----------------\n");
    ret = -1;
    sahara_reset_target(edl_port);
    gettimeofday(&endTime, NULL); //get end time
    show_used_time(&startTime, &endTime);
    dump_pull_log_close();
    dump_remove_dir_if_only_log();
    return ret;
}

static int mkdirs(char *path)
{
    int i;
    int len=strlen(path);
    char str[len+1];
    printf("path=%s\n",path);
    strncpy(str, path, sizeof(str));
    printf("str=%s\n",str);
    for(i=0; i<len; i++)
    {
        if (str[i]=='/' )
        {
            str[i] = '\0';
            if (access(str,0))
            {
                umask(0);
                if (mkdir(str, 0777))
                {
                    printf("mkdir '%s' failed.\n", str);
                    perror("mkdir"),exit(-1);
                }
            }
            str[i]='/';
        }
    }

    if (len>0 && access(str, 0))
    {
        umask(0); 
        if (mkdir(str, 0777)!=0)
        {
            printf("1mkdir '%s' failed.\n", str);
            perror("mkdir"),exit(-1);
        }
    }
    return 0;
}

int set_sahara_dump_path(char *file_path)
{
    printf("file_path=%s\n",file_path);
    if (file_path == NULL) {
        printf("sahara_dump_path1=%s\n",sahara_dump_path);
        snprintf(sahara_dump_path, sizeof(sahara_dump_path), "%s", ".");
        printf("sahara_dump_path2=%s\n",sahara_dump_path);
    } else { 
        snprintf(sahara_dump_path, sizeof(sahara_dump_path), "%s", file_path);
        printf("sahara_dump_path22=%s\n",sahara_dump_path);
    }

    printf("[%s] sahara_dump_path=%s\n", __func__, sahara_dump_path);

    return mkdirs(sahara_dump_path);
}

void show_used_time(struct timeval *startTime, struct timeval *endTime)
{
    struct timeval diff;

    //DEBUG_INFO("%s start.\n", __func__);

    if (startTime->tv_sec > endTime->tv_sec)
        return;

    if ((startTime->tv_sec == endTime->tv_sec) && (startTime->tv_usec > endTime->tv_usec))
        return;

    if (endTime->tv_usec >= startTime->tv_usec) {
        diff.tv_sec = endTime->tv_sec - startTime->tv_sec;
        diff.tv_usec = endTime->tv_usec - startTime->tv_usec;
    } else {
        endTime->tv_sec--;
        diff.tv_sec = endTime->tv_sec - startTime->tv_sec;
        diff.tv_usec = (1000*1000) + endTime->tv_usec-startTime->tv_usec;
    }
    cprintf("========================================\n");
    cprintf("Used Time: %d.%ds\n",
                (int)diff.tv_sec,
                (int)diff.tv_usec/1000);
    cprintf("========================================\n");
}

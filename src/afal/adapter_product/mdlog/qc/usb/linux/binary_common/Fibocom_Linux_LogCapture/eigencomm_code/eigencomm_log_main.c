/*
 ============================================================================
 Name        : ECLog.c
 Author      : sxwang
 Version     :
 Copyright   : EigenComm
 Description : prj in C, Ansi-style
 ============================================================================
 */

#include "logdef.h"
#include <sys/socket.h>
#include <linux/netlink.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <libgen.h>
#include "queue.h"
#include "misc_usb.h"
#include "eigencomm_devices_list.h"
#include "log_control.h"


#define UEVENT_BUFFER_SIZE      2048
#define MAX_BIN_LOG_FILE_SIZE   (200 * 1024 * 1024)
#define TIME_INTER_READ_FLAG    (5* 1000)
#define BAND_RATE               3000000
#define READ_BUF_LEN            65535

typedef struct
{
    char* dev;
    int   speed;
    int   fd;
    pthread_t tid;
}ttyuart;


typedef struct _PacketTime
{
    unsigned char prefix[8];// = { 0xBA, 0xBA, 0xBA, 0xBA, 0xBA, 0xBA, 0xBA, 0xAA };
    SYSTEMTIME time;
    unsigned char suffix[8];// = { 0xBA, 0xBA, 0xBA, 0xBA, 0xBA, 0xBA, 0xBA, 0xAA };
}PacketTime;

QUEUE *g_queue;
PacketTime pkg_time;
int g_dumpmode = 0;
pthread_t tid_ramdump = 0;
int g_write_mode = 0;

static char s_logpath[FIBO_BUF_SIZE+EXTEND] = ".";
static int single_logfile_size = 50*1024*1024;
static int s_max_file_num = 0;
static uint8_t s_buf[READ_BUF_LEN] = {0};


fibo_usbdev_t *eigencomm_find_devices_in_table(int idvendor, int idproduct)
{
    int i, size = 0;

    size = sizeof(eigencomm_devices_table)/sizeof(eigencomm_devices_table[0]);
    for (i=0; i<size; i++)
    {
        fibo_usbdev_t *udev = &eigencomm_devices_table[i];

        if ((udev->idVendor == idvendor) && (udev->idProduct == idproduct)) {
            return udev;
        }
    }

    return NULL;
}


static void usage(char *arg)
{
    trace_log("========================================\n");
    trace_log("Usage:\n");
    trace_log("%s <-d [diag port]> <-s [log save dir]> <-m [single logfile size(MB)]> <-n [max_log_filenum]>\n", arg);
    trace_log("example: %s\n", arg);
    trace_log("========================================\n");
    exit(-1);
}

int openuart(char* dev, int bdr)
{
    trace_log("[%s] start\n", __func__);
    int fd = open(dev, O_RDWR);
    if (fd == -1)
    {
        trace_log("Open device %s failed, error:%d(%s)\n", dev, errno, strerror(errno));
    }
    else
    {
        struct termios2 tty;

        if (ioctl(fd, TCGETS2, &tty) != -1)
        {
            // Set num. data bits
            tty.c_cflag     &=  ~CSIZE;			// CSIZE is a mask for the number of bits per character
            tty.c_cflag     |=  CS8;
            // Set parity
            tty.c_cflag     &=  ~PARENB; //NONE
            // Set num. stop bits
            tty.c_cflag     &=  ~CSTOPB;
            // Configure flow control
            tty.c_cflag &= ~CRTSCTS;

            tty.c_cflag     |=  CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
            //Set baud rates
            tty.c_cflag &= ~CBAUD;
            tty.c_cflag |= CBAUDEX;
            tty.c_ispeed = bdr;
            tty.c_ospeed = bdr;

            tty.c_oflag     =   0;              // No remapping, no delays
            tty.c_oflag     &=  ~OPOST;         // Make raw

            tty.c_cc[VTIME] = 0; 
            tty.c_cc[VMIN] = 1;
            //.c_iflag
            tty.c_iflag &= ~(IXON | IXOFF | IXANY);
            tty.c_iflag 	&= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|INPCK|IGNPAR|IUCLC|IMAXBEL|IUTF8);
            //LOCAL MODES (c_lflag)
            tty.c_lflag		&= ~ICANON;    // Turn off canonical input, which is suitable for pass-through
            tty.c_lflag &= ~(ECHO);
            tty.c_lflag		&= ~ECHOE;     // Turn off echo erase (echo erase only relevant if canonical input is active)
            //tty.c_lflag		&= ~ECHONL;    //
            tty.c_lflag		&= ~ISIG;      // Disables recognition of INTR (interrupt), QUIT and SUSP (suspend) characters
            
            if(ioctl(fd, TCSETS2, &tty) == -1)
                trace_log("set uart arg failed, error:%d", errno);
        }
    }

    trace_log("open dev: %s, result:%d\n", dev, fd);

    return fd;
}

size_t write_uart_binary(int fd, uint8_t* pbuf, size_t size)
{
    size_t write_size = 0;
    if ( g_dumpmode == 0)
        write_size = write(fd, pbuf, size);
    else
    {
        g_write_mode = 1;
        //trace_log("enter write\n");
        //pthread_mutex_lock(&mutex_read_write);
        write_size = write(fd, pbuf, size);
        //pthread_mutex_unlock(&mutex_read_write);
        g_write_mode = 0;
        //trace_log("enter write end\n");
    }
    return write_size;
}

size_t read_uart_binary(int fd, uint8_t* pbuf, size_t size, int uartfd)
{
    size_t readcount = 0;
    
    if ( g_dumpmode == 0)
        readcount = read(fd, pbuf, size);
    else
    {
        //trace_log("enter read\n");
        //pthread_mutex_lock(&mutex_read_write);
        readcount = read(fd, pbuf, size);
        //pthread_mutex_unlock(&mutex_read_write);
        //trace_log("enter read end\n");
    }

    if (readcount > 0)
    {
        Add_Queue_Item(g_queue, (char*)pbuf, readcount, uartfd);
    }
    return readcount;
}

size_t get_data(uint8_t* pBuf)
{
    QUEUE_ITEM *item = NULL;
    size_t size = 0;
     if (g_queue->items) 
        {
            item = Get_Queue_Item(g_queue);
            if (item && item->sz > 0)
                memcpy(pBuf, item->data, item->sz);
            size = item->sz;
            Free_Queue_Item(item);
        }

    return size;
}


void clear_data()
{
    QUEUE_ITEM *item = NULL;
    while (g_queue->items) 
    {
        item = Get_Queue_Item(g_queue);
        Free_Queue_Item(item);
    }
}

int find_dump_id(uint8_t* buf_data, size_t size, int* usbmode)
{
	const unsigned char szChar[4] = { 0x22, 0x04, 0x02, 0x00};
	const unsigned char szUartChar[4] = { 0x22, 0x04, 0x01, 0x80};
	const unsigned char c = 0x22;

	unsigned char* pBuf = NULL;
    int nLoc = 0;
    int signSize = 4 /*sizeof(szChar)*/;

    while ((pBuf = (unsigned char*)memchr(buf_data + nLoc, c, size)) != NULL)
    {
        nLoc = (int)(pBuf - buf_data);
        if (nLoc < size && size - nLoc > 4)
        {
            if (memcmp(pBuf, szChar, signSize) == 0)
			{
				*usbmode = 1;
				return nLoc;
			}
			else if (memcmp(pBuf, szUartChar, signSize) == 0 )
            {
                *usbmode = 0;
				return nLoc;
            }
            else
                nLoc += 1;
        }
        else
            break;
    }

	return -1;
}

static void* ram_dump_start(void* arg)
{
    if (g_dumpmode == 1)
    {
        trace_log("has dump proc thread is running!\n");
        return NULL;
    }
    int *usbmode = (int*)arg;
    g_dumpmode = 1;
	int ret = catch_dump_proc(*usbmode);
	if (ret == -1)
		trace_log("dump proc failed!\n");
	g_dumpmode = 0;
	trace_log("exit dump proc!\n");
	return NULL;
}


static void* uart_read_data(void* arg)
{
    size_t nRead = 0;
    uint32_t read_data_len=0;
    unsigned int times = 0;
    struct timeval start, end;
    unsigned char cmd[] = "at^logversion\r\n";

    ttyuart* ptty = (ttyuart*)arg;
    trace_log("start uart_read_data\n");
    if (!ptty)
    {
        trace_log("arg is null, exite read data\n");
        return NULL;
    }

    if ( write(ptty->fd, cmd, sizeof(cmd)) == -1)
    {
        trace_log("write at^logversion failed!\n");
    }

	gettimeofday(&start, NULL);
    while (1)
    {
        if (g_write_mode == 1)
            {
                usleep(1);
                continue;
            }

        nRead = read_uart_binary(ptty->fd, s_buf, READ_BUF_LEN, ptty->fd);
        //trace_log("[0x%08lx]:recv data :%ld\n", pthread_self(), nRead);
        if (nRead > 0)
        {
            read_data_len += nRead;
            /*
            if ( g_dumpmode == 0)
            {
                if (find_dump_id(buf, nRead, &usbmode) >= 0)
                {
                    nRead = read_uart_binary(ptty->fd, buf, 65535);
                    if (find_dump_id(buf, nRead, &usbmode) >= 0) //find flag again
                    {
                        dump_uart_fd = ptty->fd;
                        pthread_create(&tid_ramdump, NULL, ram_dump_start, &usbmode);
                    }
                }
            }
            
            
            dumpret = catch_dump_proc(ptty->fd, buf, nRead);
            if ( dumpret == 1)
            	break;
            else if (dumpret == -1)
                trace_log("dump proc failed!\n");
            */
        }
        else if (nRead == 0)
        {
            struct termios2 term2;
            int rv = ioctl(ptty->fd, TCGETS2, &term2);

            if(rv != 0) {
            	trace_log("%s is closed!\n", ptty->dev);
                break;
            }

            //trace_log("read data is nulll, error:%d\n",  errno);
        }

        gettimeofday(&end, NULL);

        times = (end.tv_sec * 1000 + end.tv_usec/1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
        if ( times > TIME_INTER_READ_FLAG )
        {
            trace_log("read_bytes(%u), tick(%u), %uB/s\n", read_data_len, times/1000, (read_data_len*1000)/times);
            read_data_len=0;
            gettimeofday(&start, NULL);
        }

    }

    if (g_dumpmode == 1 && dump_uart_fd == ptty->fd)
    {
        //pthread_cancel(tid_ramdump);
        trace_log("force ramdump thread exited!\n");
    }
    int32_t ret = 0;
    ioctl(ptty->fd, FIONREAD, &ret);
    if (ret >= 0)
    {
        close(ptty->fd);
    }

    trace_log("%s thread exit!\n", ptty->dev);
    return NULL;
}

int create_new_log_file()
{
    char logFileName[EXTEND] = {0};
    char cur_dir_path[FIBO_BUF_SIZE+2*EXTEND] = {0};

    snprintf(logFileName, sizeof(logFileName), "%s_file.bin",get_time_name(0));
    snprintf(cur_dir_path,sizeof(cur_dir_path),"%s%s",s_logpath,logFileName);
    trace_log("%s : create new file:%s\n", __func__, cur_dir_path);

    int fd = open(cur_dir_path, O_CREAT | O_RDWR | O_TRUNC, 0444);
    if (fd == -1)
    {
        trace_log("%s : open %s failed, error:%d\n", __func__, cur_dir_path, errno);
        return -1;
    }

    log_storage_control(cur_dir_path, s_max_file_num, 1);
    return fd;
}

int get_time(SYSTEMTIME* _tm)
{
    struct timeval _time;
    
    if (gettimeofday(&_time, NULL) == 0)
    {
        struct tm* local = localtime(&_time.tv_sec);
        if (local != NULL)
        {
            _tm->Year = local->tm_year + 1900;
            _tm->Month = local->tm_mon + 1;
            _tm->DayOfWeek = local->tm_wday;
            _tm->Day = local->tm_mday;
            _tm->Hour = local->tm_hour;
            _tm->Minute = local->tm_min;
            _tm->Second = local->tm_sec;
            _tm->Milliseconds = _time.tv_usec / 1000;
            return 0;
        }
    }

    return errno;
}

int write_log_data(int fd, char* pbuf, size_t size)
{
    int ret = 0;
    if (fd < 0)
        return -1;
    get_time(&pkg_time.time);

    //trace_log("time:%d %d %d %d %d %d %d\n", pkg_time.time.Year, pkg_time.time.Month, pkg_time.time.Day,
     //       pkg_time.time.Hour, pkg_time.time.Minute, pkg_time.time.Second, pkg_time.time.Milliseconds);
    ret = write(fd, (void*)&pkg_time, sizeof(pkg_time));
    if (ret < 0)
        trace_log("write time data failed, error:%d\n", errno);

    ret = write(fd, pbuf, size);
    if (ret < 0)
        trace_log("write data failed, len:%lu, error:%d\n", size, errno);

    return ret;
}

static void* save_log_proc(void* arg)
{
    size_t file_len = 0;
    QUEUE_ITEM *item = NULL;
    int wres = 0;
    int fd = -1;
    int nCnt = 0;
    int usbmode = 0;
    int find = 0;

    trace_log("start create new log file\n");
    fd = create_new_log_file();

    while (1)
    {
        if ( g_dumpmode == 1)
        {
            usleep(1000);
            continue;
        }

        while (g_queue->items && ++nCnt < 1000) 
        {
            item = Get_Queue_Item(g_queue);
            if (item && item->sz > 0 && fd != -1)
            {
                if ( g_dumpmode == 0)
                {
                    if (find_dump_id(item->data, item->sz, &usbmode) >= 0)
                    {
                        if (find == 1) //find flag again
                        {
                            trace_log("find dump,usbmode is %d\r\n", usbmode);
                            dump_uart_fd = item->fd;
                            pthread_create(&tid_ramdump, NULL, ram_dump_start, &usbmode);
                            find = 0;
                        }
                        else
                            find = 1;
                    }
                    else if (find == 1)
                        find = 0;
                }

                wres = write_log_data(fd, item->data, item->sz);
                if (wres < 0)
                {
                    trace_log("write data failed, %d\n", errno);
                    break;
                }
                file_len += item->sz;
            }

            Free_Queue_Item(item);
        }

        if ( file_len >= single_logfile_size)
        {
            close(fd);
            file_len = 0;
            fd = create_new_log_file();
        }

        if (!g_queue->items)
            usleep(1000);
        nCnt = 0;
    }
    
    while (g_queue->items) 
    {
        item = Get_Queue_Item(g_queue);
        if (item && item->sz > 0 && fd != -1)
            {
                wres = write_log_data(fd, item->data, item->sz);
                if (wres < 0)
                {
                    trace_log("write data failed, %d\n", errno);
                    break;
                }
            }

        Free_Queue_Item(item);
    }

    if (fd != -1)
        close(fd);

    return NULL;
}

static int init_hotplug_sock(void)
{
    struct sockaddr_nl snl;
    const int buffersize = 4096; /*16 * 1024 * 1024;*/
    int retval;
    memset(&snl, 0x00, sizeof(struct sockaddr_nl));
    snl.nl_family = AF_NETLINK;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;
    int hotplug_sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    if (hotplug_sock == -1) {
        trace_log("error getting socket: %s", strerror(errno));
        return -1;
    }
    /* set receive buffersize */
    setsockopt(hotplug_sock, SOL_SOCKET, SO_RCVBUFFORCE, &buffersize, sizeof(buffersize));
    retval = bind(hotplug_sock, (struct sockaddr *) &snl, sizeof(struct sockaddr_nl));
    if (retval < 0) {
        trace_log("bind failed: %s", strerror(errno));
        close(hotplug_sock);
        hotplug_sock = -1;
        return -1;
    }
    return hotplug_sock;
}


int eigencomm_log_main(int argc, char** argv)
{
    int i, opt = -1;;
    ttyuart uartdev[2] = {0};
    pthread_t tid_save_log = 0;
    char portname[FIBO_BUF_SIZE] = {0};
    fibo_usbdev_t *pdev = NULL;
    struct timeval tv;
    fd_set fds;
    int rcvlen, ret;
    int hotplug_sock = -1;
    char buf[UEVENT_BUFFER_SIZE * 2] = { 0 };

    char log_dir[FIBO_BUF_SIZE] = ".";

    trace_log("[%s] start\n", __func__);

    optind = 1;//must set to 1
    while ((opt = getopt(argc, argv, "d:s:m:n:t:h")) != -1)
    {
        switch (opt)
        {
            case 'd':
                strcpy(portname, optarg);
                break;
            case 's':
                strcpy(log_dir, optarg);
                break;
            case 'm':
                single_logfile_size = atoi(optarg)*1024*1024;
                break;
            case 'n':
                s_max_file_num = atoi(optarg);
                break;
            case 't':
                trace_log("platform is eigencomm\n");
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:;
        }
    }

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    sprintf(s_logpath, "%s/fibolog_%02d%02d%02d%02d%02d%02d/", log_dir,
        1900+tm->tm_year, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    trace_log("%s: s_logpath: %s\n", __func__, s_logpath);

    for (i=1; i<sizeof(s_logpath) && s_logpath[i] != 0; i++)
    {
        if (s_logpath[i] == '\\'  || s_logpath[i] == '/' )
        {
            char str_dir[FIBO_BUF_SIZE] = {0};
            strcpy(str_dir, s_logpath);
            str_dir[i] = '\0';
            if (access(str_dir, 0)) {
                mkdir(str_dir, 0777);
                trace_log("[%s] mkdir:%s\n", __func__, str_dir);
            }
        }
    }

    if(portname[0] == 0)
    {
        pdev = fibo_get_fibocom_device(eigencomm_find_devices_in_table, portname, 0);
        if (pdev == NULL)
        {
            return -1;
        }
        uartdev[0].dev = pdev->portname;
    }
    else
    {
        uartdev[0].dev = portname;
    }

    trace_log("start Initialize Queue\n");
    g_queue = Initialize_Queue();
    for (int i = 0; i < 7; i ++)
    {
        pkg_time.prefix[i] = 0xBA;
        pkg_time.suffix[i] = 0xBA;
    }
    pkg_time.prefix[7] = 0xAA;
    pkg_time.suffix[7] = 0xAA;

    trace_log("create save_log_proc thread\n");
    pthread_create(&tid_save_log, NULL, save_log_proc, NULL);

    for (i = 0; i < 2; i ++)
    {
    	if (uartdev[i].dev)
        {
    		uartdev[i].fd = openuart(uartdev[i].dev, BAND_RATE);

			if (uartdev[i].fd != -1)
			{
                trace_log("create uart_read_data thread\n");
				pthread_create(&uartdev[i].tid, NULL, uart_read_data, &uartdev[i]);
			}
        }
    }

    trace_log("init hotplug sock\n");
    hotplug_sock = init_hotplug_sock();
    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(hotplug_sock, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        ret = select(hotplug_sock + 1, &fds, NULL, NULL, &tv);
        if (ret < 0)
            continue;
        if (!(ret > 0 && FD_ISSET(hotplug_sock, &fds)))
            continue;
        /* receive data */
        rcvlen = recv(hotplug_sock, &buf, sizeof(buf), 0);
        if (rcvlen > 0)
        {
            if (strstr(buf, "/tty/") != NULL)
            {
                if (strstr(buf, "add") != NULL)
                {
                	char* pbufname = basename(buf);
                    trace_log("add %s\n", pbufname);
                    for (i = 0; i < 2; i ++)
					{
						if (uartdev[i].dev && strstr(uartdev[i].dev, pbufname))
						{
							uartdev[i].fd = openuart(uartdev[i].dev, BAND_RATE);

							if (uartdev[i].fd != -1)
							{
								pthread_create(&uartdev[i].tid, NULL, uart_read_data, &uartdev[i]);
							}
						}
					}

                }else if (strstr(buf, "remove") != NULL)
                {
                    trace_log("remove %s\n", basename(buf));
                    for (i = 0; i < 2; i ++)
					{
						if (uartdev[i].dev && strstr(uartdev[i].dev, basename(buf)))
						{
							pthread_join(uartdev[i].tid, NULL);
							pthread_cancel(uartdev[i].tid);
							uartdev[i].tid = -1;
							//close(uartdev[i].fd);
						}
					}


                }
            }
        }
    }

    return 0;
}

/*******************************************************************
 *          CopyRight(C) 2022-2026  Fibocom Wireless Inc
 *******************************************************************
 * FileName : unisoc_log_main.c
 * Author   : Frank.zhou
 * Date     : 2022.05.25
 * Used     : Capture Unisoc module's diag log
 *******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <termios.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <signal.h>
#include <stdbool.h>

#include "fifo.h"
#include "misc_usb.h"
#include "unisoc_devices_list.h"
#include "log_control.h"
#include "unisoc_log_main.h"
#include "ftpclient.h"
#include "list.h"
#include "cplog.h"

#define LOGFILE_SIZE_MIN (2*1024*1024)
#define LOGFILE_SIZE_MAX (512*1024*1024)
#define LOGFILE_SIZE_DEFAULT (512*1024*1024)
#define LOGFILE_NUM 512
#define RX_URB_SIZE (41000)
#define safe_close_fd(_fd) do { if (_fd != -1) { int tmpfd = _fd; _fd = -1; close(tmpfd); }} while(0)

#define MAX_SINGLE_LOG_SIZE  (50 *1024 * 1024) //MB
#define LOG_BUFFER_SIZE     (32 * 1024) // (0x100000) //1M
#define LOG_WRITE_BUFFER_SIZE     (128 * 1024) // (0x100000) //1M
#define FIFO_BUFFER_SIZE    (1024 * 1024) //1M  (0xA00000) zhangboxing MBB0063-566 2023/02/1
#define UDX710_PCIE_DEVICES 0
#define FILENAME_SIZE 2048

#define AP_AND_CP 0
#define ONLY_CP 1
#define ONLY_TFTP_AP 2

static unsigned s_logfile_num = 0;
unsigned ulog_exit_requested = 0;
int g_is_usb_disconnect = 0;
int g_donot_split_logfile = 0;
int qlog_read_com_data = 0;
static char s_logfile_List[LOGFILE_NUM][32];
static unsigned s_logfile_idx;
static int second_logfile = -1;
unsigned g_rx_log_count = 0;
static unsigned s_logfile_seq;
static unsigned exit_after_usb_disconnet = 0;
int g_is_unisoc_chip = 0;
int g_is_unisoc_exx00u_chip = 0;
int g_unisoc_log_type = 0; // 0 ~ DIAG, 1 ~ LOG
char modem_name_para[32] = {0};
uint32_t query_panic_addr = 0;

int g_tcp_server_port = 0;
int g_tcp_client_port = 0;
char g_tcp_client_ip[16] = {0};
const char *g_tftp_server_ip = NULL;
const char *g_ftp_server_ip = NULL;
const char *g_ftp_server_usr = NULL;
const char *g_ftp_server_pass = NULL;
int block_size = 16384;
bool set_ftpipaddr=false;
bool set_username=false;
bool set_passwd=false;

int ap_log = 1;

char username[128];/* username entered by the user */
char password[128];
char ftpservipaddr[128];
char g_ftpfilename[128];
bool use_ftp=false;
char g_filename[128];
pthread_t g_tid_logtest;
char g_str_unisoc_log_dir[1024];
int ftp_put_unisoc_flag = 0;
char g_str_sub_unisoc_log_dir[128];//Modify for MBB0080-496 20230615 zhangboxing
char taskinfo1_portname[512];

ulog_ops_t ulog_ops;
int logfile_fd = -1;    //Use as cplogfile_fd in unisoc ecxxxu
extern bool  is_dump_finish;
bool islittleendian = true;
extern int save_to_file(char *log_dir, unsigned char *buffer, int length);
extern int m_bSendTCmd;
typedef struct
{
    const ulog_ops_t *ops;
    int fd;
    const char *filter;
} init_filter_cfg_t;

typedef struct
{
    int usbfd;
    int ep;
    int outfd;
    int rx_size;
    const char *dev;
} usbfs_read_cfg_t;

typedef struct
{
    int dm_ttyfd;
    int dm_usbfd;
    int dm_sockets[2];
    pthread_t dm_tid;
    usbfs_read_cfg_t cfg;

    int general_ttyfd;
    int general_usbfd;
    int general_sockets[2];
    pthread_t general_tid;
} ql_fds_t;

struct argument
{
    // arguments
    char ttyDM[256];
    char logdir[256];
    char ttyGENERAL[256];

    // configurations
    int logfile_num;
    int logfile_sz;
    const char *filter_cfg;
    const char *delete_logs; // Remove all logfiles in the logdir before catching logs

    const  struct ql_usb_device_info *ql_dev;
    // profiles

    ql_fds_t fds;
};
static struct argument *ulog_args;

typedef struct
{
    int portnum;
    int isRun;
    char portname[FIBO_BUF_SIZE];
    char logdir[1024];
    fibo_usbdev_t *pdev;
    int single_logfile_size;
    struct fifo *plog_fifo;
    uint32_t start_tick;
    uint32_t read_bytes;
} TaskInfo_t;

static volatile int g_log_process_flag = 1;
int g_log_unisoc_file_maxNum = 0;
static int fd_650_port = -1;

int isLittleEndian()
{
    union
    {
        unsigned int i;
        unsigned char c[4];
    } num;

    num.i = 1;

    return (num.c[0] == 1); // 如果低字节存放的是数值的最低有效位，则为小端
}

unsigned short swapShort(unsigned short num)
{
    return (num>>8) | (num<<8);
}

unsigned int swapInt(unsigned int num)
{
    return ((num>>24) & 0xFF) | ((num>>8) & 0xFF00) | ((num<<8) & 0xFF0000) | ((num<<24) & 0xFF000000);
}



fibo_usbdev_t *udx710_find_pcie_devices()
{
    return &unisoc_devices_table[UDX710_PCIE_DEVICES];
}

fibo_usbdev_t *uinisoc_find_devices_in_table(int idvendor, int idproduct)
{
    int i, size = 0;

    size = sizeof(unisoc_devices_table)/sizeof(unisoc_devices_table[0]);
    for(i=0; i<size; i++)
    {
        fibo_usbdev_t *pdev = &unisoc_devices_table[i];

        if((pdev->idVendor == idvendor) && (pdev->idProduct == idproduct))
        {
            return pdev;
        }
    }

    return NULL;
}

static TaskInfo_t *taskinfo_init(int portnum)
{
    TaskInfo_t *pTaskInfo = (TaskInfo_t *)malloc(sizeof(TaskInfo_t));

    if (pTaskInfo == NULL) {
        LogInfo("malloc pTaskInfo failed.\n");
        return NULL;
    }
    memset(pTaskInfo, 0, sizeof(TaskInfo_t));

    pTaskInfo->portnum = portnum;
    pTaskInfo->isRun = 0;
    pTaskInfo->single_logfile_size = MAX_SINGLE_LOG_SIZE;
    snprintf(pTaskInfo->logdir, sizeof(pTaskInfo->logdir), ".");

    pTaskInfo->plog_fifo = fifo_alloc(FIFO_BUFFER_SIZE);
    if (pTaskInfo->plog_fifo == NULL) {
        LogInfo("malloc pTaskInfo->plog_fifo failed.\n");
        if (pTaskInfo) {
            free(pTaskInfo);
            pTaskInfo = NULL;
        }
        return NULL;
    }

    return pTaskInfo;
}

static void FreeTask(TaskInfo_t *pTaskInfo)
{
    if (pTaskInfo) {
        fifo_free(pTaskInfo->plog_fifo);
        free(pTaskInfo);
    }
}

/*zhangboxing 650APlog GRAB 2022/01/16 begin*/
static int serial_write_hexstring(const char *str_data)
{
    char ch_high, ch_low;
    uint8_t *write_buf = NULL;
    ssize_t write_cnt = 0, wr_ret = 0;

    if (fd_650_port < 0)
    {
        printf("please open diag port first!\n");
        return 0;
    }

    if (!str_data || strlen(str_data) <= 0)
        goto error_exit;

    write_buf = (uint8_t *)malloc(strlen(str_data));
    if (!write_buf)
    {
        printf("malloc write_buf failed, errno:%d(%s)\n", errno, strerror(errno));
        goto error_exit;
    }

    while (*str_data != '\0' && (*str_data + 1) != '\0')
    {
        ch_high = tolower(*str_data);
        ch_low = tolower(*(str_data + 1));
        if ((('0' <= ch_high && '9' >= ch_high) || ('a' <= ch_high && 'f' >= ch_high)) &&
                        (('0' <= ch_low && '9' >= ch_low) || ('a' <= ch_low && 'f' >= ch_low)))
        {
            if ('0' <= ch_high && '9' >= ch_high)
            {
                write_buf[write_cnt] = (ch_high - '0') << 4;
            }
            else
            {
                write_buf[write_cnt] = (0x0a + ch_high - 'a') << 4;
            }

            if ('0' <= ch_low && '9' >= ch_low)
            {
                write_buf[write_cnt] |= (ch_low - '0');
            }
            else
            {
                write_buf[write_cnt] |= (0x0a + ch_low - 'a');
            }
            write_cnt++;
            str_data += 2;
        }
        else
        {
            str_data++;
        }
    }

    if (write_cnt <= 0)
    {
        goto error_exit;
    }

    /*just write data to device*/

    wr_ret = write(fd_650_port, write_buf, write_cnt);
    if (wr_ret <= 0)
    {
        printf("port[0x%x] write error! errno:%d(%s)\n", fd_650_port, errno, strerror(errno));
        goto error_exit;
    }
    if (write_buf)
        free(write_buf);

    return wr_ret;
error_exit:

    if (write_buf)
        free(write_buf);

    return 0;
}

static void *thread_read_serial_aplog(void *arg)
{
    TaskInfo_t *pTaskInfo = (TaskInfo_t *)arg;
    uint8_t *rx_buffer = NULL;
    int logfd = 0;

    LogInfo("[%s] start, logdir:%s\n", pTaskInfo->pdev->syspath, pTaskInfo->logdir);
    if(!serial_write_hexstring("7E 00 00 00 00 11 00 38 FB 00 00 46 58 36 35 30 ED 1B 7E"))
    {
        LogInfo("start serial_write_hexstring fail\n");
        if (fd_650_port >= 0)
        {
            close(fd_650_port);
            fd_650_port = -1;
            pTaskInfo->pdev->ttyfd[pTaskInfo->portnum] = -1;
        }
    }
    usleep(10*1000);

    if(!serial_write_hexstring("7E 00 00 00 00 0E 00 38 FB 04 00 01 00 00 78 7E"))
    {
        LogInfo("init serial_write_hexstring fail\n");
        if (fd_650_port >= 0)
        {
            close(fd_650_port);
            fd_650_port = -1;
            pTaskInfo->pdev->ttyfd[pTaskInfo->portnum] = -1;
        }
    }
    usleep(10*1000);

    rx_buffer = (uint8_t *)malloc(LOG_BUFFER_SIZE);
    if (rx_buffer == NULL)
    {
        LogInfo("[%d], malloc rx_buffer failed. errno:%d(%s)\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], errno, strerror(errno));
        return NULL;
    }

    sleep(1);
    if(ap_log == ONLY_TFTP_AP)
    {
        logfd = ulog_logfile_create_fullname(0, "ap.log", 0, 1);
    }

    while (pTaskInfo->isRun)
    {
        int readlen = 0;
        if(!serial_write_hexstring("7E 00 00 00 00 0E 00 38 FB 04 01 00 08 bd 86 7E"))
        {
            LogInfo("read serial_write_hexstring fail\n");
            if (fd_650_port >= 0)
            {
                close(fd_650_port);
                fd_650_port = -1;
                pTaskInfo->pdev->ttyfd[pTaskInfo->portnum] = -1;
            }
        }
        usleep(10*1000);

        readlen = pTaskInfo->pdev->read(pTaskInfo->pdev, (char *)rx_buffer, LOG_BUFFER_SIZE, pTaskInfo->portnum);
        readlen = readlen-16;
        if(ap_log == ONLY_TFTP_AP)
        {
            if(readlen > 0)
            {
                memcpy(rx_buffer,rx_buffer+13,readlen);
                ulog_logfile_save(logfd, rx_buffer , readlen);
            }
        }
        else
        {
            /*zhangboxing 650APlog GRAB 2022/01/17 begin*/
            if (readlen > 0)
            {
                /*put the log to fifo*/
                memcpy(rx_buffer,rx_buffer+13,readlen);
                if (readlen > __fifo_put(pTaskInfo->plog_fifo, rx_buffer, readlen))
                {
                    LogInfo("[%d] warning, fifo buf is full now.\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);
                }
    
                /* print current status */
                {
                    uint32_t cur_tick = 0;
                    uint32_t diff_tick = 0;
    
                    pTaskInfo->read_bytes += readlen;
                    if (pTaskInfo->start_tick == 0)
                    {
                        pTaskInfo->start_tick = time(NULL);
                    }
                    else
                    {
                        cur_tick = time(NULL);
                        diff_tick = cur_tick - pTaskInfo->start_tick;
                        if (diff_tick >= 3)
                        {
                            LogInfo("[%d] read_bytes(%u), tick(%u), %uB/s\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], pTaskInfo->read_bytes, diff_tick, pTaskInfo->read_bytes/diff_tick);
                            pTaskInfo->start_tick = cur_tick;
                            pTaskInfo->read_bytes = 0;
                        }
                    }
                }
            }
            /*zhangboxing 650APlog GRAB 2022/01/17 end*/
        }
        usleep(10000* 5);
    }

    if (rx_buffer)
    {
        free(rx_buffer);
        rx_buffer = NULL;
    }

    LogInfo("[%d] stopped\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);
    if(ap_log == ONLY_TFTP_AP)
    {
        ulog_logfile_close(logfd);
    }

    return NULL;
}

#if 0
static void *thread_read_serial_cplog(void *arg)
{
    TaskInfo_t *pTaskInfo = (TaskInfo_t *)arg;
    uint8_t *rx_buffer = NULL;

    LogInfo("[%s] start, logdir:%s\n", pTaskInfo->pdev->syspath, pTaskInfo->logdir);


    rx_buffer = (uint8_t *)malloc(LOG_BUFFER_SIZE);
    if (rx_buffer == NULL) {
        LogInfo("[%d], malloc rx_buffer failed. errno:%d(%s)\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], errno, strerror(errno));
        return NULL;
    }

    sleep(1);

    while (pTaskInfo->isRun)
    {
        int readlen = 0;

        readlen = pTaskInfo->pdev->read(pTaskInfo->pdev, (char *)rx_buffer, LOG_BUFFER_SIZE, pTaskInfo->portnum);

        if (readlen > 0)
        {
            /*put the log to fifo*/
            if (readlen > __fifo_put(pTaskInfo->plog_fifo, rx_buffer, readlen))
            {
                LogInfo("[%d] warning, fifo buf is full now.\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);
            }

            /* print current status */
            {
                uint32_t cur_tick = 0;
                uint32_t diff_tick = 0;

                pTaskInfo->read_bytes += readlen;
                if (pTaskInfo->start_tick == 0) {
                    pTaskInfo->start_tick = time(NULL);
                }
                else
                {
                    cur_tick = time(NULL);
                    diff_tick = cur_tick - pTaskInfo->start_tick;
                    if (diff_tick >= 5)
                    {
                        LogInfo("[%d] read_bytes(%u), tick(%u), %uB/s\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], pTaskInfo->read_bytes, diff_tick, pTaskInfo->read_bytes/diff_tick);
                        pTaskInfo->start_tick = cur_tick;
                        pTaskInfo->read_bytes = 0;
                    }
                }
            }
        }
        //usleep(10000);
    }

    if (rx_buffer) {
        free(rx_buffer);
        rx_buffer = NULL;
    }

    LogInfo("[%d] stopped\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);

    return NULL;
}
#endif

/*zhangboxing 650APlog GRAB 2022/01/16 end*/

static void *thread_save_log_to_file(void *arg)
{
    TaskInfo_t *pTaskInfo = (TaskInfo_t *)arg;
    uint32_t i, get_fifo_len = 0;
    int file_size = 0;
    FILE *p_logfile = NULL;
    char log_filename[FILENAME_SIZE] = { 0 };
    uint8_t *get_fifo_buff = (uint8_t *)malloc(LOG_WRITE_BUFFER_SIZE);

    LogInfo("[%d] logdir:%s\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], pTaskInfo->logdir);

    if (get_fifo_buff == NULL)
    {
        LogInfo("[%d] malloc get_fifo_buff failed.\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);
        return NULL;
    }

    for (i=1; i<sizeof(pTaskInfo->logdir) && pTaskInfo->logdir[i] != 0; i++)
    {
        if (pTaskInfo->logdir[i] == '/')
        {
            char str_dir[512] = {0};

            strcpy(str_dir, pTaskInfo->logdir);
            str_dir[i] = '\0';
            if (access(str_dir, 0)) {
                mkdir(str_dir, 0777);
                // LogInfo("[%s] mkdir:%s\n", pTaskInfo->pdev->syspath, str_dir);
            }
        }
    }

    while (pTaskInfo->isRun)
    {
        if (p_logfile == NULL) {
            struct tm *tm = NULL;
            time_t t;

            t = time(NULL);
            tm = localtime(&t);
            sprintf(log_filename, "%s%02d%02d%02d%02d%02d%02d.log", pTaskInfo->logdir,
                1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
            LogInfo("log_filename: %s\n", log_filename);

            p_logfile = fopen(log_filename, "wb");
            if (p_logfile == NULL) {
                LogInfo("[%d] create log file failed, errno:%d(%s)\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], errno, strerror(errno));
                break;
            }
            /*zhangboxing MBB0063-571 2023/01/31  start*/
            if(strstr(pTaskInfo->logdir,"cp") != NULL)
            {
                log_storage_control(log_filename, g_log_unisoc_file_maxNum, 1);
            }
            /*zhangboxing MBB0063-571 2023/01/31  end*/
        }
        else
        {
            /*int cur_pos = 0, file_size = 0;
            cur_pos = ftell(p_logfile);
            fseek(p_logfile, 0, SEEK_END);
            file_size = ftell(p_logfile);
            fseek(p_logfile, cur_pos, SEEK_SET);*/
            if (file_size > pTaskInfo->single_logfile_size) {
                //fflush(p_logfile);
                fclose(p_logfile);
                p_logfile = NULL;
                file_size = 0;
                continue;
            }
        }
        get_fifo_len = __fifo_len(pTaskInfo->plog_fifo);
        if (get_fifo_len)
        {
            get_fifo_len = __fifo_get(pTaskInfo->plog_fifo, get_fifo_buff, LOG_WRITE_BUFFER_SIZE);
            if (fwrite(get_fifo_buff, get_fifo_len, 1, p_logfile) == get_fifo_len) {
                // LogInfo("write %d Bytes to log_filename: %s\n", get_fifo_len, log_filename);
            }
            file_size += get_fifo_len;
        }
        else
        {
            /*zhangboxing  2022/12/02 begin*/
            usleep(1000);
            /*zhangboxing  2022/12/02 end*/
        }
    }

    get_fifo_len = __fifo_len(pTaskInfo->plog_fifo);
    if (get_fifo_len)
    {
        get_fifo_len = __fifo_get(pTaskInfo->plog_fifo, get_fifo_buff, LOG_WRITE_BUFFER_SIZE);
        fwrite(get_fifo_buff, get_fifo_len, 1, p_logfile);
    }

    if (p_logfile) {
        //fflush(p_logfile);
        fclose(p_logfile);
        p_logfile = NULL;
    }

    if (get_fifo_buff) {
        free(get_fifo_buff);
        get_fifo_buff = NULL;
    }

    return NULL;
}

static int fibo_log_capture_ap_process(TaskInfo_t *pTaskInfo)
{
    pthread_t thread_id_readlog;
    pthread_t thread_id_savelog;
    pthread_attr_t attr;
    struct sched_param param;

    LogInfo("[%d] start\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);

    pthread_attr_init(&attr);
    pthread_attr_getschedparam(&attr, &param);
    param.sched_priority = 99;
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pTaskInfo->isRun = 1;
    if (pthread_create(&thread_id_readlog, &attr, thread_read_serial_aplog, pTaskInfo) < 0)
    {
        LogInfo("[%d] pthread_create failed! errno:%d(%s)\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], errno, strerror(errno));
        goto error_exit;
    }

    if(ap_log == AP_AND_CP)
    {
        if (pthread_create(&thread_id_savelog, NULL, thread_save_log_to_file, pTaskInfo) < 0)
        {
            LogInfo("[%d] pthread_create failed! errno:%d(%s)\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], errno, strerror(errno));
            goto error_exit;
        }
    }

    pthread_attr_destroy(&attr);

    LogInfo("[%d] OK\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);
    return 0;
error_exit:
    LogInfo("[%d] failed\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);
    return -1;
}

#if 0
static int fibo_log_capture_process(TaskInfo_t *pTaskInfo)
{
    pthread_t thread_id_readlog;
    pthread_t thread_id_savelog;
    pthread_attr_t attr;
    struct sched_param param;

    LogInfo("[%d] start\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);

    pthread_attr_init(&attr);
    pthread_attr_getschedparam(&attr, &param);
    param.sched_priority = 99;
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pTaskInfo->isRun = 1;
    if (pthread_create(&thread_id_readlog, &attr, thread_read_serial_cplog, pTaskInfo) < 0)
    {
        LogInfo("[%d] pthread_create failed! errno:%d(%s)\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], errno, strerror(errno));
        goto error_exit;
    }

    if (pthread_create(&thread_id_savelog, NULL, thread_save_log_to_file, pTaskInfo) < 0) {
        LogInfo("[%d] pthread_create failed! errno:%d(%s)\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], errno, strerror(errno));
        goto error_exit;
    }

    pthread_attr_destroy(&attr);

    LogInfo("[%d] OK\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);
    return 0;
error_exit:
    LogInfo("[%d] failed\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);
    return -1;
}

#endif
void *upload_log_file_thread(void *arg)
{
        LogInfo("upload_log_file_thread start ####\n");
        char* ftp_main_param[4]={"ftp_unisoc_main", ftpservipaddr,username,password};
        ftp_unisoc_main(4,ftp_main_param);
        LogInfo("upload_log_file_thread end ####\n");
        return ((void *)0);
}

int log_upload_by_ftp()
{
        int ret;
        pthread_attr_t attr;
        struct sched_param param;

        LogInfo("log_upload_by_ftp called\n");

        pthread_attr_init (&attr);
        pthread_attr_getschedparam(&attr, &param);
        param.sched_priority=98;
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setschedpolicy(&attr, SCHED_RR);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        ret = pthread_create(&g_tid_logtest, &attr, upload_log_file_thread, NULL);
        if (ret < 0)
        {
                LogInfo("pthread_create log test thread create failed! errno[%d]", errno);
                goto error_exit;
        }

error_exit:

        pthread_attr_destroy(&attr);

        return 0;
}

static int is_tftp(void)
{
    return (g_tftp_server_ip != NULL);
}

static int is_ftp(void)
{
    return (g_ftp_server_ip != NULL);
}

static int is_tty2tcp(void)
{   //work as tcp server
    return (g_tcp_server_port > 0);
}

static int is_tcp_client(void)
{
    return (g_tcp_client_port > 0);
}

#if 0
static int unisoc_usb_option_setup(fibo_usbdev_t *pdev, int portnum)
{
    int ret = -1, val = 0;
    struct usbdevfs_ctrltransfer control;

    // if (dtr_state)
    val |= 0x01;
    // if (rts_state)
    // val |= 0x02;

    control.bRequestType = 0x21;
    control.bRequest = 0x22;
    control.wValue = val;
    control.wIndex = pdev->ifnum[portnum];
    control.wLength = 0;
    control.timeout = 0;
    control.data = NULL;

    ret = ioctl(pdev->usbdev, USBDEVFS_CONTROL, &control);
    if (ret == -1)
    {
        printf("errno: %d (%s)\n", errno, strerror(errno));
    }
    return ret;
}

#endif
static void fibo_exit_function(int msg)
{
    LogInfo("%d\n", msg);
    log_storage_control(NULL, 0, 0);
    g_log_process_flag = 0;
    sleep(1);
    signal(SIGINT, SIG_DFL); //Enable Ctrl+C to exit
    if (msg == SIGTERM || msg == SIGHUP || msg == SIGINT)
    {
            ulog_exit_requested = 1;
    }
    LogInfo("recv signal %d\n", msg);
}

void usage(char *arg)
{
    LogInfo("========================================\n");
    LogInfo("Usage:\n");
    LogInfo("%s <-a (ap log switch)> <-p [cp logport]> <-s [log save dir]> <-m [single_log_filesize(MB)]> <-n [max_log_filenum]> <-i [IP] -u [username] -w [passward]>\n", arg);
    LogInfo("Example: %s -p /dev/ttyUSB3 \n", arg);
    LogInfo("========================================\n");
}

uint64_t ulog_le64(uint64_t v64)
{
    const uint64_t is_bigendian = 1;
    uint64_t tmp = v64;

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

unsigned ulog_msecs(void)
{
    static unsigned start = 0;
    unsigned now;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = (unsigned)ts.tv_sec*1000 + (unsigned)(ts.tv_nsec / 1000000);
    if (start == 0)
        start = now;
    return now - start;
}

#if 0
static int ulog_is_not_dir(const char *logdir)
{
    return (is_ftp() || is_tftp() || !strncmp(logdir, "/dev/null", strlen("/dev/null")));
}
#endif

static const char * ulog_time_name(int type)
{
    static char time_name[80];
    time_t ltime;
    struct tm *currtime;

    time(&ltime);
    currtime = localtime(&ltime);

    if(type == 1)
    {
        snprintf(time_name, sizeof(time_name), "%04d%02d%02d_%02d%02d%02d",
        (currtime->tm_year+1900), (currtime->tm_mon+1), currtime->tm_mday,
        currtime->tm_hour, currtime->tm_min, currtime->tm_sec);
    }
    else if(type == 2)
    {
        snprintf(time_name, sizeof(time_name), "%04d_%02d%02d_%02d%02d%02d", (currtime->tm_year+1900),
        (currtime->tm_mon+1), currtime->tm_mday, currtime->tm_hour, currtime->tm_min, currtime->tm_sec);
    }
    else if(type == 3)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        snprintf(time_name, sizeof(time_name), "%02d-%02d-%02d-%02d-%03d", currtime->tm_mday, currtime->tm_hour,
        currtime->tm_min, currtime->tm_sec, (unsigned)(ts.tv_nsec / 1000000));
    }

    return time_name;
}

int ulog_logfile_create_fullname(int file_type, const char *fullname, long tftp_size, int is_dump)
{
    int fd = -1;

    if(!strncmp(fullname, "/dev/null", strlen("/dev/null")))
    {
        fd = open("/dev/null", O_CREAT | O_RDWR | O_TRUNC, 0444);
    }
    else if(is_tftp())
    {
        const char *filename = fullname;
        const char *p = strchr(filename, '/');
        while(p)
        {
            p++;
            filename = p;
            p = strchr(filename, '/');
        }

        fd = tftp_write_request(filename, tftp_size);
    }
    else if(is_ftp())
    {
        const char *filename = fullname;
        const char *p = strchr(filename, '/');
        while(p)
        {
            p++;
            filename = p;
            p = strchr(filename, '/');
        }
        LogInfo("%s  filename:%s  g_ftp_server_pass:%s\n",__func__,filename, g_ftp_server_pass);
        fd = ftp_write_request(file_type, g_ftp_server_ip, g_ftp_server_usr, g_ftp_server_pass, filename);
        if (!is_dump)
            kfifo_alloc(fd);
    }
    else
    {
        fd = open(fullname, O_CREAT | O_RDWR | O_TRUNC, 0444);
        if (!is_dump)
            kfifo_alloc(fd);
    }

    return fd;
}

static int ulog_logfile_create(const char *logfile_dir, const char *logfile_suffix, unsigned logfile_seq) {
    int logfd;
    char shortname[100] = {0};
    char filename[FILENAME_SIZE] = {0};

    //delete old logfile
    if(s_logfile_num && s_logfile_List[logfile_seq%s_logfile_num][0])
    {
        sprintf(filename, "%s/%s.%s", logfile_dir, s_logfile_List[logfile_seq%s_logfile_num], logfile_suffix);
        if(access(filename, R_OK) == 0)
        {
            remove(filename);
        }
    }
    snprintf(shortname, sizeof(shortname), "%.80s_%04d", ulog_time_name(1), logfile_seq);
    sprintf(filename, "%s/%s.%s", g_str_unisoc_log_dir, shortname, logfile_suffix);
    sprintf(g_filename, "%s.%s",shortname, logfile_suffix);
    LogInfo("filename===%s\n",filename);
    LogInfo("g_filename===%s\n",g_filename);


    logfd = ulog_logfile_create_fullname(0, filename, 0, 0);
    if(logfd <= 0)
    {
        LogInfo("Fail to create new logfile! errno : %d (%s)\n", errno, strerror(errno));
    }

    LogInfo("%s %s logfd=%d\n", __func__, filename, logfd);

    if(s_logfile_num)
    {
        s_logfile_idx = (logfile_seq%s_logfile_num);
        strcpy(s_logfile_List[s_logfile_idx], shortname);
    }

    return logfd;
}
size_t ulog_logfile_save(int logfd, const void *buf, size_t size)
{
    int idx = kfifo_idx(logfd);
    if (idx != -1 )
    {
        return kfifo_write(idx, buf, size);
    }

    return ulog_poll_write(logfd, buf, size, 1000);
}

int ulog_logfile_close(int logfd)
{
    kfifo_free(kfifo_idx(logfd));
    kfifo_free(kfifo_idx(second_logfile));
    safe_close_fd(second_logfile);
    return close(logfd);
}

ssize_t ulog_poll_write(int fd, const void *buf, size_t size, unsigned timeout_msec)
{
    size_t wc = 0;
    ssize_t nbytes;

    if (!qlog_read_com_data && fd == ulog_args->fds.dm_sockets[0])
    {
        return ql_usbfs_write(ulog_args->fds.dm_usbfd, ulog_args->ql_dev->dm_intf.ep_out, buf, size);;
    }

    nbytes = write(fd, (char *)buf+wc, size-wc);

    if (nbytes <= 0)
    {
        if (errno != EAGAIN)
        {
            LogInfo("Fail to write fd = %d, errno : %d (%s)\n", fd, errno, strerror(errno));
            goto out;
        }
        else
        {
            nbytes = 0;
        }
    }

    wc += nbytes;

    if (timeout_msec == 0)
        return (wc);

    while (wc < size)
    {
        int ret;
        struct pollfd pollfds[] = {{fd, POLLOUT, 0}};

        do
        {
            ret = poll(pollfds, 1, timeout_msec);
        }  while (ret == -1 && errno == EINTR && ulog_exit_requested == 0);

        if(ret <= 0)
        {
            LogInfo("Fail to poll fd = %d, errno : %d (%s)\n", fd, errno, strerror(errno));
            break;
        }

        if(pollfds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            LogInfo("Fail to poll fd = %d, revents = %04x\n", fd, pollfds[0].revents);
            break;
        }

        if(pollfds[0].revents & (POLLOUT))
        {
            nbytes = write(fd, (char *)buf+wc, size-wc);

            if(nbytes <= 0)
            {
                LogInfo("Fail to write fd = %d, errno : %d (%s)\n", fd, errno, strerror(errno));
                break;
            }
            wc += nbytes;
        }
    }

out:
    if (wc != size)
    {
        LogInfo("%s fd=%d, size=%zd, timeout=%d, wc=%zd\n", __func__, fd, size, timeout_msec, wc);
    }

    return (wc);
}

static void* ulog_usbfs_read(void *arg)
{
    const usbfs_read_cfg_t *cfg = (usbfs_read_cfg_t *)arg;
    void *pbuf;
    int n = 0;
    int idx = 0;

    LogInfo("%s ( %s ) enter\n", __func__, cfg->dev);
    pbuf = malloc(cfg->rx_size);
    if (pbuf == NULL) {
        LogInfo("%s malloc %d fail\n", __func__, cfg->rx_size);
        return NULL;
    }

    idx = kfifo_alloc(cfg->outfd);
    while (1)
    {
        n = ql_usbfs_read(cfg->usbfd, cfg->ep, pbuf, cfg->rx_size);
        if (n < 0) {
             if (ulog_exit_requested == 0) {
               LogInfo("%s (%s) n = %d, usbfd=%d, ep=%02x, errno: %d (%s)\n",
                    __func__, cfg->dev, n, cfg->usbfd, cfg->ep, errno, strerror(errno));
                g_is_usb_disconnect = 1;
                unused_result_write(cfg->outfd, "\0", 1); //to wakeup read thread
            }
            break;
        }
        else if (n == 0) {
            //zero length packet
        }

        if (n > 0) {
            kfifo_write(idx, pbuf, n);
        }
    }
    kfifo_free(idx);

    free(pbuf);
    LogInfo("%s ( %s ) exit\n", __func__, cfg->dev);
    return NULL;
}

ssize_t ulog_poll_read_fds(int *fds, int n, void *pbuf, size_t size, unsigned timeout_msec)
{
    ssize_t rc = 0;

    while(ulog_exit_requested == 0 && timeout_msec > 0)
    {
        struct pollfd pollfds[4];
        int ret = -1;
        int i = 0;

        for(i = 0; i < n; i++)
        {
            pollfds[i].events = POLLIN;
            pollfds[i].fd = fds[i];
        }

        do
        {
            ret = poll(pollfds, n, timeout_msec);
        }
        while (ret == -1 && errno == EINTR && ulog_exit_requested == 0);

        if (g_is_usb_disconnect)
            break;

        if(ret <= 0)
        {
            LogInfo("poll() = %d, errno: %d (%s)\n", ret, errno, strerror(errno));
            if (ret == 0) errno = ETIMEDOUT;
            break;
        }

        for(i = 0; i < n; i++)
        {
            if(pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                LogInfo("poll fd=%d, revents = %04x\n", pollfds[i].fd, pollfds[i].revents);
                goto _out;
            }

            if(pollfds[i].revents & (POLLIN))
            {
                // FIXME:
                // pbuf should not always begin from SEEK_SET when this function monitor more than one fd (that is 'n > 0')
                rc = read(pollfds[i].fd, pbuf, size);
                if (rc <= 0)
                {
                    LogInfo("read( %d ) = %d, errno: %d (%s)\n", pollfds[i].fd, (int)rc, errno, strerror(errno));
                }
                fds[0] = pollfds[i].fd;
                goto _out;
            }
        }
    }

_out:
    return rc;
}

ssize_t ulog_poll_read(int fd,  void *pbuf, size_t size, unsigned timeout_msec)
{
    return ulog_poll_read_fds(&fd, 1, pbuf, size, timeout_msec);
}

int ulog_avail_space_for_dump(const char *dir, long need_MB)
{
    long free_space = 0;
    struct statfs stat;

    if (!statfs(dir, &stat))
    {
        free_space = stat.f_bavail*(stat.f_bsize/512)/2; //KBytes
    }
    else
    {
        LogInfo("statfs %s, errno : %d (%s)\n", dir, errno, strerror(errno));
    }

    free_space = (free_space/1024);
    if (free_space < need_MB)
    {
        LogInfo("free space is %ldMBytes, need %ldMB\n", free_space, need_MB);
        return 0;
    }

    return 1;
}

static int serial_open(const char *device)
{
    int ttyfd = open(device, O_RDWR | O_NDELAY | O_NOCTTY);
    if (ttyfd < 0)
    {
        LogInfo("Fail to open %s, errno : %d (%s)\n", device, errno, strerror(errno));
    }
    else
    {
        LogInfo("open %s ttyfd = %d\n", device, ttyfd);
        struct termios ios;
        memset(&ios, 0, sizeof(ios));
        tcgetattr(ttyfd, &ios);
        cfmakeraw(&ios);
        cfsetispeed(&ios, B115200);
        cfsetospeed(&ios, B115200);
        tcsetattr(ttyfd, TCSANOW, &ios);
    }
    return ttyfd;
}

static void parser_tcp(const char *str)
{
    int rc;
    int ip[4];

    if(str[0] == '9' && atoi(str) >= 9000)
    {
        g_tcp_server_port = atoi(str);
        return;
    }

    if(strstr(str, ":") && strstr(str, "."))
    {
        if (strstr(str, ":") > strstr(str, "."))
            rc = sscanf(str, "%d.%d.%d.%d:%d",
                    &ip[0], &ip[1], &ip[2], &ip[3], &g_tcp_client_port);
        else
            rc = sscanf(str, "%d:%d.%d.%d.%d",
                    &g_tcp_client_port, &ip[0], &ip[1], &ip[2], &ip[3]);

        if (rc == 5)
        {
            snprintf(g_tcp_client_ip, sizeof(g_tcp_client_ip),
                "%d.%d.%d.%d", (uint8_t)ip[0], (uint8_t)ip[1], (uint8_t)ip[2], (uint8_t)ip[3]);

            LogInfo("save log to tcp server %s:%d\n",
                g_tcp_client_ip, g_tcp_client_port);
        }
    }
}

#if 0
static void parser_tftp(const char *str)
{
    if (!strncmp(str, TFTP_F, strlen(TFTP_F)))
    {
        g_tftp_server_ip = str+strlen(TFTP_F);
        if (tftp_test_server(g_tftp_server_ip))
            LogInfo("save dump to tcp server %s\n", g_tftp_server_ip);
        else
            exit(1);
    }
}
#endif

static void parser_ftp(const char *str)
{
    if(!strncmp(str, FTP_F, strlen(FTP_F)))
    {
        static char g_ftp_server_ip_temp[16] = {0};
        static char g_ftp_server_usr_temp[32] = {0};
        static char g_ftp_server_pass_temp[32] = {0};
        char *buf_temp1 = NULL;
        char *buf_temp2 = NULL;
        buf_temp1 = strstr(str,"user:");
        buf_temp2 = strstr(str,"pass:");
        if(!buf_temp1 || !buf_temp2)
             exit(1);

        strncpy(g_ftp_server_ip_temp, str+4, buf_temp1 - str - 5);
        strncpy(g_ftp_server_usr_temp, buf_temp1+5, buf_temp2 - buf_temp1 - 6);
        strncpy(g_ftp_server_pass_temp, buf_temp2+5, strlen(buf_temp2) - 5);
        g_ftp_server_ip = g_ftp_server_ip_temp;
        g_ftp_server_usr = g_ftp_server_usr_temp;
        g_ftp_server_pass = g_ftp_server_pass_temp;
    }
}

static struct argument *parser_args(int argc, char **argv)
{
    int opt;
    static struct argument args = {
        .ttyDM = "",
        .logdir = "log_files",
        .logfile_num = LOGFILE_NUM,
        .logfile_sz = LOGFILE_SIZE_DEFAULT,
        .delete_logs = NULL, // Do not remove logs
        .filter_cfg = NULL,
    };

    optind = 1; //call by popen(), optind mayby is not 1
    while (-1 != (opt = getopt(argc, argv, "d:p:P:s:n:g:m:f:D:i:u:w:a:t:qh")))
    {
        switch (opt)
        {
        case 'p':
            if (optarg[0] == 't') //ttyUSB0
                snprintf(args.ttyGENERAL, sizeof(args.ttyGENERAL), "/dev/%.250s", optarg);
            else if (optarg[0] == 'U') //USB0
                snprintf(args.ttyGENERAL, sizeof(args.ttyGENERAL), "/dev/tty%.247s", optarg);
            else if (optarg[0] == '/')
                snprintf(args.ttyGENERAL, sizeof(args.ttyGENERAL), "%.255s", optarg);
            else
            {
                LogInfo("unknow dev %s\n", optarg);
                goto error;
            }
            LogInfo("will use logport: %s\n", args.ttyGENERAL);
            break;
        case 'd':
            if (optarg[0] == 't') //ttyUSB0
                snprintf(args.ttyDM, sizeof(args.ttyDM), "/dev/%.250s", optarg);
            else if (optarg[0] == 'U') //USB0
                snprintf(args.ttyDM, sizeof(args.ttyDM), "/dev/tty%.247s", optarg);
            else if (optarg[0] == '/')
                snprintf(args.ttyDM, sizeof(args.ttyDM), "%.255s", optarg);
            else
            {
                LogInfo("unknow dev %s\n", optarg);
                goto error;
            }
            LogInfo("will use diag port: %s\n", args.ttyDM);
            strncpy(taskinfo1_portname, optarg, FIBO_BUF_SIZE);
            break;
        case 's':
            snprintf(args.logdir, sizeof(args.logdir), "%.255s", optarg);
            //parser_tftp(optarg);
            parser_ftp(optarg);
            break;
        case 'P':
            parser_tcp(optarg);
            break;
        case 'D':
            args.delete_logs = optarg ? optarg : "";
            break;
        case 'n':
            args.logfile_num = atoi(optarg);
            if (args.logfile_num < 0)
            {
                args.logfile_num = 0;
            }
            else if (args.logfile_num > LOGFILE_NUM)
            {
                args.logfile_num = LOGFILE_NUM;
            }
            s_logfile_num = args.logfile_num;
            g_log_unisoc_file_maxNum = args.logfile_num;
            break;
        case 'm':
            args.logfile_sz = atoi(optarg) * 1024 * 1024;
            if(args.logfile_sz < LOGFILE_SIZE_MIN)
            {
                args.logfile_sz = LOGFILE_SIZE_MIN;
            }
            else if(args.logfile_sz > LOGFILE_SIZE_MAX)
            {
                args.logfile_sz = LOGFILE_SIZE_MAX;
            }
            break;
        case 'a':
            ap_log = atoi(optarg);
            break;
        case 'i':
            if (!strncmp(optarg, TFTP_F, strlen(TFTP_F)))
            {
                g_tftp_server_ip = optarg+strlen(TFTP_F);
                if (tftp_test_server(g_tftp_server_ip))
                {
                    LogInfo("save dump to tcp server %s\n", g_tftp_server_ip);
                }
                else
                {
                    goto error;
                }
            }
            else
            {
                snprintf(ftpservipaddr, 128, "%s", optarg);
                set_ftpipaddr = true;
                LogInfo("set_ftpservipaddr successfully,is %s", ftpservipaddr);
            }
            break;
        case 'u':
            snprintf(username, 128, "%s", optarg);
            set_username = true;
            LogInfo("set_username successfully,is %s", username);
            break;
        case 'w':
            snprintf(password, 128, "%s", optarg);
            set_passwd = true;
            LogInfo("set_password successfully,is %s", password);
            break;
        case 't':
            LogInfo("platform is unisoc\n");
            break;
        case 'h':
        default:
            usage(argv[0]);
            goto error;
        }
    }

    //LogInfo("will use filter file: %s\n", args.filter_cfg ? args.filter_cfg : "default filter");

    return &args;
error:
    return NULL;
}

static int prepare(struct argument *args)
{
    const  struct ql_usb_device_info *usb_dev = args->ql_dev;
    int force_use_usbfs = 0;
    memset(&args->fds, -1, sizeof(args->fds));

    if(args->ql_dev->idVendor == 0x2c7c && (args->ql_dev->idProduct&0xF000) == 0x0000
        && args->ql_dev->bNumInterfaces == 1 && !args->ql_dev->hardware)
     {
        //to avoid tty's echo cause fail
        force_use_usbfs = 1;
     }

#if 0
    //ap dump
    if(args->ql_dev->idVendor == 0x2cb7 && (args->ql_dev->idProduct == 0x0a09))
    {
        force_use_usbfs = 1;
    }
#endif

    //diag port
    if(usb_dev->ttyDM[0] && !force_use_usbfs)
    {
        args->fds.dm_ttyfd = serial_open(usb_dev->ttyDM);
        if (args->fds.dm_ttyfd < 0)
        {
            LogInfo("tty open %s failed, errno: %d (%s)\n", usb_dev->ttyDM, errno, strerror(errno));
            goto error;
        }
    }
    else if(usb_dev->dm_intf.bInterfaceNumber != 0xFF)
    {
        //cp
        static usbfs_read_cfg_t cfg;

        args->fds.dm_usbfd = ql_usbfs_open_interface(usb_dev, usb_dev->dm_intf.bInterfaceNumber);
        LogInfo("open /dev/%s dm_usbfd = %d\n", usb_dev->devname, args->fds.dm_usbfd);
        if(args->fds.dm_usbfd < 0)
        {
            goto error;
        }

        if(socketpair(AF_LOCAL, SOCK_STREAM, 0, args->fds.dm_sockets))
        {
            safe_close_fd(args->fds.dm_usbfd);
            LogInfo("socketpair( dm ) failed, errno: %d (%s)\n", errno, strerror(errno));
            goto error;
        }

        cfg.usbfd = args->fds.dm_usbfd;
        cfg.ep = usb_dev->dm_intf.ep_in;
        cfg.outfd = args->fds.dm_sockets[1];
        cfg.rx_size= RX_URB_SIZE;
        cfg.dev = "dm";
        if(pthread_create(&args->fds.dm_tid, NULL, ulog_usbfs_read, (void*)&cfg))
        {
            LogInfo("pthread_create( dm ) failed, errno: %d (%s)\n", errno, strerror(errno));
            safe_close_fd(args->fds.dm_usbfd);
            safe_close_fd(args->fds.dm_sockets[0]);
            safe_close_fd(args->fds.dm_sockets[1]);
            goto error;
        }
    }

    //log port
    if(usb_dev->ttyGENERAL[0] && !force_use_usbfs)
    {
        args->fds.general_ttyfd = serial_open(usb_dev->ttyGENERAL);
        if (args->fds.general_ttyfd< 0)
        {
            LogInfo("tty open %s failed, errno: %d (%s)\n", usb_dev->ttyGENERAL, errno, strerror(errno));
            goto error;
        }
    }
    else if(usb_dev->general_intf.bInterfaceNumber != 0xFF)
    {
        //ap
        static usbfs_read_cfg_t cfg;

        args->fds.general_usbfd = ql_usbfs_open_interface(usb_dev, usb_dev->general_intf.bInterfaceNumber);
        LogInfo("open /dev/%s general_usbfd = %d\n", usb_dev->devname, args->fds.general_usbfd);

        if(args->fds.general_usbfd < 0)
        {
            goto error;
        }

        if(socketpair(AF_LOCAL, SOCK_STREAM, 0, args->fds.general_sockets))
        {
            safe_close_fd(args->fds.general_usbfd);
            LogInfo("socketpair( log ) failed, errno: %d (%s)\n", errno, strerror(errno));
            goto error;
        }

        cfg.usbfd = args->fds.general_usbfd;
        cfg.ep = usb_dev->general_intf.ep_in;
        cfg.outfd = args->fds.general_sockets[1];
        cfg.rx_size= RX_URB_SIZE;
        cfg.dev = "general";
        if(pthread_create(&args->fds.general_tid, NULL, ulog_usbfs_read, (void*)&cfg))
        {
            LogInfo("pthread_create( general ) failed, errno: %d (%s)\n", errno, strerror(errno));
            safe_close_fd(args->fds.general_usbfd);
            safe_close_fd(args->fds.general_sockets[0]);
            safe_close_fd(args->fds.general_sockets[1]);
            goto error;
        }
    }

    return 0;
error:
    return -1;
}

static void close_fds(struct argument *args)
{
    int intf = 0;
    //LogInfo("%s enter\n", __func__);
    if(args->fds.dm_usbfd != -1)
    {
        intf = args->ql_dev->dm_intf.bInterfaceNumber;
        ioctl(args->fds.dm_usbfd, USBDEVFS_RELEASEINTERFACE, &intf);
        safe_close_fd(args->fds.dm_usbfd);
        pthread_join(args->fds.dm_tid, NULL);
        safe_close_fd(args->fds.dm_sockets[0]);
        safe_close_fd(args->fds.dm_sockets[1]);
    }
    else
    {
        safe_close_fd(args->fds.dm_ttyfd);
    }

    if(args->fds.general_usbfd != -1)
    {
        intf = args->ql_dev->general_intf.bInterfaceNumber;
        ioctl(args->fds.general_usbfd, USBDEVFS_RELEASEINTERFACE, &intf);
        safe_close_fd(args->fds.general_usbfd);
        pthread_join(args->fds.general_tid, NULL);
        safe_close_fd(args->fds.general_sockets[0]);
        safe_close_fd(args->fds.general_sockets[1]);
    }
    else
    {
        safe_close_fd(args->fds.general_ttyfd);
    }

    //LogInfo("%s exit\n", __func__);
}

static void* ulog_logfile_init_filter_thread(void* arg)
{
    init_filter_cfg_t *cfg = (init_filter_cfg_t *)arg;

    if (cfg && cfg->ops && cfg->ops->init_filter)
        cfg->ops->init_filter(cfg->fd, cfg->filter);

    LogInfo("qlog_init_filter_finished\n");
    return NULL;
}

static int read_serial_cplog(const struct argument *args)
{
    size_t savelog_size_dm = 0;
    size_t savelog_size_general = 0;
    size_t savelog_size_total = 0;
    uint8_t *rbuf;
    const size_t rbuf_size = QLOG_BUF_SIZE;
    const char *logfile_suffix = "logel";
    //static ulog_ops_t ulog_ops;
    pthread_t init_filter_tid;
    pthread_t log_save_tid;
    init_filter_cfg_t init_filter_cfg;
    size_t total_read = 0;
    unsigned now_msec = 0;
    unsigned last_msec = 0;

    char* temp_p;
    char working_dir[128];

    const char *logfile_dir = args->logdir;
    size_t logfile_size = args->logfile_sz;
    const char *filter_cfg = args->filter_cfg;

    int dmfd = -1;
    int generalfd = -1;
    int dump_usbfd;

    strcpy(g_str_sub_unisoc_log_dir,logfile_dir);
    temp_p = getcwd(working_dir,128);
    if(temp_p!=NULL)
    {
        /* Modify for MBB0063-692 20230721 zhangboxing begin*/
        if (logfile_dir[0] == '/')
        {
            strcpy(g_str_unisoc_log_dir,logfile_dir);
        }
        else
        {
            snprintf(g_str_unisoc_log_dir, 1024, "%s/%s", working_dir,logfile_dir);
        }
        /* Modify for MBB0063-692 20230721 zhangboxing end*/
    }

    if(ap_log == ONLY_CP)
    {
        if ((args->fds.dm_ttyfd != -1))
        {
            dmfd = args->fds.dm_ttyfd;
            LogInfo("dmfd is args->fds.dm_ttyfd=%d\n",dmfd);
        }
        else if (args->fds.dm_sockets[0] != -1)
        {
            dmfd = args->fds.dm_sockets[0];
            LogInfo("dmfd is args->fds.dm_sockets[0]=%d\n",dmfd);
        }
    }

    if (args->fds.general_ttyfd != -1)
    {
        generalfd = args->fds.general_ttyfd;
        LogInfo("generalfd is args->fds.general_ttyfd=%d\n",generalfd);
    }
    else if (args->fds.general_sockets[0] != -1)
    {
        generalfd = args->fds.general_sockets[0];
        LogInfo("generalfd is args->fds.general_sockets[0]=%d\n",generalfd);
    }

    if (is_tty2tcp())
    {
        LogInfo("ulog_ops is tty2tcp_log_ops\n");
        filter_cfg = logfile_dir;
        ulog_ops = tty2tcp_log_ops;
        exit_after_usb_disconnet = 1; // do not continue when tty2tcp mode
    }
    else
    {
        LogInfo("ulog_ops is unisoc_log_ops\n");
        ulog_ops =  unisoc_log_ops;
    }

    if (!ulog_ops.logfile_create)
    {
        if(is_tcp_client())
        {
            g_donot_split_logfile = 1;
            ulog_ops.logfile_create = tcp_client_log_ops.logfile_create;
        }
        else
        {
            ulog_ops.logfile_create = ulog_logfile_create;
        }
    }
    if (!ulog_ops.logfile_save)
    {
        ulog_ops.logfile_save = ulog_logfile_save;
    }
    if (!ulog_ops.logfile_close)
    {
        ulog_ops.logfile_close = ulog_logfile_close;
    }

    rbuf = (uint8_t *)malloc(rbuf_size);
    if (rbuf == NULL)
    {
          LogInfo("Fail to malloc rbuf_size=%zd, errno: %d (%s)\n", rbuf_size, errno, strerror(errno));
          return -1;
    }

    if(ap_log == ONLY_CP)
    {
        init_filter_cfg.ops = &ulog_ops;
        init_filter_cfg.fd = dmfd;
        init_filter_cfg.filter = filter_cfg;
        if (pthread_create(&init_filter_tid, NULL, ulog_logfile_init_filter_thread, (void*)&init_filter_cfg)) {
              LogInfo("Fail to create init_filter_thread, errno: %d (%s)\n", errno, strerror(errno));
              free(rbuf);
              return -1;
        }
    }

    if(logfile_fd == -1)
    {
        if(is_tftp())
        {
            logfile_fd = ulog_logfile_create_fullname(0, "cp.logel", 0, 1);
        }
        else
        {
            logfile_fd = ulog_ops.logfile_create(logfile_dir, logfile_suffix, s_logfile_seq);
        }
        if (logfile_fd <= 0)
        {
            return -1;
        }
    
        if (ulog_ops.logfile_init)
        {
            ulog_ops.logfile_init(logfile_fd, s_logfile_seq);
            s_logfile_seq++;
        }
    }

    //now_msec = last_msec = ulog_msecs();
    /* 创建单链表,创建线程,从链表中取数据处理 */
    struct Head head;
    initHead(&head);

    if (pthread_create(&log_save_tid, NULL, ulog_logfile_save_thread, (void*)&head))
    {
        LogInfo("Fail to create ulog_logfile_save_thread, errno: %d (%s)\n", errno, strerror(errno));
        free(rbuf);
        return -1;
    }

    while (ulog_exit_requested == 0)
    {
        ssize_t rc, wc = 0;
        int fds[2];
        int fd_n = 0;
        static bool has_open_usb = false;

        if(m_bSendTCmd == 1)
        {
            /* cp dump */
            if(!has_open_usb)
            {
                has_open_usb = true;
                
                /* use ioctl to get ap dump */
                dump_usbfd = ql_usbfs_open_interface(args->ql_dev, args->ql_dev->dm_intf.bInterfaceNumber);
                LogInfo("%s dump_usbfd is %d, args->ql_dev->dm_intf.ep_in id %u,args->ql_dev->dm_intf.ep_out id %u\n", __func__, dump_usbfd, args->ql_dev->dm_intf.ep_in, args->ql_dev->dm_intf.ep_out);

                ql_usbfs_write(dump_usbfd,  args->ql_dev->dm_intf.ep_out, "t\n", 2);
            }

            rc = ql_usbfs_read(dump_usbfd, args->ql_dev->dm_intf.ep_in, rbuf, rbuf_size);
            //LogInfo("ql_usbfs_read %zd\n", rc);
        }
        else
        {
            if(ap_log == ONLY_CP)
            {
                if (dmfd != -1)
                {
                    fds[fd_n++] = dmfd;
                }
            }

            if(generalfd != -1)
            {
                fds[fd_n++] = generalfd;
            }

            rc = ulog_poll_read_fds(fds, fd_n, rbuf, rbuf_size, 5000);
            //LogInfo("ulog_poll_read_fds %zd\n", rc);
        }
        if(rc > 0)
        {
            //LogInfo("ulog_poll_read_fds read %zd, list->count is %d\n", rc, head.count);

            struct Node *node = (struct Node *)malloc(sizeof(struct Node));

            if (fds[0] == dmfd || m_bSendTCmd == 1)
            {
                node->logtype = 0;
            }
            else if (fds[0] == generalfd)
            {
                node->logtype = 1;
            }
            node->size =rc;
            node->rbuf = (uint8_t *)malloc(rc);
            memcpy(node->rbuf, rbuf, rc);
            node->next = NULL;
            insertNode(&head, node);
        }


#if 0
        if(!(g_rx_log_count%128))
        {
            now_msec = ulog_msecs();
        }
        total_read += rc;

        if((total_read >= (16*1024*1024)) || (now_msec >= (last_msec + 5000)))
        {
            now_msec = ulog_msecs();
            LogInfo("read_bytes(%zu),tick(%d),%zdM %zdK %zdB\n", total_read,
              (now_msec-last_msec)/1000, total_read/(1024*1024),
                total_read/1024%1024,total_read%1024);
            last_msec = now_msec;
            total_read = 0;
        }

        g_rx_log_count++;


        if(logfile_fd == -1)
        {
            if(is_tftp())
            {
                logfile_fd = ulog_logfile_create_fullname(0, "cp.logel", 0, 1);
            }
            else
            {
			    logfile_fd = ulog_ops.logfile_create(logfile_dir, logfile_suffix, s_logfile_seq);
            }
            if (logfile_fd <= 0)
            {
                break;
            }

            if (ulog_ops.logfile_init)
            {
                ulog_ops.logfile_init(logfile_fd, s_logfile_seq);
                s_logfile_seq++;
            }
        }

        if (fds[0] == dmfd)
        {
            g_unisoc_log_type = 0;
        }
        else if (fds[0] == generalfd)
        {
            g_unisoc_log_type = 1;
        }

        wc = ulog_ops.logfile_save(logfile_fd, rbuf, rc);     //normal cp log
        if (wc != rc)
        {
            LogInfo("savelog fail %zd/%zd, break\n", wc, rc);
            exit_after_usb_disconnet = 1; // do not continue when not usb disconnect
            ulog_exit_requested = 1;
            break;
        }
        savelog_size_dm += wc;

        savelog_size_total = savelog_size_dm + savelog_size_general;
        if (savelog_size_total >= logfile_size && g_donot_split_logfile == 0)
        {
            savelog_size_dm = 0;
            savelog_size_general = 0;
            savelog_size_total = 0;
            ulog_ops.logfile_close(logfile_fd);
            logfile_fd = -1;

            strcpy(g_ftpfilename,g_filename);
            LogInfo("CLOSE LOG FILE %s\n",g_ftpfilename);
            ftp_put_unisoc_flag = 1;
            m_bVer_Obtained_change();
        }
#endif
    }

    if(m_bSendTCmd == 1)
    {
        safe_close_fd(dump_usbfd);
    }

    //LogInfo("wait for data process finish!\n");

    while(head.count > 0)
    {
        /* wait all list data process finish */
        sleep(1);
        //LogInfo("wait for data process finish! sleep 1, head.count is %d\n", head.count);
    }

    LogInfo("data process finish!\n");

    if (logfile_fd != -1)
    {
        ulog_ops.logfile_close(logfile_fd);
        logfile_fd = -1;
    }
    free(rbuf);
    if (ulog_exit_requested && ulog_ops.clean_filter && ap_log)
    {
        LogInfo("clean_filter\n");
        ulog_ops.clean_filter(dmfd);
    }

    if (!pthread_kill(init_filter_tid, 0))
    {
        LogInfo("pthread_join( filter )\n");
#ifdef USE_NDK
        //TODO Android NDK do not support pthread_cancel
#else
        pthread_cancel(init_filter_tid);
#endif
        pthread_join(init_filter_tid, NULL);
    }

    return 0;
}

static int str_has_suffix(const char *str1, const char *str2)
{
    if (!str1 || !str2)
        return 0;

    size_t slen1 = strlen(str1);
    size_t slen2 = strlen(str2);
    return !strncasecmp(str1 + slen1 - slen2, str2, slen2);
}

static void delete_logs(const char *dir, const char *suffix)
{
    char _suffix[256] = {'\0'};
    char tmpstr[512] = {'\0'};
    struct dirent *entptr = NULL;
    DIR *dirptr = NULL;

    if (!dir || !suffix)
        return;

    snprintf (_suffix, sizeof(_suffix), ".%.248s", suffix);
    dirptr = opendir(dir);
    if (!dirptr)
        return;

    tmpstr[0] = '\0';
    while((entptr = readdir(dirptr)))
    {
        if (entptr->d_name[0] == '.')
            continue;

        if (!str_has_suffix(entptr->d_name, ".logel"))
            continue;

        if (suffix[0] == '\0' || str_has_suffix(entptr->d_name, _suffix))
        {
            snprintf(tmpstr, sizeof(tmpstr), "%.255s/%.255s", dir, entptr->d_name);
            LogInfo("try to remove %s\n", tmpstr);
            unlink(tmpstr);
        }
    }
    closedir(dirptr);
}

int unisoc_log_main(int argc, char** argv)
{
#if 0
    int opt = -1;
    int cplogfilesize = 0;
    char logdir[FIBO_BUF_SIZE] = ".";
    char timebuf[FIBO_BUF_SIZE] = {0};
    char port2name[FIBO_BUF_SIZE] = {0};
    struct tm *tm = NULL;
    time_t t;
    TaskInfo_t *taskinfo1 = NULL;
    TaskInfo_t *taskinfo2 = NULL;

    LogInfo("start\n");

    taskinfo1 = taskinfo_init(0);

    if (taskinfo1 == NULL) {
        goto error_exit;
    }

    optind = 1;//must set to 1
    while ((opt = getopt(argc, argv, "d:p:s:m:n:h")) != EOF) {
        switch (opt) {
            case 'd':
                strncpy(taskinfo1->portname, optarg, FIBO_BUF_SIZE);
                break;
            case 'p':
                strncpy(port2name, optarg, FIBO_BUF_SIZE);
                break;
            case 's':
                strncpy(logdir, optarg, FIBO_BUF_SIZE);
                break;
            case 'm':
                cplogfilesize = atoi(optarg) * 1024*1024;
                break;
            case 'n':
                g_log_unisoc_file_maxNum = atoi(optarg);
                break;
            case 'h':
                usage(argv[0]);
                goto error_exit;
            default:;
        }
    }

    LogInfo("single_logfile_size: %u\n", taskinfo1->single_logfile_size);

    t = time(NULL);
    tm = localtime(&t);
    sprintf(timebuf, "%02d%02d%02d_%02d%02d%02d", 1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    sprintf(taskinfo1->logdir, "%s/fibolog_ap_%s/", logdir, timebuf);
    //LogInfo("ap logdir: %s\n", taskinfo1->logdir);

    taskinfo1->pdev = fibo_get_fibocom_device(uinisoc_find_devices_in_table, taskinfo1->portname, taskinfo1->portnum);
    if (taskinfo1->pdev == NULL) {
        goto error_exit;
    }

    if (fibo_usb_open(taskinfo1->pdev, taskinfo1->portnum, 1)) {
        goto error_exit;
    }

    /*zhangboxing 650APlog GRAB 2022/01/16 begin*/
    fd_650_port = taskinfo1->pdev->ttyfd[taskinfo1->portnum];
    LogInfo("fd_650_port: %d\n", fd_650_port);

    /*if (unisoc_usb_option_setup(taskinfo1->pdev, taskinfo1->portnum)) {
        goto error_exit;
    }*/

    if (fibo_log_capture_ap_process(taskinfo1)) {
        goto error_exit;
    }
    /*zhangboxing 650APlog GRAB 2022/01/16 end*/

    if (taskinfo1->pdev->ifnum[1] >= 0) {
        taskinfo2 = taskinfo_init(1);
        if (taskinfo2 == NULL) {
            goto error_exit;
        }
        /*zhangboxing MBB0063-571 2023/01/31  start*/
        if(cplogfilesize == 0)
        {
            taskinfo2->single_logfile_size = taskinfo1->single_logfile_size;
        }
        else
        {
            taskinfo2->single_logfile_size = cplogfilesize;
        }
        LogInfo("taskinfo2->single_logfile_size: %u\n", taskinfo2->single_logfile_size);
        /*zhangboxing MBB0063-571 2023/01/31  end*/

        strncpy(taskinfo2->portname, port2name, FIBO_BUF_SIZE);
        taskinfo2->pdev = fibo_get_fibocom_device(uinisoc_find_devices_in_table, taskinfo2->portname, taskinfo2->portnum);
        if (taskinfo2->pdev != NULL)  {
            sprintf(taskinfo2->logdir, "%s/fibolog_cp_%s/", logdir, timebuf);
            LogInfo("cp logdir: %s\n", taskinfo2->logdir);

            /*zhangboxing 650APlog GRAB 2022/01/16 begin*/

            if (fibo_usb_open(taskinfo2->pdev, taskinfo2->portnum, 1)) {
                goto error_exit;
            }

            /*if (unisoc_usb_option_setup(taskinfo2->pdev, taskinfo2->portnum)) {
                goto error_exit;
            }*/
            /*zhangboxing 650APlog GRAB 2022/01/16 end*/
            if (fibo_log_capture_process(taskinfo2)) {
                goto error_exit;
            }
        }
    }

    signal(SIGINT, fibo_exit_function); //Ctrl+C
    signal(SIGTERM, fibo_exit_function); //Kill
    while (g_log_process_flag)
    {
        sleep(2);
    }

error_exit:
    taskinfo1->isRun = 0;
    if (taskinfo2) {
        taskinfo2->isRun = 0;
    }
    sleep(1);
    fibo_usb_close(taskinfo1->pdev, taskinfo1->portnum);
    if (taskinfo2) {
        fibo_usb_close(taskinfo2->pdev, taskinfo2->portnum);
    }
    FreeTask(taskinfo1);
    if (taskinfo2) {
        FreeTask(taskinfo2);
    }

    return 0;
#endif
    int ret = -1;
    struct argument *args;
    int modules_num = 0;
    int cur_module = 0;
    int loop_times = 0;

    char logdir[FIBO_BUF_SIZE] = ".";
    char timebuf[256] = {0};
    struct tm *tm = NULL;
    time_t t;
    TaskInfo_t *taskinfo1 = NULL;

    args = parser_args(argc, argv);
    if(!args)
    {
        return 0;
    }

    if (args->delete_logs)
    {
        delete_logs(args->logdir, args->delete_logs);
    }

    if(set_ftpipaddr && set_username && set_passwd)
    {
        use_ftp = true;
        LogInfo("set_ftpipaddr && set_username && set_passwd,will upload log to remote via FTP\n");
    }

    if((set_ftpipaddr && (!set_username || !set_passwd)) ||
       (set_username  && (!set_ftpipaddr || !set_passwd))||
       (set_passwd    && (!set_ftpipaddr || !set_username)))
    {
        usage(argv[0]);
        goto error;
    }

    if(use_ftp)
    {
        sleep(2);
        log_upload_by_ftp();
    }

    if(ap_log != ONLY_CP)
    {
        taskinfo1 = taskinfo_init(0);
        if (taskinfo1 == NULL)
        {
          goto error;
        }
        strcpy(logdir,args->logdir);
        strcpy(taskinfo1->portname,taskinfo1_portname);
        t = time(NULL);
        tm = localtime(&t);
        sprintf(timebuf, "%02d%02d%02d_%02d%02d%02d", 1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
        LogInfo("timebuf is %s \n",timebuf);
        sprintf(taskinfo1->logdir, "%s/fibolog_ap_%s/", logdir, timebuf);
        LogInfo("taskinfo1->logdir is %s \n",taskinfo1->logdir);
        LogInfo("taskinfo1->portname is %s \n",taskinfo1->portname);
        LogInfo("taskinfo1->portnum is %d \n",taskinfo1->portnum);

        if (strcmp(taskinfo1->portname, "/dev/sdiag_nr") == 0) 
        {
            taskinfo1->pdev = malloc(sizeof(fibo_usbdev_t));
            strcpy(taskinfo1->pdev->portname , "/dev/sdiag_nr");
            LogInfo("taskinfo1->portnum is %s \n",taskinfo1->pdev->portname);
            if(fibo_usb_open(taskinfo1->pdev, taskinfo1->portnum, 1))
            {
                LogInfo("fibo_usb_open fail\n");
                goto error;
            }

            fd_650_port = open(taskinfo1->pdev->portname, O_RDWR | O_SYNC);;
            if (fibo_log_capture_ap_process(taskinfo1))
            {
                LogInfo("fibo_log_capture_ap_process fail\n");
                goto error;
            }
        }
        else
        {
            taskinfo1->pdev = fibo_get_fibocom_device(uinisoc_find_devices_in_table, taskinfo1->portname, taskinfo1->portnum);
            if(taskinfo1->pdev == NULL)
            {
                LogInfo("taskinfo1->pdev is NULL\n");
                goto error;
            }

            if(fibo_usb_open(taskinfo1->pdev, taskinfo1->portnum, 1))
            {
                LogInfo("fibo_usb_open fail\n");
                goto error;
            }


            fd_650_port = taskinfo1->pdev->ttyfd[taskinfo1->portnum];
            LogInfo("fd_650_port: %d\n", fd_650_port);
            if(fd_650_port == -1)
            {
                LogInfo("fd_650_port is  -1\n");
                goto error;
            }

            LogInfo("taskinfo1->pdev->portname is %s \n",taskinfo1->pdev->portname);
            LogInfo("taskinfo1->pdev->ifnum[1] is %d \n",taskinfo1->pdev->usbdev);

            if (fibo_log_capture_ap_process(taskinfo1))
            {
                LogInfo("fibo_log_capture_ap_process fail\n");
                goto error;
            }
        }
    }

__restart:

    if (ulog_exit_requested)
    {
        return 0;
    }
    args->ql_dev = NULL;
    memset(s_usb_device_info, 0, 8 * sizeof(struct ql_usb_device_info));
    s_usb_device_info[0].dm_intf.bInterfaceNumber = 0xff;
    s_usb_device_info[0].general_intf.bInterfaceNumber = 0xff;
    s_usb_device_info[0].general_type = -1;
    cur_module = modules_num = 0;

    if(strStartsWith(args->ttyDM, "/dev/ttyUSB"))
    {
        strcpy(s_usb_device_info[cur_module].ttyDM,args->ttyDM);
        LogInfo("s_usb_device_info[cur_module].ttyDM====%s!\n", s_usb_device_info[cur_module].ttyDM);
    }

    if(strStartsWith(args->ttyDM, "/dev/sdiag_nr"))
    {
        strcpy(s_usb_device_info[cur_module].ttyDM,args->ttyDM);
        LogInfo("s_usb_device_info[cur_module].ttyDM====%s!\n", s_usb_device_info[cur_module].ttyDM);
        modules_num = 1;
        s_usb_device_info[cur_module].hardware = 1;
    }

    if(strStartsWith(args->ttyGENERAL, "/dev/ttyUSB"))
    {
        strcpy(s_usb_device_info[cur_module].ttyGENERAL,args->ttyGENERAL);
        LogInfo("s_usb_device_info[cur_module].ttyGENERAL====%s!\n", s_usb_device_info[cur_module].ttyGENERAL);
        modules_num = 1;
    }

    if(strStartsWith(args->ttyGENERAL, "/dev/slog_nr"))
    {
        strcpy(s_usb_device_info[cur_module].ttyGENERAL,args->ttyGENERAL);
        LogInfo("s_usb_device_info[cur_module].ttyDM====%s!\n", s_usb_device_info[cur_module].ttyGENERAL);
        modules_num = 1;
        s_usb_device_info[cur_module].hardware = 1;
    }

    if (modules_num == 0)
    {
        modules_num = find_unisoc_modules();
        if (modules_num == 0)
        {
            LogInfo("No FIBOCOM Modules found, Wait for connect or Press CTRL+C to quit!\n");
            sleep(2);
            goto __restart;
        }
    }

    args->ql_dev = &s_usb_device_info[cur_module];
    g_is_usb_disconnect = 0;
    g_donot_split_logfile = 0;
    g_is_unisoc_chip = 1;

    ulog_args = args;

    ret = prepare(args);
    if (ret < 0)
    {
        LogInfo("arg do prepare failed\n");
        return ret;
    }
    loop_times++;

    if (access(args->logdir, F_OK) && errno == ENOENT
        && !is_tftp() && !is_ftp() && !is_tty2tcp() && !is_tcp_client())
    {
        mkdir(args->logdir, 0777);
    }

    LogInfo("Press CTRL+C to stop catch log.\n");

    if (args->ql_dev->bNumInterfaces == 1)
    {
        // ap dump
        int i = 0;
        if (isLittleEndian())
        {
            LogInfo("Little Endian\n");
        }
        else
        {
            LogInfo("Big Endian\n");
            islittleendian = false;
        }

        /* use ioctl to get ap dump */
        int dump_usbfd;
        dump_usbfd = ql_usbfs_open_interface(args->ql_dev, args->ql_dev->dm_intf.bInterfaceNumber);
        LogInfo("%s dump_usbfd is %d, args->ql_dev->dm_intf.ep_in id %u,args->ql_dev->dm_intf.ep_out id %u\n", __func__, dump_usbfd, args->ql_dev->dm_intf.ep_in, args->ql_dev->dm_intf.ep_out);

        ql_usbfs_write(dump_usbfd,  args->ql_dev->dm_intf.ep_out, "t\n", 2);

        while (!is_dump_finish)
        {
            unsigned char buffer[41000] = {0};
            int n = 0;

            n = ql_usbfs_read(dump_usbfd, args->ql_dev->dm_intf.ep_in, buffer, 41000);
            if(n > 0 )
            {
                /*
                LogInfo("ql_usbfs_read %d\n", n);

                for(i = 0; i < n ; i++)
                {
                   printf("%02X ", buffer[i]); 
                }
                */
                
                save_to_file(args->logdir, buffer, n);
            }
            else if(n == 0)
            {
                LogInfo("ql_usbfs_read 0\n");
            }
            else
            {
                LogInfo("ql_usbfs_read error!\n");
            } 
        }
        safe_close_fd(dump_usbfd);
        return 0;
    }
    else if(args->ql_dev->bNumInterfaces > 1 || s_usb_device_info[cur_module].hardware == 1)
    {
        if (args->fds.dm_usbfd != -1 || args->fds.general_usbfd != -1)
        {
            LogInfo("catch log via usbfs\n");
        }
        else
        {
            LogInfo("catch log via tty port\n");
        }

        if(ap_log == ONLY_TFTP_AP)
        {
            while(1)
            {
                LogInfo("only tftp ap log.............\n");
                sleep(1);
            }
        }
        else
        {
            /*if (is_tftp())
            {
                LogInfo("tftp only support catch dump, but modem is not in ram dump state\n");
                ulog_exit_requested = 1;
                goto error;
            }*/
            ret = read_serial_cplog(args);
        }
    }
    else
    {
        LogInfo("unknow state! quit!\n");
        goto error;
    }

    signal(SIGINT, fibo_exit_function); //Ctrl+C
    signal(SIGTERM, fibo_exit_function); //Kill
    signal(SIGHUP, fibo_exit_function);

error:

        if(ap_log != ONLY_CP)
        {
            taskinfo1->isRun = 0;
            sleep(1);
            fibo_usb_close(taskinfo1->pdev, taskinfo1->portnum);
            FreeTask(taskinfo1);
        }

        close_fds(args);
        if (ulog_exit_requested == 0 && exit_after_usb_disconnet == 0)
        {
            sleep(1);
            goto __restart;
        }

        return ret;
}

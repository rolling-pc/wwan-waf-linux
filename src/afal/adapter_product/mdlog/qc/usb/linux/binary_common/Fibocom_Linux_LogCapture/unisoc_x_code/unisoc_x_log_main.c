/*******************************************************************
 *          CopyRight(C) 2022-2026  Fibocom Wireless Inc
 *******************************************************************
 * FileName : zte_trace_tool.c
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
#include <signal.h>
#include "fifo.h"
#include "misc_usb.h"
#include "unisoc_x_devices_list.h"
#include "xlog_wrapper.h"

typedef struct
{
    int portnum;
    int isRun;
    char portname[FIBO_BUF_SIZE];
    char logdir[FIBO_BUF_SIZE];
    fibo_usbdev_t *pdev;
    int single_logfile_size;
    struct fifo *plog_fifo;
    uint32_t start_tick;
    uint32_t read_bytes;
} TaskInfo_t;

#define MAX_SINGLE_LOG_SIZE (50 * 1024 * 1024) // MB
#define LOG_BUFFER_SIZE (2048)                 // (0x100000) //1M
#define FIFO_BUFFER_SIZE (0xA00000)            // 10M

static volatile int g_log_process_flag = 1;

fibo_usbdev_t *unisoc_x_find_devices_in_table(int idvendor, int idproduct)
{
    int i, size = 0;

    size = sizeof(unisoc_x_devices_table) / sizeof(unisoc_x_devices_table[0]);
    for (i = 0; i < size; i++)
    {
        fibo_usbdev_t *pdev = &unisoc_x_devices_table[i];

        if ((pdev->idVendor == idvendor) && (pdev->idProduct == idproduct))
        {
            return pdev;
        }
    }

    return NULL;
}
#if 0
static TaskInfo_t *taskinfo_init(int portnum)
{
    TaskInfo_t *pTaskInfo = (TaskInfo_t *)malloc(sizeof(TaskInfo_t));

    if (pTaskInfo == NULL)
    {
        LogInfo("malloc pTaskInfo failed.\n");
        return NULL;
    }
    memset(pTaskInfo, 0, sizeof(TaskInfo_t));

    pTaskInfo->portnum = portnum;
    pTaskInfo->isRun = 0;
    pTaskInfo->single_logfile_size = MAX_SINGLE_LOG_SIZE;
    snprintf(pTaskInfo->logdir, sizeof(pTaskInfo->logdir), ".");

    pTaskInfo->plog_fifo = fifo_alloc(FIFO_BUFFER_SIZE);
    if (pTaskInfo->plog_fifo == NULL)
    {
        LogInfo("malloc pTaskInfo->plog_fifo failed.\n");
        if (pTaskInfo)
        {
            free(pTaskInfo);
            pTaskInfo = NULL;
        }
        return NULL;
    }

    return pTaskInfo;
}

static void FreeTask(TaskInfo_t *pTaskInfo)
{
    if (pTaskInfo)
    {
        fifo_free(pTaskInfo->plog_fifo);
        free(pTaskInfo);
    }
}

static void *thread_read_serial_log(void *arg)
{
    TaskInfo_t *pTaskInfo = (TaskInfo_t *)arg;
    uint8_t *rx_buffer = NULL;

    LogInfo("[%s] start, logdir:%s\n", pTaskInfo->pdev->syspath, pTaskInfo->logdir);

    rx_buffer = (uint8_t *)malloc(LOG_BUFFER_SIZE);
    if (rx_buffer == NULL)
    {
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
                    LogInfo("[%d] read_bytes(%u), tick(%u), %uB/s\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], pTaskInfo->read_bytes, diff_tick, pTaskInfo->read_bytes / diff_tick);
                    pTaskInfo->start_tick = cur_tick;
                    pTaskInfo->read_bytes = 0;
                }
            }
        }
        usleep(10000);
    }

    if (rx_buffer)
    {
        free(rx_buffer);
        rx_buffer = NULL;
    }

    LogInfo("[%d] stopped\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);

    return NULL;
}

void WriteDataToFile(FILE *logf, uint8_t *lpBuffer, uint32_t dwDataSize)
{
    if (NULL == logf)
    {
        return;
    }

    uint32_t dwWriteSize = dwDataSize;

    // if (!m_bSendTCmd && m_lFileSize + sizeof(dwWriteSize) + dwDataSize > log_file_threashold)
    //{
    //     CreateLogelFile();
    // }

    fwrite(&dwWriteSize, 1, sizeof(dwWriteSize), logf);
    fwrite(lpBuffer, 1, dwDataSize, logf);
    // fflush(m_logf);

    // m_lFileSize += sizeof(dwWriteSize) + dwDataSize;
}

/* get sys timestamp */
long GetTimeStamp()
{
    struct timeval tm;
    gettimeofday(&tm, NULL);

    return (tm.tv_sec * 1000 + tm.tv_usec / 1000);
}

static void *thread_save_log_to_file(void *arg)
{
    TaskInfo_t *pTaskInfo = (TaskInfo_t *)arg;
    uint32_t i, get_fifo_len = 0;
    FILE *p_logfile = NULL;
    char log_filename[FIBO_BUF_SIZE] = {0};
    uint8_t *get_fifo_buff = (uint8_t *)malloc(LOG_BUFFER_SIZE);

    LogInfo("[%d] logdir:%s\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], pTaskInfo->logdir);

    if (get_fifo_buff == NULL)
    {
        LogInfo("[%d] malloc get_fifo_buff failed.\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum]);
        return NULL;
    }

    for (i = 1; i < sizeof(pTaskInfo->logdir) && pTaskInfo->logdir[i] != 0; i++)
    {
        if (pTaskInfo->logdir[i] == '/')
        {
            char str_dir[512] = {0};

            strcpy(str_dir, pTaskInfo->logdir);
            str_dir[i] = '\0';
            if (access(str_dir, 0))
            {
                mkdir(str_dir, 0777);
                // LogInfo("[%s] mkdir:%s\n", pTaskInfo->pdev->syspath, str_dir);
            }
        }
    }

    while (pTaskInfo->isRun)
    {
        if (p_logfile == NULL)
        {
            struct tm *tm = NULL;
            time_t t;
            struct timeval tmv;
            gettimeofday(&tmv, NULL);

            t = time(NULL);
            tm = localtime(&t);
            if (pTaskInfo->portnum == 0) // ap
            {
                // log_-1_221101-112834.bin
                sprintf(log_filename, "%slog_-1_%02u%02u%02u-%02u%02u%02u.bin", pTaskInfo->logdir,
                        1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
            }
            else // cp
            {
                // 8910的cp Log文件格式：name(日-时-分-秒-白分秒)[.partxx.Sn(xxxx)].tra
                // e.g: Arm(17-21-59-25-500).Part4.Sn(0).tra
                sprintf(log_filename, "%sArm(%02u-%02u-%02u-%02u-%03lu).tra", pTaskInfo->logdir,
                        tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tmv.tv_usec / 1000);
            }

            LogInfo("log_filename: %s\n", log_filename);

            p_logfile = fopen(log_filename, "wb");
            if (p_logfile == NULL)
            {
                LogInfo("[%d] create log file failed, errno:%d(%s)\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], errno, strerror(errno));
                break;
            }
            if (pTaskInfo->portnum == 0)
            {
                CH_TIMESTAMP log_ts;
                log_ts.sync = 0xAD;
                log_ts.lenM = 0;
                log_ts.lenL = 0x08;
                log_ts.flowid = 0xa2;
                log_ts.date = ((1900 + tm->tm_year) << 16) + ((tm->tm_mon + 1) << 8) + (tm->tm_mday);
                log_ts.ms = tm->tm_hour * 3600 * 1000 + tm->tm_min * 60 * 1000 + tm->tm_sec * 1000 + tmv.tv_usec / 1000;
                fwrite((void *)&log_ts, 1, sizeof(CH_TIMESTAMP), p_logfile);
            }
        }
        else
        {
            int cur_pos = 0, file_size = 0;
            cur_pos = ftell(p_logfile);
            fseek(p_logfile, 0, SEEK_END);
            file_size = ftell(p_logfile);
            fseek(p_logfile, cur_pos, SEEK_SET);
            if (file_size > pTaskInfo->single_logfile_size)
            {
                fflush(p_logfile);
                fclose(p_logfile);
                p_logfile = NULL;
                continue;
            }
        }
        get_fifo_len = __fifo_len(pTaskInfo->plog_fifo);
        if (get_fifo_len)
        {
            get_fifo_len = __fifo_get(pTaskInfo->plog_fifo, get_fifo_buff, LOG_BUFFER_SIZE);
            if (fwrite(get_fifo_buff, 1, get_fifo_len, p_logfile) == get_fifo_len)
            {
                // LogInfo("write %d Bytes to log_filename: %s\n", get_fifo_len, log_filename);
            }
        }
        usleep(100000);
    }

    get_fifo_len = __fifo_len(pTaskInfo->plog_fifo);
    if (get_fifo_len)
    {
        get_fifo_len = __fifo_get(pTaskInfo->plog_fifo, get_fifo_buff, LOG_BUFFER_SIZE);
        fwrite(get_fifo_buff, get_fifo_len, 1, p_logfile);
    }

    if (p_logfile)
    {
        fflush(p_logfile);
        fclose(p_logfile);
        p_logfile = NULL;
    }

    if (get_fifo_buff)
    {
        free(get_fifo_buff);
        get_fifo_buff = NULL;
    }

    return NULL;
}

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
    if (pthread_create(&thread_id_readlog, &attr, thread_read_serial_log, pTaskInfo) < 0)
    {
        LogInfo("[%d] pthread_create failed! errno:%d(%s)\n", pTaskInfo->pdev->ifnum[pTaskInfo->portnum], errno, strerror(errno));
        goto error_exit;
    }

    if (pthread_create(&thread_id_savelog, NULL, thread_save_log_to_file, pTaskInfo) < 0)
    {
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

static void fibo_exit_function(int msg)
{
    LogInfo("%d\n", msg);
    g_log_process_flag = 0;
    sleep(1);
    signal(SIGINT, SIG_DFL); // Enable Ctrl+C to exit
}
#endif

static void usage(char *arg)
{
    LogInfo("========================================\n");
    LogInfo("Usage:\n");
    LogInfo("%s <-s [log save dir]> <-m [log file threashold(1MB)]> <-n [log dir threashold(256MB)]>\n", arg);
    LogInfo("Example: %s -s ./armlog -m 32 -n 4\n", arg);
    LogInfo("========================================\n");
}

int unisoc_x_log_main(int argc, char **argv)
{
    int opt = -1;
    optind = 1; // must set to 1
    XLOG_CONFIG xlog_cfg;
    memset(&xlog_cfg, 0, sizeof(XLOG_CONFIG));
    strncpy(xlog_cfg.logPath, "./armlog", XLOG_LOG_PATH_LEN_MAX);
    while ((opt = getopt(argc, argv, "d:p:s:m:n:ht:")) != EOF)
    {
        switch (opt)
        {
        case 'd':
            strncpy(xlog_cfg.device[0], optarg, XLOG_DEVICE_NAME_LEN_MAX);
            break;
        case 'p':
            strncpy(xlog_cfg.device[1], optarg, XLOG_DEVICE_NAME_LEN_MAX);
            break;
        case 's':
            memset(xlog_cfg.logPath, 0, XLOG_LOG_PATH_LEN_MAX);
            strncpy(xlog_cfg.logPath, optarg, XLOG_LOG_PATH_LEN_MAX);
            break;
        case 'm':
            xlog_cfg.logFileThreashold = atoi(optarg); // per 1MB
            xlog_cfg.logFileThreashold *= 1024 * 1024;
            break;
        case 'n':
            xlog_cfg.logFolderThreashold = atoi(optarg); // per 256MB
            xlog_cfg.logFolderThreashold *= 256 * 1024 * 1024;
            break;
        case 't':
            LogInfo("platform is unisoc_x\n");
            break;
        case 'h':
        default:
            usage(argv[0]);
            return 1;
        }
    }

    fibo_usbdev_t *apDev = NULL;

    apDev = (fibo_usbdev_t *)malloc(sizeof(fibo_usbdev_t));
    if (NULL != apDev)
    {
        memset(apDev, 0, sizeof(fibo_usbdev_t));
        memcpy(apDev, fibo_get_fibocom_device(unisoc_x_find_devices_in_table, "", 0), sizeof(fibo_usbdev_t));
        if (strlen(xlog_cfg.device[0]) == 0)
        {
            strcpy(xlog_cfg.device[0], apDev->portname);
        }
    }
    else
    {
        LogInfo("malloc apDev error\r\n");
        return -1;
    }

    fibo_usbdev_t *cpDev = NULL;

    cpDev = (fibo_usbdev_t *)malloc(sizeof(fibo_usbdev_t));
    if (NULL != cpDev)
    {
        memset(cpDev, 0, sizeof(fibo_usbdev_t));
        if (strlen(xlog_cfg.device[1]) == 0)
        {
            memcpy(cpDev, fibo_get_fibocom_device(unisoc_x_find_devices_in_table, "", 1), sizeof(fibo_usbdev_t));
            strcpy(xlog_cfg.device[1], cpDev->portname);
        }
    }
    else
    {
        free(apDev);
        LogInfo("malloc cpDev error\r\n");
        return -1;
    }

    LogInfo("xlog m:%s\n", apDev->ModuleName);

    while (access(xlog_cfg.logPath, 0))
    {
        if (0 == mkdir(xlog_cfg.logPath, 0777))
        {
            break;
        }
        sleep(1);
        LogInfo("xlog mkdir %s failed!\n", xlog_cfg.logPath);
    }

    /* The logging tool will obtain terminal input information
    and trigger the system to send SIGTTIN and SIGTTOU signals
    when the process is a background process. Not ignoring these signals will cause the process to stop*/
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    if (0 == strcmp(apDev->ModuleName, "FIBOCOM UNISOC 8850"))
    {
        return xlog_unisoc8850_entry(&xlog_cfg);
    }
    else
    {
        return xlog_unisoc8910_entry(&xlog_cfg);
    }
    return 0;
}

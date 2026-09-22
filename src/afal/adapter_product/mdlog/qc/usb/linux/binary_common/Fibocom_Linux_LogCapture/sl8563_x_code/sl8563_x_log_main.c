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
#include "sl8563_x_devices_list.h"
#include "xlog_wrapper.h"

extern char tty_port[20];
long long log_file_threashold;
int log_dir_threashold;
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

fibo_usbdev_t *sl8563_x_find_devices_in_table(int idvendor, int idproduct)
{
    int i, size = 0;

    size = sizeof(sl8563_x_devices_table) / sizeof(sl8563_x_devices_table[0]);
    for (i = 0; i < size; i++)
    {
        fibo_usbdev_t *pdev = &sl8563_x_devices_table[i];

        if ((pdev->idVendor == idvendor) && (pdev->idProduct == idproduct))
        {
            return pdev;
        }
    }

    return NULL;
}

static void usage(char *arg)
{
    LogInfo("========================================\n");
    LogInfo("Usage:\n");
    LogInfo("%s <-s [log save dir]> <-m [log file threashold(1MB)]> <-n [log dir threashold(256MB)]>\n", arg);
    LogInfo("Example: %s -s ./armlog -m 32 -n 4\n", arg);
    LogInfo("========================================\n");
}

int sl8563_x_log_main(int argc, char **argv)
{
    int opt = -1;
    optind = 1; // must set to 1
    XLOG_CONFIG xlog_cfg;
    memset(&xlog_cfg, 0, sizeof(XLOG_CONFIG));
    FILE *fp;
    char path[XLOG_LOG_PATH_LEN_MAX];
    fp = popen("pwd", "r");
    if(fp == NULL)
    {
         printf("Error: Failed to run command\n");
         return 1;
    }
    fgets(path, sizeof(path)-1, fp);
    pclose(fp);
    path[strcspn(path, "\n")] = 0;
    strcat(path, "/armlog");
    strncpy(xlog_cfg.logPath, path, XLOG_LOG_PATH_LEN_MAX);

    while ((opt = getopt(argc, argv, "d:p:m:n:s:t:hza")) != EOF)
    {
        switch (opt)
        {
        case 'd':
            strncpy(xlog_cfg.device[0], optarg, XLOG_DEVICE_NAME_LEN_MAX);
            break;
        case 'p':
            strncpy(xlog_cfg.device[1], optarg, XLOG_DEVICE_NAME_LEN_MAX);
            break;
        case 'm':
            log_file_threashold = atoi(optarg);
            log_file_threashold *= _SIZE_MB;
            break;
        case 'n':
            log_dir_threashold = atoi(optarg);
            break;
        case 's':
            memset(xlog_cfg.logPath, 0, XLOG_LOG_PATH_LEN_MAX);
            if(optarg[0] == '/')
            {
                strncpy(xlog_cfg.logPath, optarg, XLOG_LOG_PATH_LEN_MAX);
            }
            else
            {
                FILE *fp;
                char path[XLOG_LOG_PATH_LEN_MAX] = {0};
                fp = popen("pwd", "r");
                if(fp == NULL)
                {
                    printf("Error: Failed to run command\n");
                    return 1;
                }
                fgets(path, sizeof(path)-1, fp);
                pclose(fp);
                path[strcspn(path, "\n")] = 0;
                strcat(path, optarg+1);
                strcpy(xlog_cfg.logPath, path);
            }
            break;
        case 't':
            LogInfo("platform is sl8563\n");
            break;
        case 'z':
            break;
        case 'a':
            break;
        case 'h':
        default:
            usage(argv[0]);
            return 1;
        }
    }
    fibo_usbdev_t *apDev = NULL;
    if (strlen(xlog_cfg.device[0]) == 0)
    {
        apDev = (fibo_usbdev_t *)malloc(sizeof(fibo_usbdev_t));
        if (NULL != apDev)
        {
            memset(apDev, 0, sizeof(fibo_usbdev_t));
            memcpy(apDev, fibo_get_fibocom_device(sl8563_x_find_devices_in_table, "", 0), sizeof(fibo_usbdev_t));
            strcpy(xlog_cfg.device[0], apDev->portname);
        }
    }
    fibo_usbdev_t *cpDev = NULL;
    if (strlen(xlog_cfg.device[1]) == 0)
    {
        cpDev = (fibo_usbdev_t *)malloc(sizeof(fibo_usbdev_t));
        if (NULL != cpDev)
        {
            memset(cpDev, 0, sizeof(fibo_usbdev_t));
            memcpy(cpDev, fibo_get_fibocom_device(sl8563_x_find_devices_in_table, "", 1), sizeof(fibo_usbdev_t));
            strcpy(xlog_cfg.device[1], cpDev->portname);
        }
    }

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

    return xlog_sl8563_entry(&xlog_cfg);
}

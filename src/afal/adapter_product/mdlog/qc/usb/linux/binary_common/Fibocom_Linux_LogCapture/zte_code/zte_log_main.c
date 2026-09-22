/*******************************************************************
 *          CopyRight(C) 2022-2026  Fibocom Wireless Inc
 *******************************************************************
 * FileName : zte_trace_tool.c
 * Author   : Frank.zhou
 * Date     : 2022.05.25
 * Used     : Capture ZTE module's trace log
 *******************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "zte_devices_list.h"
#include "IPStart_rule.h"
#include "modemrule_rule.h"
#include "fifo.h"
#include "log_control.h"


#define LOG_BUFFER_SIZE     (32*1024) // 32K
#define LOG_WRITE_BUFFER_SIZE (128*1024)
#define FIFO_BUFFER_SIZE    (512*1024)

static uint8_t s_readbuf[LOG_BUFFER_SIZE] = {0};
static uint8_t s_writebuf[LOG_WRITE_BUFFER_SIZE] = {0};

static volatile int s_log_process = 0;
static char s_logpath[FIBO_BUF_SIZE+EXTEND] = ".";
static int single_logfile_size = 50*1024*1024;
static struct fifo *plog_fifo = NULL;
static int s_max_file_num = 0;
static platform_enum zte_platform = NONE;


fibo_usbdev_t *zte_find_devices_in_table(int idvendor, int idproduct)
{
    int i, size = 0;

    size = sizeof(zte_devices_table)/sizeof(zte_devices_table[0]);
    for (i=0; i<size; i++)
    {
        fibo_usbdev_t *udev = &zte_devices_table[i];

        if ((udev->idVendor == idvendor) && (udev->idProduct == idproduct)) {
            return udev;
        }
    }

    return NULL;
}

static int fibo_send_HeartBeat_data(fibo_usbdev_t *pdev)
{
    const char databuf[] = {0x01,0xaa,0xaa,0xaa,0x01,0x55,0x73,0x01,0x14,0x00,0x00,0x00,0x06,0x67,0xbb,0xbb,0x04,
                      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x44,0x09,0x7e,};
    int write_len = 0, left_len = sizeof(databuf);

    while (left_len > 0) {
        int retval = pdev->write(pdev, (char *)databuf+write_len, left_len, 0);
        if (retval < 0) {
            LogInfo("%s error,retval:%d\n", __func__, retval);
            return retval;
        }
        else if (retval == 0) {
            break;
        }
        left_len -= retval;
        write_len += retval;
    }

    if (write_len != sizeof(databuf)) {
        LogInfo("%s failed. write_len:%d\n", __func__, write_len);
    }

    return 0;
}

static void *thread_HeartBeat(void *param)
{
    fibo_usbdev_t *pdev = (fibo_usbdev_t *)param;

    sleep(1);

    while (s_log_process)
    {
        if (fibo_send_HeartBeat_data(pdev)) {
            break;
        }
        
        sleep(2);
    }

    s_log_process = 0;

    return NULL;
}

platform_enum fibo_get_zte_platform(fibo_usbdev_t *pdev)
{
    int bInterfaceSubClass=0;

    if(pdev == NULL)
        return NONE;

    if((pdev->idVendor == 0x2CB7) && (pdev->idProduct == 0x0001))
    {
        bInterfaceSubClass = fibo_get_usb_Interface6_SubClass(pdev);
    }

    if(bInterfaceSubClass == 0x42)
        return V3T;
    else if(bInterfaceSubClass == 0xFF)
        return V3E;
    else
        return NONE;
}

//send config file data to the Fibocom device.
static int fibo_send_config_files(fibo_usbdev_t *pdev)
{
    int w_ret = -1, read_size = 0;
    int fibo_fp = -1;
    char buffer[2048] = {0};

    fibo_fp = open("modemrule.rule", O_RDONLY);
    if (fibo_fp >= 0) {
        while ((read_size = read(fibo_fp, buffer, sizeof(buffer))) > 0)
        {
            w_ret = pdev->write(pdev, buffer, read_size, 0);
            if (w_ret < 0) {
                LogInfo("write trace port failed\n");
                goto END;
            }

            memset(buffer, 0, sizeof(buffer));
        }
        close(fibo_fp);
        fibo_fp = -1;
        LogInfo("use modemrule.rule\n");
    }
    else
    {
        if(zte_platform == NONE)
        {
            zte_platform = fibo_get_zte_platform(pdev);
        }

        if(zte_platform == V3E)
        {
            LogInfo("use default V3E modemrule_rule.h\n");
            w_ret = pdev->write(pdev, (void *)modemrule_v3e_rule_buf, sizeof(modemrule_v3e_rule_buf), 0);
            if (w_ret < 0) {
                LogInfo("write trace port failed\n");
                goto END;
            }
        }
        else
        {
            LogInfo("use default V3T modemrule_rule.h\n");
            w_ret = pdev->write(pdev, (void *)modemrule_v3t_rule_buf, sizeof(modemrule_v3t_rule_buf), 0);
            if (w_ret < 0) {
                LogInfo("write trace port failed\n");
                goto END;
            }
        }
    }
    LogInfo("send modemrule_rule ok, ret is %d\r\n", w_ret);

    fibo_fp = open("IPStart.rule",O_RDONLY);
    if (fibo_fp >= 0) {
        memset(buffer, 0, sizeof(buffer));
        while ((read_size = read(fibo_fp, buffer, sizeof(buffer))) > 0)
        {
            w_ret = pdev->write(pdev, buffer, read_size, 0);
            if (w_ret < 0) {
                LogInfo("write trace port failed\n");
                goto END;
            }
            memset(buffer, 0, sizeof(buffer));
        }
        close(fibo_fp);
        fibo_fp = -1;
    } else {
        LogInfo("use default IPStart_rule.h\n");
        w_ret = pdev->write(pdev, (void *)IPStart_rule_buf, sizeof(IPStart_rule_buf), 0);
        if (w_ret < 0) {
            LogInfo("write trace port failed\n");
            goto END;
        }
    }
    LogInfo("send IPStart_rule ok, ret is %d\r\n", w_ret);

    w_ret = 0;
END:
    if (fibo_fp >= 0) {
        close(fibo_fp);
        fibo_fp = -1;
    }

    return w_ret;
}

static void *thread_save_log_to_file(void *arg)
{
    uint32_t get_fifo_len = 0;
    int file_size = 0;
    FILE *p_logfile = NULL;
    char log_filename[MAX_PATH_LEN+2*EXTEND] = { 0 };
    arg = NULL;

    while (s_log_process)
    {
        if (p_logfile == NULL) {
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);

            sprintf(log_filename, "%s%02d_%02d%02d_%02d%02d%02d.log", s_logpath,
                1900+tm->tm_year, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
            LogInfo("%s: filename: %s\n", __func__, log_filename);

            p_logfile = fopen(log_filename, "wb");
            if (p_logfile == NULL) {
                LogInfo("create log file failed, errno:%d(%s)\n", errno, strerror(errno));
                break;
            }

            log_storage_control(log_filename, s_max_file_num, 1);
        }
        else
        {
            if (file_size > single_logfile_size) {
                fclose(p_logfile);
                p_logfile = NULL;
                file_size = 0;
                continue;
            }
        }
        get_fifo_len = __fifo_len(plog_fifo);
        if (get_fifo_len > 0)
        {
            get_fifo_len = __fifo_get(plog_fifo, s_writebuf, LOG_WRITE_BUFFER_SIZE);
            fwrite(s_writebuf, get_fifo_len, 1, p_logfile);
            file_size += get_fifo_len;
        }
        else
        {
            usleep(50000);//max=512k/50ms=10M/s
        }
    }

    get_fifo_len = __fifo_len(plog_fifo);
    if (get_fifo_len > 0)
    {
        get_fifo_len = __fifo_get(plog_fifo, s_writebuf, LOG_WRITE_BUFFER_SIZE);
        fwrite(s_writebuf, get_fifo_len, 1, p_logfile);
    }

    if (p_logfile) {
        fclose(p_logfile);
        p_logfile = NULL;
    }

    return NULL;
}

static void fibo_exit_function(int msg)
{
    LogInfo("\n%s: %d\n", __func__, msg);
    log_storage_control(NULL, 0, 0);
    s_log_process = 0;

    sleep(1);
    signal(SIGINT, SIG_DFL); //Enable Ctrl+C to exit
}

static void usage(char *arg)
{
    LogInfo("========================================\n");
    LogInfo("Usage:\n");
    LogInfo("%s <-d [diag port]> <-s [log save dir]> <-m [single logfile size(MB)]> <-n [max_log_filenum]>\n", arg);
    LogInfo("example: %s\n", arg);
    LogInfo("========================================\n");
}

int zte_log_main(int argc, char **argv)
{
    int ret = -1, opt = -1;
    fibo_usbdev_t *pdev = NULL;
    char portname[FIBO_BUF_SIZE] = {0};
    char log_dir[FIBO_BUF_SIZE] = ".";
    pthread_t thread1, thread2;

    LogInfo("[%s] start\n", __func__);

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
                zte_platform = atoi(optarg);
                LogInfo("platform is zte\n");
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:;
        }
    }

    {
        int i;
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);

        sprintf(s_logpath, "%s/fibolog_%02d%02d%02d%02d%02d%02d/", log_dir,
            1900+tm->tm_year, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
        LogInfo("%s: s_logpath: %s\n", __func__, s_logpath);

        for (i=1; i<sizeof(s_logpath) && s_logpath[i] != 0; i++)
        {
            if (s_logpath[i] == '\\'  || s_logpath[i] == '/' )
            {
                char str_dir[FIBO_BUF_SIZE] = {0};
                strcpy(str_dir, s_logpath);
                str_dir[i] = '\0';
                if (access(str_dir, 0)) {
                    mkdir(str_dir, 0777);
                    LogInfo("[%s] mkdir:%s\n", __func__, str_dir);
                }
            }
        }

    }

    pdev = fibo_get_fibocom_device(zte_find_devices_in_table, portname, 0);
    if (pdev == NULL)
    {
        goto END;
    }
    
    if (fibo_usb_open(pdev, 0, 1)) {
        goto END;
    }

    if (fibo_send_HeartBeat_data(pdev)) {
        goto END;
    }

    if (fibo_send_config_files(pdev)) {
        goto END;
    }

    plog_fifo = fifo_alloc(FIFO_BUFFER_SIZE);
    if(plog_fifo == NULL)
    {
        LogInfo("fifo_alloc failed.\n");
        goto END;
    }
    printf("FIFO size is %d\r\n", plog_fifo->size);
    
    s_log_process = 1;

    if (pthread_create(&thread1, NULL, thread_HeartBeat, (void *)pdev)) {
        LogInfo("%s create thread failed.\n", __func__);
        goto END;
    }

    if (pthread_create(&thread2, NULL, thread_save_log_to_file, NULL) < 0) {
        LogInfo("savelog pthread_create failed!\n");
        goto END;
    }

    signal(SIGINT, fibo_exit_function); //Ctrl+C
    signal(SIGTERM, fibo_exit_function); //Kill

    while (s_log_process)
    {
        int readlen = pdev->read(pdev, s_readbuf, LOG_BUFFER_SIZE, 0);
        if (readlen > 0)
        {
            if (readlen > __fifo_put(plog_fifo, s_readbuf, readlen))
            {
                LogInfo("warning, fifo buf is full now.\n");
            }

            /* print current status */
            {
                uint32_t cur_tick = 0, diff_tick = 0;
                static uint64_t read_bytes = 0, start_tick = 0;

                read_bytes += readlen;
                if (start_tick == 0) {
                    start_tick = time(NULL);
                }
                else
                {
                    cur_tick = time(NULL);
                    diff_tick = cur_tick - start_tick;
                    if (diff_tick >= 5)
                    {
                        LogInfo("read_bytes(%lu), tick(%u), %luB/s\n", read_bytes, diff_tick, read_bytes/diff_tick);
                        start_tick = cur_tick;
                        read_bytes = 0;
                    }
                }
            }
        }
        else
        {
            usleep(2000);//32k/3.125ms=10M/s
        }
    }

    ret = 0;
END:
    fibo_usb_close(pdev, 0);
    fifo_free(plog_fifo);

    return ret;
}

#ifndef __QLOG_H
#define __QLOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 20)
#include <linux/usb/ch9.h>
#else
#include <linux/usb_ch9.h>
#endif
#include <linux/usbdevice_fs.h>

#include "qshrink4.h"
#include "ftp_client.h"
#include "misc_usb.h"

#define qmdl2_v2_mode    1

#define FIBO_BUF_SIZE   512

#define LOG_FILE_NAME       "process_log.txt"

typedef struct {
    uint8_t fb_flag_1;//0xFF
    uint8_t fb_flag_2;//0xBB
    uint8_t package_type;
    uint8_t package_flag;
    uint32_t payload_lenth;
} FB_TCP_HEAD, *P_FB_TCP_HEAD;


typedef struct arguments
{
    int usbdev;
    int ttyfd;
    int usb_sockets[2];
    fibo_usbdev_t *udev;
} arguments_t;

enum FB_Body_Type
{
    FB_TYPE_GET_LOG_FILES_LIST = 1, //1
    FB_TYPE_LOG_FILES_LIST_INFO,    //2
    FB_TYPE_GET_SPECIFIED_FILE,     //3
    FB_TYPE_GET_SPECIFIED_STREAM,   //4
    FB_TYPE_GET_STREAM_LIST,        //5
    FB_TYPE_LOGS_STREAM_LIST_INFO,  //6
    FB_TYPE_SET_APP_LOG_LEVEL,      //7
    FB_TYPE_GET_APP_LOG_LEVEL,      //8
    FB_TYPE_CURRENT_APP_LOG_LEVEL,  //9
    FB_TYPE_GET_QXDM_STATUS,        //10
    FB_TYPE_CURRENT_QXDM_STATUS,    //11
    FB_TYPE_START_QXDM_GATHING,     //12
    FB_TYPE_STOP_QXDM_GATHEING,     //13
    FB_TYPE_CLEAR_MODULE_QXDM_LOG,  //14
    FB_TYPE_GET_QXDM_LOG_LIST,      //15
    FB_TYPE_CURRENT_QXDM_LOG_LIST,  //16
    FB_TYPE_GET_SPECIFY_QXDM_LOG,   //17
    FB_TYPE_UPDATE_QXDM_CFG_FILE,   //18
    //0xFE
    //0xFF
};

typedef struct {
    int (*init_filter)(int fd, const char *log_dir, const char *cfg_name);
    int (*clean_filter)(int fd);
    int (*logfile_create)(const char *log_dir, const char *logfile_suffix, unsigned logfile_seq);
    int (*logfile_init)(int logfd, unsigned logfile_seq);
    size_t (*logfile_save)(int logfd, const void *buf, size_t size);
    int (*logfile_close)(int logfd);
} qlog_ops_t;

extern const uint8_t *g_qcom_req;
extern qlog_ops_t qcom_qlog_ops;
extern pthread_mutex_t mutex;   /*resolve the bug g_mdm_req issue (mantis 63061), yanghaitao 2020.11.25 */

extern char g_qxdm_log_filename[];
extern char g_str_sub_log_dir[];
extern char g_qcom_logdir[];
extern char qxdm_default_cfg_buf[];

ssize_t qcom_send_cmd(int fd, const uint8_t *buf, size_t size);

uint16_t qlog_le16 (uint16_t v16);
uint32_t qlog_le32 (uint32_t v32);
uint64_t qlog_le64 (uint64_t v64);

size_t log_poll_write(int fd, const void *buf, size_t size);
void file_log_message(char *log_dir, const char *str_fmt,...);
int qlog_logfile_create_fullname(int file_type, const char *fullname, long tftp_size, int is_dump);
int qlog_logfile_close(int logfd);
size_t qlog_logfile_save(int logfd, const void *buf, size_t size);



#endif

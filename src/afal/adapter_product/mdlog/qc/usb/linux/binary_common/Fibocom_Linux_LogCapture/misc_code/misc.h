#ifndef __MISC_H__
#define __MISC_H__


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <termios.h>
#include <pthread.h>

#define MAX_PATH_LEN    512
#define NAME_BUF_SIZE   255

#define MIN(X, Y)       (((X)<(Y))? (X):(Y))
#define MAX(X, Y)       (((X)>(Y))? (X):(Y))

extern FILE *g_dl_logfile_fp;

#define LogInfo(fmt, args...)  \
    do {    \
        fprintf(stdout, "[%s: %d]: " fmt, __func__, __LINE__, ##args);    \
        fflush(stdout); \
        if (g_dl_logfile_fp) { \
            fprintf(g_dl_logfile_fp, "[%s: %d]: " fmt, __func__, __LINE__, ##args); \
        } \
    } while (0)

typedef enum{
    NONE,
    V3E,
    V3T,
    QCOM,
    UNISOC,
    UNISOC_X,
    EIGENCOMM,
    SL8563,
}platform_enum;

int fibo_strStartsWith(const char *str, const char *match_str);
int poll_wait(int poll_fd, short events, int timeout_msec);
int fibo_find_file_in_dir(const char *dir, const char *prefix, char *xmlfile);
int fibo_usbmon_log_start(char *filename);
int fibo_usbmon_log_stop(void);

#endif
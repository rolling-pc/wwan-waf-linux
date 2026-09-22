#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <poll.h>
#include "misc.h"

FILE *g_dl_logfile_fp = NULL;


int fibo_strStartsWith(const char *str, const char *match_str)
{
    for ( ; *str != '\0' && *match_str != '\0'; str++, match_str++) {
        if (*str != *match_str) {
            return 0;
        }
    }
    return *match_str == '\0';
}


int poll_wait(int poll_fd, short events, int timeout_msec)
{
    struct pollfd pollfds[] = {{poll_fd, events, 0}};
    int ret = 0;

    //LogInfo("start\n");
    ret = poll(pollfds, 1, timeout_msec);
    if (ret == 0) {
        LogInfo("events:%s msec:%d timeout\n", (events & POLLIN)? "POLLIN" : "POLLOUT", timeout_msec);
        return ETIMEDOUT;
    }
    else if (ret < 0) {
        LogInfo("errno:%d(%s)\n", errno, strerror(errno));
    }

    if (pollfds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return EIO;
    }

    if (pollfds[0].revents & events) {
        return 0;
    }

    return EIO;
}

int fibo_find_file_in_dir(const char *dir, const char *prefix, char *xmlfile)
{
    struct dirent *ent = NULL;
    DIR *pdir = NULL;

    LogInfo("dir:%s\n", dir);

    pdir = opendir(dir);
    if (pdir)
    {
        while((ent = readdir(pdir)) != NULL)
        {
            if(!strncmp(ent->d_name, prefix, strlen(prefix)))
            {
                LogInfo("find: %s\n", ent->d_name);
                strncpy(xmlfile, ent->d_name, MAX_PATH_LEN);
                closedir(pdir);
                return 0;
            }
        }
        closedir(pdir);
    }

    return 1;
}

static volatile int s_usbmon_process_flag = 1;

#define USBMON_PATH  "/sys/kernel/debug/usb/usbmon/0u"

static void *thread_catch_usbmon_log(void *arg)
{
    char buff[MAX_PATH_LEN] = {0}, tbuff[MAX_PATH_LEN*2] = {0};
    time_t t;
    struct tm *tm;
    int fd_log = -1;
    int usbmon_fd = -1;
    char *usbmon_logfile = (char *)arg;

    if (access("/sys/kernel/debug/usb", F_OK)) {
        LogInfo("debugfs is not mount, please execute \"mount -t debugfs none_debugs /sys/kernel/debug\"\n");
        return NULL;
    }

    if (access("/sys/kernel/debug/usb/usbmon", F_OK)) {
        LogInfo("usbmon is not load, please execute \"modprobe usbmon\" or \"insmod usbmon.ko\"\n");
        return NULL;
    }

    usbmon_fd = open(USBMON_PATH, O_RDONLY);
    if (usbmon_fd < 0)
    {
        LogInfo("open %s error(%d) (%s)\n", USBMON_PATH, errno, strerror(errno));
        return NULL;
    }

    fd_log = open(usbmon_logfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd_log < 0) {
        LogInfo("open %s failed, errno:%d(%s)\n", usbmon_logfile, errno, strerror(errno));
        close(usbmon_fd);
        return NULL;
    }

    while(s_usbmon_process_flag && usbmon_fd >= 0)
    {
        int nreads = read(usbmon_fd, buff, sizeof(buff));
        if (nreads <= 0) {
            break;
        }
        buff[nreads] = '\0';

        time(&t);
        tm = localtime(&t);
        sprintf(tbuff, "%04d/%02d/%02d_%02d:%02d:%02d %s",
                tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, buff);

        write(fd_log, tbuff, strlen(tbuff));
    }

    if (fd_log >= 0) {
        close(fd_log);
    }

    if (usbmon_fd >= 0) {
        close(usbmon_fd);
    }

    return NULL;
}

int fibo_usbmon_log_start(char *filename)
{
    pthread_t pt;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&pt, &attr, thread_catch_usbmon_log, filename)) {
        LogInfo("pthread_create failed, errno:%d(%s)\n", errno, strerror(errno));
        return -1;
    }
    
    return 0;
}

int fibo_usbmon_log_stop(void)
{
    s_usbmon_process_flag = 0;
    return 0;
}


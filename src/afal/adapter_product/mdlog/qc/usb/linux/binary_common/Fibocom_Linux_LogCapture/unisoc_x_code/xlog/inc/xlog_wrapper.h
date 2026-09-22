#ifndef __XLOG_WRAPPER_H__
#define __XLOG_WRAPPER_H__

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XLOG_DEVICE_NUM_MAX 3
#define XLOG_DEVICE_NAME_LEN_MAX 32
#define XLOG_LOG_PATH_LEN_MAX 150
typedef struct _xlog_config {
    char device[XLOG_DEVICE_NUM_MAX][XLOG_DEVICE_NAME_LEN_MAX];
    char logPath[XLOG_LOG_PATH_LEN_MAX];
    uint64_t logFileThreashold;
    uint64_t logFolderThreashold;
} XLOG_CONFIG;

int xlog_unisoc8850_entry(const XLOG_CONFIG *cfg);
int xlog_unisoc8910_entry(const XLOG_CONFIG *cfg);
#ifdef __cplusplus
}
#endif

#endif
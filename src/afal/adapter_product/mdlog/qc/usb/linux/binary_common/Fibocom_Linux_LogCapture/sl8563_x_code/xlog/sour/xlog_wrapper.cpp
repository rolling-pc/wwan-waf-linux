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
#include "ChannelDataHandler.h"
#include "UserInputMonitor.h"
#include "xlog_wrapper.h"

extern char szDevPath_diag[];
extern char szDevPath_log[];
extern char szDevPath_u2s[];
extern int logtype;
extern uint64_t log_file_threashold;
extern int log_dir_threashold;

#ifdef __cplusplus
extern "C" {
#endif

int xlog_sl8563_entry(XLOG_CONFIG *cfg)
{
    /* init data oper */
    CChannelDataHandler dataHandler;
    CUserInputMonitor   userInputMonitor;
    printf("xlog_sl8563_entry:%s %s %s %lld %d\n", cfg->device[0], cfg->device[1], cfg->logPath, cfg->logFileThreashold, cfg->logFolderThreashold);
    dataHandler.InitXlogConfig(cfg);
    dataHandler.SetUserInputMonitorPtr(&userInputMonitor);
    dataHandler.SetLogType(logtype);
_PROC_LOOP:

    /* start data dump process (blocking) */
    dataHandler.StartProc();

    /* wait for user input cmd */
	userInputMonitor.WaitForUserInput();

    /* stop data dump & release resource */
    dataHandler.StopProc();

    /*
        do loop job in case user specified
        e.g. send AT cmd to reboot modem.
    */
    if (dataHandler.NeedReStart())
    {
        printf("xlog_entry restart!\n");
        goto _PROC_LOOP;
    }

    return 0;
}

#ifdef __cplusplus
}
#endif
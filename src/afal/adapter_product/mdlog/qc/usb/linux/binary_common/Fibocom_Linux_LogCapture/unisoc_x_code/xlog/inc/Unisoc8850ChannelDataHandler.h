// ChannelDataHandler.h: interface for the Unisoc8850ChannelDataHandler class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__UNISOC_8850_CHANNEL_DATA_HANDLER_H__)
#define __UNISOC_8850_CHANNEL_DATA_HANDLER_H__

#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#include <pthread.h>

#include "ProtocalChannelFactory.h"
#include "UserInputMonitor.h"
#include "xlog_wrapper.h"

class CUnisoc8850ChannelDataHandler : public IProtocolObserver
{
public:
    CUnisoc8850ChannelDataHandler();
    virtual ~CUnisoc8850ChannelDataHandler();

public:
    void SetLogPath(const char *logPath, int nPathLen);
    BOOL StartProc();
    void StopProc();
    BOOL NeedReStart();

    virtual int OnChannelEvent(uint32_t event, void *lpEventData);
    virtual int OnChannelData(void *lpData, uint32_t ulDataSize, void *reserved = 0);

    void SetUserInputMonitorPtr(CUserInputMonitor *pMonitor);

    void InitXlogConfig(const XLOG_CONFIG *cfg);

protected:
    void LockDataProc(bool bLock);
    void LockATCmdProc(bool bLock);

    static void *handlerServiceFunc(void *lpParam);
    static void *scanAssertCmdFunc(void *lpParam);
    static void *waitAssertTimeoutFunc(void *lpParam);

private:
    BOOL LoadConfig();
    BOOL CreateLogelFile();
    BOOL IsModemInfoObtained();
    BOOL SendRequest(BYTE type, BYTE subtype, LPBYTE lpBuffer, DWORD dwDataSize);

    void ProcessChannelData(void *lpData, uint32_t ulDataSize);
    void WriteDataToFile(LPBYTE lpBuffer, DWORD dwDataSize);

    void HandleApSysDump(LPBYTE pStart, UINT nLength);
    void HandleAssertProc(LPBYTE lpBuffer, DWORD dwDataSize);

protected:
    CBaseChannel *m_pDiagChannel;
    CBaseChannel *m_pSmpChannel;

    pthread_mutex_t m_scData;
    pthread_mutex_t m_scAtCmd;

    pthread_t m_hReqThread;
    pthread_t m_hInputThread;
    pthread_t m_hTimeoutThread;

    int m_nPathLen;
    char m_szLogPath[MAX_PATH];

    char *m_plogFileName;
    FILE *m_logf;

    long long m_lFileSize;
    uint64_t m_LogFolderThreasHold;
    uint64_t m_LogFileThreasHold;

    CUserInputMonitor *m_pMonitor;

private:
    BOOL m_bUE_Obtained;
    BOOL m_bVer_Obtained;
    BOOL m_bSendTCmd;
    BOOL m_bNeedRestart;

    DWORD m_AssertTickCount;
};

#endif // !defined(AFX_CHANNELDATAHANDLER_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)

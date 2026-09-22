// ChannelDataHandler.h: interface for the Unisoc8850ChannelDataHandler class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__UNISOC_8910_CHANNEL_DATA_HANDLER_H__)
#define __UNISOC_8910_CHANNEL_DATA_HANDLER_H__

#include <stdlib.h>
#include <stdio.h>
#include<sys/ioctl.h>
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

class CUnisoc8910ChannelDataHandler : public IProtocolObserver
{
public:
	CUnisoc8910ChannelDataHandler();
	virtual ~CUnisoc8910ChannelDataHandler();

public:
    void SetLogPath(const char* logPath, int nPathLen);
    BOOL StartProc();
    void StopProc();
    BOOL NeedReStart();

	virtual int OnChannelEvent( uint32_t event,void* lpEventData );
	virtual int OnChannelData(void* lpData, uint32_t ulDataSize,void* reserved =0 );

	void SetUserInputMonitorPtr(CUserInputMonitor *pMonitor);

    void InitXlogConfig(const XLOG_CONFIG *cfg);

protected:
    void LockDataProc( bool bLock );

    static void* handlerServiceFunc(void* lpParam);

    BOOL CreateLogelPath();

private:
    //BOOL CreateLogelPath();

protected:
    CBaseChannel*    m_pArmTraceChannel;
    CBaseChannel*    m_pDebugHostChannel;

    pthread_mutex_t  m_scData;

    pthread_t        m_hReqThread;

    int   m_nPathLen;
    char  m_szLogPath[MAX_PATH];
    char  m_szCurLogPath[MAX_PATH];
    uint64_t m_LogFolderThreasHold;
    uint64_t m_LogPathSize;
    uint64_t m_CLogPathSize; // child
    uint64_t m_CLogFolderThreasHold;
    uint64_t m_LogFileThreasHold;

    CUserInputMonitor *m_pMonitor;

private:
    BOOL m_bNeedRestart;
};


#endif // !defined(__UNISOC_8910_CHANNEL_DATA_HANDLER_H__)

// ChannelDataHandler.h: interface for the CChannelDataHandler class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CHANNELDATAHANDLER_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)
#define AFX_CHANNELDATAHANDLER_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_

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

class CChannelDataHandler : public IProtocolObserver
{
public:
	CChannelDataHandler();
	virtual ~CChannelDataHandler();

public:
    void SetLogPath(const char* logPath, int nPathLen);
    BOOL StartProc();
    void StopProc();
    BOOL NeedReStart();

	virtual int OnChannelEvent( uint32_t event,void* lpEventData );
	virtual int OnChannelData(void* lpData, uint32_t ulDataSize,void* reserved =0 );

	void SetUserInputMonitorPtr(CUserInputMonitor *pMonitor);

protected:
    void LockDataProc( bool bLock );
    void LockATCmdProc( bool bLock );

    static void* handlerServiceFunc(void* lpParam);
    static void* scanAssertCmdFunc(void* lpParam);
    static void* waitAssertTimeoutFunc(void* lpParam);

private:
    BOOL LoadConfig();
    BOOL CreateLogelFile();
    BOOL IsModemInfoObtained();
    BOOL SendRequest(BYTE type, BYTE subtype, LPBYTE lpBuffer, DWORD dwDataSize);

    void ProcessChannelData(void* lpData, uint32_t ulDataSize);
    void WriteDataToFile(LPBYTE lpBuffer, DWORD dwDataSize);

    void HandleApSysDump(LPBYTE pStart, UINT nLength);
    void HandleAssertProc(LPBYTE lpBuffer, DWORD dwDataSize);

protected:
    CBaseChannel*    m_pDiagChannel;
    CBaseChannel*    m_pSmpChannel;

    pthread_mutex_t  m_scData;
    pthread_mutex_t  m_scAtCmd;

    pthread_t        m_hReqThread;
    pthread_t        m_hInputThread;
    pthread_t        m_hTimeoutThread;

    int   m_nPathLen;
    char  m_szLogPath[MAX_PATH];

    char* m_plogFileName;
    FILE* m_logf;

    long long m_lFileSize;

    CUserInputMonitor *m_pMonitor;

private:
    BOOL m_bUE_Obtained;
    BOOL m_bVer_Obtained;
    BOOL m_bSendTCmd;
    BOOL m_bNeedRestart;

    DWORD m_AssertTickCount;
};


#endif // !defined(AFX_CHANNELDATAHANDLER_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)

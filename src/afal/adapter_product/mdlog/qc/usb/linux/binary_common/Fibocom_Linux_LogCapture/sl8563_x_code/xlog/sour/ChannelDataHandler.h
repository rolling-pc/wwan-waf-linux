/* *****************************************************
SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0

Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
Licensed under the Unisoc General Software License, version 1.0 (the License);
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
Software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OF ANY KIND, either express or implied.
See the Unisoc General Software License, version 1.0 for more details.
******************************************************* */


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
#include <thread>
#include <mutex>

#include "ProtocalChannelFactory.h"
#include "UserInputMonitor.h"
#include "timercontrl.h"
#include "xlog_wrapper.h"

using namespace std;

class CChannelDataHandler : public IProtocolObserver
{
public:
	CChannelDataHandler();
	virtual ~CChannelDataHandler();

public:
    void SetLogPath(const char* logPath, int nPathLen);
    void SetLogType(int type);
    BOOL StartProc();
    void StopProc();
    BOOL NeedReStart();

	virtual int OnChannelEvent( uint32_t event,void* lpEventData );
	virtual int OnChannelData(void* lpData, uint32_t ulDataSize,uint32_t reserved =0 );

	void SetUserInputMonitorPtr(CUserInputMonitor *pMonitor);

	void InitXlogConfig(const XLOG_CONFIG *cfg);

protected:
    void LockDataProc( bool bLock );
    void LockATCmdProc( bool bLock );

    static void* handlerServiceFunc(void* lpParam);
    static void* scanAssertCmdFunc(void* lpParam);
    static void* waitAssertTimeoutFunc(void* lpParam);

private:
    void handlerServiceFuncEx();

private:
    BOOL LoadConfig();
    BOOL CreateLogelFile();
    BOOL IsModemInfoObtained();
    BOOL SendRequest(BYTE type, BYTE subtype, LPBYTE lpBuffer, DWORD dwDataSize);

    void ProcessChannelData(void* lpData, uint32_t ulDataSize);
    void WriteDataToFile(LPBYTE lpBuffer, DWORD dwDataSize);

    void HandleApSysDump(LPBYTE pStart, UINT nLength);
    void HandleAssertProc(LPBYTE lpBuffer, DWORD dwDataSize);
    void FiboGetApLog(DIAG_PACKAGE* lpPkg, DWORD dwDataSize);



protected:
    CBaseChannel*    m_pDiagChannel;
    CBaseChannel*    m_pSmpChannel;

    pthread_mutex_t  m_scData;
    pthread_mutex_t  m_scAtCmd;

    pthread_t        m_hReqThread;
    pthread_t        m_hInputThread;
    pthread_t        m_hTimeoutThread;

    int   m_nPathLen;
    int   m_logtype;
    int   m_background;
    char  m_szLogPath[MAX_PATH];

    char* m_aplogFileName;
    char* m_plogFileName;
    FILE* m_logf;
    static FILE* m_aplogf;

    long long m_lFileSize;

    CUserInputMonitor *m_pMonitor;

private:
    BOOL m_bUE_Obtained;
    BOOL m_bVer_Obtained;
    BOOL m_ATComm_Obtained;
    BOOL m_bSendTCmd;
    BOOL m_bNeedRestart;
    CSpLog m_log;

    DWORD m_AssertTickCount;

    //UINT64 m_uSingleFileTotalSize;    //文件总大小

public:
    //Get Currect DateTime Format yyyy_MM_dd_HH_mm_ss
    void GetCurDateTime(char *szCurDT);

    //Timer Clock
    TimerContrl m_tc;
    //Timer Out Fun
    static void ApSysDumpTimerOut(int nToRet);

private:
    FILE* m_ApSysFile;
    //Open AP sys Dump File
    int OpenApSysDumpFile(unsigned char *buf, int buflen);
    //Write AP sys Dump to File;
    int WriteApSysDumpData(unsigned char *buf, int buflen);
    //Close AP sys Dump File
    int CloseApSysDumpFile();
};

#endif // !defined(AFX_CHANNELDATAHANDLER_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)

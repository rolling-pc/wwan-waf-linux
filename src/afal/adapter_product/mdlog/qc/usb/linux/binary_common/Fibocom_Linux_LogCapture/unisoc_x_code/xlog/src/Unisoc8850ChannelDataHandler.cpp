// ChannelDataHandler.cpp: implementation of the CUnisoc8850ChannelDataHandler class.
//
//////////////////////////////////////////////////////////////////////

#include "Unisoc8850ChannelDataHandler.h"
#include "DiagChan.h"

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <algorithm>
#include <dirent.h>
#include <sys/file.h>
#include <vector>

#include <pthread.h>

#include <assert.h>
#include "ExePathHelper.h"
extern "C"
{
#include "confile.h"
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

/* DIAG */
#define DIAG_SWVER_F             0                   // Information regarding MS software
#define DIAG_SYSTEM_F            5
#define MSG_BOARD_ASSERT         255

#define ITC_REQ_TYPE	         209
#define ITC_REQ_SUBTYPE          100
#define ITC_REP_SUBTYPE          101

#define UET_SUBTYPE_CORE0        102                 // UE Time Diag SubType field, core0
#define TRACET_SUBTYPE           103                 // Trace Time Diag subtype filed
#define UET_SUBTYPE_CORE1        104                 // UE Time Diag SubType field, core1

#define UET_SEQ_NUM              0x0000FFFF          // UE Time Diag Sequnce number fi
#define UET_DIAG_LEN             (DIAG_HDR_LEN + 12) // UE Time Diag Length field (20)

/* ASSERT */
#define NORMAL_INFO              0x00
#define DUMP_MEM_DATA            0x01
#define DUMP_ALL_ASSERT_INFO_END 0x04
#define DUMP_AP_MEMORY           0x08

#define DUMP_DEFAULT_PATH        "/usr/armlog/"

#define MAX_REQ_TRY_NUM          5
#define MAX_AT_CMD_LEN           8

/* Others */
#define SIZE_GB 1024 * 1024 * 1024

/* Global */
extern BOOL g_bAutoProcAssert;
static char   g_szHistoryFolder[MAX_PATH] = {0};

static char g_diagDevName[MAX_DEVICE_NAME];
static char g_logDevName[MAX_DEVICE_NAME];
static char g_u2sDevName[MAX_DEVICE_NAME];

/* get sys timestamp */
static long GetTimeStamp()
{
    struct timeval tm;
    struct tm *tmlocal;
    time_t now;
    gettimeofday(&tm, NULL);
    time(&now);
    tmlocal = localtime(&now);
    return (tm.tv_sec * 1000 + tm.tv_usec / 1000 + tmlocal->tm_gmtoff * 1000);
}

/* get log file path & name */
static char* getLogFileName(const char* logPath, size_t logPathLen, BOOL bWrapDir)
{
    time_t     tNow;
    struct tm* pTime;
    char*      pNameBuf;
    unsigned   nYear;

    tNow  = time(0);
    pTime = localtime(&tNow);
    nYear = 1900 + pTime->tm_year;

    char szData[64] = {0};
    sprintf(szData, "%04u-%02u-%02u_%02u-%02u-%02u",
            nYear, pTime->tm_mon + 1, pTime->tm_mday,
            pTime->tm_hour, pTime->tm_min, pTime->tm_sec);

    if (bWrapDir)
    {
        strcat(g_szHistoryFolder, szData);
        strcat(g_szHistoryFolder, "/");
    }    

    if (access(g_szHistoryFolder, F_OK) != 0)
    {
        if (0 == logPathLen && access(DUMP_DEFAULT_PATH, F_OK) != 0)
        {
            int nRet = mkdir(DUMP_DEFAULT_PATH, S_IRUSR | S_IWUSR);
            if (nRet < 0)
            {
                return NULL;
            }
        }

        int nRet = mkdir(g_szHistoryFolder, S_IRUSR | S_IWUSR);
        if (nRet < 0)
        {
            return NULL;
        }
    }

    pNameBuf = (char*)malloc(strlen(g_szHistoryFolder) + strlen(szData) + strlen(".logel") + 1);
    if (pNameBuf)
    {
        strcpy(pNameBuf, g_szHistoryFolder);
        strcat(pNameBuf, szData);
        strcat(pNameBuf, ".logel");

        pNameBuf[strlen(g_szHistoryFolder) + strlen(szData) + strlen(".logel")] = '\0';
    }

    return pNameBuf;
}


CUnisoc8850ChannelDataHandler::CUnisoc8850ChannelDataHandler()
{
    m_bUE_Obtained  = FALSE;
    m_bVer_Obtained = FALSE;
    m_bNeedRestart  = FALSE;
    m_bSendTCmd     = FALSE;

    m_AssertTickCount = 0;

    m_pDiagChannel  = NULL;
    m_pSmpChannel   = NULL;

    m_logf          = NULL;
    m_plogFileName  = NULL;
    m_nPathLen      = 0;
    m_lFileSize     = 0;

    m_hReqThread     = NULL;
    m_hInputThread   = NULL;
    m_hTimeoutThread = NULL;

    m_LogFileThreasHold = 50; // default 50MB
    m_LogFileThreasHold *= 1024 * 1024;
    m_LogFolderThreasHold  = 0;

    memset(m_szLogPath, 0, sizeof(m_szLogPath));

    pthread_mutex_init(&m_scData, NULL);
    pthread_mutex_init(&m_scAtCmd, NULL);

    m_pMonitor = NULL;
}

CUnisoc8850ChannelDataHandler::~CUnisoc8850ChannelDataHandler()
{
    pthread_mutex_destroy(&m_scData);
    pthread_mutex_destroy(&m_scAtCmd);

    CProtocalChannelFactory::ReleaseInstance(m_pDiagChannel);
    CProtocalChannelFactory::ReleaseInstance(m_pSmpChannel);
}

BOOL CUnisoc8850ChannelDataHandler::LoadConfig()
{
    INI_CONFIG *config = NULL;

    GetExePath helper;
    std::string strIniPath = helper.getExeDir();
    strIniPath.insert(0,"/");
    strIniPath += "Channel.ini";

    config = ini_config_create_from_file(strIniPath.c_str(),0);
	
	if (config)
	{
		strcpy(g_diagDevName, ini_config_get_string(config,"DIAG","DevPath","tty_usb_unknown"));
		strcpy(g_logDevName, ini_config_get_string(config,"SMP","DevPath","tty_usb_unknown"));
		strcpy(g_u2sDevName, ini_config_get_string(config,"SMP","DevPath_Dump_Mode","tty_usb_unknown"));

        m_LogFileThreasHold  = ini_config_get_int(config, "Setting", "m_LogFileThreasHold", 1); // default 1 GB
        m_LogFileThreasHold *= SIZE_GB;

        m_LogFolderThreasHold  = ini_config_get_int(config, "Setting", "m_LogFolderThreasHold", 10); // default 10 GB
        m_LogFolderThreasHold *= SIZE_GB;
	}
	else
	{		
		return FALSE;
	}

    ini_config_destroy(config);
    return TRUE;
}

BOOL CUnisoc8850ChannelDataHandler::CreateLogelFile()
{
    BOOL bRet = FALSE;
    do
    {
        // save last file
        m_lFileSize = 0;
        if (NULL != m_logf)
        {
            fflush(m_logf);
            fclose(m_logf);
            m_logf = NULL;            
        }        

        // check if reaches log dir threashold and remove log file(s)
        if (m_LogFolderThreasHold > 0)
        {
            long long lDirSize = 0;

            struct dirent *pDirEntry = NULL;
            DIR *pDir = NULL;
    
            if( (pDir = opendir(g_szHistoryFolder)) == NULL )
            {
                printf("opendir failed : %s\n", g_szHistoryFolder);
                break;
            }

            std::vector<string> vecFileName;
            while( NULL != (pDirEntry = readdir(pDir)))
            {
                if(strcmp(pDirEntry->d_name, "..")==0 || strcmp(pDirEntry->d_name, ".")==0)
                    continue;                

                string strFileName = pDirEntry->d_name;
                vecFileName.push_back(strFileName);
            }

            closedir(pDir);
            std::sort(vecFileName.begin(), vecFileName.end()); // sort by asc

            std::vector<off_t> vecSize;
            for (int j = 0; j < vecFileName.size(); ++j)
            {
                string strFilePath = g_szHistoryFolder;
		        strFilePath += vecFileName[j];
                //printf("Log File: %s\n", strFilePath.c_str());

                struct stat statbuff;
                if(-1 != stat(strFilePath.c_str(), &statbuff))
                {
                    lDirSize += statbuff.st_size;
                    vecSize.push_back(statbuff.st_size);
                }
                else
                {
                    printf("Log File: %s, failed to stat file info, error code: %d,\"%s\"\n", strFilePath.c_str(), errno, strerror(errno));
                    vecSize.push_back(0);
                }                
            }

            int _index = 0;
            while (lDirSize > m_LogFolderThreasHold)
            {
                string strFilePath = g_szHistoryFolder;
		        strFilePath += vecFileName[_index];
                if (-1 == remove(strFilePath.c_str()))
                {
                    printf("Remove file %s failed, error code: %d,\"%s\"\n", strFilePath.c_str(), errno, strerror(errno));
                }
                else
                {
                    lDirSize -= vecSize[_index];                    
                }

                _index++;
                
                if (_index >= vecFileName.size())
                    break;
            }
        }

        // create new logel file
        if (NULL != m_plogFileName)
        {
            free(m_plogFileName);
            m_plogFileName = NULL;
        }

        m_plogFileName = getLogFileName(m_szLogPath, m_nPathLen, FALSE);
        if (NULL == m_plogFileName || strlen(m_plogFileName) == 0)
        {
            printf("get log file name failed, given log path: %s", m_szLogPath);            
            break;
        }

        m_logf = fopen(m_plogFileName, "w+b");
        if (NULL == m_logf)
        {
            printf("open log file failed, given file name: %s, \"%s\"\n", m_plogFileName, strerror(errno));
            break;
        }

        bRet = TRUE;
      
    } while (0);
    
    return bRet;
}

void CUnisoc8850ChannelDataHandler::SetLogPath(const char* logPath, int nPathLen)
{
    if (NULL != logPath && strlen(logPath) > 0 && nPathLen > 0)
    {
        strcpy(m_szLogPath, logPath);
        m_nPathLen = nPathLen;

        memset(g_szHistoryFolder, 0, sizeof(g_szHistoryFolder));

        if (strlen(m_szLogPath) > 0)
        {
            strcpy(g_szHistoryFolder, m_szLogPath);

            if('/' != g_szHistoryFolder[m_nPathLen - 1])
            {
                strcat(g_szHistoryFolder, "/");
            }
        }
        else
        {
            strcpy(g_szHistoryFolder, DUMP_DEFAULT_PATH);
        }
    }
}

BOOL CUnisoc8850ChannelDataHandler::StartProc()
{
    BOOL bRet = TRUE;

    do {
	    /* load INI file */
        #if 0
	    if (!LoadConfig())
        {
            printf("load INI file: Channel.ini failed\n");
            bRet = FALSE;
            break;
        }
        #endif
        printf("CUnisoc8850ChannelDataHandler::StartProc:%s %s %llu %llu", g_diagDevName, g_logDevName, m_LogFileThreasHold, m_LogFolderThreasHold);
        /* create diag & smp channel */
        if (NULL == m_pDiagChannel)
        {
            m_pDiagChannel = CProtocalChannelFactory::GetInstance(PROTOCOL_DIAG);

            assert(m_pDiagChannel);
            m_pDiagChannel->AddObserver(this);
        }

        if (NULL == m_pSmpChannel)
        {
            m_pSmpChannel = CProtocalChannelFactory::GetInstance(PROTOCOL_SMP);

            assert(m_pSmpChannel);
            m_pSmpChannel->AddObserver(this);
        }

		if (NULL == m_pDiagChannel || NULL == m_pSmpChannel)
		{
			printf("create channel object failed");
			bRet = FALSE;
			break;
		}

		/* create & open log file */
        if (!CreateLogelFile())
        {
            bRet = FALSE;
            break;
        }

		{
			// write first reserved package
			BYTE lpBuf[DIAG_HDR_LEN + 8] = {0};
			DIAG_HEADER ph = {0};
			ph.sn      = 0;
			ph.type    = ITC_REQ_TYPE;
			ph.subtype = ITC_REP_SUBTYPE;
			ph.len     = DIAG_HDR_LEN + 8;

			memcpy(lpBuf, &ph, DIAG_HDR_LEN);
			((DWORD*)(lpBuf + DIAG_HDR_LEN ))[0] = 7;
			lpBuf[ DIAG_HDR_LEN + 4 ] = 0; //0:Little Endian,1: Big Endian
			lpBuf[ DIAG_HDR_LEN + 5 ] = 1;
			lpBuf[ DIAG_HDR_LEN + 6 ] = 1;

			WriteDataToFile(lpBuf, DIAG_HDR_LEN + 8);

			// write default sys time package
			LPBYTE lpFrm = new BYTE[UET_DIAG_LEN];
			memset(&ph, 0, sizeof(DIAG_HEADER));
			ph.sn      = UET_SEQ_NUM;
			ph.type    = ITC_REQ_TYPE;
			ph.subtype = UET_SUBTYPE_CORE0;
			ph.len     = UET_DIAG_LEN;

			memcpy(lpFrm, &ph, DIAG_HDR_LEN);
			((UINT64*)(lpFrm + DIAG_HDR_LEN ))[0] = GetTimeStamp();
			((DWORD*)(lpFrm + DIAG_HDR_LEN ))[2]  = 0;

			WriteDataToFile(lpFrm, UET_DIAG_LEN);
			delete []lpFrm;
		}

		/* start service thread */
        if (NULL == m_hReqThread)
        {
            int nRet = pthread_create(&m_hReqThread,
                                      NULL,
                                      (PTHREAD_START_ROUTINE)handlerServiceFunc,
                                      this);

            if (nRet == -1)
            {
                bRet = FALSE;
                break;
            }
        }
    }
    while (0);

    // init data
    m_bUE_Obtained    = FALSE;
    m_bVer_Obtained   = FALSE;
    m_bNeedRestart    = FALSE;
    m_bSendTCmd       = FALSE;
    m_AssertTickCount = 0;

    return bRet;
}

void* CUnisoc8850ChannelDataHandler::handlerServiceFunc(void* lpParam)
{
    CUnisoc8850ChannelDataHandler * p = (CUnisoc8850ChannelDataHandler *)lpParam;
    if (NULL != p)
    {
        int  nTryNum = 0;
        int  nMode   = -1; // 0-normal mode  1-ap dump mode
        bool bOpened = false;

        int nFd_diag = -1;
        int nFd_smp  = -1;
        int nFd_u2s  = -1; // for ap sysdump use

        while (1)
        {
			/* detect device node periodly */
            nFd_diag = open(g_diagDevName, O_RDWR|O_NOCTTY,0666);
            close(nFd_diag);

            nFd_smp  = open(g_logDevName,  O_RDWR|O_NOCTTY,0666);
            close(nFd_smp);

            nFd_u2s  = open(g_u2sDevName,  O_RDWR|O_NOCTTY,0666);
            close(nFd_u2s);

            if (nMode != -1)
            {
                switch(nMode)
                {
                case 0:
                    {
                        if (nFd_u2s != -1 && nFd_diag == -1 && nFd_smp == -1)
                        {
                            if (bOpened)
                            {
                                if (NULL != p->m_pMonitor)
                                {
                                    p->m_bNeedRestart = TRUE;
                                    p->m_pMonitor->CancelWait();

                                    return 0;
                                }
                            }
                        }
                    }
                    break;
                case 1:
                    {
                        if (nFd_u2s == -1 && nFd_diag != -1)
                        {
                            if (bOpened)
                            {
                                if (NULL != p->m_pMonitor)
                                {
                                    p->m_bNeedRestart = TRUE;
                                    p->m_pMonitor->CancelWait();

                                    return 0;
                                }
                            }
                        }
                    }
                    break;
                default:
                    break;
                }
            }

			/* open device node */
			if (!bOpened && nTryNum == 0)
			{
                if (nFd_u2s == -1)
                {
					// open diag port
                    if (nFd_diag != -1)
                    {
                        CHANNEL_ATTRIBUTE ca;
                        memset(&ca, 0, sizeof(ca));
                        ca.ChannelType = CHANNEL_TYPE_TTY;
                        ca.tty.pDevPath = g_diagDevName;
                        ca.tty.dwPortNum = 2;
                        ca.tty.dwBaudRate = 115200;
                        if (p->m_pDiagChannel->Open(&ca, TRUE))
                        {
                            sleep(1);

                            // print assert debug menu
                            char szCmd[MAX_AT_CMD_LEN] = {0};
                            strcpy(szCmd, "0\n");
                            p->SendRequest(MSG_BOARD_ASSERT, 0x0, (LPBYTE)szCmd, strlen(szCmd));

                            nMode   = 0;
                            bOpened = true;
                        }
						
						// open log port
						if (nFd_smp != -1)
						{
                            CHANNEL_ATTRIBUTE ca;
                            memset(&ca, 0, sizeof(ca));
                            ca.ChannelType = CHANNEL_TYPE_TTY;
                            ca.tty.pDevPath = g_logDevName;
                            ca.tty.dwPortNum = 2;
                            ca.tty.dwBaudRate = 115200;
							p->m_pSmpChannel->Open(&ca, TRUE);
						}
                    }
                }
                else
                {
                    CHANNEL_ATTRIBUTE ca;
                    memset(&ca, 0, sizeof(ca));

                    ca.ChannelType       = CHANNEL_TYPE_TTY;
                    //ca.tty.nProtocolType = PROTOCOL_TYPE_U2S;
                    ca.tty.pDevPath = g_u2sDevName;
                    ca.tty.dwPortNum = 3;
                    ca.tty.dwBaudRate = 115200;

                    if (p->m_pSmpChannel->Open(&ca, TRUE))
                    {
                        nMode   = 1;
                        bOpened = true;
                    }
                }
			}

            /* query modem info */
            if (bOpened && nMode == 0 && !p->m_bSendTCmd && !p->IsModemInfoObtained() && nTryNum < MAX_REQ_TRY_NUM)
            {
                // query modem UE time
                if (!p->m_bUE_Obtained)
                    p->SendRequest(DIAG_SYSTEM_F, 0x11, NULL, 0);

                // query modem version
                if (!p->m_bVer_Obtained)
                    p->SendRequest(DIAG_SWVER_F, 0x0, NULL, 0);

                nTryNum++;
            }

            /* print log file size */
            // if (bOpened && !p->m_bSendTCmd)
            // {
                // if (NULL != p->m_logf)
                // {
                    // struct stat statbuff;
                    // if(-1 != stat(p->m_plogFileName, &statbuff))
                    // {
                        // printf("\r");
                        // printf("Log File: %s, size: %d bytes", p->m_plogFileName, statbuff.st_size);
                        // fflush(stdout);
                    // }
                // }
            // }

            sleep(5);
        }
    }
}

void* CUnisoc8850ChannelDataHandler::scanAssertCmdFunc(void* lpParam)
{
    CUnisoc8850ChannelDataHandler * p = (CUnisoc8850ChannelDataHandler *)lpParam;
    if (NULL != p)
    {
        char c;
        do{
            c = getchar();

            char szCmd[MAX_AT_CMD_LEN] = {0};
            szCmd[0] = c;
            strcat(szCmd, "\n");

            p->SendRequest(MSG_BOARD_ASSERT, 0x0, (LPBYTE)szCmd, strlen(szCmd));

        }while(1);
    }
}

void* CUnisoc8850ChannelDataHandler::waitAssertTimeoutFunc(void* lpParam)
{
    CUnisoc8850ChannelDataHandler * p = (CUnisoc8850ChannelDataHandler *)lpParam;
    if (NULL != p)
    {
        while (p->m_AssertTickCount == 0) // invalid tickcount
        {
            sleep(5);
        }

        DWORD __tickCount = p->m_AssertTickCount;
        while(1)
        {
            if (__tickCount - p->m_AssertTickCount > 30000)
            {
                printf("\nassert dumping data end, now rebooting modem...\n");

                char szCmd[MAX_AT_CMD_LEN] = {0};
                strcpy(szCmd, "z\n");

                p->SendRequest(MSG_BOARD_ASSERT, 0x0, (LPBYTE)szCmd, strlen(szCmd));
                p->m_bNeedRestart = TRUE;

                if (NULL != p->m_pMonitor)
                    p->m_pMonitor->CancelWait();

                break;
            }

            sleep(5);
            __tickCount += 5000;
        }
    }
}

void CUnisoc8850ChannelDataHandler::StopProc()
{
    if (NULL != m_logf)
    {
        fflush(m_logf);
        fclose(m_logf);
        m_logf = NULL;

        m_lFileSize = 0;
    }

    if (NULL != m_plogFileName)
    {
        free(m_plogFileName);
        m_plogFileName = NULL;
    }

    m_bUE_Obtained  = FALSE;
    m_bVer_Obtained = FALSE;
    m_bSendTCmd     = FALSE;

    m_AssertTickCount = 0;

    pthread_cancel(m_hReqThread);
    pthread_join(m_hReqThread,NULL);
    m_hReqThread = NULL;

    pthread_cancel(m_hInputThread);
    pthread_join(m_hInputThread,NULL);
    m_hInputThread = NULL;

    pthread_cancel(m_hTimeoutThread);
    pthread_join(m_hTimeoutThread,NULL);
    m_hTimeoutThread = NULL;

    assert(m_pDiagChannel);
    m_pDiagChannel->Close();

    assert(m_pSmpChannel);
    m_pSmpChannel->Close();
}

BOOL CUnisoc8850ChannelDataHandler::NeedReStart()
{
    return m_bNeedRestart;
}

void CUnisoc8850ChannelDataHandler::SetUserInputMonitorPtr(CUserInputMonitor *pMonitor)
{
    assert(pMonitor);
    m_pMonitor = pMonitor;
}

int CUnisoc8850ChannelDataHandler::OnChannelEvent( uint32_t event,void* lpEventData )
{
    // reserved
	return 0;
}

int CUnisoc8850ChannelDataHandler::OnChannelData(void* lpData, uint32_t ulDataSize,void * reserved /*=0*/ )
{
    LockDataProc( true );

    ProcessChannelData(lpData, ulDataSize);

    LockDataProc( false );
	return 0;
}

void CUnisoc8850ChannelDataHandler::LockDataProc( bool bLock )
{
    if( bLock )
	{
		pthread_mutex_lock(&m_scData);
	}
	else
	{
		pthread_mutex_unlock(&m_scData);
	}
}

void CUnisoc8850ChannelDataHandler::LockATCmdProc( bool bLock )
{
    if( bLock )
    {
        pthread_mutex_lock(&m_scAtCmd);
    }
    else
    {
        pthread_mutex_unlock(&m_scAtCmd);
    }
}

BOOL CUnisoc8850ChannelDataHandler::SendRequest(BYTE type, BYTE subtype, LPBYTE lpBuffer, DWORD dwDataSize)
{
    LockATCmdProc( true );

    char szCmd[MAX_AT_CMD_LEN] = {0};
    if (NULL != lpBuffer && dwDataSize > 0)
    {
        strcpy(szCmd, (char*)lpBuffer);
    }

    if (m_bSendTCmd)
    {
        if (strcmp(szCmd, "z\n") != 0)
        {
            printf("\nsince t cmd is sent, other diag cmd except z will be ignored.\n");

            LockATCmdProc( false );
            return FALSE;
        }
    }

    DIAG_HEADER request = {0};
    request.len         = DIAG_HDR_LEN;
    request.type        = type;
    request.subtype     = subtype;
    request.sn          = 0;

    PRT_WRITE_T writeData = {0};
    writeData.action = PRT_WRITE_no_respond;
    writeData.nCond  = -1;

    DIAG_PACKAGE pkgData = {0};
    memcpy( (LPBYTE)(&(pkgData.header)), &request, sizeof( DIAG_HEADER ) );
    pkgData.header.len       = dwDataSize;
    pkgData.data             = (NULL == lpBuffer) ? (&request + sizeof( DIAG_HEADER )) : (void*)lpBuffer;
    writeData.lpProtocolData = &pkgData;

    DWORD dwSend = m_pDiagChannel->Write( &writeData, DIAG_HDR_LEN );
    if( dwSend == 0 )
    {
        LockATCmdProc( false );
        return FALSE;
    }

    if (strcmp(szCmd, "t\n") == 0)
    {
        printf("<<<<<<<<<<     sending t command...     >>>>>>>>>>\n");
        m_bSendTCmd = TRUE;
    }

    if (strcmp(szCmd, "z\n") == 0)
    {
        printf("<<<<<<<<<<     sending z command...     >>>>>>>>>>\n");
    }

    LockATCmdProc( false );
    return TRUE;
}

BOOL CUnisoc8850ChannelDataHandler::IsModemInfoObtained()
{
    return (m_bUE_Obtained && m_bVer_Obtained);
}

void CUnisoc8850ChannelDataHandler::ProcessChannelData(void* lpData, uint32_t ulDataSize)
{
    PRT_BUFF* lpBuff    = (PRT_BUFF*)lpData;
	DIAG_PACKAGE* lpPkg = (DIAG_PACKAGE*)lpBuff->lpData;
	DWORD dwDataSize    = lpBuff->size;

    do
    {
        if (DIAG_SYSTEM_F == lpPkg->header.type && 0x11 == lpPkg->header.subtype)
        {
            m_bUE_Obtained = TRUE;
        }

        if (DIAG_SWVER_F == lpPkg->header.type && 0x0 == lpPkg->header.subtype)
        {
            m_bVer_Obtained = TRUE;
        }

        if (MSG_BOARD_ASSERT == lpPkg->header.type && DUMP_AP_MEMORY == lpPkg->header.subtype) // for AP sysdump
        {
            // save ap sys dump data in other mem file(s)
            HandleApSysDump((LPBYTE)lpPkg, dwDataSize - DIAG_HDR_LEN);
            break;
        }

        // write cp log to file
        WriteDataToFile((LPBYTE)lpPkg, dwDataSize);

        // deal assert process
        if (MSG_BOARD_ASSERT == lpPkg->header.type)
        {
            if (NULL != m_hReqThread)
            {
                pthread_cancel(m_hReqThread);
                pthread_join(m_hReqThread,NULL);
                m_hReqThread = NULL;
            }

            HandleAssertProc((LPBYTE)lpPkg, dwDataSize);
        }

    } while (0);
}

void CUnisoc8850ChannelDataHandler::WriteDataToFile(LPBYTE lpBuffer, DWORD dwDataSize)
{
    if (NULL == m_logf)
    {
        return;
    }

    DWORD dwWriteSize = dwDataSize;

    if (m_LogFileThreasHold > 0 && !m_bSendTCmd && m_lFileSize + sizeof(dwWriteSize) + dwDataSize > m_LogFileThreasHold)
    {
        CreateLogelFile();
    }

    if(NULL != m_logf)
    {
        fwrite(&dwWriteSize, 1, sizeof(dwWriteSize), m_logf);
        fwrite(lpBuffer, 1, dwDataSize, m_logf);
        //fflush(m_logf);

        m_lFileSize += sizeof(dwWriteSize) + dwDataSize;
    }
}

void CUnisoc8850ChannelDataHandler::HandleApSysDump(LPBYTE pStart, UINT nLength)
{
    do
    {
        static BOOL   bExDumpDir = FALSE;
        static BOOL   bApDumping = FALSE;
        static BOOL   bDoUnknown = FALSE;
        static UINT64 uTotalSize = 0;
        static UINT64 uRecvSize  = 0;
        static int    nProgress  = 0;
        static UINT64 uTickCount = 0;

        static char  szDirPath[MAX_PATH] = {0};
        static FILE* pApFile             = NULL;

        // create dump directory
		if (!bExDumpDir)
		{
			memset(szDirPath, 0, sizeof(szDirPath));

			strcpy(szDirPath, g_szHistoryFolder);
			strcat(szDirPath, _T("ap_sys_dump/"));

			if (access(szDirPath, F_OK) != 0)
			{
				int nRet = mkdir(szDirPath, S_IRUSR | S_IWUSR);
				if (nRet < 0)
				{
					printf("\nfailed to create ap sys dump directory, mkdir err_cd: %d\n", nRet);
					break;
				}
				else
				{
					bExDumpDir = TRUE;
				}
			}
		}

        // buffer format according to fdl2 protocol
        LPBYTE pbyfdl2  = pStart;

        DWORD dwCmdType = 0;
        memcpy(&dwCmdType, pbyfdl2, sizeof(DWORD));

        switch(dwCmdType)
        {
            // start
        case 0:
            {
                if (bApDumping)
                {
                    if (NULL != pApFile)
                    {
                        fflush(pApFile);
                        fclose(pApFile);
                        pApFile = NULL;
                    }

                    if (bDoUnknown)
					{
						printf("\ndump ap unknown memory file end: total received: %lld bytes.\n", uRecvSize);
					}
					else
					{
						printf("\ndump ap memory file end: total received: %lld bytes, lost: %lld bytes.\n", uRecvSize, uTotalSize - uRecvSize);
					}
                }

                memcpy(&uTotalSize, pbyfdl2 + sizeof(DWORD), sizeof(UINT64));

                char chFileNm[128] = {0};
                memcpy(chFileNm, pbyfdl2 + sizeof(DWORD) + sizeof(UINT64), sizeof(chFileNm));
                chFileNm[127] = '\0';

                char chFilePath[MAX_PATH] = {0};
                strcpy(chFilePath, szDirPath);
                strcat(chFilePath, chFileNm);                

                pApFile = fopen(chFilePath, "wb+");

                if (!bApDumping)
				{
					printf("\n");
				}

				printf("dump ap memory file begin: %s, size: %lld bytes.\n", chFileNm, uTotalSize);

                bApDumping = TRUE;
                bDoUnknown = FALSE;
                uRecvSize  = 0;
                nProgress  = 0;
                uTickCount = 0;
            }
            break;

            // midst
        case 1:
            {
                if (!bApDumping)
                {
                    if (NULL != pApFile)
                    {
                        fflush(pApFile);
                        fclose(pApFile);
                        pApFile = NULL;
                    }

                    char chFilePath[MAX_PATH] = {0};

					int nIndex = 0;
					do
					{
						memset(chFilePath, 0, sizeof(chFilePath));
						strcpy(chFilePath, szDirPath);

						sprintf(chFilePath + strlen(szDirPath), "ap_dump_unknown%d.mem", nIndex);
						nIndex++;

					} while (access(chFilePath, F_OK) == 0);

					pApFile = fopen(chFilePath, "wb+");

                    printf("\ndump ap unknown memory file begin: %s, size: unknown.\n", chFilePath);

                    bApDumping = TRUE;
                    bDoUnknown = TRUE;
                    uTotalSize = 0;
                    uRecvSize  = 0;
                    nProgress  = 0;
                    uTickCount = 0;
                }

                if (pApFile != NULL)
                {
                    nLength -= sizeof(DWORD);

                    // Write assert body to the file
                    fseek(pApFile, 0, SEEK_END);
                    fwrite(pbyfdl2 + sizeof(DWORD), sizeof(BYTE), nLength, pApFile);
                    uRecvSize += nLength;
                    fflush(pApFile);  // Force to write to the file immediately
                }

                if (bDoUnknown)
                {
                    DWORD __tickCount = GetTickCount();
                    if (uTickCount == 0)
                    {
                        uTickCount = __tickCount;
                    }
                    else
                    {
                        if (__tickCount - uTickCount > 5000)
                        {
                            printf(".");
                            fflush(stdout);
                            uTickCount = __tickCount;
                        }
                    }
                }
                else
                {
                    int __nProgress = (int)((uRecvSize * 1.0f / uTotalSize) * 100);
                    if (__nProgress > nProgress)
                    {
                        printf(".");
                        fflush(stdout);
                        nProgress = __nProgress;
                    }
                }
            }
            break;

            // end (single file transfer end)
        case 2:
            {
                if (NULL != pApFile)
                {
                    fflush(pApFile);
                    fclose(pApFile);
                    pApFile = NULL;
                }

                if (bDoUnknown)
				{
					printf("\ndump ap unknown memory file end: total received: %lld bytes.\n", uRecvSize);
				}
				else
				{                            
					if (nLength == 4) // None compressed mode contains only command type(4 Bytes)
					{
						printf("\ndump ap memory file end: total received: %lld bytes, lost: %lld bytes.\n", uRecvSize, uTotalSize - uRecvSize);
					}
					else if (nLength == 12) // Compressed mode contains command type(4 Bytes) and compressed file length(8 Bytes)
					{
						UINT64 nRealSize = *(UINT64*)(pStart + 4);
						printf("\ndump ap memory file end: total received: %lld bytes(Compressed), lost: %lld bytes.\n", uRecvSize, nRealSize - uRecvSize);
					}
				}

                bApDumping = FALSE;
                bDoUnknown = FALSE;
                bExDumpDir = FALSE;
                uTotalSize = 0;
                uRecvSize  = 0;
                nProgress  = 0;
                uTickCount = 0;
            }
            break;

            // all finish
        case 3:
            {
				printf("\nTotal sysdump finished!\n");				
            }
            break;

        default:
            break;
        }
    } while (0);
}

void CUnisoc8850ChannelDataHandler::HandleAssertProc(LPBYTE lpBuffer, DWORD dwDataSize)
{
    /* header type is assumed to be 0xFF */
    DIAG_PACKAGE* lpPkg = (DIAG_PACKAGE*)lpBuffer;
    if (NULL != lpPkg && dwDataSize > 0)
    {
        /* print assert text info */
        if (0x0 == lpPkg->header.subtype && lpPkg->header.len > DIAG_HDR_LEN)
            printf("%s", (char*)(lpBuffer + DIAG_HDR_LEN));

        // process auto sending t cmd or create a work
        // thread to scan user AT cmd
        // make sure that only send t cmd once
        if (NORMAL_INFO == lpPkg->header.subtype)
        {
            if (g_bAutoProcAssert) // auto dumping
            {
                if (!m_bSendTCmd)
                {
                    printf("\nnow start auto dumping assert data...\n");

                    char szCmd[MAX_AT_CMD_LEN] = {0};
                    strcpy(szCmd, "t\n");

                    SendRequest(MSG_BOARD_ASSERT, 0x0, (LPBYTE)szCmd, strlen(szCmd));
                }
            }
            else
            {
                if (NULL == m_hInputThread)
                {
                    pthread_create(&m_hInputThread,
                                   NULL,
                                   (PTHREAD_START_ROUTINE)scanAssertCmdFunc,
                                   this);
                }
            }
        }
        else if (DUMP_MEM_DATA == lpPkg->header.subtype) // recv mem data
        {
            static int nDotNum = 0;

            if (m_bSendTCmd)
            {
                struct winsize info;
                ioctl(STDIN_FILENO,TIOCGWINSZ,&info);

                if (0 == nDotNum)
                {
                    printf("\n");
                }

                if(++nDotNum >= info.ws_col)
                {
                    nDotNum = 0;
                }

                printf(".");
                fflush(stdout);
            }
        }
        else if (DUMP_ALL_ASSERT_INFO_END == lpPkg->header.subtype) // recv assert end
        {
            if (NULL == m_hTimeoutThread)
            {
				printf("start waitAssertTimeoutFunc thread...");
                pthread_create(&m_hTimeoutThread,
                               NULL,
                               (PTHREAD_START_ROUTINE)waitAssertTimeoutFunc,
                               this);
            }
        }

        // update assert tickcount
        if (m_bSendTCmd)
        {
            m_AssertTickCount = GetTickCount();
        }
    }
}

void CUnisoc8850ChannelDataHandler::InitXlogConfig(const XLOG_CONFIG * cfg)
{
    if (NULL == cfg)
        return;
    if (cfg->logFolderThreashold > 0)
    {
        m_LogFolderThreasHold = cfg->logFolderThreashold;
    }
    if (cfg->logFileThreashold > 0)
    {
        m_LogFileThreasHold = cfg->logFileThreashold;
    }
    if (NULL != cfg->device[0] && strlen(cfg->device[0]) > 0)
    {
        strcpy(g_diagDevName, cfg->device[0]);
    }
    if (NULL != cfg->device[1] && strlen(cfg->device[1]) > 0)
    {
        strcpy(g_logDevName, cfg->device[1]);
    }
    if (NULL != cfg->device[2] && strlen(cfg->device[2]) > 0)
    {
        strcpy(g_u2sDevName, cfg->device[2]);
    }

    SetLogPath(cfg->logPath, strlen(cfg->logPath));
}

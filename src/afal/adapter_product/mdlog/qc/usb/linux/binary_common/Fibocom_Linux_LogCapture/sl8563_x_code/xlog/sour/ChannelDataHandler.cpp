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

// ChannelDataHandler.cpp: implementation of the CChannelDataHandler class.
//
//////////////////////////////////////////////////////////////////////

#include "ChannelDataHandler.h"
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
#include <math.h>

#include <pthread.h>

#include <assert.h>
#include "ExePathHelper.h"

CChannelDataHandler *g_pcdh;

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
#define SIZE_MB 1024 * 1024

/* dev path info */
char szDevPath_diag[64] = {0};
char szDevPath_log[64] = {0};
char szDevPath_u2s[64] = {0};

/* log threashold */
extern long long log_file_threashold;
extern int log_dir_threashold;
int log_file_num = 0;
int assert_flag = 0;
int fibo_log_flag = 0;

/* Global */
extern BOOL g_bAutoProcAssert;
char   g_szHistoryFolder[MAX_PATH] = {0};

/* get sys tickcount */
unsigned long GetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* get sys timestamp */
long GetTimeStamp()
{
    struct timeval tm;
    gettimeofday(&tm, NULL);

    return (tm.tv_sec * 1000 + tm.tv_usec / 1000);
}

/* get log file path & name */
static char* getLogFileName(const char* logPath, size_t logPathLen, BOOL bWrapDir, char* aplog_name, char* cplog_name)
{
    time_t     tNow;
    struct tm* pTime;
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

    if (aplog_name)
    {
        strcpy(aplog_name, g_szHistoryFolder);
        strcat(aplog_name, szData);
        strcat(aplog_name, ".ap");

        aplog_name[strlen(g_szHistoryFolder) + strlen(szData) + strlen(".ap")] = '\0';
    }

    if (cplog_name)
    {
        strcpy(cplog_name, g_szHistoryFolder);
        strcat(cplog_name, szData);
        strcat(cplog_name, ".logel");

        cplog_name[strlen(g_szHistoryFolder) + strlen(szData) + strlen(".logel")] = '\0';
    }
    return NULL;
}

FILE * CChannelDataHandler::m_aplogf = NULL;

CChannelDataHandler::CChannelDataHandler()
{
    g_pcdh = this;

    m_bUE_Obtained      = FALSE;
    m_bVer_Obtained     = FALSE;
    m_ATComm_Obtained   = FALSE;
    m_bNeedRestart      = FALSE;
    m_bSendTCmd         = FALSE;

    m_AssertTickCount = 0;

    m_pDiagChannel  = NULL;
    m_pSmpChannel   = NULL;

    m_logf          = NULL;
    m_aplogf        = NULL;
    m_plogFileName  = NULL;
    m_aplogFileName  = NULL;
    m_nPathLen      = 0;
    m_lFileSize     = 0;

    m_hReqThread     = NULL;
    m_hInputThread   = NULL;
    m_hTimeoutThread = NULL;

    memset(m_szLogPath, 0, sizeof(m_szLogPath));

    pthread_mutex_init(&m_scData, NULL);
    pthread_mutex_init(&m_scAtCmd, NULL);

    m_pMonitor = NULL;
}

CChannelDataHandler::~CChannelDataHandler()
{
    pthread_mutex_destroy(&m_scData);
    pthread_mutex_destroy(&m_scAtCmd);

    CProtocalChannelFactory::ReleaseInstance(m_pDiagChannel);
    CProtocalChannelFactory::ReleaseInstance(m_pSmpChannel);
}

BOOL CChannelDataHandler::LoadConfig()
{
    INI_CONFIG *config = NULL;

    GetExePath helper;
    std::string strIniPath = helper.getExeDir();
    strIniPath.insert(0,"/");
    strIniPath += "Channel.ini";

    config = ini_config_create_from_file(strIniPath.c_str(),0);

	if (config)
	{
        if(NULL == szDevPath_diag)
        {
            strcpy(szDevPath_diag, ini_config_get_string(config,"DIAG","DevPath","tty_usb_unknown"));
        }
        if(NULL == szDevPath_log)
        {
            strcpy(szDevPath_log, ini_config_get_string(config,"SMP","DevPath","tty_usb_unknown"));
        }

        strcpy(szDevPath_u2s, ini_config_get_string(config,"SMP","DevPath_Dump_Mode","tty_usb_unknown"));

        if(log_file_threashold == NULL)
        {
            log_file_threashold  = ini_config_get_int(config, "Setting", "log_file_threashold", 1); // default 1 GB
            log_file_threashold *= SIZE_MB;
        }
        if(log_dir_threashold == NULL)
        {
            log_dir_threashold  = ini_config_get_int(config, "Setting", "log_dir_threashold", 3); // default 3 counts
        }
	}
	else
	{		
		return FALSE;
	}

    ini_config_destroy(config);
    return TRUE;
}

BOOL CChannelDataHandler::CreateLogelFile()
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
/*        {
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
            while (lDirSize > log_dir_threashold)
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
*/
        // create new logel file
        if (NULL != m_plogFileName)
        {
            free(m_plogFileName);
            m_plogFileName = NULL;
        }

        if ((log_file_num == 0) && (NULL != m_aplogFileName))
        {
            free(m_aplogFileName);
            m_aplogFileName = NULL;
        }

        m_log.LogFmtStr(SPLOGLV_ERROR,"m_logtype is %02x", m_logtype);

        if(m_logtype & 0x01)
        {
            m_plogFileName = (char*)malloc(MAX_PATH);
        }
        if((log_file_num == 0) && (m_logtype & 0x02))
        {
            m_aplogFileName = (char*)malloc(MAX_PATH);
        }
        getLogFileName(m_szLogPath, m_nPathLen, FALSE, m_aplogFileName, m_plogFileName);

        if(m_logtype & 0x01){
            m_logf = fopen(m_plogFileName, "w+b");
            if (NULL == m_logf)
            {
                printf("open log file failed, given file name: %s, \"%s\"\n", m_plogFileName, strerror(errno));
                break;
            }
        }
        if((log_file_num == 0) && (m_logtype & 0x02)){
            m_aplogf = fopen(m_aplogFileName, "a+");
            if (NULL == m_aplogf)
            {
                printf("open aplog file failed, given file name: %s, \"%s\"\n", m_aplogFileName, strerror(errno));
                break;
            }
        }

        log_file_num++;
        if(log_dir_threashold != -1 && log_file_num > log_dir_threashold)
        {
            if (chdir(g_szHistoryFolder) < 0)
            {
                printf("can not change current working directory:\n");
            }

            system("pwd");
            if (system("ls -l | grep '\.logel$' | wc -l") < 0)
            {
                printf("system cmd error:\n");
            }

            if (system("ls -tr *.logel | head -n 1 | xargs rm -rf") < 0)
            {
                printf("system cmd error:\n");
            }
            log_file_num--;
        }
        bRet = TRUE;
      
    } while (0);
    
    return bRet;
}

void CChannelDataHandler::SetLogPath(const char* logPath, int nPathLen)
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

void CChannelDataHandler::SetLogType(int type)
{
    m_logtype = type;
}

BOOL CChannelDataHandler::StartProc()
{
    BOOL bRet = TRUE;

    do {
	    /* load INI file */
	    if (!LoadConfig())
        {
            printf("load INI file: Channel.ini failed\n");
            bRet = FALSE;
            break;
        }

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

        if(m_logtype & 0x01)
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

        if(m_logtype & 0x02)
        {
            fibo_log_flag = 0;
            assert_flag = 0;
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
    m_ATComm_Obtained = FALSE;
    m_bNeedRestart    = FALSE;
    m_bSendTCmd       = FALSE;
    m_AssertTickCount = 0;

    return bRet;
}

void* CChannelDataHandler::handlerServiceFunc(void* lpParam)
{
    CChannelDataHandler * p = (CChannelDataHandler *)lpParam;
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
            nMode = p->m_pDiagChannel->GetChannelType() - 1;
            if (-1 == nFd_diag)
            {
                nFd_diag = open(szDevPath_diag, O_RDWR|O_NOCTTY,0666);
                close(nFd_diag);
            }

            if (-1 == nFd_smp)
            {
                nFd_smp  = open(szDevPath_log,  O_RDWR|O_NOCTTY,0666);
                close(nFd_smp);
            }

            if (1 == nMode)
            {
                if (p->m_pSmpChannel->m_LowerChannel->IsConnect())
                    nFd_u2s = 1;
            }
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
                        // open log port
                        if (nFd_smp != -1)
                        {
                            printf("Clearing cached data...\n");

                            p->m_pSmpChannel->Open(NULL, TRUE);

                            usleep(200 * 1000);
                        }

                        printf("The system starts to save device data...\n");

                        if (p->m_pDiagChannel->Open(NULL, TRUE))
                        {
                            usleep(1 * 1000);

                            // print assert debug menu
                            char szCmd[MAX_AT_CMD_LEN] = {0};
                            strcpy(szCmd, "0\n");
                            p->SendRequest(MSG_BOARD_ASSERT, 0x0, (LPBYTE)szCmd, strlen(szCmd));                            

                            // query modem UE time
                            if (!p->m_bUE_Obtained)
                                p->SendRequest(DIAG_SYSTEM_F, 0x11, NULL, 0);

                            usleep(5 * 1000);

                            // query modem version
                            if (!p->m_bVer_Obtained)
                                p->SendRequest(DIAG_SWVER_F, 0x0, NULL, 0);

                            usleep(5 * 1000);

                            if (!p->m_ATComm_Obtained)
                            {
                                //at+armlog=1\r
                                unsigned char atlog_on[12] {0x61, 0x74, 0x2b, 0x61, 0x72, 0x6d, 0x6c, 0x6f, 0x67, 0x3d,
                                                            0x31, 0x0d};
                                //unsigned char atlog_on[12] {"at+armlog=1\r"};
                                p->SendRequest(0x68, 0x0, atlog_on, 12);

                                usleep(5 * 1000);

                                //at+splogping=19536,1,0,0,0,0\r
                                unsigned char atping[29] {0x61, 0x74, 0x2B, 0x73, 0x70, 0x6C, 0x6F, 0x67, 0x70, 0x69,
                                                         0x6E, 0x67, 0x3D, 0x31, 0x39, 0x35, 0x33, 0x36, 0x2C, 0x31,
                                                         0x2C, 0x30, 0x2C, 0x30, 0x2C, 0x30, 0x2C, 0x30, 0x0D};
                                //unsigned char atping[29] {"at+splogping=19536,1,0,0,0,0\r"};
                                BOOL pingOK = p->SendRequest(0x68, 0x0, atping, 29);

                                usleep(5 * 1000);
                            }

                            nMode   = 0;
                            bOpened = true;
                        }
						
                        // // open log port
                        //if (nFd_smp != -1)
                        //{
                        //	p->m_pSmpChannel->Open(NULL, TRUE);
                        //}
                    }
                }
                else
                {
                    CHANNEL_ATTRIBUTE ca;
                    memset(&ca, 0, sizeof(ca));

                    ca.ChannelType       = CHANNEL_TYPE_TTY;
                    ca.tty.nProtocolType = PROTOCOL_TYPE_U2S;

                    if (p->m_pSmpChannel->Open(&ca, TRUE))
                    {
                        //send 't\n'
                        unsigned char AP_CC[2] {0x74, 0x0A };
                        p->SendRequest(MSG_BOARD_ASSERT, DUMP_AP_MEMORY, AP_CC, 2);
                        usleep(5 * 1000);
                        p->SendRequest(MSG_BOARD_ASSERT, DUMP_AP_MEMORY, AP_CC, 2);
                        usleep(5 * 1000);

                        //set timer 30s
                        //p->m_tc.StartTimerS(30, (void *)p->ApSysDumpTimerOut);//set timer 30s

                        nMode   = 1;
                        bOpened = true;
                    }
                }
			}

            /* query modem info */
            if (bOpened && nMode == 0 && !p->m_bSendTCmd && !p->IsModemInfoObtained() && nTryNum < MAX_REQ_TRY_NUM)
            {
                /*
                // at+splogping=19536,1,0,0,0,0\r
                if (!p->m_ATComm_Obtained)
                {
                    unsigned char atcom[29] {0x61, 0x74, 0x2B, 0x73, 0x70, 0x6C, 0x6F, 0x67, 0x70, 0x69,
                                             0x6E, 0x67, 0x3D, 0x31, 0x39, 0x35, 0x33, 0x36, 0x2C, 0x31,
                                             0x2C, 0x30, 0x2C, 0x30, 0x2C, 0x30, 0x2C, 0x30, 0x0D};
                    //unsigned char atcom[29] {"at+splogping=19536,1,0,0,0,0\r"};
                    p->SendRequest(0x68, 0x0, atcom, 29);
                }
                */

                // query modem UE time
                if (!p->m_bUE_Obtained)
                    p->SendRequest(DIAG_SYSTEM_F, 0x11, NULL, 0);

                usleep(2 * 1000);

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

            usleep(5 * 1000);
        }
    }

    return 0;
}

void* CChannelDataHandler::scanAssertCmdFunc(void* lpParam)
{
    CChannelDataHandler * p = (CChannelDataHandler *)lpParam;
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

    return 0;
}

void* CChannelDataHandler::waitAssertTimeoutFunc(void* lpParam)
{
    CChannelDataHandler * p = (CChannelDataHandler *)lpParam;
    if (NULL != p)
    {
        while (p->m_AssertTickCount == 0) // invalid tickcount
        {
            usleep(5 * 1000);
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

            usleep(5 * 1000);
            __tickCount += 5000;
        }
    }

    return 0;
}

void CChannelDataHandler::StopProc()
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
    m_ATComm_Obtained = FALSE;
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

BOOL CChannelDataHandler::NeedReStart()
{
    return m_bNeedRestart;
}

void CChannelDataHandler::SetUserInputMonitorPtr(CUserInputMonitor *pMonitor)
{
    assert(pMonitor);
    m_pMonitor = pMonitor;
}

int CChannelDataHandler::OnChannelEvent( uint32_t event,void* lpEventData )
{
    // reserved
	return 0;
}

int CChannelDataHandler::OnChannelData(void* lpData, uint32_t ulDataSize,uint32_t reserved /*=0*/ )
{
    LockDataProc( true );

    ProcessChannelData(lpData, ulDataSize);

    LockDataProc( false );
	return 0;
}

void CChannelDataHandler::LockDataProc( bool bLock )
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

void CChannelDataHandler::LockATCmdProc( bool bLock )
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

BOOL CChannelDataHandler::SendRequest(BYTE type, BYTE subtype, LPBYTE lpBuffer, DWORD dwDataSize)
{
    LockATCmdProc( true );

    DWORD dwSend = 0;
    unsigned short wdlen = 0;
    char *szCmd = new char[dwDataSize+1];
    bzero(szCmd, dwDataSize+1);
    if (NULL != lpBuffer && dwDataSize > 0)
    {
        strcpy(szCmd, (char*)lpBuffer);
    }

    PRT_WRITE_T writeData = {0};
    writeData.action = PRT_WRITE_no_respond;
    writeData.nCond  = -1;

    //AP Sys Dump
    if (DUMP_AP_MEMORY == subtype)
    {
        writeData.lpProtocolData = szCmd;
        wdlen = dwDataSize;
        dwSend = m_pSmpChannel->Write(&writeData, wdlen, subtype);
    }
    else
    {
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

        DIAG_PACKAGE pkgData = {0};
        memcpy( (LPBYTE)(&(pkgData.header)), &request, sizeof( DIAG_HEADER ) );
        pkgData.header.len       = dwDataSize;
        pkgData.data             = (NULL == lpBuffer) ? (&request + sizeof( DIAG_HEADER )) : (void*)lpBuffer;
        writeData.lpProtocolData = &pkgData;
        unsigned short wdlen =  DIAG_HDR_LEN;

        dwSend = m_pDiagChannel->Write( &writeData, wdlen );
    }

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

    delete [] szCmd;

    LockATCmdProc( false );

    return TRUE;
}

BOOL CChannelDataHandler::IsModemInfoObtained()
{
    m_log.LogFmtStr(SPLOGLV_ERROR,"%d.", (m_bUE_Obtained && m_bVer_Obtained));
    return (m_bUE_Obtained && m_bVer_Obtained);
}

void CChannelDataHandler::FiboGetApLog(DIAG_PACKAGE* lpPkg, DWORD dwDataSize)
{
    LPBYTE logbuf;
    BOOL fibo_diag = FALSE;
    static int switch_flag = -1;

    m_log.LogFmtStr(SPLOGLV_ERROR,"%s.", __func__);

    unsigned char diagstart[] = {0x00,0x00,0x46,0x58,0x36,0x35,0x30,0xED,0x1B};
    unsigned char loginit[] = {0x04,0x00,0x01,0x00,0x00,0x78};
    unsigned char logread[] = {0x04,0x01,0x00,0x08,0xbd,0x86};

    if (MSG_BOARD_ASSERT == lpPkg->header.type)
    {
        assert_flag = 1;
        return;
    }

    if (0x38 == lpPkg->header.type && 0xfb == lpPkg->header.subtype)
         fibo_diag = TRUE;

    if(IsModemInfoObtained())//IsModemInfoObtained())
    {
        m_log.LogFmtStr(SPLOGLV_ERROR,"switch_flag is %d.", switch_flag);
        switch(switch_flag)
        {
            case -1:
                SendRequest(0x38, 0xfb, diagstart, sizeof(diagstart));
                switch_flag = 0;
                fibo_log_flag = 1;
                break;
            case 0:
                if(fibo_diag && ((LPBYTE)lpPkg)[11] == 0)
                {
                    SendRequest(0x38, 0xfb, loginit, sizeof(loginit));
                    switch_flag = 1;
                }
                break;
            case 1:
                if(fibo_diag && (((LPBYTE)lpPkg)[11] == 0 || ((LPBYTE)lpPkg)[11] == 5))
                {
                    SendRequest(0x38, 0xfb, logread, sizeof(logread));
                    switch_flag = 2;
                }
                break;
            case 2:
                if(fibo_diag && ((LPBYTE)lpPkg)[11] == 0)
                {
                    if (NULL == m_aplogf)
                    {
                        printf("%s m_aplogf is NULL\n",__func__);
                        return;
                    }

                    logbuf = &((LPBYTE)lpPkg)[12];
                    if(dwDataSize-14 > 0)
                    {
                        fwrite(logbuf, 1, dwDataSize - 14, m_aplogf);
                        if(!m_background)
                        {
                            printf(".");
                        }
                        usleep(200*000);
                    }
                    else
                    {
                        if(!m_background)
                            printf("\nNo more AP logs were obtained\n");
                        if(m_logtype & 0x01)
                            usleep(200*000);
                        else
                            sleep(5);
                    }

                    SendRequest(0x38, 0xfb, logread, sizeof(logread));
                    switch_flag = 2;
                }
                break;
            default:
                break;
        }
    }
}


void CChannelDataHandler::ProcessChannelData(void* lpData, uint32_t ulDataSize)
{
    PRT_BUFF* lpBuff    = (PRT_BUFF*)lpData;
	DIAG_PACKAGE* lpPkg = (DIAG_PACKAGE*)lpBuff->lpData;
	DWORD dwDataSize    = lpBuff->size;

    do
    {
        if(m_logtype & 0x02)
        {
            do{
                if(assert_flag)
                    break;
                if (!fibo_log_flag || lpPkg->header.type == 0x38)
                {
                     FiboGetApLog(lpPkg, dwDataSize);
                }
            }while(0);

            if(lpPkg->header.type == 0x38)
            {
                if(assert_flag)
                {
                    if (g_bAutoProcAssert) // auto dumping
                    {
                        if (!m_bSendTCmd)
                        {
                            char szCmd[MAX_AT_CMD_LEN] = {0};
                            strcpy(szCmd, "3\n");

                            SendRequest(MSG_BOARD_ASSERT, 0x0, (LPBYTE)szCmd, strlen(szCmd));
                        }
                    }
                }
                break;
            }
        }

        if (MSG_BOARD_ASSERT == lpPkg->header.type && DUMP_AP_MEMORY == lpPkg->header.subtype)  // for AP sysdump
        {
            usleep(5 * 1000);

            // save ap sys dump data in other mem file(s)
            HandleApSysDump((LPBYTE)lpPkg, dwDataSize - DIAG_HDR_LEN);
            break;
        }
        else
        {
            if (DIAG_SYSTEM_F == lpPkg->header.type && 0x01 == (lpPkg->header.subtype & 0x0F))
            {
                m_bUE_Obtained = TRUE;
            }

            if (DIAG_SWVER_F == lpPkg->header.type && 0x0 == lpPkg->header.subtype)
            {
                m_bVer_Obtained = TRUE;
            }

            if (0x68 == lpPkg->header.type && 0x0 == lpPkg->header.subtype)
            {
                m_ATComm_Obtained = TRUE;
            }

            // write cp log to file
        if(m_logtype & 0x01){
            if(lpPkg->header.type == 0xf8 && !m_background)
                printf(".");
            WriteDataToFile((LPBYTE)lpPkg, dwDataSize);
        }

            // deal assert process
            if (MSG_BOARD_ASSERT == lpPkg->header.type)
            {
                assert_flag = 1;
                if (NULL != m_hReqThread)
                {
                    pthread_cancel(m_hReqThread);
                    pthread_join(m_hReqThread,NULL);
                    m_hReqThread = NULL;
                }

                HandleAssertProc((LPBYTE)lpPkg, dwDataSize);
            }
        }
    } while (0);
}

void CChannelDataHandler::WriteDataToFile(LPBYTE lpBuffer, DWORD dwDataSize)
{
    if (NULL == m_logf)
    {
        return;
    }

    DWORD dwWriteSize = dwDataSize;

    if (!m_bSendTCmd && m_lFileSize + sizeof(dwWriteSize) + dwDataSize > log_file_threashold)
    {
        CreateLogelFile();
    }

    fwrite(&dwWriteSize, 1, sizeof(dwWriteSize), m_logf);
    fwrite(lpBuffer, 1, dwDataSize, m_logf);
    //fflush(m_logf);

    m_lFileSize += sizeof(dwWriteSize) + dwDataSize;
}

void CChannelDataHandler::HandleApSysDump(LPBYTE pStart, UINT nLength)
{
    m_tc.SetDeltaTimeS(30);   //Res Timer

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
        char szCDT[25] {0};

        // create dump directory
		if (!bExDumpDir)
		{
			memset(szDirPath, 0, sizeof(szDirPath));

            //strcpy(szDirPath, g_szHistoryFolder);
            GetCurDateTime(szCDT);
            sprintf(szDirPath, "ap_sys_dump_%s/", szCDT);
            //strcat(szDirPath, "ap_sys_dump/");

			if (access(szDirPath, F_OK) != 0)
			{
                int nRet = mkdir(szDirPath, 0777 /* S_IRUSR | S_IWUSR */);
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

        int nSeek = 8;
        //type
        DWORD dwCmdType = 0;
        for (int i = 0; i < 4; i++)
        {
            dwCmdType += ((unsigned char)pbyfdl2[nSeek++] * pow(0x100, i));
        }

        switch(dwCmdType)
        {        
        case 0:     // start
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
                        //printf("\ndump ap memory file end: total received: %lld bytes, lost: %lld bytes.\n", uRecvSize, uTotalSize - uRecvSize);
                        printf("\ndump ap memory file end: No end-of-file sign received!!!\n");
                    }
                }

                //FileSize 8byte
                uTotalSize = 0;

                //printf("%02X %02X %02X %02X %02X %02X %02X %02X\n",
                //       pbyfdl2[nSeek],   pbyfdl2[nSeek+1], pbyfdl2[nSeek+2], pbyfdl2[nSeek+3],
                //       pbyfdl2[nSeek+4], pbyfdl2[nSeek+5], pbyfdl2[nSeek+6], pbyfdl2[nSeek+7]);

                for (int i = 0; i < 8; i++)
                {
                    uTotalSize += ((unsigned char)pbyfdl2[nSeek++] * pow(0x100, i));
                    //m_uSingleFileTotalSize += (pbyfdl2[nSeek++] * pow(10, i));
                }
                //printf("filesize=%d\n", uTotalSize);

                nSeek += 8;
                //file name
                char chFileNm[128] = {0};
                for (int i = 0; i < 127; i++)
                {
                    chFileNm[i] = pbyfdl2[nSeek++];
                }
                //memcpy(chFileNm, &pbyfdl2[nSeek], sizeof(chFileNm));
                chFileNm[127] = '\0';
                char chFilePath[MAX_PATH] = {0};
                strcpy(chFilePath, szDirPath);
                strcat(chFilePath, chFileNm);

                pApFile = fopen(chFilePath, "wb+");

                if (!bApDumping)
                {
                    printf("\n");
                }

                printf("dump ap memory file begin: %s, size: %lld bytes.\n", chFileNm, (long long)uTotalSize);
                //printf("dump ap memory file : %s , ", chFileNm);

                bApDumping = TRUE;
                bDoUnknown = FALSE;
                uRecvSize  = 0;
                nProgress  = 0;
                uTickCount = 0;

            }
            break;
        case 1:     //data
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
                        sprintf(chFilePath, "ap_dump_unknown%d.mem", nIndex);
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
                    fwrite(pbyfdl2 + sizeof(DWORD) + 8, sizeof(BYTE), nLength, pApFile);    //***
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
                    int __nProgress = (int)((uRecvSize * 1.0f / uTotalSize) * 100.);
                    if (__nProgress > nProgress)
                    {
                        printf(".");
                        fflush(stdout);
                        nProgress = __nProgress;
                    }
                }
            }
            break;
        case 2:     // end (single file transfer end)
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
                        //printf("\ndump ap memory file end: total received: %lld bytes, lost: %lld bytes.\n", uRecvSize, uTotalSize - uRecvSize);
                        printf("\ndump ap memory file end: OK\n");
					}
					else if (nLength == 12) // Compressed mode contains command type(4 Bytes) and compressed file length(8 Bytes)
					{
						UINT64 nRealSize = *(UINT64*)(pStart + 4);
                        //printf("\ndump ap memory file end: total received: %lld bytes(Compressed), lost: %lld bytes.\n", uRecvSize, nRealSize - uRecvSize);
                        printf("\ndump ap memory file end(Compressed): OK\n");
					}
				}

                bApDumping = FALSE;
                bDoUnknown = FALSE;
                uTotalSize = 0;
                uRecvSize  = 0;
                nProgress  = 0;
                uTickCount = 0;

            }
            break;
         case 3: // all finish
            {
                m_tc.KillTimerS();

                printf("\nTotal sysdump finished!\n");
            }
            break;
        case 4:  // time out , all finish
            m_tc.KillTimerS();
            printf("\nTotal sysdump finished -- timer out -- !\n");
            break;

        default:
            break;
        }
    } while (0);
}

void CChannelDataHandler::HandleAssertProc(LPBYTE lpBuffer, DWORD dwDataSize)
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
                if (!m_bSendTCmd && !(m_logtype & 0x02))
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

void CChannelDataHandler::InitXlogConfig(const XLOG_CONFIG * cfg)
{
    if (NULL == cfg)
        return;
    if (cfg->logFileThreashold > 0)
    {
        log_file_threashold = cfg->logFileThreashold;
    }
    if (cfg->logFolderThreashold > 0)
    {
        log_dir_threashold = cfg->logFolderThreashold;
    }
    if (NULL != cfg->device[0] && strlen(cfg->device[0]) > 0)
    {
        strcpy(szDevPath_diag, cfg->device[0]);
    }
    if (NULL != cfg->device[1] && strlen(cfg->device[1]) > 0)
    {
        strcpy(szDevPath_log, cfg->device[1]);
    }
    if (NULL != cfg->device[2] && strlen(cfg->device[2]) > 0)
    {
        strcpy(szDevPath_u2s, cfg->device[2]);
    }

    SetLogPath(cfg->logPath, strlen(cfg->logPath));
}


void CChannelDataHandler::GetCurDateTime(char *szCurDT)
{
    time_t now;
    now = time(0);
    tm *t= localtime(&now);

    sprintf(szCurDT, "%04d_%02d_%02d_%02d_%02d_%02d\0",
                            t->tm_year+1900,
                            t->tm_mon+1,
                            t->tm_mday,
                            t->tm_hour,
                            t->tm_min+1,
                            t->tm_sec+1);
}

//Timer Out Fun
void CChannelDataHandler::ApSysDumpTimerOut(int nToRet)
{
    unsigned char enddump[13];
    memset(enddump, 0x00, 13);
    enddump[8] = 0x04;

    if (0 == nToRet)
    {
        g_pcdh->HandleApSysDump(enddump, 13);
    }
}

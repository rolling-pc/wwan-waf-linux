// ChannelDataHandler.cpp: implementation of the CUnisoc8910ChannelDataHandler class.
//
//////////////////////////////////////////////////////////////////////

#include "Unisoc8910ChannelDataHandler.h"
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
#include "vfs.h"
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
#define DIAG_SWVER_F 0 // Information regarding MS software
#define DIAG_SYSTEM_F 5
#define MSG_BOARD_ASSERT 255

#define ITC_REQ_TYPE 209
#define ITC_REQ_SUBTYPE 100
#define ITC_REP_SUBTYPE 101

#define UET_SUBTYPE_CORE0 102 // UE Time Diag SubType field, core0
#define TRACET_SUBTYPE 103    // Trace Time Diag subtype filed
#define UET_SUBTYPE_CORE1 104 // UE Time Diag SubType field, core1

#define UET_SEQ_NUM 0x0000FFFF           // UE Time Diag Sequnce number fi
#define UET_DIAG_LEN (DIAG_HDR_LEN + 12) // UE Time Diag Length field (20)

/* ASSERT */
#define NORMAL_INFO 0x00
#define DUMP_MEM_DATA 0x01
#define DUMP_ALL_ASSERT_INFO_END 0x04
#define DUMP_AP_MEMORY 0x08

#define DUMP_DEFAULT_PATH "./armlog/"

#define MAX_REQ_TRY_NUM 5
#define MAX_AT_CMD_LEN 8

/* Others */
#define SIZE_GB 1024 * 1024 * 1024
/* dev path info */

static char g_debugHostDevName[MAX_DEVICE_NAME];
static char g_armTracerDevName[MAX_DEVICE_NAME];

/* Global */
extern BOOL g_bAutoProcAssert;
// char g_szHistoryFolder[MAX_PATH] = {0};

CUnisoc8910ChannelDataHandler::CUnisoc8910ChannelDataHandler()
{
    m_bNeedRestart = FALSE;
    m_pArmTraceChannel = NULL;
    m_pDebugHostChannel = NULL;

    m_nPathLen = 0;
    m_hReqThread = NULL;

    m_LogPathSize = 0;
    m_CLogPathSize = 0;

    m_LogFolderThreasHold = 1 * 1024 * 1024 * 1024;      // 1GB
    m_CLogFolderThreasHold = m_LogFolderThreasHold >> 4; // 64MB
    m_LogFileThreasHold = m_CLogFolderThreasHold >> 1;   // 32MB

    memset(m_szLogPath, 0, sizeof(m_szLogPath));

    pthread_mutex_init(&m_scData, NULL);

    m_pMonitor = NULL;
}

CUnisoc8910ChannelDataHandler::~CUnisoc8910ChannelDataHandler()
{
    pthread_mutex_destroy(&m_scData);

    free(m_pArmTraceChannel);
    free(m_pDebugHostChannel);
}

BOOL CUnisoc8910ChannelDataHandler::CreateLogelPath()
{
    BOOL bRet = FALSE;
    {
        time_t tNow;
        struct tm *pTime;
        char *pNameBuf;
        unsigned nYear;

        tNow = time(0);
        pTime = localtime(&tNow);
        nYear = 1900 + pTime->tm_year;

        char szData[64] = {0};
        sprintf(szData, "%04u-%02u-%02u_%02u-%02u-%02u",
                nYear, pTime->tm_mon + 1, pTime->tm_mday,
                pTime->tm_hour, pTime->tm_min, pTime->tm_sec);

        strcpy(m_szCurLogPath, m_szLogPath);
        strcat(m_szCurLogPath, szData);
        strcat(m_szCurLogPath, "/");
        if (access(m_szCurLogPath, 0))
        {
            mkdir(m_szCurLogPath, 0777);
            // LogInfo("[%s] mkdir:%s\n", pTaskInfo->pdev->syspath, str_dir);
        }
    }

    bRet = TRUE;
    return bRet;
}

void CUnisoc8910ChannelDataHandler::SetLogPath(const char *logPath, int nPathLen)
{
    if (NULL != logPath && strlen(logPath) > 0 && nPathLen > 0)
    {
        memset(m_szLogPath, 0, sizeof(m_szLogPath));
        memset(m_szCurLogPath, 0, sizeof(m_szCurLogPath));

        strcpy(m_szLogPath, logPath);
        m_nPathLen = nPathLen;

        if (strlen(m_szLogPath) > 0)
        {
            if ('/' != m_szLogPath[m_nPathLen - 1])
            {
                strcat(m_szLogPath, "/");
            }
        }
        CreateLogelPath();
    }
}

BOOL CUnisoc8910ChannelDataHandler::StartProc()
{
    BOOL bRet = TRUE;

    do
    {
        std::cout << "ThreasHold:" << m_LogFolderThreasHold << " " << m_CLogFolderThreasHold << " " << m_LogFileThreasHold << std::endl;
        if (NULL == m_pArmTraceChannel)
        {
            m_pArmTraceChannel = new CArmTracerChannel("ArmTracer");

            assert(m_pArmTraceChannel);
            m_pArmTraceChannel->AddObserver(this);
            m_pArmTraceChannel->SetOutFileThreasHold(m_LogFileThreasHold);
        }

        if (NULL == m_pDebugHostChannel)
        {
            m_pDebugHostChannel = new CDebugHostChannel("DebugHost");

            assert(m_pDebugHostChannel);
            m_pDebugHostChannel->AddObserver(this);
            m_pDebugHostChannel->SetOutFileThreasHold(m_LogFileThreasHold >> 2);
        }

        if (NULL == m_pArmTraceChannel || NULL == m_pDebugHostChannel)
        {
            printf("create channel object failed");
            bRet = FALSE;
            break;
        }

        m_LogPathSize = vfs_dir_total_size(m_szLogPath);

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
    } while (0);

    m_bNeedRestart = FALSE;
    return bRet;
}

void *CUnisoc8910ChannelDataHandler::handlerServiceFunc(void *lpParam)
{
    CUnisoc8910ChannelDataHandler *p = (CUnisoc8910ChannelDataHandler *)lpParam;
    if (NULL != p)
    {
        int nTryNum = 0;
        bool bOpened = false;

        int nFd_armTracer = -1;
        int nFd_debugHost = -1;

        while (1)
        {
            /* detect device node periodly */
            nFd_armTracer = open(g_armTracerDevName, O_RDWR | O_NOCTTY, 0666);
            close(nFd_armTracer);
            nFd_debugHost = open(g_debugHostDevName, O_RDWR | O_NOCTTY, 0666);
            close(nFd_debugHost);
            if (nFd_armTracer == -1 && nFd_debugHost == -1)
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

            /* open device node */
            if (!bOpened && nTryNum == 0)
            {
                // open log port
                if (nFd_debugHost != -1)
                {
                    
                    CHANNEL_ATTRIBUTE ca;
                    memset(&ca, 0, sizeof(ca));
                    ca.ChannelType = CHANNEL_TYPE_TTY;
                    ca.tty.pDevPath = g_debugHostDevName;
                    ca.tty.dwPortNum = 1;
                    ca.tty.dwBaudRate = 115200;
                    if (p->m_pDebugHostChannel->Open(&ca, TRUE))
                    {
                        bOpened = true;
                    }
                }

                if (nFd_armTracer != -1)
                {
                    CHANNEL_ATTRIBUTE ca;
                    memset(&ca, 0, sizeof(ca));
                    ca.ChannelType = CHANNEL_TYPE_TTY;
                    ca.tty.pDevPath = g_armTracerDevName;
                    ca.tty.dwPortNum = 2;
                    ca.tty.dwBaudRate = 115200;
                    p->m_pArmTraceChannel->Open(&ca, TRUE);
                }
            }
            sleep(5);
        }
    }
}

void CUnisoc8910ChannelDataHandler::StopProc()
{
    pthread_cancel(m_hReqThread);
    pthread_join(m_hReqThread, NULL);
    m_hReqThread = NULL;

    assert(m_pArmTraceChannel);
    m_pArmTraceChannel->Close();

    assert(m_pDebugHostChannel);
    m_pDebugHostChannel->Close();
}

BOOL CUnisoc8910ChannelDataHandler::NeedReStart()
{
    return m_bNeedRestart;
}

void CUnisoc8910ChannelDataHandler::SetUserInputMonitorPtr(CUserInputMonitor *pMonitor)
{
    assert(pMonitor);
    m_pMonitor = pMonitor;
}

int CUnisoc8910ChannelDataHandler::OnChannelEvent(uint32_t event, void *lpEventData)
{
    // reserved
    return 0;
}

int CUnisoc8910ChannelDataHandler::OnChannelData(void *lpData, uint32_t ulDataSize, void *reserved /*=0*/)
{
    PRT_BUFF *lpBuff = (PRT_BUFF *)lpData;
    DIAG_PACKAGE *lpPkg = (DIAG_PACKAGE *)lpBuff->lpData;
    DWORD dwDataSize = lpBuff->size;
    CBaseChannel *ch = (CBaseChannel *)reserved;
    LockDataProc(true);
    do
    {
        if (m_LogPathSize + dwDataSize > m_LogFolderThreasHold)
        {
            std::cout << "over path ThreasHold:" << m_LogFolderThreasHold << " dwsize:" << m_LogPathSize + dwDataSize << std::endl;
            {
                long long lDirSize = 0;

                struct dirent *pDirEntry = NULL;
                DIR *pDir = NULL;

                if ((pDir = opendir(m_szLogPath)) == NULL)
                {
                    printf("opendir failed : %s\n", m_szLogPath);
                    break;
                }
                std::vector<string> vecFileName;
                while (NULL != (pDirEntry = readdir(pDir)))
                {
                    if (strcmp(pDirEntry->d_name, "..") == 0 || strcmp(pDirEntry->d_name, ".") == 0)
                        continue;

                    string strFileName = pDirEntry->d_name;
                    vecFileName.push_back(strFileName);
                }
                closedir(pDir);
                std::sort(vecFileName.begin(), vecFileName.end()); // sort by asc
                std::vector<off_t> vecSize;
                for (int j = 0; j < vecFileName.size(); ++j)
                {
                    string strFilePath = m_szLogPath;
                    strFilePath += vecFileName[j];
                    // printf("Log File: %s\n", strFilePath.c_str());
                    int64_t dir_size = vfs_dir_total_size(strFilePath.c_str());
                    lDirSize += dir_size;
                    vecSize.push_back(dir_size);
                }
                m_LogPathSize = lDirSize;
                size_t _index = 0;
                size_t _index_del = vecFileName.size() / 3;
                for (_index = 0; _index < _index_del; _index++)
                {
                    string strFilePath = m_szLogPath;
                    strFilePath += vecFileName[_index];
                    if (vfs_rmdir_recursive(strFilePath.c_str()) < 0)
                    {
                        printf("Remove file %s failed, error code: %d,\"%s\"\n", strFilePath.c_str(), errno, strerror(errno));
                    }
                    m_LogPathSize -= vecSize[_index];
                }
            }
        }
        else if (m_CLogPathSize + dwDataSize > m_CLogFolderThreasHold)
        {
            CreateLogelPath();
            m_CLogPathSize = 0;
        }

        {
            size_t w_size = ch->WriteDataToFile(m_szCurLogPath, (LPBYTE)lpPkg, dwDataSize);
            m_LogPathSize += w_size;
            m_CLogPathSize += w_size;
        }
    } while (0);

    LockDataProc(false);
    return 0;
}

void CUnisoc8910ChannelDataHandler::LockDataProc(bool bLock)
{
    if (bLock)
    {
        pthread_mutex_lock(&m_scData);
    }
    else
    {
        pthread_mutex_unlock(&m_scData);
    }
}

void CUnisoc8910ChannelDataHandler::InitXlogConfig(const XLOG_CONFIG * cfg)
{
    if (NULL == cfg)
        return;
    if (cfg->logFolderThreashold > 0)
    {
        m_LogFolderThreasHold = cfg->logFolderThreashold;
        m_CLogFolderThreasHold = m_LogFolderThreasHold >> 4; // 1/16
    }
    if (cfg->logFileThreashold > 0)
    {
        m_LogFileThreasHold = cfg->logFileThreashold;
    }
    if (NULL != cfg->device[0] && strlen(cfg->device[0]) > 0)
    {
        strcpy(g_debugHostDevName, cfg->device[0]);
    }
    if (NULL != cfg->device[1] && strlen(cfg->device[1]) > 0)
    {
        strcpy(g_armTracerDevName, cfg->device[1]);
    }

    SetLogPath(cfg->logPath, strlen(cfg->logPath));
}

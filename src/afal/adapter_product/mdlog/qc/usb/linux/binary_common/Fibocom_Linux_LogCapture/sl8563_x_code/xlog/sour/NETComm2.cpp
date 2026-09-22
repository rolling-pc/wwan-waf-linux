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

#include "NETComm2.h"

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
#include <sys/file.h>

#include <pthread.h>

#include <assert.h>
#include "ExePathHelper.h"
extern "C"
{
#include "confile.h"
}

extern BOOL g_bAutoProcAssert;

typedef void *(*PTHREAD_START_ROUTINE) (void *);

CNETComm2::CNETComm2()
{
    m_pObserver = NULL;
    m_fdNet = -1;
    m_bConnected = false;
    m_bUseMempool = true;
    m_uiLogLevel = INVALID_VALUE;
    m_hRecvThread = NULL;
    m_bExitThread = FALSE;
    memset(m_szLogName,0,sizeof(m_szLogName));

    memset(&m_tNET_Diag, 0, sizeof(NET_CONFIG));
    memset(&m_tNET_Smp, 0, sizeof(NET_CONFIG));
}

CNETComm2::~CNETComm2()
{
    Close();
}
bool CNETComm2::InitLog( char * pszLogName,
                 uint32_t uiLogLevel)
{
    if(m_bConnected)
	{
		return false;
	}

	if(pszLogName != NULL && strlen(pszLogName)>= _MAX_PATH)
	{
		m_dwErrorCode = CH_E_INVALIDARG;
		return false;
	}

	if(pszLogName != NULL && strlen(pszLogName) != 0)
	{
		strcpy(m_szLogName, pszLogName);
	}

	m_uiLogLevel = uiLogLevel;

	m_dwErrorCode = CH_S_OK;

	return true;
}

bool CNETComm2::Open( PCCHANNEL_ATTRIBUTE pOpenArgument, bool bWaitOpen/*=false*/ )
{
	if(NULL == pOpenArgument|| CHANNEL_TYPE_TTY != pOpenArgument->ChannelType  )
	{
		m_dwErrorCode = CH_E_INVALIDARG;
		return false;
	}

	m_dwErrorCode = CH_S_OK;

	// first load configure
	// mainly for user can set not to create log folder and files.
	if(!LoadConfig())
	{
		m_dwErrorCode = CH_E_LOAD_CONFIG_FAILED;
		return false;
	}

    uint32_t dwPort = 0;
	char *pDevPath  = NULL;

	if (NULL == pOpenArgument->tty.pDevPath)
	{
        switch(pOpenArgument->tty.nProtocolType)
        {
        case 0: // Diag
            {
                dwPort   =  m_tNET_Diag.dwPortNum;
                pDevPath =  m_tNET_Diag.szDevPath;
            }
            break;
        case 1: // Smp
            {
                dwPort   =  m_tNET_Smp.dwPortNum;
                pDevPath =  m_tNET_Smp.szDevPath;
            }
            break;
        case 2: // u2s
            {
                dwPort   =  m_tNET_Smp.dwPortNum;
                pDevPath =  m_tNET_Smp.szReserved;
            }
            break;
        default:
            break;
        }
	}
//	else
//	{
//        dwPort   =  pOpenArgument->Socket.dwPort;
//        pDevPath =  pOpenArgument->Socket.dwIP;
//	}

	assert( dwPort > 0 );


    if(dwPort == 0 || pDevPath == NULL) //lint !e774
	{
		m_dwErrorCode = CH_E_INVALIDARG;
		return false;
	}

	// second open log
	if(!OpenLogFile( dwPort, pDevPath ))
	{
		m_dwErrorCode = CH_E_OPEN_LOG_FAILED;
	    return false;
	}

	/* Init memory pool */
	m_MemMgr.Init(m_bUseMempool);

	/* opening the file */
	int fd = -1;
    int iRlt = 0;
	if(m_pObserver)
	{
	    /* user wants event driven reading */
#if 0
	    /*here to wait a moment for really dev checking starting*/
	    printf("Please Press the power key and volume down key to start downloading.");
	    printf("Please input S to notice the programme that you are ready.");
	    char c =' ';
	    do{
	    c = getchar();
	    }while(c != 's' );
#endif        
        if (bWaitOpen)
        {
            usleep(1 * 1000);

            while(fd == -1)
            {                
                iRlt = ConnectNet(fd, pDevPath, dwPort);
                if (fd == -1 || iRlt == 0)
                {
                    printf("\n[Info]: specified device \"%s\" is not available, please wait...\n", pDevPath);
                    usleep(10 * 1000);
                }
            }
        }
        else
        {
            iRlt = ConnectNet(fd, pDevPath, dwPort);
        }
	}
//	else
//	{
	    /* the read/write operations will be bloking */
//	    fd = open(pDevPath,O_RDWR|O_NOCTTY);
//	}

	/* oops, cannot open */
    if (fd == -1 || 0 == iRlt) {
		printf("\n[Error]: can not open device \"%s\", error code: 0x%x,\"%s\"\n",pDevPath,errno,strerror(errno) );
        m_log.LogFmtStr(SPLOGLV_ERROR,"Open: can not connect net \"%s\", error code: %d,\"%s\"",pDevPath,errno,strerror(errno));
		CloseLogFile();
		return false;
	}

	if(-1 == flock(fd, LOCK_EX|LOCK_NB))
	{
		m_log.LogFmtStr(SPLOGLV_ERROR,"device \"%s\" has been opened by other process,error code: %d,\"%s\".",pDevPath,errno,strerror(errno));
		CloseLogFile();
		close(fd);
		return false;
	}

	m_log.LogFmtStr(SPLOGLV_INFO,"Open: open channel \"%s\" success.",pDevPath);

	if( ioctl(fd,TIOCEXCL) == -1)
	{
		m_log.LogFmtStr(SPLOGLV_ERROR,"Open: set TIOCEXCL failed,error code: %d,\"%s\".",errno,strerror(errno));
		CloseLogFile();
		flock(fd, LOCK_UN);
        DisconnectNet(fd);
		return false;
	}

	/* we remember old termios */
//	if(tcgetattr(fd,&(m_tioOld)) == -1)
//	{
//		m_log.LogFmtStr(SPLOGLV_ERROR,"Open: tcgetattr failed,error code: %d,\"%s\".",errno,strerror(errno));
//		CloseLogFile();
//		flock(fd, LOCK_UN);
//		close(fd);
//		return false;
//	}


    m_fdNet = fd;
	m_bConnected = true;
	if(m_pObserver)
	{
		if( !CreateRecvThread())
		{
		    m_log.LogRawStr(SPLOGLV_ERROR,"Open: CreateRecvThread failed.");
		    Close();
		    return false;
		}
	}

	return TRUE;
}

void CNETComm2::Close()
{
	/* first we flush the port */
	Clear();

	DestroyRecvThread();

    DisconnectNet(m_fdNet);
    m_fdNet = -1;
	m_bConnected = false;

	m_log.LogRawStr(SPLOGLV_INFO,"Close tty channel.");
	CloseLogFile();
}

bool CNETComm2::Clear()
{
    timeval time_out;
    time_out.tv_sec = 0;
    time_out.tv_usec = 0;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(m_fdNet, &read_fds);

    int res  = -1;
    char recv_data[2];
    memset(recv_data, 0, 2);
    while(true) {
        res = select(FD_SETSIZE, &read_fds, nullptr, nullptr, &time_out);
        if (res == 0) break;
        recv(m_fdNet, recv_data, 1, 0);
    }

    return true;
}

bool CNETComm2::Drain()
{
    if(m_fdNet != -1)
    {
        tcdrain(m_fdNet);
    }

    return true;
}

uint32_t CNETComm2::Read( uint8_t* lpData,
                uint32_t ulDataSize,
                uint32_t dwTimeOut,
                uint32_t dwReserved /*= 0*/ )
{
    if (m_fdNet == -1 && m_bConnected == false) {
        m_log.LogRawStr(SPLOGLV_WARN,"Read: m_hNET is invalid.");
        return 0;
    }

    if(lpData == NULL || ulDataSize == 0)
	{
		m_log.LogRawStr(SPLOGLV_WARN,"Write: parameters are invalid.");
		return 0;
	}

    uint32_t uRead =  read(m_fdNet, lpData, ulDataSize);
    if(uRead == (uint32_t)(-1))
    {
        m_log.LogFmtStr(SPLOGLV_ERROR,"Read failed, error code: %d,\"%s\"",errno,strerror(errno));
        uRead = 0;
    }

    m_log.LogBufData(SPLOGLV_DATA,lpData,uRead,LOG_READ,&ulDataSize);

    return uRead;
}

uint32_t CNETComm2::Write(uint8_t* lpData,
                uint32_t ulDataSize,
                uint32_t dwReserved /*= 0 */ )
{
    m_log.LogRawStr(SPLOGLV_INFO,"Write: +++");

    if (m_fdNet == -1) {
        m_log.LogRawStr(SPLOGLV_WARN,"Write: m_fdNET is invalid.");
        m_log.LogRawStr(SPLOGLV_INFO,"Write: ---");
        return 0;
    }

    if(lpData == NULL || ulDataSize == 0)
	{
		m_log.LogRawStr(SPLOGLV_WARN,"Write: parameters are invalid.");
		m_log.LogRawStr(SPLOGLV_INFO,"Write: ---");
		return 0;
	}

	uint32_t dwMaxLength = ulDataSize;
	uint32_t dwLeft = ulDataSize;
	uint32_t dwRealWritten = 0;
	int32_t dwSize = 0;

	while(dwLeft)
	{
	    if(dwLeft > dwMaxLength)
	    {
	        dwSize = dwMaxLength;
	    }
	    else
	    {
	        dwSize = dwLeft;
	    }
        dwSize = write(m_fdNet, lpData+dwRealWritten, dwSize);
        if(dwSize <= 0)
        {
            if(errno==EINTR) /* 中断错误 我们继续写*/
            {
                m_log.LogFmtStr(SPLOGLV_ERROR,"Write failed, errno=EINTR continue.");
                continue;
            }
            else if(errno==EAGAIN) /* EAGAIN : Resource temporarily unavailable*/
            {
                m_log.LogFmtStr(SPLOGLV_ERROR,"Write failed, errno=EAGAIN continue.");
                usleep(100 * 1000);//等待100ms，希望发送缓冲区能得到释放
                continue;
            }
            else /* 其他错误 没有办法,只好退了*/
            {
               m_log.LogFmtStr(SPLOGLV_ERROR,"Write failed, error code: %d,\"%s\".",errno,strerror(errno));
               break;
            }
        }

        dwRealWritten += dwSize;
        dwLeft -= dwSize;
        //tcdrain(m_fdTTY);
	}

    m_log.LogBufData(SPLOGLV_DATA,lpData,dwRealWritten,LOG_WRITE,&ulDataSize);
    m_log.LogRawStr(SPLOGLV_INFO,"Write: ---");
    return dwRealWritten;
}

void CNETComm2::FreeMem( void* pMemBlock )
{
    m_MemMgr.FreeMemory(pMemBlock);
}

bool CNETComm2::GetProperty( int32_t lFlags,
                       uint32_t dwPropertyID,
                       void* pValue )
{
    return true;
}

bool CNETComm2::SetProperty( int32_t lFlags,
                       uint32_t dwPropertyID,
                       void* pValue )
{
    return true;
}

bool CNETComm2::SetObserver( IProtocolObserver * lpObserver )
{
    m_pObserver = lpObserver;

    return true;
}

bool CNETComm2::OpenLogFile( int32_t dwPort , char * pDevPath)
{
    CloseLogFile();

    char szLogFileName[ _MAX_PATH ] = {0};
	if(strlen(m_szLogName)==0 ) // not initialized, so set default value
	{
		sprintf( szLogFileName, "TTYComm_%s_ID%d", PathFindFileName(pDevPath), dwPort);
		strcpy(m_szLogName, szLogFileName);
	}

    if( m_log.Open(m_szLogName,m_uiLogLevel))
    {
        //GetModuleFileName( NULL,szLogFileName, MAX_PATH );
        GetExePath helper;
        std::string strDir = helper.getExeDir();
        std::string strName = helper.getExeName();

        m_log.LogFmtStr(SPLOGLV_ERROR,"===%s%s", strDir.c_str(), strName.c_str() );
        return true;
    }
    return false;
}

void CNETComm2::CloseLogFile()
{
    m_log.Close();
}

bool CNETComm2::LoadConfig()
{
    INI_CONFIG *config = NULL;

    GetExePath helper;
    std::string strIniPath = helper.getExeDir();
    strIniPath.insert(0,"/");
    strIniPath += "Channel.ini";

    m_log.LogFmtStr(SPLOGLV_ERROR,"IniFile Path \"%s\"", strIniPath.c_str());

    config = ini_config_create_from_file(strIniPath.c_str(),0);
    if(m_uiLogLevel == INVALID_VALUE)
    {
        m_uiLogLevel = 0;
        if(config)
        {
            m_uiLogLevel = ini_config_get_int(config, "Log", "Level", 0);
        }
    }

    // get diag ini config
    m_tNET_Diag.dwPortNum  = ini_config_get_int(config, "DIAG", "NetPort", 8080);
    strcpy(m_tNET_Diag.szDevPath, ini_config_get_string(config, "DIAG", "NetIP", "127.0.0.1"));

    // get smp ini config
    m_tNET_Smp.dwPortNum  = ini_config_get_int(config, "SMP", "NetPort", 8080);
    strcpy(m_tNET_Smp.szDevPath, ini_config_get_string(config, "SMP", "NetIP", "127.0.0.1"));
    strcpy(m_tNET_Smp.szReserved, ini_config_get_string(config, "SMP", "NetIP_Dump_Mode", "127.0.0.1"));

    // auto dump assert flag
    g_bAutoProcAssert = ini_config_get_int(config,"ASSERT","AutoDump",1);

    ini_config_destroy(config);

    //printf("SMP = %s, %s\n", m_tNET_Smp.szReserved, m_tNET_Smp.dwPortNum);       //*******Test

    return true;
}



void CNETComm2::_Read()
{
	uint32_t nRealSize = 0;
	uint8_t *pBuf = (uint8_t *)m_MemMgr.GetMemory(1024,&nRealSize);

	if(nRealSize == 0 || pBuf == NULL)
	{
	    m_log.LogFmtStr(SPLOGLV_ERROR,"CTTYComm2::_Read GetMemory fail.");
	    return;
	}
	bool bContinue = false;
	uint32_t uRead =  0;

	do
	{		
        uRead =  recv(m_fdNet, (void*)pBuf, nRealSize, 0);

		bContinue = false;
		m_log.LogFmtStr(SPLOGLV_INFO,"Read= %d,Readed= %d",nRealSize,uRead);
		if(uRead == (uint32_t)(-1))
		{
		     if(errno==EINTR)
	            {
	                m_log.LogFmtStr(SPLOGLV_ERROR,"Read failed, errno=EINTR continue.");
	                bContinue = true;
	            }
	            else if(errno==EAGAIN) /* EAGAIN : Resource temporarily unavailable*/
	            {
	                m_log.LogFmtStr(SPLOGLV_ERROR,"Read failed, errno=EAGAIN continue.");
                    usleep(100 * 1000);
	                bContinue = true;
	            }
		     else
		     {
		        m_log.LogFmtStr(SPLOGLV_ERROR,"Read failed, error code: %d,\"%s\"",errno,strerror(errno));
		     	 uRead = 0;
		     }

		}

	}while(bContinue);

	m_log.LogBufData(SPLOGLV_DATA,pBuf,uRead,LOG_ASYNC_READ,NULL);

	/* Execute callback */
	if ((uRead>0)&&(m_pObserver))
	{
		m_pObserver->OnChannelData(pBuf,uRead,m_dwPort);
	}
	
	m_MemMgr.FreeMemory(pBuf);
	return;
}

void* CNETComm2::GetRecvFunc(void* lpParam)
{
    CNETComm2 * p = (CNETComm2 *)lpParam;
	return p->RecvFunc();
}

void* CNETComm2::RecvFunc()
{
    int fs_sel=0;
    fd_set fs_read;
    struct timeval tv_timeout;
    tv_timeout.tv_sec = 0; //time out : unit sec
    tv_timeout.tv_usec = 0;

    while( !m_bExitThread && m_fdNet != -1)
    {
        FD_ZERO(&fs_read);
        FD_SET(m_fdNet, &fs_read);
        fs_sel = select( m_fdNet, &fs_read, NULL, NULL, &tv_timeout);

        if(fs_sel>=0 &&  FD_ISSET(m_fdNet, &fs_read))
        {
            _Read();
        }
        usleep(1);
    }
}

bool CNETComm2::CreateRecvThread()
{
    m_bExitThread = FALSE;
    int nRet = pthread_create(&m_hRecvThread,
                              NULL,
                              (PTHREAD_START_ROUTINE)GetRecvFunc,
                              this);

    if(nRet==-1)
    {
        m_log.LogRawStr(SPLOGLV_ERROR, _T("Create boot mode thread failed."));
        m_bExitThread = TRUE;
        return FALSE;
    }

    return TRUE;
}

void CNETComm2::DestroyRecvThread()
{
    if(m_hRecvThread != NULL )
    {
        m_bExitThread = TRUE;

        pthread_join(m_hRecvThread,NULL);

        m_hRecvThread = NULL;
    }
}

//关闭socket连接
void CNETComm2::DisconnectNet(int fdSocket)
{
    if (-1 != fdSocket)
    {
        shutdown(fdSocket, 2);
    }
}

//网络连接
int CNETComm2::ConnectNet(int& fdSocket, char* szIpAds, uint32_t unIpPort)
{
    int ret = 0;

    struct sockaddr_in netadr;
    bzero(&netadr, sizeof(netadr));

    if (-1 == fdSocket)
        fdSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (fdSocket != -1)
    {
        netadr.sin_family = AF_INET;
        netadr.sin_addr.s_addr = inet_addr(szIpAds);
        netadr.sin_port = htons(unIpPort);

        if (connect(fdSocket, (struct sockaddr *)&netadr, sizeof(netadr)) != -1)
        {
            ret = 1;
        }
    }

    return ret;
}

bool CNETComm2::IsConnect()
{
    return true;
}

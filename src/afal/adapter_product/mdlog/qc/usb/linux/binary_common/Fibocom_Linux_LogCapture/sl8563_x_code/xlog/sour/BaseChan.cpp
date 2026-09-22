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

#include "BaseChan.h"
#include "malloc.h"
#include <unistd.h>

#include "ExePathHelper.h"

//#define RCV_DATA_MSG WM_USER + 333
//#define DEFAULT_WAIT_CLOSE 1000

extern void Free_PRT_BUFF( void* lpBuff );
extern PRT_BUFF* Alloc_PRT_BUFF( unsigned int size );

CBaseChannel::CBaseChannel()
{
    //判断是串口通讯还是网口通讯
    int nCommType = GetCommuntType();
    //printf("GetCommuntType = %d\n", nCommType);       //*******Test
    switch (nCommType)
    {
    case COMM_COM:
        //串口通讯
        m_LowerChannel = new CTTYComm2;
        break;
    case COMM_NET:
        //网口通讯
        m_LowerChannel = new CNETComm2;
        break;
    case COMM_ERR:
        break;
    case COMM_NOT:
        break;
    default:
        break;
    }

	m_lpProtocolPackage = NULL;

	m_bProcessThreadRun = false;

	pthread_mutex_init(&m_csObserver, NULL);

	m_nLoLvlEndian = PP_LITTLE_ENDIAN;
	m_nHiLvlEndian = PP_LITTLE_ENDIAN;

	m_uLogLevel = SPLOGLV_NONE;
	//m_uLogType = CSpLog::defaultBinaryFlag;
	m_bInitLogCalled = false;
	memset(m_szLogName, 0, MAX_PATH);

	m_hProcessThread = NULL;

	m_bEndianEvent = false;

	m_nInternalPackgeCap = 0;
	m_bClearKeepPkg = false;
	m_bConnected = false;

    //m_lpWrtCallback = NULL;
    //m_cbInput = 0;
}

CBaseChannel::~CBaseChannel()
{
	pthread_mutex_destroy(&m_csObserver);
	m_Log.Close();
}

BOOL CBaseChannel::InitLog( char * pszLogName,
                            uint32_t uiLogLevel/*=INVALID_VALUE*/)
{
	m_uLogLevel = uiLogLevel;
	m_bInitLogCalled = true;

	if (SPLOGLV_NONE == m_uLogLevel)
	{
        m_Log.Close(); // Close
	    return TRUE;
    }

	if( NULL != pszLogName )
	{
#ifdef _UNICODE
        wcscpy( m_szLogName, pszLogName );
#else
        strcpy( m_szLogName, pszLogName);
#endif
	}
	return m_Log.Open( m_szLogName,uiLogLevel );
}

BOOL CBaseChannel::SetReceiver( ULONG /*ulMsgId*/,
					            BOOL /*bRcvThread*/,
					            LPCVOID /*pReceiver*/ )
{
	return false;
}

void CBaseChannel::GetReceiver( ULONG  &/*ulMsgId*/,
							    BOOL &/*bRcvThread*/,
							    LPVOID &/*pReceiver*/ )
{
}

BOOL CBaseChannel::GetProperty( LONG lFlags, DWORD dwPropertyID,
							    LPVOID lpValue )
{
	if( NULL == lpValue )
	{
		return false;
	}

	switch( dwPropertyID )
	{
	case PPI_Endian:
		{
			unsigned int* lpData = (unsigned int*)lpValue;
			*lpData = m_nHiLvlEndian << 8 | m_nLoLvlEndian;
			return true;
		}
		break;
	case PPI_GetError:
		{
			// Return error code of IProtocolPackage object
			if( m_lpProtocolPackage )
			{
				return m_lpProtocolPackage->GetProperty( lFlags,dwPropertyID,lpValue );
			}
			else
			{
				return false;
			}
			break;
		}
	case PPI_INTERNAL_PACKAGE_CAPACITY:
		{
			*((unsigned int*)lpValue) = m_nInternalPackgeCap;
		}
		break;
	case PPI_CLEAR_KEEP_PACKAGE:
		{
			*((bool*)lpValue) = m_bClearKeepPkg;
			return true;
		}
		break;
	case PPI_CONNECT_STATE:
		{
			*((bool*)lpValue) = m_bConnected;
			return true;
		}
		break;
	default:
		break;
	}

	return false;
}

BOOL CBaseChannel::SetProperty( LONG lFlags, DWORD dwPropertyID,
							    LPCVOID pValue )
{
	switch( dwPropertyID )
	{
	case PPI_Endian:
		{
			// not support PP_UNKOWN_ENDIAN
			unsigned int data = *((unsigned int*)pValue);
			if( ((unsigned char)(data & 0xFF)) >= PP_UNKOWN_ENDIAN ||
				((unsigned char)( data >> 8)) >= PP_UNKOWN_ENDIAN)
			{
				return FALSE;
			}
			m_nLoLvlEndian = (unsigned char)(data & 0xFF);
			m_nHiLvlEndian = (unsigned char)( data >> 8 );
			if( m_lpProtocolPackage )
			{
				m_lpProtocolPackage->SetProperty( lFlags,dwPropertyID,pValue );
				m_Log.LogFmtStr( SPLOGLV_INFO,_T( "Endian is set,Low level endian is %d,high level endian is %d." ),m_nLoLvlEndian,m_nHiLvlEndian );
			}
			return TRUE;
		}
		break;
	case PPI_WATCH_DEVICE_CHANGE:
		{
			bool bWatch = *((unsigned int*)pValue) == 0 ? false : true;
			if(m_bConnected)
			{
                m_LowerChannel->SetProperty(lFlags,CH_PROP_WATCH_DEV_CHANGE,&bWatch);

				m_Log.LogFmtStr( SPLOGLV_INFO,_T("Device change flag is set to %d." ),bWatch );
			}
			return true;
		}
		break;
	case PPI_INTERNAL_PACKAGE_CAPACITY:
		{
			m_nInternalPackgeCap = *((unsigned int*)pValue);
			m_Log.LogFmtStr( SPLOGLV_INFO,_T("async package capacity is set to %d." ),m_nInternalPackgeCap );
			return true;
		}
		break;
	case PPI_CLEAR_KEEP_PACKAGE:
		{
			m_bClearKeepPkg = *((unsigned int*)pValue) == 0 ? false : true;
			m_Log.LogFmtStr( SPLOGLV_INFO,_T("Clear and keep package flag is set to %d." ),m_bClearKeepPkg );
			return true;
		}
		break;
    case PPI_WRITEDATA_CALLBACK:
        {
            return true;
        }
        break;
	default:
		break;
	}

	return false;
}

BOOL CBaseChannel::Open( PCCHANNEL_ATTRIBUTE pOpenArgument, BOOL bWaitOpen/*=FALSE*/ )
{
	if( !m_bInitLogCalled && m_uLogLevel != SPLOGLV_NONE)
	{
		// If InitLog is not called now,we will create log file with default
		// log parameter
		m_Log.Open( m_szLogName, m_uLogLevel );
	}

	m_Log.LogRawStr(SPLOGLV_INFO,_T("Open: +++"));

	// Check input parameter
	PCHANNEL_ATTRIBUTE _pOpenArgument;
	CHANNEL_ATTRIBUTE ca;
	memset(&ca, 0, sizeof(ca));

	if( NULL == pOpenArgument )
	{
        ca.ChannelType       = CHANNEL_TYPE_TTY;
        ca.tty.nProtocolType = GetProtocolType();
        _pOpenArgument       = &ca;
	}
	else
	{
        _pOpenArgument = pOpenArgument;
	}

    //  [5/16/2011 Xiaoping.Jing] [[[
    //  Temporary modification
    if (m_lpProtocolPackage)
    {
        m_lpProtocolPackage->Clear();
    }
    //  [5/16/2011 Xiaoping.Jing] ]]]

	// Create a communication channel by input channel type
    m_LowerChannel->SetObserver(this);
    if (!m_LowerChannel->Open(_pOpenArgument, bWaitOpen))
	{
		// Can not open lower channel
		m_Log.LogRawStr( SPLOGLV_ERROR,_T("Open: create channel failed") );
		m_Log.LogRawStr(SPLOGLV_INFO,_T("Open: ---"));
		return FALSE;
	}

	// Create data process thread
	if(!StartProcessThread())
	{
        m_LowerChannel->Close();

		m_Log.LogRawStr( SPLOGLV_ERROR,_T("Open: create process thread failed") );
		m_Log.LogRawStr(SPLOGLV_INFO,_T("Open: ---"));
		return FALSE;
	}

	m_bConnected = true;

	m_Log.LogRawStr(SPLOGLV_INFO,_T("Open: successed, ---"));

	return TRUE;
}

void CBaseChannel::Close()
{
	m_Log.LogRawStr( SPLOGLV_INFO,_T("Close: +++"));

	Clear();

	if (m_bConnected)
	{
        m_LowerChannel->Close();
	}

	StopProcessThread();

	m_bConnected = FALSE;
	m_Log.LogRawStr( SPLOGLV_INFO,_T("Close: ---"));

    if (!m_bInitLogCalled)
    {
	    m_Log.Close();
    }
}

BOOL CBaseChannel::Clear()
{
	if (m_bConnected)
	{
        m_Log.LogFmtStr( SPLOGLV_INFO,_T("Clear: clear channel data"));
        m_LowerChannel->Clear();
	}

	if(	m_lpProtocolPackage )
	{
		m_Log.LogFmtStr( SPLOGLV_INFO,_T("Clear: clear protocol package"));
		m_lpProtocolPackage->Clear();
	}

	return true;
}

void CBaseChannel::FreeMem( LPVOID pMemBlock )
{
	PRT_BUFF* lpBuff = (PRT_BUFF*)pMemBlock;
	if( lpBuff )
	{
		lpBuff->free_prt( lpBuff );
	}
}

DWORD CBaseChannel::Write( LPVOID lpData, DWORD dwDataSize,DWORD dwReserved )
{
    if (!m_bConnected)
    {
        return 0;
    }

    m_Log.LogRawStr( SPLOGLV_INFO,_T("Write: +++"));

    if( NULL == lpData || 0 == dwDataSize )
    {
        // No data need to write
        m_Log.LogRawStr( SPLOGLV_WARN,_T("Write: input data pointer is null or size is zero, nothing is write") );
        m_Log.LogRawStr( SPLOGLV_INFO,_T("Write: ---"));
        return 0;
    }

    PRT_WRITE_T* lpWriteData = (PRT_WRITE_T*)lpData;
    m_Log.LogFmtStr( SPLOGLV_INFO,_T("Write: action is %d"), lpWriteData->action );

    // Package data
    unsigned char* lpPkgData;
    int nSize = 0;

    // Derived class must assign m_lpProtocolPackage a valid value
    assert( m_lpProtocolPackage );
    unsigned int nRet = 0;
    if (0x08 != dwReserved)
        nRet = m_lpProtocolPackage->Package( lpWriteData->lpProtocolData,dwDataSize,(void**)&lpPkgData,&nSize );
    if( 0 != nRet )
    {
        m_Log.LogFmtStr( SPLOGLV_ERROR,_T("Write: package failed, error code is %d!"),nRet );
        m_Log.LogRawStr( SPLOGLV_INFO,_T("Write: ---"));
        return 0;
    }

    // // X 2016.5.6
    // if (NULL != m_lpWrtCallback)
    // {
    // m_lpWrtCallback((LPCVOID)lpPkgData, nSize, m_cbInput);
    // }    

    if (0x08 == dwReserved)
    {
        //m_lpProtocolPackage->FreeMem( lpPkgData );

        lpPkgData = new unsigned char[2];
        lpPkgData[0] = 0x74;
        lpPkgData[1] = 0x0a;
        nSize = 2;
        dwReserved = 0;
    }

    m_Log.LogBufData( SPLOGLV_DATA,lpPkgData,nSize );

    unsigned long dwRet = m_LowerChannel->Write( lpPkgData,nSize,dwReserved );
    if( dwRet > 0 )
    {
        // m_Log.LogHexData( lpPkgData,dwRet,LOG_WRITE );
    }
    m_Log.LogFmtStr( SPLOGLV_INFO,_T("Write: lower write is called, return value is %d"),dwRet );

    if (0x08 == dwReserved)
    {
        delete [] lpPkgData;
    }
    else
    {
        // Free memory
        m_lpProtocolPackage->FreeMem( lpPkgData );
    }

    m_Log.LogRawStr( SPLOGLV_INFO,_T("Write: ---"));
    return dwRet;
}

DWORD CBaseChannel::Read( void* lpData, DWORD dwDataSize,
                          DWORD dwTimeOut, DWORD dwReserved )
{
    /*
	m_Log.LogRawStr( SPLOGLV_INFO,_T("Read: +++"));

	if( NULL == lpData || 0 == dwDataSize )
	{
		m_Log.LogRawStr( SPLOGLV_WARN,_T("Read: input data pointer is null or size is zero, no space to contain read data") );
		m_Log.LogRawStr( SPLOGLV_INFO,_T("Read: ---"));
		return 0;
	}

	PRT_READ_T* lpRead = (PRT_READ_T*)lpData;

	m_Log.LogFmtStr( SPLOGLV_INFO,_T("Read: lpRead->nCond = %d"),  lpRead->nCond);
	m_Log.LogRawStr( SPLOGLV_INFO,_T("Read: ---"));
    */
	return 0;
}

int CBaseChannel::AddObserver( IProtocolObserver * lpObserver )
{
	m_Log.LogRawStr( SPLOGLV_INFO,_T("AddObserver: +++") );
	if( NULL == lpObserver )
	{
		m_Log.LogRawStr( SPLOGLV_WARN,_T("AddObserver: input observer pointer is NULL") );
		m_Log.LogRawStr( SPLOGLV_INFO,_T("AddObserver: ---") );
		return INVALID_OBSERVER_ID;
	}

	// Grant access of observer list
	LockObserverList( true );

	// Find the observer in observer list first
	int nId = FindObserver( lpObserver );

	if( INVALID_OBSERVER_ID == nId )
	{
		// The observer is not in the observer list
		for( unsigned int i=0;i<m_obsList.size();i++ )
		{
			if( NULL == m_obsList[i] )
			{
				// Find an empty place
				m_obsList[i] = lpObserver;
				nId = i;
				break; //Xiaoping.Jing
			}
		}

		if( INVALID_OBSERVER_ID == nId )
		{
			// The observer list is full
			m_obsList.push_back( lpObserver );
			nId = m_obsList.size() - 1;
		}
		m_Log.LogRawStr( SPLOGLV_INFO,_T("AddObserver: add observer to list"));
	}
	else
	{
		m_Log.LogRawStr( SPLOGLV_INFO,_T("AddObserver: observer is already in list"));
	}

	m_Log.LogFmtStr( SPLOGLV_INFO,_T("AddObserver: id = %d, ---"),nId);

	LockObserverList( false );

	return nId;
}

bool CBaseChannel::RemoveObserver( int nObserverId )
{
	m_Log.LogRawStr( SPLOGLV_INFO,_T("RemoveObserver: +++") );
	// Grant access of observer list and the given observer
	LockObserverList( true );

	bool bRet = false;

	// Check input id
	if( nObserverId < (int)m_obsList.size() && nObserverId >= 0 )
	{
		m_obsList[nObserverId] = NULL;
		bRet = true;
	}
	else
	{
		m_Log.LogRawStr( SPLOGLV_WARN,_T("RemoveObserver: Input observer id is invalid") );
	}

	LockObserverList( false );
	m_Log.LogRawStr( SPLOGLV_INFO,_T("RemoveObserver: ---") );
	// return the result
	return bRet;
}

IProtocolObserver* CBaseChannel::GetNextObserver( int& nPos )
{
	IProtocolObserver* lpObserver = NULL;
	nPos++;

	// Grant access of observer list and the given observer
	LockObserverList( true );

	if( nPos < (int)m_obsList.size() || nPos >= 0 )
	{
		for( ;nPos < (int)m_obsList.size();nPos++ )
		{
			if( NULL != m_obsList[nPos] )
			{
				lpObserver = m_obsList[nPos];
				break;
			}
		}
		if( NULL == lpObserver )
		{
			nPos = INVALID_OBSERVER_ID;
		}
	}
	else
	{
		nPos = INVALID_OBSERVER_ID;
	}

	LockObserverList( false );

	//m_Log.LogFmtStr( SPLOGLV_INFO,_T("Returned Observer id is %d"),nPos );

	return lpObserver;
}

int CBaseChannel::FindObserver( IProtocolObserver* lpObserver )
{
	for( unsigned int i=0;i<m_obsList.size();i++ )
	{
		if( m_obsList[i] == lpObserver )
		{
			return i;
		}
	}

	return INVALID_OBSERVER_ID;
}

int	CBaseChannel::OnChannelData( void* lpData, uint32_t ulDataSize,uint32_t reserved /*=0*/ )
{
	// Check parameter
	if( NULL == lpData || ulDataSize == 0)
	{
		return 0;
	}

	PRT_BUFF* lpBuff = Alloc_PRT_BUFF( ulDataSize );
    if (!lpBuff)
    {
		printf("CBaseChannel::OnChannelData - Alloc_PRT_BUFF failed.\n");
        return 0;
    }
    else
    {
        memcpy(lpBuff->lpData, (void*)lpData, ulDataSize );
    }

	m_Log.LogRawStr( SPLOGLV_INFO,_T("OnChannelData: +++"));

	//PRT_BUFF* lpBuff = (PRT_BUFF*)lpData;
	//m_Log.LogHexData( lpBuff->lpData,lpBuff->size,LOG_READ );

	// Decode
	// Derived class must assign m_lpProtocolPackage a valid value
	// assert( m_lpProtocolPackage );

	m_Log.LogFmtStr( SPLOGLV_INFO,_T("OnChannelData: %d bytes received"),lpBuff->size );
	m_Log.LogBufData( SPLOGLV_DATA,lpBuff->lpData,lpBuff->size,LOG_READ );
	int nPackage = m_lpProtocolPackage->Append( lpBuff->lpData,lpBuff->size );
	lpBuff->free_prt( lpBuff );

	// PACKAGE_INFO pi = {0};
	// m_lpProtocolPackage->GetProperty(0,PPI_GetError,&pi);

	// m_Log.LogFmtStr( SPLOGLV_INFO,_T("OnChannelData: %d packages received, [notail:%d,invlen:%d,crcerr:%d]"),
		             // nPackage,pi.bNoTail&0x1,pi.bInvalidLen &0x1,pi.bCrcError&0x1);

	/*
	if( PP_UNKOWN_ENDIAN == m_nLoLvlEndian && nPackage > 0 )
	{
		unsigned int nEndian = 0;
		if( m_lpProtocolPackage->GetProperty(NULL,PPI_Endian,&nEndian) )
		{
			m_nLoLvlEndian = (unsigned char)(nEndian & 0xFF);
		}
		// Generate an endian event
		m_Log.LogFmtStr( SPLOGLV_INFO,_T("OnChannelData: endian decided,value is %d"),m_nLoLvlEndian );
		m_bEndianEvent = true;
		WakeupOnPackages();
	}*/

	// Read packages
	// while( nPackage > 0 )
	// {
		// int nCount = m_lpProtocolPackage->GetPackages( pPackages,PACKAGE_LIST_SIZE );
		// if( 0 == nCount )
		// {
			// // Maybe data is cleared by another thread
			// break;
		// }

		// nPackage -= nCount;
		// // Add to internal package list
		// AddtoPackageList( pPackages,nCount,(0 == nPackage)?TRUE:FALSE);
	// }
	m_Log.LogRawStr( SPLOGLV_INFO,_T("OnChannelData: ---"));

	return 0;
}

int CBaseChannel::OnChannelEvent( uint32_t event, void* lpEventData )
{
	// Implementation here just call Observer's OnChannelEvent
	m_Log.LogFmtStr( SPLOGLV_WARN,_T("OnChannelEvent: Event %d happened"),event );

	PRT_BUFF* pPackage = (PRT_BUFF*)lpEventData;

	if( PP_EVENT_LITTLE_ENDIAN == event )
	{
		m_nLoLvlEndian = PP_LITTLE_ENDIAN;
	}
	else if( PP_EVENT_BIG_ENDIAN == event )
	{
		m_nLoLvlEndian = PP_BIG_ENDIAN;
	}
	else if(PP_EVENT_CHANNLE_DISCON == event)
	{
		m_bConnected = false;
	}
	int nPos = -1;
	IProtocolObserver* lpObserver = GetNextObserver(nPos);
	while( lpObserver )
	{
		if(pPackage != NULL)
		{
			//InterlockedIncrement( (long*)&pPackage->ref_count );
			//__sync_fetch_and_add( (long*)&pPackage->ref_count, 1 );
		}
		lpObserver->OnChannelEvent( event,pPackage );
		lpObserver = GetNextObserver( nPos );
	}

	if(pPackage != NULL)
	{
		pPackage->free_prt(pPackage);
	}

	return 0;
}

int CBaseChannel::GetProtocolType()
{
    return (int)PROTOCOL_TYPE_DIAG;
}

void* CBaseChannel::ProcessThread(void* lpParam)
{
	CBaseChannel * p = (CBaseChannel *)lpParam;
	return p->ProcessData();
}

void* CBaseChannel::ProcessData()
{
	m_Log.LogRawStr( SPLOGLV_INFO,_T("ProcessData: +++") );

#define PACKAGE_LIST_SIZE 8
	PRT_BUFF* pPackages[PACKAGE_LIST_SIZE];

  	// Wait for quit event and packages
	PRT_BUFF* lpBuff = NULL;
	while( m_bProcessThreadRun )
	{
		memset(pPackages, 0, sizeof(pPackages));

		assert( m_lpProtocolPackage );
		int nCount = m_lpProtocolPackage->GetPackages( pPackages, PACKAGE_LIST_SIZE );
		if (nCount > 0)
		{
			for (int i = 0;i < nCount;i++)
			{
				lpBuff = pPackages[i];
				if (NULL != lpBuff)
				{
					// Find a observer from the beginning of the observer list
					int nPos = -1;
					IProtocolObserver* lpObserver = GetNextObserver(nPos);
					while( lpObserver )
					{
						m_Log.LogFmtStr( SPLOGLV_INFO,_T("ProcessData: OnChannelData of Observer %d is called"),nPos );
						//InterlockedIncrement( (long*)&lpBuff->ref_count );
						//__sync_fetch_and_add( (long*)&lpBuff->ref_count,1);

						lpObserver->OnChannelData( lpBuff,0 );
						lpObserver = GetNextObserver( nPos );
					}

					// free package data
					lpBuff->free_prt( lpBuff );
				}
			}
		}
		else
		{
            usleep(300 * 1000);
			continue;
		}
	};

	m_Log.LogRawStr( SPLOGLV_INFO,_T("ProcessData: ---") );
}

BOOL CBaseChannel::StartProcessThread()
{
	m_bProcessThreadRun = true;
    int nRet = pthread_create(&m_hProcessThread,
                              NULL,
                              (PTHREAD_START_ROUTINE)ProcessThread,
                              this);

    if(nRet==-1)
    {
        m_Log.LogRawStr(SPLOGLV_ERROR, _T("Create process thread failed."));
        m_bProcessThreadRun = false;
        StopProcessThread();
		return FALSE;
    }

	return TRUE;
}

void CBaseChannel::StopProcessThread()
{
	m_Log.LogRawStr( SPLOGLV_INFO,_T("StopProcessThread...") );

	if(m_hProcessThread != NULL )
    {
        m_bProcessThreadRun = false;

        pthread_join(m_hProcessThread,NULL);

        m_hProcessThread = NULL;
    }
}

void CBaseChannel::LockObserverList( bool bLock )
{
	if( bLock )
	{
		pthread_mutex_lock(&m_csObserver);
	}
	else
	{
		pthread_mutex_unlock(&m_csObserver);
	}
}

//    ret 0  1-serial 2-net
int CBaseChannel::GetCommuntType()
{
    int ret = 0;
    int nDiagType = 0, nSmpType = 0;
    INI_CONFIG *config = NULL;
    GetExePath helper;
    std::string strIniPath = helper.getExeDir();

    strIniPath.insert(0,"/");
    strIniPath += "Channel.ini";

    config = ini_config_create_from_file(strIniPath.c_str(),0);

    m_nChannelType = 0;

    // get diag type

    if (GetSingleCommType(config, "DIAG","DevPath", COMM_COM))
    {
        m_nChannelType = 1;
       nDiagType = COMM_COM;
    }
    else if (GetSingleCommType(config, "DIAG","NetIP", COMM_NET))
    {
        m_nChannelType = 1;
        nDiagType = COMM_NET;
    }

    // get smp type
    if (GetSingleCommType(config, "SMP","DevPath", COMM_COM))
    {
        m_nChannelType = 1;
       nSmpType = COMM_COM;
    }
    else if (GetSingleCommType(config, "SMP","NetIP", COMM_NET))
    {
        m_nChannelType = 1;
        nSmpType = COMM_NET;
    }

    //get sys ap dump type
    if (GetSingleCommType(config, "SMP","DevPath_Dump_Mode", COMM_COM))
    {
        m_nChannelType = 2;
       nDiagType = nSmpType = COMM_COM;
    }
    else if (GetSingleCommType(config, "SMP", "NetIP_Dump_Mode", COMM_NET))
    {
        m_nChannelType = 2;
        nDiagType = nSmpType = COMM_NET;
    }


    ini_config_destroy(config);

    if (nDiagType == nSmpType)
    {
        ret = nDiagType;
    }
    else
    {
        ret = COMM_ERR;
    }

    return ret;
}

// single ret comm type
int CBaseChannel::GetSingleCommType(INI_CONFIG* config, const char* section, const char* key, int nType)
{
    int ret = 0;
    char sls[64];
    char sds[64];

    memset(sls, 0x00, 64);
    if (nType == COMM_COM)
    {
        strcpy(sds, "no_serial");
    }
    else if (nType == COMM_NET)
    {
       strcpy(sds, "no_network");
    }

    strcpy(sls, ini_config_get_string(config, section, key, sds));

    if (strcmp(sls, sds))
    {
        ret = nType;
    }

    return ret;
}

int CBaseChannel::GetChannelType()
{
    return m_nChannelType;
}

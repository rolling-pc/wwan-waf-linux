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

// DiagPackage.cpp: implementation of the CDiagPackage class.
//
//////////////////////////////////////////////////////////////////////

#include "DiagChan.h"
#include "DiagPackage.h"
#include "malloc.h"

#include <string.h>


#define HDLC_ESCAPE 0x7d

#define MSG_TYPE_ASSERT 255
#define DEFAULT_ASSERT_LEN 8

#define DIAG_MODE_TYPE 98
#define HDLC_DIAG_MODE  0
#define RAW_DATA_MODE	1

#define DIAG_MODE_SWITCH       0
#define DIAG_MODE_SWITCH_ACK  1

void Free_PRT_BUFF( void* lpBuff )
{
	if( NULL != lpBuff )
	{
		PRT_BUFF* lpData = (PRT_BUFF*)lpBuff;
// #ifdef _WIN32
		// if( 0 == InterlockedDecrement( (long*)&lpData->ref_count ) )
// #else
        // if( 0 == __sync_fetch_and_sub( (long*)&lpData->ref_count,1) )
// #endif
		{
            free( lpData );
		}
	}
}

PRT_BUFF* Alloc_PRT_BUFF( unsigned int size )
{
    PRT_BUFF* lpBuff = (PRT_BUFF*)malloc( sizeof( PRT_BUFF ) + size );
	if( NULL != lpBuff )
	{
		lpBuff->type = 0;
		lpBuff->ref_count = 1;
		lpBuff->size = size;
		lpBuff->lpData = (unsigned char*)(lpBuff + 1);
		lpBuff->free_prt = Free_PRT_BUFF;
	}

	return lpBuff;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDiagPackage::CDiagPackage()
{
	m_nLoLvlEndian = PP_LITTLE_ENDIAN;
	m_nHiLvlEndian = PP_LITTLE_ENDIAN;

	m_nDataSize = 0;
	m_Status = DPS_NoValidData;

#ifdef _WIN32
	InitializeCriticalSection( &m_Section );
#else
    pthread_mutex_init(&m_Section,NULL);
#endif

	m_nSequenceNumber = 0;

	m_PackageInfo.nTotalPackages = 0;
	m_PackageInfo.nValidPackages = 0;
	m_PackageInfo.bInvalidLen = 0;
	m_PackageInfo.bNoTail = 0;
	m_PackageInfo.bCrcError = 0;
	m_PackageInfo.Reserved = 0;

	m_nPackageType = DIAG_TYPE_NORMAL;

	m_lpPkg = NULL;
	m_nMaxSn = SPECIAL_SN;

	//@hongliang.xin 2011-6-27
	m_nMinPackageSize = sizeof( DIAG_HEADER );

	//apple add for supporting raw data mode
	m_bSentDataModeSwitchReq = false;
	m_bRawDataMode = false;
	m_bRawDataReq = false;

	m_nProtocol = PT_DIAG;
}

CDiagPackage::~CDiagPackage()
{
#ifdef _WIN32
	DeleteCriticalSection( &m_Section );
#else
    pthread_mutex_destroy(&m_Section);
#endif
}

unsigned int CDiagPackage::Append( void* lpBuffer,unsigned int uSize )
{
	if( NULL == lpBuffer || 0 == uSize )
	{
		return m_Packages.size();
	}
	memset( &m_PackageInfo,0,sizeof( m_PackageInfo ) );
	//apple add for supporting raw data mode
	if( m_bRawDataMode )
	{
		// When DIAG in raw data mode all DAIG message will be ignored by PHONE except DIAG_MODE_SWITCH and ASSERT message.
		//PC need analysis data after this switch command. So we think over in the cases if the raw data including DIAG_MODE_SWITCH ACK coincidently.
		if( !m_bSentDataModeSwitchReq )
		{
			return RawDataAppend( lpBuffer,uSize );
		}
	}
	if( DIAG_TYPE_NO_HEADER == m_nPackageType )
	{
		return NoHeaderAppend( lpBuffer,uSize );
	}
	if( DIAG_TYPE_NO_ESCAPE == m_nPackageType )
	{
		return NoEscapeAppend( lpBuffer,uSize );
	}
	LockPakcageList( true );
	//unsigned int uPos = 0;
	unsigned char* lpBuff = (unsigned char*)lpBuffer;
	unsigned char* lpBuffEnd = lpBuff + uSize;
	unsigned char* lpBuffPackageStart = lpBuff;
	while( lpBuff < lpBuffEnd )
	{
		switch( m_Status )
		{
		case DPS_NoValidData:
			{
				// No invalidate data now,find 0x7E in the lpbuffer
				while( lpBuff < lpBuffEnd )
				{
					if( HDLC_HEADER == *lpBuff++ )
					{
						m_Status = DPS_Header;
						//apple add for supporting raw data mode
						lpBuffPackageStart = lpBuff ;
						break;
					}
				}
				break;
			}
		case DPS_Header:
			{
				// ignore continuous 0x7e
				while( lpBuff < lpBuffEnd )
				{
					if( HDLC_HEADER != *lpBuff )
					{
						m_Status = DPS_Unpacking;
						break;
					}
					lpBuff++;
				}
				break;
			}
		case DPS_Unpacking:
			{
				// Check data one by one till another 0x7e is found
				while( lpBuff < lpBuffEnd )
				{
					if( *lpBuff == HDLC_ESCAPE )
					{
						lpBuff++;
						// the escape char
						if( lpBuff == lpBuffEnd )
						{
							// The last character in buffer
							// Record the status and quit
							m_Status = DPS_Escape;
							break;
						}
						else
						{
							m_RingBuf[m_nDataSize++] = (unsigned char)(*lpBuff ^ 0x20);
							lpBuff++;
						}
					}
					else if( *lpBuff == HDLC_HEADER )
					{
						// Check the package
						// If the data size is less than size of the DIAG_HEADER,
						// discard this package
                        if (m_nDataSize >= m_nMinPackageSize )
                        {
							// If this is not a valid package,remain the last 0x7E
							// in the buffer
							if( AddPackage() )
							{
								lpBuff++;
								// apple add for supporting raw data mode
								// When DIAG in raw data mode all DAIG message will be ignored by PHONE except DIAG_MODE_SWITCH and ASSERT message.
								// PC need analysis data after this switch command. So we think over in the cases if the raw data including DIAG_MODE_SWITCH ACK coincidently.
								if( m_bSentDataModeSwitchReq  )
								{
									bool bOldDataMode = m_bRawDataMode;
									if( CheckIsDataModeSwitch() )
									{
										if( bOldDataMode ) //如果之前是raw mode,反馈包前面的数据都是raw data
										{
											int nSize = lpBuffPackageStart - (unsigned char*)lpBuffer -1 ;
											if( nSize > 0  )
											{
												RawDataAppend( lpBuffer , nSize );
											}
										}
										m_bSentDataModeSwitchReq = false;
										if( m_bRawDataMode ) //后面的数据都是raw data
										{
											LockPakcageList( false );
											return RawDataAppend( lpBuff ,lpBuffEnd - lpBuff );
										}
									}
								}
								else if(m_nProtocol == PT_DIAG)
								{
									// For DIAG remain the last 0x7E always
									m_nDataSize = 0;
									m_Status = DPS_Header;
									break;
								}
							}
							else
							{
								// The following code is before
								//if( lpBuff + 1 == lpBuffEnd )
								//{
								//	m_Status = DPS_Header;
								//	lpBuff++;
								//	break;
								//}
                                // end

								//The following code is fix code by liang.zhao 2013-8-8
								m_nDataSize = 0;
								m_Status = DPS_Header;
								lpBuff++;
								break;
								// end
							}
						}
						// Reset internal status
						m_nDataSize = 0;
						m_Status = DPS_NoValidData;
						break;
					}
					else
					{
						m_RingBuf[m_nDataSize++] = *lpBuff;
						lpBuff++;
					}
					if( m_nDataSize == MAX_DIAG_PKG_SIZE )
					{
						// The packet is too long
						m_nDataSize = 0;
						m_Status = DPS_NoValidData;
						m_PackageInfo.bNoTail = 1;
						break;
					}
				}
				break;
			}
		case DPS_Escape:
			{
				m_RingBuf[m_nDataSize++] = (unsigned char)(*lpBuff ^ 0x20);
				lpBuff++;

				if( m_nDataSize == MAX_DIAG_PKG_SIZE )
				{
					// The packet is too long
					m_nDataSize = 0;
					m_Status = DPS_NoValidData;
					m_PackageInfo.bNoTail = 1;
				}
				else
				{
					m_Status = DPS_Unpacking;
				}
				break;
			}
		}
	}
	LockPakcageList( false );
	//apple add for supporting raw data mode
	// When DIAG in raw data mode all DAIG message will be ignored by PHONE except DIAG_MODE_SWITCH and ASSERT message.
	//PC need analysis data after this switch command. So we think over in the cases if the raw data including DIAG_MODE_SWITCH ACK coincidently.
	if( m_bRawDataMode )
	{
		//说明没有找到datamodeswitch的反馈包
		if( m_bSentDataModeSwitchReq )
		{
			return RawDataAppend( lpBuffer,uSize );
		}
	}
	if( m_nDataSize )
	{
		// If there are some data in the internal buffer, it means a incomplete package received
		// We just save the situation here, the caller will decide it is a error or not
		m_PackageInfo.bNoTail = 1;
	}
	return m_Packages.size();
}

unsigned int CDiagPackage::NoHeaderAppend( void* lpBuffer,unsigned int uSize )
{
	/*
	if( PP_UNKOWN_ENDIAN == m_nLoLvlEndian )
	{
		return m_Packages.size();
	}*/

	const int cSize = 6; // total size of sequence number and length field of DIAG_HEADER
	unsigned int nPos = 0;
	unsigned char* lpBuff = (unsigned char*)lpBuffer;
	while( uSize )
	{
		if( NULL == m_lpPkg )
		{
			// Package length is unknown yet
			if( m_nDataSize + uSize < cSize )
			{
				// The input buffer did not contain a full length field
				memcpy( m_RingBuf+m_nDataSize,lpBuff,uSize );
				m_nDataSize += uSize;
				break;
			}
			else
			{
				// The input buffer contain a full length field
				nPos = cSize - m_nDataSize;
				DIAG_HEADER* lpHead;
				if( 0 == m_nDataSize )
				{
					// We will copy the buffer directly from input buffer to output buffer
					lpHead = (DIAG_HEADER*)lpBuff;
				}
				else
				{
					// the first part of package has been copied to m_RingBuf
					memcpy( m_RingBuf+m_nDataSize,lpBuffer,nPos );
					lpHead = (DIAG_HEADER*)m_RingBuf;
				}

				uSize -= nPos;
				lpBuff += nPos;
				m_nDataSize = cSize;

				if( PP_BIG_ENDIAN == m_nLoLvlEndian )
				{
					CONVERT_INT( lpHead->sn,lpHead->sn )
					CONVERT_SHORT( lpHead->len,lpHead->len )
				}
				if( lpHead->len < sizeof( DIAG_HEADER ) )
				{
					// The length is error,discard all remail buffer
					m_nDataSize = 0;
					m_PackageInfo.nTotalPackages++;
					m_PackageInfo.bInvalidLen = 1;
					break;
				}
				m_lpPkg = Alloc_PRT_BUFF( lpHead->len );
                if (m_lpPkg)
                {
				    memcpy( m_lpPkg->lpData,lpHead,cSize );
				    unsigned int nPkgRemain = lpHead->len - cSize;
				    if( uSize >= nPkgRemain )
				    {
					    // The buffer contain a full package
					    memcpy( m_lpPkg->lpData + cSize,lpBuff,nPkgRemain );
					    uSize -= nPkgRemain;
					    lpBuff += nPkgRemain;
					    m_nDataSize = 0;
					    m_Packages.push_back( m_lpPkg );
					    m_lpPkg = NULL;
					    m_PackageInfo.nTotalPackages++;
					    m_PackageInfo.nValidPackages++;
					    continue;
				    }
				    else
				    {
					    memcpy( m_lpPkg->lpData + cSize,lpBuff,uSize );
					    m_nDataSize += uSize;
					    break;
				    }
                }
                else
                {
                    m_nDataSize = 0;
                    break;
                }
			}
		}
		else
		{
			// The package length is known
			nPos = m_lpPkg->size - m_nDataSize;
			if( uSize >= nPos )
			{
				// This buffer contain all the remain data of current package
				memcpy( m_lpPkg->lpData + m_nDataSize,lpBuff,nPos );
				uSize -= nPos;
				lpBuff += nPos;
				m_nDataSize = 0;
				m_Packages.push_back( m_lpPkg );
				m_lpPkg = NULL;
				m_PackageInfo.nTotalPackages++;
				m_PackageInfo.nValidPackages++;
				continue;
			}
			else
			{
				// Input buffer only contain part of remain data of current package
				memcpy( m_lpPkg->lpData + m_nDataSize,lpBuff,uSize );
				m_nDataSize += uSize;
				break;
			}
		}
	}

	return m_Packages.size();
}

unsigned int CDiagPackage::NoEscapeAppend( void* lpBuffer,unsigned int uSize )
{
	/*
	if( PP_UNKOWN_ENDIAN == m_nLoLvlEndian )
	{
		return m_Packages.size();
	}*/

	const int cSize = 6; // total size of sequence number and length field of DIAG_HEADER and one 0x7E header
	unsigned int nPos = 0;
	unsigned char* lpBuff = (unsigned char*)lpBuffer;
	unsigned int i = 0;
	while( uSize )
	{
		if( NULL == m_lpPkg )
		{
			// Package length is unknown yet
			if( m_nDataSize + uSize < (cSize+1) )
			{
				// The input buffer did not contain a full length field
				memcpy( m_RingBuf+m_nDataSize,lpBuff,uSize );
				m_nDataSize += uSize;

				for(i=0; i< m_nDataSize; i++)
				{
					if(m_RingBuf[i] == HDLC_HEADER)
						break;
				}
				if(i>= m_nDataSize)
				{
					m_nDataSize = 0;
				}
				else
				{
					m_nDataSize -= i;
					memcpy(m_RingBuf,m_RingBuf+i,m_nDataSize);
				}
				break;
			}
			else
			{
				if(0 == m_nDataSize)
				{
					for(i=0; i< uSize; i++)
					{
						if(lpBuff[i] == HDLC_HEADER)
							break;
					}
					if(i>= uSize)
					{
						break;
					}
					else
					{
						uSize -= i;
						lpBuff += i;
					}
				}
				if( m_nDataSize + uSize < (cSize+1) )
				{
					// The input buffer did not contain a full length field
					memcpy( m_RingBuf+m_nDataSize,lpBuff,uSize );
					m_nDataSize += uSize;

					for(i=0; i< m_nDataSize; i++)
					{
						if(m_RingBuf[i] == HDLC_HEADER)
							break;
					}
					if(i>= m_nDataSize)
					{
						m_nDataSize = 0;
					}
					else
					{
						m_nDataSize -= i;
						memcpy(m_RingBuf,m_RingBuf+i,m_nDataSize);
					}
					break;
				}

				// The input buffer contain a full length field
				nPos = (cSize+1) - m_nDataSize;
				DIAG_HEADER* lpHead;
				if( 0 == m_nDataSize )
				{
					// We will copy the buffer directly from input buffer to output buffer
					lpHead = (DIAG_HEADER*)(lpBuff+1);
				}
				else
				{
					// the first part of package has been copied to m_RingBuf
					memcpy( m_RingBuf+m_nDataSize,lpBuff,nPos );
					lpHead = (DIAG_HEADER*)(m_RingBuf+1);
				}

				uSize -= nPos;
				lpBuff += nPos;
				m_nDataSize = cSize;

				if( PP_BIG_ENDIAN == m_nLoLvlEndian )
				{
					CONVERT_INT( lpHead->sn,lpHead->sn )
					CONVERT_SHORT( lpHead->len,lpHead->len )
				}
				if( lpHead->len < sizeof( DIAG_HEADER ) )
				{
					// The length is error,discard all remail buffer
					m_nDataSize = 0;
					m_PackageInfo.nTotalPackages++;
					m_PackageInfo.bInvalidLen = 1;
					break;
				}
				m_lpPkg = Alloc_PRT_BUFF( lpHead->len );
                if (m_lpPkg)
                {
				    memcpy( m_lpPkg->lpData,lpHead,cSize );
				    unsigned int nPkgRemain = lpHead->len - cSize+1;
				    if( uSize >= nPkgRemain )
				    {
					    // The buffer contain a full package
					    memcpy( m_lpPkg->lpData + cSize,lpBuff,nPkgRemain-1 );
					    uSize -= nPkgRemain;
					    lpBuff += nPkgRemain;
					    m_nDataSize = 0;
					    m_Packages.push_back( m_lpPkg );
					    m_lpPkg = NULL;
					    m_PackageInfo.nTotalPackages++;
					    m_PackageInfo.nValidPackages++;
					    continue;
				    }
				    else
				    {
					    memcpy( m_lpPkg->lpData + cSize,lpBuff,uSize );
					    m_nDataSize += uSize;
					    break;
				    }
                }
                else
                {   // no free memory to use, clear data                    
                    m_nDataSize = 0;
                    break;
                }
			}
		}
		else
		{
			// The package length is known
			nPos = ( m_lpPkg->size +1 ) - m_nDataSize;
			if( uSize >= nPos )
			{
				// This buffer contain all the remain data of current package
				memcpy( m_lpPkg->lpData + m_nDataSize,lpBuff,nPos-1 );
				uSize -= nPos;
				lpBuff += nPos;
				m_nDataSize = 0;
				m_Packages.push_back( m_lpPkg );
				m_lpPkg = NULL;
				m_PackageInfo.nTotalPackages++;
				m_PackageInfo.nValidPackages++;
				continue;
			}
			else
			{
				// Input buffer only contain part of remain data of current package
				memcpy( m_lpPkg->lpData + m_nDataSize,lpBuff,uSize );
				m_nDataSize += uSize;
				break;
			}
		}
	}

	return m_Packages.size();
}

void CDiagPackage::Clear()
{
	LockPakcageList( true );
	// Remove all packages
	m_Packages.erase( m_Packages.begin(),m_Packages.end() );
	// Remove all internal data
	m_nDataSize = 0;
	m_Status = DPS_NoValidData;
	m_PackageInfo.nTotalPackages = 0;
	m_PackageInfo.nValidPackages = 0;
	m_PackageInfo.bInvalidLen = 0;
	m_PackageInfo.bNoTail = 0;
	m_PackageInfo.bCrcError = 0;
	LockPakcageList( false );

	//apple add for supporting raw data mode
	m_bSentDataModeSwitchReq = false;
	m_bRawDataMode = false;
	m_bRawDataReq = false;
}

unsigned int CDiagPackage::GetPackages( void* lpPackage,int size )
{
	if( NULL == lpPackage || 0 >= size )
	{
		return 0;
	}

	PRT_BUFF** lpPkg = (PRT_BUFF**)lpPackage;

	LockPakcageList( true );
	int nCount = size < (int)m_Packages.size() ? size : (int)m_Packages.size();
	for( int i=0;i<nCount;i++ )
	{
		lpPkg[i] = m_Packages.front();
		m_Packages.pop_front();
	}
	LockPakcageList( false );

	return nCount;
}

void CDiagPackage::FreeMem( void* lpMemBlock )
{
    if (lpMemBlock != NULL)
        free( lpMemBlock );
}

bool CDiagPackage::GetProperty( long /*lFlags*/,unsigned int dwPropertyID,void* lpValue )
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
	case PPI_GetError:
		{
			memcpy( lpValue,&m_PackageInfo,sizeof( m_PackageInfo ) );
			return true;
		}
	case PPI_DIAG_Type:
		{
			int* lpType = (int*)lpValue;
			*lpType = m_nPackageType;
			return true;
		}
	default:
		break;
	}

	return false;
}

bool CDiagPackage::SetProperty( long /*lFlags*/,unsigned int dwPropertyID,const void* lpValue )
{
	switch( dwPropertyID )
	{
	case PPI_Endian:
		{
			// only support little or bigendian endian
			unsigned int data = *(unsigned int*)lpValue;
			if( ((unsigned char)(data & 0xFF)) >= PP_UNKOWN_ENDIAN ||
				((unsigned char)( data >> 8)) >= PP_UNKOWN_ENDIAN)
			{
				return false;
			}
			m_nLoLvlEndian = (unsigned char)(data & 0xFF);
			m_nHiLvlEndian = (unsigned char)( data >> 8 );

			return true;
		}
		break;
	case PPI_DIAG_Type:
		{
			if( m_nPackageType != *(int*)lpValue )
			{
				m_nPackageType = *(int*)lpValue;
				Clear();
			}
			return true;
		}
		break;
	case PPI_DIAG_MAX_SN:
		{
			m_nMaxSn = *(unsigned int*)lpValue;
			return true;
		}
		break;
	default:
		break;
	}
	return false;
}

void CDiagPackage::LockPakcageList( bool bLock )
{
#ifdef _WIN32
	if( bLock )
	{
		EnterCriticalSection( &m_Section );
	}
	else
	{
		LeaveCriticalSection( &m_Section );
	}
#else
	if( bLock )
	{
		pthread_mutex_lock(&m_Section);
	}
	else
	{
		pthread_mutex_unlock(&m_Section );
	}
#endif

}

unsigned int CDiagPackage::Package( void* lpInput,int nInputSize,void** lppMemBlock,int* lpSize )
{
	if( NULL == lppMemBlock || NULL == lpSize )
	{
		return DP_INVALID_PARAMETER;
	}

	if( NULL == lpInput || 0 == nInputSize )
	{
		*lppMemBlock = NULL;
		*lpSize = 0;
		return 0;
	}

	DIAG_PACKAGE* lpHeader = (DIAG_PACKAGE*)lpInput;

	if(lpHeader->header.len > 0 && lpHeader->data == NULL)
	{
		return DP_INVALID_PARAMETER;
	}

	/*
	if( PP_UNKOWN_ENDIAN == m_nLoLvlEndian )
	{
		if( lpHeader->header.type == 0 && DIAG_TYPE_NO_HEADER != m_nPackageType )
		{
			// In this situation, this command will be considered as a query endian
			// command,we call a special package function
			return PackageQueryEndianCmd( lpInput,nInputSize,lppMemBlock,lpSize );
		}
		else if( lpHeader->header.type != MSG_TYPE_ASSERT )
		{
			// Assert command is a acsii string,no endian problem
			return DP_INVALID_ENDIAN;
		}
	}
	*/

	//apple add for supporting raw data mode
	if( lpHeader->header.type == DIAG_MODE_TYPE && lpHeader->header.subtype == DIAG_MODE_SWITCH )
	{

		if( ((LPBYTE)lpHeader->data)[ 0 ] == RAW_DATA_MODE )
		{
			m_bRawDataReq = true;
		}
		else
		{
			m_bRawDataReq = false;
		}
		//if( ((LPBYTE)lpHeader->data)[ 0 ] == HDLC_DIAG_MODE )
		//{
		//	m_bRawDataReq = false;
		//}

		m_bSentDataModeSwitchReq = true;
	}

	// Alloc memory
	int nSize = 0;
	unsigned char* lpData = NULL;

	if( lpHeader->header.type == MSG_TYPE_ASSERT &&
		DIAG_TYPE_NO_HEADER != m_nPackageType &&
		DIAG_TYPE_NO_ESCAPE != m_nPackageType
	  )
	{
		// If this is a dump assert info command,and if the package will be send
		// to Phone directly,not to ChannelServer,just send the original data without DIAG header
		nSize = lpHeader->header.len;
        lpData = (unsigned char*)malloc( nSize );
		if( NULL == lpData )
		{
			return DP_NO_ENUOUPH_MEMORY;
		}
		memcpy( lpData,lpHeader->data,nSize );
		*lppMemBlock = lpData;
		*lpSize = nSize;
		return 0;
	}
	else
	{
		nSize = sizeof( DIAG_HEADER ) + lpHeader->header.len;
	}

	// Sequence number
	int nSn;
	if( SPECIAL_SN != lpHeader->header.sn )
	{
		// Use input sequence number
		nSn = lpHeader->header.sn;
	}
	else
	{
		// Use internal sequence number
		nSn = m_nSequenceNumber++;
		if( m_nSequenceNumber >= m_nMaxSn )
		{
			m_nSequenceNumber = 0;
		}
	}

	// Len
	short nLen = (short)nSize;

	// Endian convert
	if( PP_BIG_ENDIAN == m_nLoLvlEndian )
	{
		CONVERT_INT( nSn,nSn );
		CONVERT_SHORT( nLen,nLen );
	}

	if( DIAG_TYPE_NO_HEADER == m_nPackageType )
	{
        lpData = (unsigned char*)malloc( nSize );
		if( NULL == lpData )
		{
			return DP_NO_ENUOUPH_MEMORY;
		}

		memcpy( lpData,lpHeader,sizeof( DIAG_HEADER ) );
		DIAG_HEADER* lpDataHead = (DIAG_HEADER*)lpData;
		lpDataHead->sn = nSn;
		lpDataHead->len = nLen;
		if( lpHeader->header.len > 0 )
		{
			memcpy( lpData + sizeof( DIAG_HEADER ),lpHeader->data,lpHeader->header.len );
		}
	}
	else if( DIAG_TYPE_NO_ESCAPE == m_nPackageType )
	{
        lpData = (unsigned char*)malloc( nSize +2 );
		if( NULL == lpData )
		{
			return DP_NO_ENUOUPH_MEMORY;
		}

		lpData[0] = HDLC_HEADER;
		lpData[nSize+1] = HDLC_HEADER;
		memcpy( lpData+1,lpHeader,sizeof( DIAG_HEADER ) );
		DIAG_HEADER* lpDataHead = (DIAG_HEADER*)(lpData+1);
		lpDataHead->sn = nSn;
		lpDataHead->len = nLen;
		if( lpHeader->header.len > 0 )
		{
			memcpy( lpData + sizeof( DIAG_HEADER ) + 1 ,lpHeader->data,lpHeader->header.len );
		}

		nSize+=2;
	}
	else
	{
        lpData = (unsigned char*)malloc( nSize * 2 + 2 );
		if( NULL == lpData )
		{
			return DP_NO_ENUOUPH_MEMORY;
		}

		nSize = 0;

		// Package header
		lpData[nSize++] = HDLC_HEADER;

		// SN
		Copy( lpData,nSize,(unsigned char*)&nSn,sizeof( nSn ) );

		// LEN
		Copy( lpData,nSize,(unsigned char*)&nLen,sizeof(nLen) );

		// type
		Copy( lpData,nSize,(unsigned char*)&(lpHeader->header.type),1 );

		// Subtype
		Copy( lpData,nSize,(unsigned char*)&(lpHeader->header.subtype),1 );

		// Data
		if( lpHeader->header.len > 0 )
		{
			Copy( lpData,nSize,(unsigned char*)lpHeader->data,lpHeader->header.len );
		}

		// Packet tail
		lpData[nSize++] = HDLC_HEADER;
	}

	*lppMemBlock = lpData;
	*lpSize = nSize;

	return 0;
}

unsigned int CDiagPackage::PackageQueryEndianCmd( void* lpInput,int /*nInputSize*/,void** lppMemBlock,int* lpSize )
{
	DIAG_PACKAGE* lpHeader = (DIAG_PACKAGE*)lpInput;

	unsigned short nLen = (short)(sizeof( DIAG_HEADER ) + lpHeader->header.len);
	int nSn = lpHeader->header.sn;

    unsigned char* lpData = (unsigned char*)malloc( nLen * 4 + 4 );
	if( NULL == lpData )
	{
		return DP_NO_ENUOUPH_MEMORY;
	}


	// No Data field/ Package a little endian package
	int nSize = 0;

	// Package header
	lpData[nSize++] = HDLC_HEADER;

	// Copy to buffer
	Copy( lpData,nSize,(unsigned char*)&nSn,sizeof( nSn ) );

	// Copy to buffer
	Copy( lpData,nSize,(unsigned char*)&nLen,sizeof(nLen) );

	// type
	Copy( lpData,nSize,(unsigned char*)&(lpHeader->header.type),1 );

	// Subtype
	Copy( lpData,nSize,(unsigned char*)&(lpHeader->header.subtype),1 );


	// Packet tail
	lpData[nSize++] = HDLC_HEADER;

	// Package a big endian package
	CONVERT_INT( nSn,nSn );
	CONVERT_SHORT( nLen,nLen );

	// Package header
	lpData[nSize++] = HDLC_HEADER;

	// Copy to buffer
	Copy( lpData,nSize,(unsigned char*)&nSn,sizeof( nSn ) );

	// Copy to buffer
	Copy( lpData,nSize,(unsigned char*)&nLen,sizeof(nLen) );

	// type
	Copy( lpData,nSize,(unsigned char*)&(lpHeader->header.type),1 );

	// Subtype
	Copy( lpData,nSize,(unsigned char*)&(lpHeader->header.subtype),1 );

	// No Data field

	// Packet tail
	lpData[nSize++] = HDLC_HEADER;

	*lppMemBlock = lpData;
	*lpSize = nSize;

	return 0;
}

bool CDiagPackage::AddPackage()
{
	// Check package length and convert endian if needed
	bool bRet = CheckLength();
	if( bRet )
	{
		// The length is valid
		AddToPackageList();
		m_PackageInfo.nValidPackages++;
	}
	m_PackageInfo.nTotalPackages++;

	return bRet;
}

bool CDiagPackage::CheckLength()
{
	unsigned int   nDataSize = m_nDataSize;
	unsigned char* pRingBuf  = m_RingBuf;

	if(m_nProtocol == PT_SMP)
	{
		nDataSize = m_nDataSize - SMP_HDR_LEN;
		pRingBuf  = pRingBuf + SMP_HDR_LEN;
	}

	if( nDataSize < sizeof( DIAG_HEADER ) )
	{
		// The buffer length is too small,discard it
		return false;
	}

	DIAG_HEADER* lpHeader = (DIAG_HEADER*)pRingBuf;

	bool bRet = false;
	unsigned short len = 0;

	if( 0 == lpHeader->len )
	{
		// length is not filled by phone,we will fill it here
		/*
		if( PP_UNKOWN_ENDIAN == m_nLoLvlEndian )
		{
			m_PackageInfo.bInvalidLen = 1;
			return false;
		}*/
		len = (unsigned short)nDataSize;
		bRet = true;
	}
	else if( PP_LITTLE_ENDIAN == m_nLoLvlEndian )
	{
		len = lpHeader->len;
		if( len == nDataSize )
		{
			bRet = true;
		}
	}
	else if( PP_BIG_ENDIAN == m_nLoLvlEndian )
	{
		CONVERT_SHORT( lpHeader->len,len );
		if( len == nDataSize )
		{
			bRet = true;
		}
	}
	else
	{
		// not support endian selfadapt
		/*
		// Decide endian
		if( nDataSize == lpHeader->len )
		{
			m_nLoLvlEndian = PP_LITTLE_ENDIAN;
			len = lpHeader->len;
			bRet = true;
		}
		else
		{
			CONVERT_SHORT( lpHeader->len,len );
			if( nDataSize == len )
			{
				m_nLoLvlEndian = PP_BIG_ENDIAN;
				bRet = true;
			}
		}
		*/
	}

	if( !bRet )
	{
		// If it is an assert package,its length maybe is 8 or length - 8,not
		// the real package length.
		/*
		if( PP_UNKOWN_ENDIAN == m_nLoLvlEndian )
		{
			if( MSG_TYPE_ASSERT == lpHeader->type )
			{
				if( lpHeader->len == DEFAULT_ASSERT_LEN  ||
					lpHeader->len == (nDataSize - DEFAULT_ASSERT_LEN) )
				{
					m_nLoLvlEndian = PP_LITTLE_ENDIAN;
				}
				else if( len == DEFAULT_ASSERT_LEN ||
					     len == (nDataSize - DEFAULT_ASSERT_LEN ))
				{
					m_nLoLvlEndian = PP_BIG_ENDIAN;
				}
				else
				{
					m_PackageInfo.bInvalidLen = 1;
					return false;
				}
				len = (unsigned short)nDataSize;
			}
			else
			{
				m_PackageInfo.bInvalidLen = 1;
				return false;
			}
		}
		else
		*/
		{
			if( ( MSG_TYPE_ASSERT == lpHeader->type && ( DEFAULT_ASSERT_LEN == len || len == nDataSize - DEFAULT_ASSERT_LEN ) ) ||
				( 0xA == lpHeader->type ) || // Add a exception for command which type is 0xA
                ( 0x21 == lpHeader->type)   //  [5/27/2011 xiaoping.jing] DIAG_CALIBRATION_BT_F exception
			  )
			{
				len = (unsigned short)nDataSize;
			}
			else
			{
				m_PackageInfo.bInvalidLen = 1;
				return false;
			}
		}
	}

	if( PP_LITTLE_ENDIAN == m_nHiLvlEndian )
	{
		lpHeader->len = len;
	}
	else
	{
		CONVERT_SHORT( len,lpHeader->len );
	}

	if( m_nLoLvlEndian != m_nHiLvlEndian )
	{
		// Endian convert
		CONVERT_INT( lpHeader->sn,lpHeader->sn );
	}

	return true;
}

void CDiagPackage::AddToPackageList()
{
	PRT_BUFF* lpBuff = Alloc_PRT_BUFF( m_nDataSize );
    if (lpBuff)
    {
	    memcpy( lpBuff->lpData,m_RingBuf,m_nDataSize );
	    m_Packages.push_back( lpBuff );
    }
}

void CDiagPackage::Copy( unsigned char* lpData,int& nStartPos,const unsigned char* lpCopy,const int nSize )
{
	for( int i=0;i<nSize;i++ )
	{
		if( HDLC_HEADER == lpCopy[i] || HDLC_ESCAPE == lpCopy[i] )
		{
			lpData[nStartPos++] = HDLC_ESCAPE;
			lpData[nStartPos++] = (unsigned char)(lpCopy[i] ^ 0x20);
		}
		else
		{
			lpData[nStartPos++] = lpCopy[i];
		}
	}
}

//apple add for supporting raw data mode
bool CDiagPackage::CheckIsDataModeSwitch()
{
	if( m_nDataSize < sizeof( DIAG_HEADER ) )
	{
		// The buffer length is too small,discard it
		return false;
	}

	DIAG_HEADER* lpHeader = (DIAG_HEADER*)m_RingBuf;
	if( lpHeader->type == DIAG_MODE_TYPE && lpHeader->subtype == DIAG_MODE_SWITCH_ACK )
	{
		BYTE bRet = m_RingBuf[ sizeof( DIAG_HEADER ) ];
		if( bRet == 0 ) // Parameter : result(8bit),it can be success(0)/failure(1).
		{
			m_bRawDataMode = m_bRawDataReq;
		}
		return true;
	}
	return false;
}


unsigned int CDiagPackage::RawDataAppend( void* lpBuffer,unsigned int uSize  )
{
	m_lpPkg = Alloc_PRT_BUFF( uSize );
    if (m_lpPkg)
    {
	    memcpy( m_lpPkg->lpData,lpBuffer,uSize );
    				
	    m_Packages.push_back( m_lpPkg );
	    m_lpPkg = NULL;
	    m_PackageInfo.nTotalPackages++;
	    m_PackageInfo.nValidPackages++;
    }
			
	return m_Packages.size();
}


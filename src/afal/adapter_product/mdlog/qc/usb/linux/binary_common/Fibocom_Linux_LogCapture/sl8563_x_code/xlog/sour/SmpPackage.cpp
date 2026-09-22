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

// SmpPackage.cpp: implementation of the CSmpPackage class.
//
//////////////////////////////////////////////////////////////////////


#include "SmpPackage.h"
#include <algorithm>
#include <string.h>

extern "C"
{
#include "crc.h"
}

extern PRT_BUFF* Alloc_PRT_BUFF( unsigned int size );
// This two functions will be implement in BaseChan.lib
extern "C" void* dlmalloc(size_t);
extern "C" void  dlfree(void*);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSmpPackage::CSmpPackage()
{
	m_nMinPackageSize = sizeof( SMP_HEADER );
	m_nLoLvlEndian = PP_LITTLE_ENDIAN;
	m_nProtocol = PT_SMP;
}

CSmpPackage::~CSmpPackage()
{

}

Diag_Pkg_Status CSmpPackage::GetSMPDataStatus(unsigned char* buffer,int index)
{

	if( HDLC_HEADER != *(buffer + index -1) )
	{//the last byte in m_RingBuf is not HDLC_HEADER
		return DPS_NoValidData;
	}
	else if( HDLC_HEADER == *(buffer + index-1) &&  HDLC_HEADER != *(buffer + index - 2))
	{//there only one HDLC_HEADER at the tail of m_RingBuf
		return DPS_Start1;
	}
	else if(HDLC_HEADER == *(buffer + index - 1) &&
		HDLC_HEADER == *(buffer + index - 2) &&
		HDLC_HEADER != *(buffer + index - 3))
	{//there only two HDLC_HEADER at the tail of m_RingBuf
		return DPS_Start2;
	}
	else if(HDLC_HEADER == *(buffer + index - 1) &&
		HDLC_HEADER == *(buffer + index - 2) &&
		HDLC_HEADER == *(buffer + index - 3))
	{//there only three HDLC_HEADER at the tail of m_RingBuf
		return DPS_Start3;
	}
	else
	{
		return DPS_NoValidData;
	}
}

unsigned int CSmpPackage::Append( void* lpBuffer,unsigned int uSize )
{
	if( NULL == lpBuffer || 0 == uSize )
	{
		return m_Packages.size();
	}
	memset( &m_PackageInfo,0,sizeof( m_PackageInfo ) );

	LockPakcageList( true );

	unsigned char  agStartFlag[SMP_START_LEN] = {0x7E,0x7E,0x7E,0x7E};

	unsigned char* lpBuff = (unsigned char*)lpBuffer;
	unsigned char* lpBuffEnd = lpBuff + uSize;
	unsigned char* pPos = NULL;
	unsigned char* pLocal = NULL;
	//
#pragma warning(disable:4127)
#define FIND_FLAG(n)  do{\
    if( HDLC_HEADER == *lpBuff++ )\
	{\
        m_Status = n;\
	}\
    else\
	{\
        m_Status = DPS_NoValidData;\
	}\
}while(0)

	while( lpBuff < lpBuffEnd )
	{
		switch( m_Status )
		{
		case DPS_NoValidData:
			{
				// No invalidate data now,find 1st 0x7E in the lpbuffer
				while( lpBuff < lpBuffEnd )
				{
					if( HDLC_HEADER == *lpBuff++ )
					{
						m_Status = DPS_Start1;
						break;
					}
				}
				break;
			}
		case DPS_Start1:
			{
				// No invalidate data now,find 2nd 0x7E in the lpbuffer
				FIND_FLAG(DPS_Start2);
				break;
			}
		case DPS_Start2:
			{
				// No invalidate data now,find 3rd 0x7E in the lpbuffer
				FIND_FLAG(DPS_Start3);
				break;
			}
		case DPS_Start3:
			{
				// No invalidate data now,find 4th 0x7E in the lpbuffer
				FIND_FLAG(DPS_Header);
				break;
			}
		case DPS_Header:
			{
				if( (lpBuffEnd - lpBuff) + m_nDataSize >= m_nMinPackageSize)
				{//has the whole header data, copy the left header data
					memcpy(m_RingBuf + m_nDataSize, lpBuff, m_nMinPackageSize - m_nDataSize );
					lpBuff += m_nMinPackageSize - m_nDataSize;
					m_nDataSize = m_nMinPackageSize;

					SMP_HEADER *pHdr = (SMP_HEADER *)m_RingBuf;
					if( CheckHeader(pHdr) )
					{
						m_Status = DPS_Unpacking;
					}
					else
					{
						if( HDLC_HEADER == m_RingBuf[0])
						{
							memmove(m_RingBuf,m_RingBuf+1,m_nMinPackageSize-1);
							m_nDataSize--;
						}
						else
						{ // find if there is SMP_START_FLAG in m_RingBuf
							pPos = std::search(m_RingBuf+1, m_RingBuf+m_nDataSize, agStartFlag,&agStartFlag[SMP_START_LEN]);
							if(pPos != m_RingBuf+m_nDataSize)
							{
								pPos += 4;
								memmove(m_RingBuf,pPos, m_RingBuf+m_nDataSize-pPos);
								m_nDataSize = m_RingBuf+m_nDataSize-pPos;
							}
							else
							{
								m_Status = GetSMPDataStatus(m_RingBuf,m_nDataSize);
								m_nDataSize = 0;
							}
						}
					}
				}
				else
				{//copy the left data
					memcpy(m_RingBuf + m_nDataSize, lpBuff, lpBuffEnd - lpBuff );
					m_nDataSize += lpBuffEnd - lpBuff ; //
					lpBuff = lpBuffEnd;
				}
				break;
			}
		case DPS_Unpacking:
			{
				// Check data header
				SMP_HEADER *pHdr = (SMP_HEADER *)m_RingBuf;
				if( (lpBuffEnd - lpBuff) + m_nDataSize >= pHdr->len )
				{//has the whole package data, copy the left package data

					//check if there is the flag of next package header (only check if there is one 0x7E)
					//if not, we regards current package as wrong, and find next header from after the current header
					if( ((lpBuffEnd - lpBuff) + m_nDataSize) - pHdr->len >= 1)
					{
	                    if(*(lpBuff + (pHdr->len - m_nDataSize)) != agStartFlag[0])
						{
							m_Status = DPS_NoValidData;
							if(m_nDataSize != m_nMinPackageSize)
							{
								LPBYTE pBak = pLocal;
								// copy the the ringbuff data and left data in lpBuff to new buffer(pLocal)
								// and to parse the new buffer
                                pLocal = (unsigned char*)malloc(m_nDataSize- m_nMinPackageSize + (lpBuffEnd - lpBuff));
	                            if (pLocal != NULL)
	                            {
									memcpy(pLocal,m_RingBuf+m_nMinPackageSize,m_nDataSize- m_nMinPackageSize);
									memcpy(pLocal + m_nDataSize - m_nMinPackageSize,lpBuff,(lpBuffEnd - lpBuff) );
									lpBuffEnd = pLocal + m_nDataSize- m_nMinPackageSize + (lpBuffEnd - lpBuff);
									lpBuff = pLocal;

									if(pBak)
									{
                                        free(pBak);
									}
	                            }
							}
							m_nDataSize = 0;
							m_Status = DPS_NoValidData;
							break;
						}
					} // if no more data after current package, we regards current package as normal
					memcpy(m_RingBuf + m_nDataSize, lpBuff, pHdr->len - m_nDataSize );
					lpBuff += pHdr->len  - m_nDataSize;
					m_nDataSize = pHdr->len;
				}
				else
				{//copy the left data
					memcpy(m_RingBuf + m_nDataSize, lpBuff, lpBuffEnd - lpBuff );
					m_nDataSize += lpBuffEnd - lpBuff ;
					lpBuff = lpBuffEnd;
					break;
				}

				if(m_nDataSize == pHdr->len )  // get the whole packet
				{
					AddPackage();
				}

				m_Status = DPS_NoValidData;
		        m_nDataSize = 0;

				break;
			}
		}
	}

    if(pLocal != NULL)
	{
        free(pLocal);
		pLocal = NULL;
	}

	LockPakcageList( false );

	if( m_nDataSize )
	{
		// If there are some data in the internal buffer, it means a incomplete package received
		// We just save the situation here, the caller will decide it is a error or not
		m_PackageInfo.bNoTail = 1;
	}
	return m_Packages.size();
}

unsigned int CSmpPackage::Package( void* lpInput,int nInputSize,void** lppMemBlock,int* lpSize )
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

	SMP_PACKAGE* lpPacket = (SMP_PACKAGE*)lpInput;

	if(lpPacket->header.len > SMP_HDR_LEN && lpPacket->data == NULL)
	{
		return DP_INVALID_PARAMETER;
	}

	unsigned int nSize = lpPacket->header.len + SMP_START_LEN;

    unsigned char* lpData = (unsigned char*)malloc( nSize );
	if( NULL == lpData )
	{
		return DP_NO_ENUOUPH_MEMORY;
	}

	memset(lpData,0x7E,SMP_START_LEN);
	memcpy(lpData+SMP_START_LEN,&lpPacket->header,SMP_HDR_LEN);
	if(lpPacket->header.len > 0)
	{
		memcpy(lpData+SMP_START_LEN+SMP_HDR_LEN,lpPacket->data,lpPacket->header.len-SMP_HDR_LEN);
	}

	SMP_HEADER *pHeader = (SMP_HEADER *)(lpData+SMP_START_LEN);

	pHeader->reserved = SMP_RESERVED;
	pHeader->check_sum = frm_chk((unsigned short*)pHeader,(SMP_HDR_LEN-sizeof(pHeader->check_sum)),0);

	*lppMemBlock = lpData;
	*lpSize = nSize;

	return 0;
}

bool CSmpPackage::CheckHeader(SMP_HEADER *pHdr)
{
	if( frm_chk((unsigned short*)pHdr, SMP_HDR_LEN, 0) == 0)
		return true;

	return false;
}

bool CSmpPackage::SetProperty( long lFlags,unsigned int dwPropertyID,const void* lpValue )
{
	return CDiagPackage::SetProperty( lFlags,dwPropertyID,lpValue );
}

bool CSmpPackage::CheckLength()
{
	if( m_nDataSize < SMP_HDR_LEN + DIAG_HDR_LEN )
	{
		// The buffer length is too small,discard it
		return false;
	}

	//Check SMP header
	SMP_HEADER *pSmpHdr = (SMP_HEADER*)m_RingBuf;
	if( 0 == pSmpHdr->len )
	{
		pSmpHdr->len = (unsigned short)m_nDataSize;
	}
	else if( pSmpHdr->len != m_nDataSize )
	{
		m_PackageInfo.bInvalidLen = 1;
		return false;
	}

	DIAG_HEADER* pDiagHdr = (DIAG_HEADER*)(m_RingBuf+SMP_HDR_LEN);

	if( 0 == pDiagHdr->len )
	{
		pDiagHdr->len = (unsigned short)(m_nDataSize-SMP_HDR_LEN);
	}
	else if( pDiagHdr->len != (m_nDataSize - SMP_HDR_LEN) )
	{
		m_PackageInfo.bInvalidLen = 1;
		return false;
	}

	return true;
}

void CSmpPackage::AddToPackageList()
{
	PRT_BUFF* lpBuff = Alloc_PRT_BUFF( m_nDataSize - SMP_HDR_LEN );
	memcpy( lpBuff->lpData,m_RingBuf +SMP_HDR_LEN ,m_nDataSize-SMP_HDR_LEN );
	//memcpy( lpBuff->lpData+ (m_nDataSize-SMP_HDR_LEN),m_RingBuf,SMP_HDR_LEN );
	m_Packages.push_back( lpBuff );
}

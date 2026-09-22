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

// DiagPackage.h: interface for the CDiagPackage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DIAGPACKAGE_H__E68DB9A9_B8A1_414B_969F_E7C097E68931__INCLUDED_)
#define AFX_DIAGPACKAGE_H__E68DB9A9_B8A1_414B_969F_E7C097E68931__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DiagChan.h"
#include "ProtoChan.h"
#include <pthread.h>

#pragma warning( disable : 4284 )
#pragma warning( push, 3 )
#include <deque>
#pragma warning( pop )

#define MAX_DIAG_PKG_SIZE 65536  // 64K
#define HDLC_HEADER 0x7e

enum Diag_Pkg_Status
{
	DPS_NoValidData,
	DPS_Header,
	DPS_Unpacking,
	DPS_Escape,
	DPS_Start1,
	DPS_Start2,
	DPS_Start3,
};

class CDiagPackage : public IProtocolPackage
{
public:
	CDiagPackage();
	virtual ~CDiagPackage();

public:
	virtual unsigned int Append( void* lpBuffer,unsigned int uSize );

    virtual void Clear();

    virtual unsigned int GetPackages( void* lpPackage,int size );

	virtual void FreeMem( void* lpMemBlock );

	virtual unsigned int Package( void* lpInput,int nInputSize,void** lppMemBlock,int* lpSize );

    virtual bool GetProperty( long lFlags,unsigned int dwPropertyID,void* lpValue );
    virtual bool SetProperty( long lFlags,unsigned int dwPropertyID,const void* lpValue );

protected:
	virtual unsigned int NoHeaderAppend( void* lpBuffer,unsigned int uSize );
	virtual unsigned int NoEscapeAppend( void* lpBuffer,unsigned int uSize );
	void LockPakcageList( bool bLock );
	virtual bool AddPackage();
	virtual bool CheckLength();
	virtual void AddToPackageList();
	unsigned int PackageQueryEndianCmd( void* lpInput,int nInputSize,void** lppMemBlock,int* lpSize );
	//apple add for supporting raw data mode
	bool CheckIsDataModeSwitch();
	unsigned int RawDataAppend( void* lpBuffer,unsigned int uSize  );
	void Copy( unsigned char* lpData,int& nStartPos,const unsigned char* lpCopy,const int nSize );

protected:

	unsigned char m_RingBuf[MAX_DIAG_PKG_SIZE];	// Internal buffer
	unsigned int m_nDataSize;					// Internal data size
	Diag_Pkg_Status m_Status;					// Unpack status

	PRT_BUFF* m_lpPkg;

	std::deque<PRT_BUFF*> m_Packages;		// The unpacked packages

	char m_nHiLvlEndian;
	char m_nLoLvlEndian;
#ifdef _WIN32
	CRITICAL_SECTION m_Section;
#else
    pthread_mutex_t  m_Section;
#endif

	unsigned int m_nSequenceNumber;
	PACKAGE_INFO m_PackageInfo;

	int m_nPackageType;

	unsigned int m_nMaxSn;

	unsigned int m_nMinPackageSize; //@hongliang.xin 2011-6-27

	//apple add for supporting raw data mode
	bool  m_bSentDataModeSwitchReq;
	bool  m_bRawDataMode;
	bool  m_bRawDataReq;

	int  m_nProtocol;
};


#endif // !defined(AFX_DIAGPACKAGE_H__E68DB9A9_B8A1_414B_969F_E7C097E68931__INCLUDED_)

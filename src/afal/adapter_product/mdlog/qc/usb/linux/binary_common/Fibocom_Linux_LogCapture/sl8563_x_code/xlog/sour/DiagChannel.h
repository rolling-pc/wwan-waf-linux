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

// DiagChannel.h: interface for the CDiagChannel class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DIAGCHANNEL_H__5D001F14_462D_4200_95DF_22B7B6A51F69__INCLUDED_)
#define AFX_DIAGCHANNEL_H__5D001F14_462D_4200_95DF_22B7B6A51F69__INCLUDED_

#include <stdlib.h>
#include "BaseChan.h"
#include "DiagPackage.h"

class CDiagChannel : public CBaseChannel
{
public:
	CDiagChannel();
	virtual ~CDiagChannel();

	virtual BOOL	GetProperty( LONG lFlags, DWORD dwPropertyID,
		LPVOID pValue );

	virtual BOOL 	SetProperty( LONG lFlags, DWORD dwPropertyID,
		LPCVOID pValue );

    virtual int GetProtocolType();

protected:
	CDiagPackage m_Package;
	int m_nPackageType;
};

#endif // !defined(AFX_DIAGCHANNEL_H__5D001F14_462D_4200_95DF_22B7B6A51F69__INCLUDED_)

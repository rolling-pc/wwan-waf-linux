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

// SmpChannel.cpp: implementation of the CSmpChannel class.
//
//////////////////////////////////////////////////////////////////////

#include "SmpChannel.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSmpChannel::CSmpChannel()
{
	m_lpProtocolPackage = &m_Package;

	m_szLogName[0] = '\0';

#ifdef _DEBUG
	_tcscpy(m_szLogName,_T("SmpChanD"));
#else
	_tcscpy(m_szLogName,_T("SmpChan"));
#endif

}

CSmpChannel::~CSmpChannel()
{
}

int CSmpChannel::GetProtocolType()
{
    return (int)PROTOCOL_TYPE_SMP;
}

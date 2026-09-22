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

// ProtocalChannelFactory.h: interface for the CProtocalChannelFactory class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROTOCALCHANNELFACTORY_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)
#define AFX_PROTOCALCHANNELFACTORY_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_

#include "BaseChan.h"
#include "DiagChannel.h"
#include "SmpChannel.h"

enum {
	PROTOCOL_DIAG = 0,
	PROTOCOL_SMP
};

class CProtocalChannelFactory
{
public:
	static CBaseChannel* GetInstance(uint32_t uType);
    static void          ReleaseInstance(CBaseChannel* pInstance);
};


#endif // !defined(AFX_PROTOCALCHANNELFACTORY_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)

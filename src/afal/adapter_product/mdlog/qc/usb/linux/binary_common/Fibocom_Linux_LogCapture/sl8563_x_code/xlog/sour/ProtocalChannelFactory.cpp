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

// ProtocalChannelFactory.cpp: implementation of the CProtocalChannelFactory class.
//
//////////////////////////////////////////////////////////////////////

#include "ProtocalChannelFactory.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


CBaseChannel* CProtocalChannelFactory::GetInstance(uint32_t uType)
{
	CBaseChannel* pBaseChannel = NULL;

	switch(uType)
	{
	case PROTOCOL_SMP:
		pBaseChannel = new CSmpChannel();
		break;
	case PROTOCOL_DIAG:
	default:
		pBaseChannel = new CDiagChannel();
		break;
	}

	return pBaseChannel;
}

void CProtocalChannelFactory::ReleaseInstance(CBaseChannel* pInstance)
{
    SAFE_DELETE_PTR(pInstance)
}

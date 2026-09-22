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

// SmpChannel.h: interface for the CSmpChannel class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SMPCHANNEL_H__24DDFE69_036E_4328_B5A6_86D49B11BB6D__INCLUDED_)
#define AFX_SMPCHANNEL_H__24DDFE69_036E_4328_B5A6_86D49B11BB6D__INCLUDED_

#include <stdlib.h>
#include "BaseChan.h"
#include "SmpPackage.h"


class CSmpChannel : public CBaseChannel
{
public:
	CSmpChannel();
	virtual ~CSmpChannel();

	virtual int GetProtocolType();

protected:
	CSmpPackage m_Package;
};

#endif // !defined(AFX_SMPCHANNEL_H__24DDFE69_036E_4328_B5A6_86D49B11BB6D__INCLUDED_)

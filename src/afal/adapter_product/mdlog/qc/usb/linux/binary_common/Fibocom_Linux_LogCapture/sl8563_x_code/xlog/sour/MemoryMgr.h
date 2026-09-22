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

// MemoryMgr.h: interface for the CMemoryMgr class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MEMORYMGR_H__5906FA6F_2A7A_46A7_9F58_9D045DABB64E__INCLUDED_)
#define AFX_MEMORYMGR_H__5906FA6F_2A7A_46A7_9F58_9D045DABB64E__INCLUDED_


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <stdint.h>
#include <stddef.h>

class CMemoryMgr
{
public:
	CMemoryMgr();
	virtual ~CMemoryMgr();
public:
	/*Init unused now*/
	bool 	Init( bool bUseMempool = true, uint32_t ulSize = 0, uint32_t ulCount = 0 );
	void*	GetMemory( uint32_t ulSize, uint32_t *pRealSize = NULL);
	void 	FreeMemory( void* lpMemBlock );
	void	Reset(); // reserved
private:
	uint32_t m_ulBlockSize;  //unused now
	uint32_t m_ulBlockCount; //unused now
	bool      m_bUseMempool;
};

#endif // !defined(AFX_MEMORYMGR_H__5906FA6F_2A7A_46A7_9F58_9D045DABB64E__INCLUDED_)

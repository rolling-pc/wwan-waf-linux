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

// MemoryMgr.cpp: implementation of the CMemoryMgr class.
//
//////////////////////////////////////////////////////////////////////

//#include "stdafx.h"
#include "MemoryMgr.h"
#include "malloc.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMemoryMgr::CMemoryMgr()
{
	m_ulBlockSize = 1000;
	m_ulBlockCount = 0;

	m_bUseMempool = true;

}

CMemoryMgr::~CMemoryMgr()
{
	m_ulBlockSize = 0;
	m_ulBlockCount = 0;
}

bool CMemoryMgr::Init( bool bUseMempool, uint32_t ulSize,uint32_t ulCount)
{
	m_ulBlockSize = ulSize;
	m_ulBlockCount = ulCount;
	m_bUseMempool = bUseMempool;
	return true;
}
void*	CMemoryMgr::GetMemory( uint32_t ulSize, uint32_t *pRealSize)
{
	if( ulSize == 0)
	{
		return NULL;
	}
	if(pRealSize != NULL)
	{
		*pRealSize = ulSize;
	}
	if(m_bUseMempool)
	{
        return malloc((size_t)ulSize);
	}
	else
	{
		return (void *)new uint8_t[ulSize];
	}

}

void 	CMemoryMgr::FreeMemory( void* lpMemBlock )
{
	if(lpMemBlock != NULL)
	{
		if(m_bUseMempool)
		{
            free(lpMemBlock);
		}
		else
		{
			delete [] lpMemBlock;
		}
	}
}
void	CMemoryMgr::Reset()
{
	return;
}

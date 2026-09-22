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

// SmpPackage.h: interface for the CSmpPackage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SMPPACKAGE_H__E14C43A7_A1AC_4BF4_90EE_E6DA4022CE49__INCLUDED_)
#define AFX_SMPPACKAGE_H__E14C43A7_A1AC_4BF4_90EE_E6DA4022CE49__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DiagPackage.h"

#define UNUSED_ALWAYS(x) x

class CSmpPackage : public CDiagPackage 
{
public:
	CSmpPackage();
	virtual ~CSmpPackage();

public:
	virtual unsigned int Append( void* lpBuffer,unsigned int uSize );

	virtual unsigned int Package( void* lpInput,int nInputSize,void** lppMemBlock,int* lpSize );

	virtual bool SetProperty( long lFlags,unsigned int dwPropertyID,const void* lpValue ); 

protected:
	virtual bool CheckHeader(SMP_HEADER *pHdr);
	virtual bool CheckLength();
	virtual void AddToPackageList();
	Diag_Pkg_Status GetSMPDataStatus(unsigned char* buffer,int index);

};

#endif // !defined(AFX_SMPPACKAGE_H__E14C43A7_A1AC_4BF4_90EE_E6DA4022CE49__INCLUDED_)

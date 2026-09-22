/*
* Copyright 2018-2020 THALES group. All Rights Reserved.
*
* Project name: LPASDK.
* Platform : Windows, Linux.
* Language : C/C++
*
* Except if otherwise stated in a NOTICE file provided by Thales together with the software, below conditions are applicable by default.
*
* This computer program includes confidential and proprietary information of Thales and is a trade secret of
* Thales. All use, disclosure, and/or reproduction is prohibited unless authorized in writing by Thales.
*
* The computer program is provided "AS IS" without warranty of any kind. Thales makes no
* warranties to any person or entity with respect to the computer program and disclaims all other warranties,
* expressed or implied. Thales expressly disclaims any implied warranty of merchantability, fitness for particular
* purpose and any warranty which may arise from course of performance, course of dealing, or usage of trade. Further
* Thales does not warrant that the computer program will meet requirements or that operation of the computer program
* will be uninterrupted or error-free.
*
*/


#ifndef LPA_SDK__CORE_EX_API_H
#define LPA_SDK__CORE_EX_API_H


#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include "lpasdk/api/lpasdk_api.h"
#include <stdint.h>	


typedef struct
{
	long countMemoryAllocCall;
	long countMemoryFreeCall;
	long currentMemoryBlockAllocated;

	long currentMemoryAllocated;
	long totalMemoryAllocated;

	long maxMemoryAllocated;
	long maxMemoryBlockAllocated;
}LPA_MEMORY_STATUS;

// Extented API
/////////////////////////////////////////////

EXPORT_DLL bool lpaExGetExtraVersion(char* ptrVersionBuffer, size_t versionBufferMaxSize);

// Since LPASDK 1.5, moved on Extended API
EXPORT_DLL bool lpaExGetFullParametersList(LPA_PARAMETERS_LIST * ptrLpaParametersList);

// LPA SDK SE Media Card Reset
EXPORT_DLL bool		lpaExCardReset();

EXPORT_DLL bool		lpaExGetProfileNotificationList(LPA_PROFILE_NOTIFICATION_LIST* ptrProfileNotificationList);
EXPORT_DLL bool		lpaExClearProfileNotification(uint16_t sequenceNumber);

EXPORT_DLL bool		lpaExWriteMemoryStatusDumpToLog();
EXPORT_DLL bool		lpaExGetMemoryStatus(LPA_MEMORY_STATUS* prtMemoryStatus);
EXPORT_DLL bool		lpaExCheckMemoryAllocated();

EXPORT_DLL LPA_API_ERROR_DESCRIPTION* lpaExGetListErrorCodeDescription();


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // LPA_SDK__CORE_EX_API_H
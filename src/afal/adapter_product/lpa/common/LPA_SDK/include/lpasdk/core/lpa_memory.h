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

#ifndef LPA_SDK__CORE_MEMORY_H
#define LPA_SDK__CORE_MEMORY_H

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include <stdbool.h>

#ifdef LPA_SDK__USING_EX_API
#include "lpasdk/api/lpasdk_ex_api.h"
#endif // LPA_SDK__USING_EX_API

#include "lpasdk/core/lpa_core.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

UT_EXPORT_DLL void lpaCoreMemoryInitialize();

#ifdef LPA_SDK__MEMORY

#define LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_EQ			1
#define LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GT			2
#define LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GE			3

#define LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_EQ	4
#define LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GT	5
#define LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GE	6
	
UT_EXPORT_DLL void lpaCoreMemoryResetParamGenerateErr();
UT_EXPORT_DLL bool lpaCoreMemorySetParamGenerateErr(uint8_t param, long value);
UT_EXPORT_DLL bool lpaCoreMemoryGetParamGenerateErr(uint8_t param, long* ptrValue);


#ifndef LPA_SDK__MEMORY_MONITORING

// Allocate memory
void* lpaCoreMemoryAlloc(size_t size);
void* lpaCoreMemoryCalloc(size_t count, size_t size);
void* lpaCoreMemoryRealloc(void* ptrMemoryBlock, size_t newSize);

// free memory
void lpaCoreMemoryFree(void* ptrMemoryBlock);

#else // LPA_SDK__MEMORY_MONITORING

UT_EXPORT_DLL void*	lpaCoreMemoryMonitorAlloc(char* ptrFilename, int line, size_t size);
UT_EXPORT_DLL void*	lpaCoreMemoryMonitorCalloc(char* ptrFilename, int line, size_t count, size_t size);
UT_EXPORT_DLL void*	lpaCoreMemoryMonitorRealloc(char* ptrFilename, int line, void* ptrMemoryBlock, size_t newSize);
UT_EXPORT_DLL void	lpaCoreMemoryMonitorFree(char* ptrFilename, int line, void* mem);

// Wrapper to monitor mamory API call
#define lpaCoreMemoryAlloc(size)						lpaCoreMemoryMonitorAlloc(__FILE__, __LINE__, size)
#define lpaCoreMemoryCalloc(count, size)				lpaCoreMemoryMonitorCalloc(__FILE__, __LINE__,count,size)
#define lpaCoreMemoryRealloc(ptrMemoryBlock, newSize)	lpaCoreMemoryMonitorRealloc(__FILE__, __LINE__,ptrMemoryBlock,newSize)
#define lpaCoreMemoryFree(mem)							lpaCoreMemoryMonitorFree(__FILE__, __LINE__, mem)

#endif // LPA_SDK__MEMORY_MONITORING
#else // LPA_SDK__MEMORY

 //Not using owner memory mecanism
#define lpaCoreMemoryAlloc(size)						malloc(size)
#define lpaCoreMemoryCalloc(count, size)				calloc(count,size)
#define lpaCoreMemoryRealloc(ptrMemoryBlock, newSize)	realloc(ptrMemoryBlock,newSize)
#define lpaCoreMemoryFree(mem)							free(mem)

#endif // LPA_SDK__MEMORY

#ifdef LPA_SDK__USING_EX_API
UT_EXPORT_DLL bool lpaCoreGetMemoryStatus(LPA_MEMORY_STATUS* prtMemoryStatus);
#endif // LPA_SDK__USING_EX_API

void lpaCoreMemoryDumpStatusIntoLog();
void lpaCoreMemoryCheckMemoryAllocated();

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // LPA_SDK__CORE_MEMORY_H
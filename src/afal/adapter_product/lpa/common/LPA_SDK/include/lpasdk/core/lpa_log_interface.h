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

#ifndef LPA_SDK__CORE_LOG_INTERFACE_H
#define LPA_SDK__CORE_LOG_INTERFACE_H

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef enum
{
	SDK_LOG_LEVEL_UNKNOWN,

	SDK_LOG_LEVEL_VERBOSE,
	SDK_LOG_LEVEL_DEBUG,
	SDK_LOG_LEVEL_INFO,
	SDK_LOG_LEVEL_WARNING,
	SDK_LOG_LEVEL_ERROR,
	SDK_LOG_LEVEL_SYSTEM	// Use only internally 
} LpaLogLevel;

typedef struct LPA_LOG_INTERFACE LPA_LOG_INTERFACE;
struct LPA_LOG_INTERFACE
{
	void (*openLog) (const char* ptrFileName, const char* prtBackupFileName);
	void (*flushLog)();
    bool (*isLogOpened)();
	void(*closeLog)();

	void (*activateLogLimitation) (bool activated);

	void (*setLogLevel) (LpaLogLevel logLevel);
	LpaLogLevel (*getLogLevel)(void);

	bool (*setLogMaxSize)(long logMaxSize);
	long (*getLogMaxSize)();

	void (*appendToLog)(LpaLogLevel logLevel, const char* ptrMessage, va_list argptr);
	void (*appendLongTextToLog)(LpaLogLevel logLevel, const char* ptrHeaderMessage, const char* ptrLongTextToLog, const size_t LongTextToLogSize);
	void (*appendByteArrayToLog)(LpaLogLevel logLevel, const char* ptrMessage, const char* ptrByteArrayName, unsigned char* ptrByteArray, size_t byteArraySize);
};


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // LPA_SDK__CORE_LOG_INTERFACE_H

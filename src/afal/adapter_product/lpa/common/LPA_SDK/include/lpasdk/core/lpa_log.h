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

#ifndef LPA_SDK__CORE_LOG_H
#define LPA_SDK__CORE_LOG_H

#include <stdio.h>
#include <stdbool.h>

#include "lpasdk/core/lpa_core.h"
#include "lpasdk/core/lpa_log_interface.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

UT_EXPORT_DLL bool lpaCoreLogInit();
UT_EXPORT_DLL bool lpaCoreLogInitEx(LPA_LOG_INTERFACE* lpaLogInterface);
UT_EXPORT_DLL bool lpaCoreLogIsInitialized();
UT_EXPORT_DLL bool lpaCoreLogRelease();

UT_EXPORT_DLL void lpaCoreLogOpen(const char* ptrFileName, const char* prtBackupFileName);
UT_EXPORT_DLL void lpaCoreLogFlush();
UT_EXPORT_DLL void lpaCoreLogClose();
UT_EXPORT_DLL bool lpaCoreLogIsOpen();

UT_EXPORT_DLL void lpaCoreActivateLogLimitation(bool activated);

UT_EXPORT_DLL void lpaCoreSetLogLevel(LpaLogLevel logLevel);
UT_EXPORT_DLL LpaLogLevel lpaCoreGetLogLevel(void);

UT_EXPORT_DLL bool lpaCoreSetLogMaxSize(long logMaxSize);
UT_EXPORT_DLL long lpaCoreGetLogMaxSize();

UT_EXPORT_DLL const char* lpaCoreGetLogLevelName(LpaLogLevel logLevel, bool* ptrIsFound);

UT_EXPORT_DLL bool lpaCoreSetLogLevelString(const char* logLevelString);

UT_EXPORT_DLL void lpaCoreLogAppend(LpaLogLevel logLevel, const char* ptrMessage, ...);
UT_EXPORT_DLL void lpaCoreLogAppendLongText(LpaLogLevel logLevel, const char* ptrHeaderMessage, const char* ptrLongTextToLog, const size_t LongTextToLogSize);
UT_EXPORT_DLL void lpaCoreLogAppendByteArray(LpaLogLevel logLevel, const char* ptrMessage, const char* ptrByteArrayName, unsigned char* ptrByteArray, size_t byteArraySize);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // LPA_SDK__CORE_LOG_H
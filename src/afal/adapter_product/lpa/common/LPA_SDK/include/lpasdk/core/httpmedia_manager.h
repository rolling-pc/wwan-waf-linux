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

#ifndef HTTPMEDIAMANAGER_H
#define HTTPMEDIAMANAGER_H
#include <stdbool.h>

#include "lpasdk/core/httpmedia_option_type.h"
#include "lpasdk/core/lpa_core.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

UT_EXPORT_DLL bool httpMediaManagerInitialize();
UT_EXPORT_DLL bool httpMediaManagerIsInitialized();
UT_EXPORT_DLL bool httpMediaManagerDelete();

bool httpMediaManagerConfigure();

bool httpMediaManagerSetBooleanOption(HttpMediaOptionType optionType, bool enabled);
bool httpMediaManagerGetBooleanOption(HttpMediaOptionType optionType, bool* ptrEnabled);

bool httpMediaManagerSetLongOption(HttpMediaOptionType optionType, long value);
bool httpMediaManagerGetLongOption(HttpMediaOptionType optionType, long* ptrValue);

bool httpMediaManagerSetCallbackEventExecutionError(LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback);

char* httpMediaManagerPost(const char* ptrCertificatePath, const char* ptrTargetURL, const char* ptrPostdata, bool* ptrIsSuccess, long* ptrHttpCode);
char* httpMediaManagerHTTPExecutePost(bool* ptrIsSuccess, long* ptrHttpCode);
        
bool httpMediaManagerSetTargetUrl( char* ptrTargetURL);
bool httpMediaManagerSetCertificatePath( char* ptrCertificatePath);

bool httpMediaManagerSetPostData( char* ptrPostdata);
bool httpMediaManagerSetHeaders();

bool httpMediaManagerSetCallback();
bool httpMediaManagerSetWriteData();
bool httpMediaManagerHttpExecuteInit();
bool httpMediaManagerHttpExecuteCleanup();

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* HTTPMEDIAMANAGER_H */


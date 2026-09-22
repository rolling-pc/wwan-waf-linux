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

#ifndef HTTPMEDIA_BASE_H
#define HTTPMEDIA_BASE_H

#include <stdbool.h>
#include "lpasdk/core/httpmedia_option_type.h"
#include "lpasdk/api/lpasdk_api.h"

    typedef struct THTTPMedia {
        void* _childStruct;
        bool (*httpMediaHttpExecuteInit) (const struct THTTPMedia*);
        bool (*httpMediaPost) (const struct THTTPMedia*, const char* ptrCertificatePath, const char* ptrTargetURL, const char* ptrPostdata, long* ptrHttpCode);        
        
        void (*httpMediaHttpExecuteCleanup) (const struct THTTPMedia*);
		char* (*httpMediaGetBufferResponse) (const struct THTTPMedia*);
        bool (*httpMediaSetBooleanOption) (const struct THTTPMedia*, HttpMediaOptionType optionType, bool enabled);
        bool (*httpMediaGetBooleanOption) (const struct THTTPMedia*, HttpMediaOptionType optionType, bool* ptrEnabled);

        bool (*httpMediaSetLongOption) (const struct THTTPMedia*, HttpMediaOptionType optionType, long value);
        bool (*httpMediaGetLongOption) (const struct THTTPMedia*, HttpMediaOptionType optionType, long* ptrValue);
		
		// EventErrorCallback support
		bool(*httpMediaSetCallbackEventExecutionError) (const struct THTTPMedia*, LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback);
    } THTTPMedia;

    THTTPMedia* New_HTTPMediaBase();



#endif /* HTTPMEDIA_BASE_H */


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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <curl/curl.h>  

#include "lpasdk/core/httpmedia_base.h"
#include "lpasdk/core/httpmedia_manager.h"
#include "lpasdk/driver/httpmedia_curl.h"
#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_memory.h"


static THTTPMedia* _httpMedia = NULL;

UT_EXPORT_DLL bool httpMediaManagerInitialize() 
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerInitialize()");

    if (_httpMedia == NULL) 
	{
        _httpMedia = New_HTTPMediaCurl();
		if (_httpMedia != NULL )
			res = true;
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Unable to create _httpMedia !");
    }

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-httpMediaManagerInitialize()");

	return res;
}

UT_EXPORT_DLL bool httpMediaManagerIsInitialized()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerIsInitialized()");

	return (_httpMedia != NULL);
}

UT_EXPORT_DLL bool httpMediaManagerDelete()
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerDelete()");

    if (_httpMedia != NULL) 
	{
        Delete_HTTPMediaCurl(_httpMedia);
        _httpMedia = NULL;
        res = true;
    }
    
	return res;
}

bool httpMediaManagerSetBooleanOption(HttpMediaOptionType optionType, bool enabled)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerSetBooleanOption()");

	if (_httpMedia != NULL) {
		res = _httpMedia->httpMediaSetBooleanOption(_httpMedia, optionType, enabled);
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMedia is not created !");


	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- httpMediaManagerSetBooleanOption() : return %s", (res ? "true" : "false"));
	return res;
}

bool httpMediaManagerGetBooleanOption(HttpMediaOptionType optionType, bool* ptrEnabled)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerGetBooleanOption()");

	if (_httpMedia != NULL && ptrEnabled != NULL) {
		res = _httpMedia->httpMediaGetBooleanOption(_httpMedia, optionType, ptrEnabled);
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMedia is not created !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- httpMediaManagerGetBooleanOption() : return %s", (res ? "true" : "false"));
	return res;
}

bool httpMediaManagerSetLongOption(HttpMediaOptionType optionType, long value)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerSetLongOption()");

	if (_httpMedia != NULL) 
	{
		res = _httpMedia->httpMediaSetLongOption(_httpMedia, optionType, value);
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMedia is not created !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- httpMediaManagerSetLongOption() : return %s", (res ? "true" : "false"));
	return res;
}

bool httpMediaManagerGetLongOption(HttpMediaOptionType optionType, long* ptrValue)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerGetLongOption()");

	if (_httpMedia != NULL) {
		
		if(ptrValue != NULL )
			res = _httpMedia->httpMediaGetLongOption(_httpMedia, optionType, ptrValue);
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMedia is not created !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- httpMediaManagerGetLongOption() : return %s", (res ? "true" : "false"));
	return res;
}

bool httpMediaManagerSetCallbackEventExecutionError(LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerSetCallbackEventExecutionError()");

	if (_httpMedia != NULL )
		res = _httpMedia->httpMediaSetCallbackEventExecutionError(_httpMedia, lpaEventExecutionErrorCallback);
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMedia is not created !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- httpMediaManagerSetCallbackEventExecutionError() : return %s", (res ? "true" : "false"));
	return res;
}

char* httpMediaManagerPost(const char* ptrCertificatePath, const char* ptrTargetURL, const char* ptrPostdata, bool* ptrIsSuccess, long* ptrHttpCode)
{
	char* resp = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerPost()");
	if (_httpMedia != NULL )
	{
		if( ptrIsSuccess != NULL && ptrCertificatePath != NULL && ptrTargetURL != NULL && ptrPostdata != NULL && ptrHttpCode != NULL)
		{
			*ptrIsSuccess = false;

			bool res = _httpMedia->httpMediaPost(_httpMedia, ptrCertificatePath, ptrTargetURL, ptrPostdata, ptrHttpCode);
			if (res)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "httpMediaPost() execute success");

				// Check Http Code
				int httpCode = *ptrHttpCode;

				if (httpCode >= 200 && httpCode < 300)
				{
					if (httpCode == 204)
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "HTTP Code 204 => no data expected (calling httpMediaGetBufferResponse() is ignored)");
						*ptrIsSuccess = true;
					}
					else
					{
						resp = _httpMedia->httpMediaGetBufferResponse(_httpMedia);
						if (resp != NULL)
							*ptrIsSuccess = true;
						else
							lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "httpMediaGetBufferResponse() return NULL");
					}
				}
				else
				{
					// Not a 2XX code , normally no data available
					*ptrIsSuccess = true;

					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Not HTTP Code 2xx => data not managed (if present)");
				}
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "httpMediaPost() failed");
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter");
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMedia is not created !");

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- httpMediaManagerHTTPExecutePost() : return res %s", (resp != NULL ? "not NULL" : "NULL"));
    
    return resp;
}



bool httpMediaManagerHttpExecuteInit() {
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaManagerHttpExecuteInit()");

    if (_httpMedia != NULL) {
        res = _httpMedia->httpMediaHttpExecuteInit(_httpMedia);
    }
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMedia is not created !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- httpMediaManagerHttpExecuteInit() : return %s", (res ? "true" : "false"));

	return res;
}

bool httpMediaManagerHttpExecuteCleanup()
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpMediaHttpExectueCleanup()");
    bool res = false;

	if (_httpMedia != NULL) {
        _httpMedia->httpMediaHttpExecuteCleanup(_httpMedia);
        res = true;
    }
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMedia is not created !");

	return res;
}

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

#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_memory.h"
#include "lpasdk/core/httpmedia_base.h"
#include "lpasdk/driver/httpmedia_curl.h"


THTTPMedia* New_HTTPMediaCurl();
void Delete_HTTPMediaCurl(THTTPMedia* httpMedia);

static size_t _WriteRespCallback(void *contents, size_t size, size_t nmemb, void *userp);

bool _httpMediaConfigure(const THTTPMedia* httpMedia);
bool _httpMediaSetTargetUrl(const THTTPMedia* httpMedia, const char* ptrTargetURL);
bool _httpMediaSetCertificatePath(const THTTPMedia* httpMedia, const char* ptrCertificatePath);
bool _httpMediaSetPostData(const THTTPMedia* httpMedia, const char* ptrPostdata);
bool _httpMediaSetHeaders(const THTTPMedia* httpMedia);

bool _httpMediaSetBooleanOption(const THTTPMedia* ptrHttpMedia, HttpMediaOptionType optionType, bool enabled);
bool _httpMediaGetBooleanOption(const THTTPMedia* ptrHttpMedia, HttpMediaOptionType optionType, bool* ptrEnabled);

bool _httpMediaSetLongOption(const THTTPMedia* ptrHttpMedia, HttpMediaOptionType optionType, long value);
bool _httpMediaGetLongOption(const THTTPMedia* ptrHttpMedia, HttpMediaOptionType optionType, long* ptrValue);

bool _httpMediaSetCallbackEventExecutionError(const THTTPMedia* httpMedia, LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback);

bool _httpMediaSetCallback(const THTTPMedia* httpMedia);
bool _httpMediaSetWriteData(const THTTPMedia* httpMedia);
char* _httpMediaGetBufferResponse(const THTTPMedia* httpMedia);

bool _httpMediaHttpExecutePost(const THTTPMedia* httpMedia, long *ptrHttpCode);

bool _httpMediaPost(const THTTPMedia* httpMedia, const char* ptrCertificatePath, const char* ptrTargetURL, const char* ptrPostdata, long* ptrHttpCode);

void _httpMediaHttpExecuteCleanup(const THTTPMedia* httpMedia);
bool _httpMediaHttpExecuteInit(const THTTPMedia* httpMedia);

static bool _curlOptionSSLVerifyPeer = true;
static bool _curlOptionSSLVerifyHost = true;
static bool _curlOptionVerbose = false;

#ifdef _FFW_PCIOT_LPA_MODIFY_
static long _curlOptionTimeOut = 60;			// seconds
static long _curlOptionConnectTimeOut = 60;		// seconds
#else
static long _curlOptionTimeOut = 30;			// seconds
static long _curlOptionConnectTimeOut = 30;		// seconds
#endif

static unsigned int _curlVersion = 0;

// Event execution error callback
static LPA_EVENT_EXECUTION_ERROR _lpaEventExecutionErrorCallback = NULL;

#ifdef LPA_SDK__CURL_MEMORY
void *_httpMediaMallocCallback(size_t size);
void _httpMediaFreeCallback(void *ptr);
void *_httpMediaReallocCallback(void *ptr, size_t size);
char *_httpMediaStrdupCallback(const char *str);
void *_httpMediaCallocCallback(size_t nmemb, size_t size);
#endif // LPA_SDK__CURL_MEMORY

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

static size_t _WriteRespCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;

		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _WriteRespCallback()");

		if (NULL == contents || NULL == userp)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Incorrect NULL parameter!");
        return 0;
		}
    
			struct RespStruct *respStr = (struct RespStruct *) userp;

			void* ptrRealloc = lpaCoreMemoryRealloc(respStr->resp, respStr->size + realsize + 1);
			if (NULL == ptrRealloc)
			{
				// out of memory!
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Not enough memory (realloc returned NULL)");
        return 0;
			}
				respStr->resp = ptrRealloc;
				memcpy(&(respStr->resp[respStr->size]), contents, realsize);
				respStr->size += realsize;
				respStr->resp[respStr->size] = 0;

    return realsize;
}

THTTPMedia* New_HTTPMediaCurl()
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> New_HTTPMediaCurl()");

	THTTPMedia* ptrHTTPMedia = NULL;
    
	THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*) lpaCoreMemoryAlloc(sizeof (THTTPMediaCURL));
	if (httpMediaCurl != NULL)
	{
		httpMediaCurl->_base = New_HTTPMediaBase();
		if (httpMediaCurl->_base != NULL)
		{
			httpMediaCurl->_base->_childStruct = httpMediaCurl;

			httpMediaCurl->_base->httpMediaPost = _httpMediaPost;
			httpMediaCurl->_base->httpMediaSetBooleanOption = _httpMediaSetBooleanOption;
			httpMediaCurl->_base->httpMediaGetBooleanOption = _httpMediaGetBooleanOption;

			httpMediaCurl->_base->httpMediaGetBufferResponse = _httpMediaGetBufferResponse;

			httpMediaCurl->_base->httpMediaSetLongOption = _httpMediaSetLongOption;
			httpMediaCurl->_base->httpMediaGetLongOption = _httpMediaGetLongOption;

			httpMediaCurl->_base->httpMediaHttpExecuteCleanup = _httpMediaHttpExecuteCleanup;
			httpMediaCurl->_base->httpMediaHttpExecuteInit = _httpMediaHttpExecuteInit;
			
			httpMediaCurl->_base->httpMediaSetCallbackEventExecutionError = _httpMediaSetCallbackEventExecutionError;

			memset(&httpMediaCurl->_respdata, 0, sizeof(struct RespStruct));
			httpMediaCurl->_headers = NULL;
			httpMediaCurl->_curl = NULL;
			
			_curlVersion = 0; // CURL Version not yet retrieved

#ifdef LPA_SDK__CURL_MEMORY
			CURLcode  curlCode = curl_global_init_mem(CURL_GLOBAL_DEFAULT, _httpMediaMallocCallback, _httpMediaFreeCallback, _httpMediaReallocCallback, _httpMediaStrdupCallback, _httpMediaCallocCallback);
			if(curlCode == CURLE_OK)
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "CURL global init mem done successfully");
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Error during curl_global_init_mem() !");
#endif // LPA_SDK__CURL_MEMORY

			ptrHTTPMedia = (THTTPMedia*)httpMediaCurl->_base;
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Unable to allocate httpMediaCurl->_base !");
			
			lpaCoreMemoryFree(httpMediaCurl);
			httpMediaCurl = NULL;
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Unable to allocate THTTPMediaCURL !");

	// By default, No callback available for LPA Event Execution error
	_lpaEventExecutionErrorCallback = NULL;

	return ptrHTTPMedia;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

void Delete_HTTPMediaCurl(THTTPMedia* httpMedia)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> Delete_HTTPMediaCurl()");

	if (httpMedia != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;
		if (httpMediaCurl != NULL)
		{
#ifdef LPA_SDK__CURL_MEMORY
			curl_global_cleanup();
#endif // LPA_SDK__CURL_MEMORY

			lpaCoreMemoryFree(httpMediaCurl);
		}
		lpaCoreMemoryFree(httpMedia);
		httpMedia = NULL;
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Delete_HTTPMediaCurl() => httpMedia is NULL !");
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

bool _httpMediaConfigure(const THTTPMedia* httpMedia)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaConfigure()");

	if (httpMedia != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;

		if (httpMediaCurl != NULL && httpMediaCurl->_curl != NULL)
		{
			CURLcode curlCodeSSL_VERIFYPEER = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_SSL_VERIFYPEER, (_curlOptionSSLVerifyPeer ? 1 : 0));
			CURLcode curlCodeSSL_VERIFYHOST = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_SSL_VERIFYHOST, (_curlOptionSSLVerifyHost ? 2 : 0));
			CURLcode curlCodeVERBOSE = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_VERBOSE, (_curlOptionVerbose ? 1 : 0));

			res = ((curlCodeSSL_VERIFYPEER == CURLE_OK) && (curlCodeSSL_VERIFYHOST == CURLE_OK) && (curlCodeVERBOSE == CURLE_OK));

#if defined(LPA_SDK__PLATFORM_WIN) && defined(LPA_SDK__CURLSSLOPT_NO_REVOKE)
			if (res)
			{
				if (_curlVersion >= 0x072C00)	// CURLSSLOPT_NO_REVOKE present since CURL 7.44.0
				{
					if (curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NO_REVOKE) != CURLE_OK)
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_httpMediaConfigure() => Unable to do CURLSSLOPT_NO_REVOKE");
						res = false;
					}
					else
						lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_httpMediaConfigure() => CURLSSLOPT_NO_REVOKE done successfully");
				}
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_httpMediaConfigure() => Unable to do CURLSSLOPT_NO_REVOKE (option not supported by current CURL library)");
			}
#endif // LPA_SDK__PLATFORM_WIN && LPA_SDK__CURLSSLOPT_NO_REVOKE

		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaConfigure() => httpMedia is NULL !");

	return res;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

bool _httpMediaSetTargetUrl(const THTTPMedia* httpMedia, const char* ptrTargetURL)
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaSetTargetUrl()");

	if (httpMedia != NULL && ptrTargetURL != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;

		if (httpMediaCurl != NULL && httpMediaCurl->_curl != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "* _httpMediaSetTargetUrl : %s", ptrTargetURL);
			CURLcode curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_URL, ptrTargetURL);
			if( curlCode == CURLE_OK )
				res = true;
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetTargetUrl failed (curl error code: %d) => %s", curlCode, curl_easy_strerror(curlCode));
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, " _httpMediaSetTargetUrl failed");
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetTargetUrl() => NULL parameter detected!");

    return res;
}

bool _httpMediaHttpExecuteInit(const THTTPMedia* httpMedia)
{
    bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaHttpExecuteInit()");
	if (httpMedia != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;
		if (httpMediaCurl != NULL)
		{
			httpMediaCurl->_curl = curl_easy_init();
			if (httpMediaCurl->_curl != NULL)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_httpMediaHttpExecuteInit() Init Success");


				curl_version_info_data * ptrData = curl_version_info(CURLVERSION_NOW);
				if (ptrData != NULL)
				{
					if (ptrData->age >= 0)
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "curl version : %s", (ptrData->version ? ptrData->version : "N/A"));
						lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "curl host : %s", (ptrData->host != NULL ? ptrData->host : "N/A"));
						lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "curl feature : 0x%lx", ptrData->features);

						_curlVersion = ptrData->version_num;

						// Check Minimal CURL version required
						if (_curlVersion > 0x072200) // Minimal releaser required : 7.34.0
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "curl version supported");
								
							CURLcode curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
							if (curlCode == CURLE_OK)
							{
								lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "CURL using TLS v1.2 or later");
								res = true;
							}
							else
								lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Unable to use TLS v1.2 or later (CURL configuration) !");
						}
						else
							lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "curl version not supported (require 7.34 or more)");
					}
				}
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_httpMediaHttpExecuteInit() Init Failed");
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "httpMedia is NULL !");

    return res;
}

bool _httpMediaSetCertificatePath(const THTTPMedia* httpMedia, const char* ptrCertificatePath)
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaSetCertificatePath()");

	if (httpMedia != NULL && ptrCertificatePath != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;
		if (httpMediaCurl->_curl)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_httpMediaSetCertificatePath ...");
			CURLcode curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_CAINFO, ptrCertificatePath);
			if (curlCode == CURLE_OK)
				res = true;
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetCertificatePath failed (curl error code: %d) => %s", curlCode, curl_easy_strerror(curlCode));
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetCertificatePath() => NULL parameter detected!");

    return res;
}

bool _httpMediaSetPostData(const THTTPMedia* httpMedia, const char* ptrPostdata)
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaSetPostData()");
    if (httpMedia != NULL && ptrPostdata != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;
		if (httpMediaCurl != NULL)
		{
			if (httpMediaCurl->_curl != NULL )
			{
				CURLcode curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_POST, 1);
				
				if( curlCode == CURLE_OK)
					curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_POSTFIELDS, ptrPostdata);

				if (curlCode == CURLE_OK)
					curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_NOSIGNAL, 1);
				
				if (curlCode == CURLE_OK)
					curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_CONNECTTIMEOUT, _curlOptionConnectTimeOut);
				
				if (curlCode == CURLE_OK)
					curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_TIMEOUT, _curlOptionTimeOut);

				if (curlCode == CURLE_OK)
					res = true;
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "setPostData failed (curl error code: %d) => %s", curlCode, curl_easy_strerror(curlCode));
			}
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetPostData => httpMediaCurl is NULL !");
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetPostData => NULL parameter detected!");

	return res;
}

bool _httpMediaSetHeaders(const THTTPMedia* httpMedia)
{
    bool res = false;

	if (httpMedia != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;
		if (httpMediaCurl != NULL)
		{
			if (httpMediaCurl->_curl != NULL)
			{
				struct curl_slist *headers = NULL;
				
				headers = curl_slist_append(headers, "X-Admin-Protocol:gsma/rsp/v2.2.0");
				headers = curl_slist_append(headers, "content-type:application/json");
				headers = curl_slist_append(headers, "charset:utf-8");
				headers = curl_slist_append(headers, "User-Agent: gsma-rsp-lpad");
				headers = curl_slist_append(headers, "Expect:");

				httpMediaCurl->_headers = headers;
				
				CURLcode curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_HTTPHEADER, httpMediaCurl->_headers);
				
				if (curlCode == CURLE_OK)
					res = true;
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetHeaders failed (curl error code: %d) => %s", curlCode, curl_easy_strerror(curlCode));
			}
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetHeaders() => httpMedia is NULL !");

    return res;
}

bool _httpMediaSetBooleanOption(const THTTPMedia* ptrHttpMedia, HttpMediaOptionType optionType, bool enabled)
{
    bool res = false;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaSetBooleanOption()");

	bool isOptionIgnored = false;

	switch (optionType)
	{
		case HTTP_MEDIA_OPTION_TYPE_CURL_SSL_VERIFYPEER:
			_curlOptionSSLVerifyPeer = enabled; 
		break;

		case HTTP_MEDIA_OPTION_TYPE_CURL_SSL_VERIFYHOST:
			_curlOptionSSLVerifyHost = enabled;
		break;

		case HTTP_MEDIA_OPTION_TYPE_CURL_VERBOSE:
			_curlOptionVerbose = enabled;
		break;

		default:
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "HttpMediaOptionType '%d'not supported", optionType);
			isOptionIgnored = true;
		break;
	}

	if (!isOptionIgnored)
		res = true;

    return res;
}

bool _httpMediaGetBooleanOption(const THTTPMedia* ptrHttpMedia, HttpMediaOptionType optionType, bool* ptrEnabled)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaGetBooleanOption()");

	if ( ptrEnabled != NULL)
	{
		bool isOptionIgnored = false;
		switch (optionType)
		{
			case HTTP_MEDIA_OPTION_TYPE_CURL_SSL_VERIFYPEER:
				*ptrEnabled = _curlOptionSSLVerifyPeer;
			break;

			case HTTP_MEDIA_OPTION_TYPE_CURL_SSL_VERIFYHOST:
				*ptrEnabled = _curlOptionSSLVerifyHost;
			break;

			case HTTP_MEDIA_OPTION_TYPE_CURL_VERBOSE:
				*ptrEnabled = _curlOptionVerbose;
			break;

			default:
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "HttpMediaOptionType '%d'not supported", optionType);
				isOptionIgnored = true;
			break;
		}

		if (!isOptionIgnored)
			res = true;
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaGetBooleanOption() => ptrEnabled is NULL !");

	return res;
}

bool _httpMediaSetLongOption(const THTTPMedia* ptrHttpMedia, HttpMediaOptionType optionType, long value)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaSetBooleanOption()");

	bool isOptionIgnored = false;

	switch (optionType)
	{
		case HTTP_MEDIA_OPTION_TYPE_CURL_CONNECT_TIMEOUT:
			_curlOptionConnectTimeOut = value;
		break;

		case HTTP_MEDIA_OPTION_TYPE_CURL_TIMEOUT:
			_curlOptionTimeOut = value;
		break;

		default:
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "HttpMediaOptionType '%d'not supported", optionType);
			isOptionIgnored = true;
		break;
	}

	if (!isOptionIgnored)
		res = true;

	return res;
}

bool _httpMediaGetLongOption(const THTTPMedia* ptrHttpMedia, HttpMediaOptionType optionType, long* ptrValue)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaGetLongOption()");

	if (ptrValue != NULL)
	{
		bool isOptionIgnored = false;
		switch (optionType)
		{
			case HTTP_MEDIA_OPTION_TYPE_CURL_CONNECT_TIMEOUT:
				*ptrValue = _curlOptionConnectTimeOut;
			break;

			case HTTP_MEDIA_OPTION_TYPE_CURL_TIMEOUT:
				*ptrValue = _curlOptionTimeOut;
			break;

			default:
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "HttpMediaOptionType '%d'not supported", optionType);
				isOptionIgnored = true;
			break;
		}

		if (!isOptionIgnored)
			res = true;
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaGetLongOption() => ptrValue is NULL !");

	return res;
}

bool _httpMediaSetCallbackEventExecutionError(const THTTPMedia* httpMedia, LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback)
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ httpmedia_curl :> _httpMediaSetCallbackEventExecutionError()");

    bool res = false;

    if (lpaEventExecutionErrorCallback != NULL)
    {
        if (_lpaEventExecutionErrorCallback != lpaEventExecutionErrorCallback)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, " registering lpaEventExecutionErrorCallback ...");
            _lpaEventExecutionErrorCallback = lpaEventExecutionErrorCallback;
            res = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, " LpaEventExecutionErrorCallback already registered !");
    }
    else
    {
        if (_lpaEventExecutionErrorCallback != NULL)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, " unregistering lpaEventExecutionErrorCallback ...");
            _lpaEventExecutionErrorCallback = NULL;
            res = true;
        }
    }

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- httpmedia_curl :> _httpMediaSetCallbackEventExecutionError() return res=%s", (res ? "true" : "false"));

    return res;
}

bool _httpMediaSetCallback(const THTTPMedia* httpMedia)
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaSetCallback()");
	
	if (httpMedia != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;
		if (httpMediaCurl != NULL)
		{
			if (httpMediaCurl->_curl != NULL)
			{
				CURLcode curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_WRITEFUNCTION, _WriteRespCallback);
				if (curlCode == CURLE_OK)
					res = true;
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetCallback failed (curl error code: %d) => %s", curlCode, curl_easy_strerror(curlCode));
			}
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetCallback() => httpMedia is NULL !");

    return res;
}

bool _httpMediaSetWriteData(const THTTPMedia* httpMedia)
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaSetWriteData()");
	
	if (httpMedia != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;
		if (httpMediaCurl != NULL)
		{
			if (httpMediaCurl->_curl != NULL)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "start to do http easy perform");
				CURLcode curlCode = curl_easy_setopt(httpMediaCurl->_curl, CURLOPT_WRITEDATA, (void *)&httpMediaCurl->_respdata);
				if (curlCode == CURLE_OK)
					res = true;
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetWriteData failed (curl error code: %d) => %s", curlCode, curl_easy_strerror(curlCode));
			}
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaSetWriteData() => httpMedia is NULL !");

    return res;
}

bool _httpMediaHttpExecutePost(const THTTPMedia* httpMedia, long *ptrHttpCode)
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaHttpExecutePost()");

    if (httpMedia == NULL)
	{
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "httpMedia is NULL !");
        return res;
    }
    if (ptrHttpCode == NULL)
    {
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "ptrHttpCode is NULL !");
            return res;
    }

    THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*) httpMedia->_childStruct;
    *ptrHttpCode = 0; // By default

    CURLcode ret;
    ret = curl_easy_perform(httpMediaCurl->_curl);
    if (CURLE_OK == ret)
	{
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "the http execution result : >> CURLE_OK :)");

		if (curl_easy_getinfo(httpMediaCurl->_curl, CURLINFO_RESPONSE_CODE, ptrHttpCode) == CURLE_OK )
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "curl => HTTP code : %d ", *ptrHttpCode);
		else
		{
			*ptrHttpCode = 0;
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "curl => Unable to get HTTP code !");
		}

        res = true;
    }
	else
	{
		const char* curlStrError = curl_easy_strerror(ret);
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Http execution failed (curl error code: %d) => %s", ret, (curlStrError != NULL ? curlStrError : "N/A"));

		if (_lpaEventExecutionErrorCallback != NULL)
		{
			LPA_EVENT_EXECUTION_ERROR_INFO eventExecutionErrorInfo;
			char errorSubjectCode[16];
			snprintf(errorSubjectCode, sizeof(errorSubjectCode), "%d", ret);

			eventExecutionErrorInfo.executionErrorType = LPA_EVENT_EXECUTION_CURL_ERROR_TYPE;
			eventExecutionErrorInfo.detailErrorMask = LPA_EVENT_EXECUTION_ERROR_SUBJECT_CODE_MASK;
			eventExecutionErrorInfo.ptrErrorReasonCode = NULL;
			eventExecutionErrorInfo.ptrErrorSubjectCode = errorSubjectCode;
			
			if (curlStrError != NULL)
			{
				eventExecutionErrorInfo.detailErrorMask |= LPA_EVENT_EXECUTION_ERROR_EXTRA_INFO_MASK;
				eventExecutionErrorInfo.ptrErrorExtraInfo = curl_easy_strerror(ret);
			}

			_lpaEventExecutionErrorCallback(NULL, &eventExecutionErrorInfo);
		}

        res = false;
    }

    return res;
}

char* _httpMediaGetBufferResponse(const THTTPMedia* httpMedia)
{
    if(httpMedia != NULL)
    {
	THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;
	return ( httpMediaCurl != NULL ? httpMediaCurl->_respdata.resp : NULL);
    }
    else
        return NULL;
}

bool _httpMediaPost(const THTTPMedia* httpMedia, const char* ptrCertificatePath, const char* ptrTargetURL, const char* ptrPostdata, long* ptrHttpCode)
{
    bool resHttpMediaCall = false;

    if(httpMedia != NULL && ptrCertificatePath != NULL && ptrTargetURL != NULL && ptrPostdata != NULL && ptrHttpCode != NULL)
    {
        resHttpMediaCall = _httpMediaConfigure(httpMedia);
        if (resHttpMediaCall)
            resHttpMediaCall = _httpMediaSetTargetUrl(httpMedia,ptrTargetURL);

        if (resHttpMediaCall)
            resHttpMediaCall = _httpMediaSetPostData(httpMedia,ptrPostdata);

        if (resHttpMediaCall)
            resHttpMediaCall = _httpMediaSetHeaders(httpMedia);

        if(ptrCertificatePath != NULL)
            {          
            if (resHttpMediaCall && strlen(ptrCertificatePath) > 0)
            {
                resHttpMediaCall = _httpMediaSetCertificatePath(httpMedia,ptrCertificatePath);
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "certificate path is : %s", ptrCertificatePath);
            }
        }
            else
                    resHttpMediaCall = false;

        if (resHttpMediaCall)
            resHttpMediaCall = _httpMediaSetCallback(httpMedia);

        if (resHttpMediaCall)
            resHttpMediaCall = _httpMediaSetWriteData(httpMedia);

        //start to execute post
        if (resHttpMediaCall)
        {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "HTTP execute post ...");
                resHttpMediaCall = _httpMediaHttpExecutePost(httpMedia, ptrHttpCode);
        }
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaGetBufferResponse() => NULL parameter detected!");

    return resHttpMediaCall;
}



void _httpMediaHttpExecuteCleanup(const THTTPMedia* httpMedia)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+httpmedia_curl :> _httpMediaHttpExecuteCleanup()");
    
	if (httpMedia != NULL)
	{
		THTTPMediaCURL* httpMediaCurl = (THTTPMediaCURL*)httpMedia->_childStruct;
		
		if (httpMediaCurl->_headers != NULL)
		{
			curl_slist_free_all(httpMediaCurl->_headers);
			httpMediaCurl->_headers = NULL;
		}

		if (httpMediaCurl->_respdata.resp != NULL )
		{
			lpaCoreMemoryFree(httpMediaCurl->_respdata.resp);
			httpMediaCurl->_respdata.resp = NULL;
		}

		httpMediaCurl->_respdata.size = 0;

		if (httpMediaCurl->_curl != NULL)
		{
			curl_easy_cleanup(httpMediaCurl->_curl);
			httpMediaCurl->_curl = NULL;
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_httpMediaHttpExecuteCleanup() => httpMedia is NULL !");
}


/////////////////////////
// Memory Callback
/////////////////////////

#ifdef LPA_SDK__CURL_MEMORY

void *_httpMediaMallocCallback(size_t size)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] + httpmedia_curl:>_httpMediaMallocCallback()");
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] malloc(%d bytes) ...", size);

	void* ptrMalloc = NULL;

#if defined(LPA_SDK__CURL_MEMORY_MONITORING) && defined(LPA_SDK__MEMORY_MONITORING)
	ptrMalloc = lpaCoreMemoryAlloc(size);
#else
	ptrMalloc = malloc(size);
#endif // LPA_SDK__CURL_MEMORY_MONITORING
	
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] allocated memory : 0x%lx", ptrMalloc);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] - httpmedia_curl:>_httpMediaMallocCallback()");

	return ptrMalloc;
}

void _httpMediaFreeCallback(void *ptr)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] + httpmedia_curl:>_httpMediaFreeCallback()");

	if(ptr != NULL )
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] Free (0x%lx) ...", ptr);

#if defined(LPA_SDK__CURL_MEMORY_MONITORING) && defined(LPA_SDK__MEMORY_MONITORING)
		lpaCoreMemoryFree(ptr);
#else
		free(ptr);
#endif // LPA_SDK__CURL_MEMORY_MONITORING
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] Free (NULL) => Do nothing");


	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] - httpmedia_curl:>_httpMediaFreeCallback()");
}

void *_httpMediaReallocCallback(void *ptr, size_t size)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] + httpmedia_curl:>_httpMediaReallocCallback()");
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] Realloc (0x%lx,%d bytes) ...", ptr, size);

	void* ptrRealloc = NULL;

#if defined(LPA_SDK__CURL_MEMORY_MONITORING) && defined(LPA_SDK__MEMORY_MONITORING)
	ptrRealloc = lpaCoreMemoryRealloc(ptr, size);
#else
	ptrRealloc = realloc(ptr, size);
#endif // LPA_SDK__CURL_MEMORY_MONITORING

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] reallocated memory : 0x%lx", ptrRealloc);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] - httpmedia_curl:>_httpMediaReallocCallback()");
	return ptrRealloc;
}

char *_httpMediaStrdupCallback(const char *str)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] + httpmedia_curl:>_httpMediaStrdupCallback()");
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] Strdup (0x%lx) ...", str);

	char* ptrStrdup = NULL;

#if defined(LPA_SDK__CURL_MEMORY_MONITORING) && defined(LPA_SDK__MEMORY_MONITORING)
	if (str != NULL)
	{
		size_t strSize = strlen(str) + 1;
		ptrStrdup = lpaCoreMemoryAlloc(strSize);
		if (ptrStrdup != NULL)
			memcpy(ptrStrdup, str, strSize);
	}
#else
#ifdef LPA_SDK__PLATFORM_WIN
	// 'strdup': The POSIX name for this item is deprecated.Instead, use the ISO C and C++ conformant name : _strdup.
	ptrStrdup = _strdup(str);
#else
	ptrStrdup = strdup(str);
#endif // LPA_SDK__PLATFORM_WIN

#endif // LPA_SDK__CURL_MEMORY_MONITORING

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] ptrStrdup => 0x%lx", ptrStrdup);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] - httpmedia_curl:>_httpMediaStrdupCallback()");

	return ptrStrdup;
}

void *_httpMediaCallocCallback(size_t nmemb, size_t size)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] + httpmedia_curl:>_httpMediaCallocCallback()");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] Calloc %d,%d ...", nmemb, size);

	void* ptrCalloc = NULL;

#if defined(LPA_SDK__CURL_MEMORY_MONITORING) && defined(LPA_SDK__MEMORY_MONITORING)
	ptrCalloc = lpaCoreMemoryCalloc(nmemb, size);
#else
	ptrCalloc = calloc(nmemb, size);
#endif // LPA_SDK__CURL_MEMORY_MONITORING

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] Calloc=> 0x%lx ", ptrCalloc);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[CURL_MEM] - httpmedia_curl:>_httpMediaCallocCallback()");

	return ptrCalloc;
}

#endif // LPA_SDK__CURL_MEMORY

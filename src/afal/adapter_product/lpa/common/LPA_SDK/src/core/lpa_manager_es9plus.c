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

#include "lpasdk/core/lpa_manager_es9plus.h"
#include "lpasdk/core/lpa_manager.h"
#include "lpasdk/core/lpa_manager_helper.h"
#include "lpasdk/core/semedia_manager.h"
#include "lpasdk/core/httpmedia_manager.h"

#include "lpasdk/lpasdk_internal_api.h"

#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/rawdata_object.h"
#include "lpasdk/core/util.h"
#include "lpasdk/core/lpa_memory.h"

#include "cJSON/cJSON.h"

#include <ctype.h>

#define INITIATE_AUTHENTICATION_PATH                    "/gsma/rsp2/es9plus/initiateAuthentication"
#define AUTHENTICATE_CLIENT_PATH                        "/gsma/rsp2/es9plus/authenticateClient"
#define GET_BOUND_PROFILE_PACKAGE_PATH                  "/gsma/rsp2/es9plus/getBoundProfilePackage"
#define HANDLE_NOTIFICATION_PATH			"/gsma/rsp2/es9plus/handleNotification"
#define CANCEL_SESSION_PATH                             "/gsma/rsp2/es9plus/cancelSession"

static char _certPath[LPA_CFG_CERT_PATH_MAX_SIZE];

static char _bufferFormatLogMessage[1024];	// 1Ko is enough (to increase it, use dynamic memory allocation)

bool _convertHostURLtoLowerCase(const char * ptrSourceURL, char * ptrDestURL, size_t ptrDestURLsize);

char* _lpaManagerAuthenticateClientSendRequest(LPA_API_ERROR* ptrLpaError, const char* ptrTransactionId, const char* ptrSmdpAddress, const char* ptrAuthenticateServerResponse, const LPA_EventCallback* ptrLpaEventCallback);
LPA_API_ERROR _lpaManagerAuthenticateClientCheckTransactionId(cJSON* ptrcjsonHttpAuthClientResp, const char* ptrTransactionId);
LPA_API_ERROR _lpaManagerAuthenticateClientExtractAllDataFromCJSON(cJSON* ptrcjsonHttpAuthClientResp, ptr_serverData p_serverData);
LPA_API_ERROR _lpaManagerAuthenticateClientExtractDataFromCJSON(cJSON* ptrcjsonHttpAuthClientResp, unsigned char* ptrBuffer, size_t* ptrBufferSize, size_t bufferSizeMax, const char* ptrName);

bool _checkJSONresponseStatusSuccessful(cJSON* cjson, const LPA_EventCallback* ptrLpaEventCallback);
bool _checkJSONresponseErrorCode(const cJSON* p_cjson, const char * p_subjectCode, const char * p_reasonCode);
void  _clearJsonAndBuffer(cJSON * ptrcjson, char *ptrBuffer);

bool _lpaManagerES9SendCancelSession(const char * transactionID, CANCEL_SESSION_RESPONSE * ptrCancelSessionResp, const char* ptrSmdpAddress, const LPA_EventCallback* ptrLpaEventCallback);

void _sendCallbackNotificationForHttpErrors(const LPA_EventCallback* ptrLpaEventCallback, const char * pErrorMessage);


/////////////////////////////////////////////
//
/////////////////////////////////////////////

void lpaManagerES9Plus_Init()
{
	memset(_certPath, 0, sizeof(_certPath));
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES9Plus_setCertPath(const char* ptrCertPath)
{
	bool res = false;

	if (ptrCertPath != NULL && strlen(ptrCertPath) < LPA_CFG_CERT_PATH_MAX_SIZE)
	{
		sprintf(_certPath, "%s", ptrCertPath);
		res = true;
	}

	return res;
}

size_t lpaManagerES9Plus_getCertPathSize()
{
	return strlen(_certPath);
}

bool lpaManagerES9Plus_getCertPath(char* ptrCertPath, size_t ptrCertPathMaxSize)
{
	bool res = false;

	if ((ptrCertPath != NULL) && (strlen(_certPath) < ptrCertPathMaxSize))
	{
		sprintf(ptrCertPath, "%s", _certPath);
		res = true;
	}

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////


char* lpaManagerES9Plus_ExecutePost(const char * ptrTargetURL, const char * ptrJsonRequest, bool* ptrIsSuccess, long *ptrHttpCode, const LPA_EventCallback* ptrLpaEventCallback)
{
    char* resp = NULL;

    char urlStr[LPA_ADDRESS_MAX_SIZE];
    char lowerCaseTargetURL[LPA_ADDRESS_MAX_SIZE];

    char * secureHttps = "https://";
    
    memset(urlStr, 0, sizeof(urlStr));
    memset(lowerCaseTargetURL, 0, sizeof(lowerCaseTargetURL));
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerES9Plus_ExecutePost()");

    if (ptrTargetURL != NULL && ptrJsonRequest != NULL && ptrIsSuccess != NULL && ptrHttpCode != NULL)
    {
        *ptrIsSuccess = false; // before starting process, set to false

        // Converts URL to address to lowercase
        if(_convertHostURLtoLowerCase(ptrTargetURL, lowerCaseTargetURL, sizeof(lowerCaseTargetURL)))
        {
            if ((strlen(secureHttps) + strlen(lowerCaseTargetURL)) < sizeof(urlStr))
            {
                    if (snprintf(urlStr, sizeof(urlStr), "%s%s", secureHttps, lowerCaseTargetURL) >= sizeof(urlStr))
                    {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Buffer too small for building https URL !!!");
                            urlStr[0] = '\0';
                    }
            }
            else
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Buffer too small for building https URL !");
                    urlStr[0] = '\0';
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Cannot convert target host URL to lowercase!");
            urlStr[0] = '\0';
        }

        // Check target URL. If empty due to problems, goes on error
        if (strlen(urlStr) > 0)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "urlStr:%s", urlStr);
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "start to set opt data...");
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "urlStr: Empty!", urlStr);
            lpaSetErrorCode(LPA_ERROR_INVALID_SERVER_ADDRESS);
        }

        if (strlen(urlStr) > 0 && httpMediaManagerHttpExecuteInit())
        {
            resp = httpMediaManagerPost(_certPath, urlStr, ptrJsonRequest, ptrIsSuccess, ptrHttpCode);
            if (*ptrIsSuccess)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Server return HTTP status code %d", *ptrHttpCode);

                switch (*ptrHttpCode)
                {
                    case 204: // HTTP OK - No data 
                    break;

                    case 200: // HTTP OK with data
                        // Note: Actually, case below of code 200 without data cannot be reached here due to filtering performed in
                        // httpmedia_manager.c -> httpMediaManagerPost() were *ptrIsSuccess is set to false when server return 2xx
                        // without data (Except for 204). Kept for possible evolution of httpmedia_manager.c removing this test.
                        if (resp == NULL)
                        {
                            // Incorrect case : HTTP 200 must having data
                            lpaSetErrorCode(LPA_ERROR_SERVER_COMMUNICATION_ISSUE);
                            _sendCallbackNotificationForHttpErrors(ptrLpaEventCallback, "http error: Server returned code 200 without data");
                        }
                    break;

                    case 404:
                        lpaSetErrorCode(LPA_ERROR_SERVER_RETURN_404_STATUS_CODE);
                        _sendCallbackNotificationForHttpErrors(ptrLpaEventCallback, "http error: Server returned code 404");
                    break;

                    case 500:
                        lpaSetErrorCode(LPA_ERROR_SERVER_RETURN_500_STATUS_CODE);
                        _sendCallbackNotificationForHttpErrors(ptrLpaEventCallback, "http error: Server returned code 500");
                    break;

                    default:
                        // Managed by lpa_manager directly
                        lpaSetErrorCode(LPA_ERROR_SERVER_COMMUNICATION_ISSUE);
                        snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "http error: Server returned code %ld", *ptrHttpCode);
                        _sendCallbackNotificationForHttpErrors(ptrLpaEventCallback, _bufferFormatLogMessage);
                    break;
                }
            }
            else
            {
                lpaSetErrorCode(LPA_ERROR_SERVER_COMMUNICATION_ISSUE);
                // Note: Check of code 200 here is here needed by pre-processing of code 200 in httpmedia_manager.c -> httpMediaManagerPost() were
                // *ptrIsSuccess is set to false when server return 200 without data
                if(*ptrHttpCode == 200)
                    _sendCallbackNotificationForHttpErrors(ptrLpaEventCallback, "http error: Server returned code 200 without data");
                else
                {
                    // Note: Other codes 2xx without data will be also notified here, without specific indication like for code 200
                    if(*ptrHttpCode > 0)
                    {
                        snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "http error: Communication issue. http code returned: %ld", *ptrHttpCode);
                        _sendCallbackNotificationForHttpErrors(ptrLpaEventCallback, _bufferFormatLogMessage);
                    }
                    else
                        _sendCallbackNotificationForHttpErrors(ptrLpaEventCallback, "http error: Communication issue");
                    
                }
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "httpMediaManagerHttpExectueInit failed!");
            lpaSetErrorCode(LPA_ERROR_UNABLE_TO_INITIALIZE_HTTP_MEDIA);
            _sendCallbackNotificationForHttpErrors(ptrLpaEventCallback, "http error: Cannot initialize http media or problem with target URL");
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "One or more input parameter not defined !");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

    return resp;
}


/**
 * Search for host at the beginning of server URL (Up to first '/' character) and convert it to lowercase
 * @param ptrSourceURL URL to convert, string format
 * @param ptrDestURL Buffer that will receive converted URL, string format
 * @param ptrDestURLSize Size of destination buffer, Must be large enough to receive destination URL with '\0' character at the end.
 * @return True if conversion is successful
 */
bool _convertHostURLtoLowerCase(const char * ptrSourceURL, char * ptrDestURL, size_t ptrDestURLsize)
{
    bool res = false;
    size_t sourceLength = 0;
    bool hostNotfinished = true;
    
    if(ptrSourceURL != NULL && ptrDestURL != NULL && ptrDestURLsize > 0)
    {
        sourceLength = strlen(ptrSourceURL);
        
        if(sourceLength < ptrDestURLsize)    // Check destination buffer size
        {
            size_t i = 0;
            
            while(i < sourceLength)
            {
                // Stop conversion when first '/' is encountered
                if(ptrSourceURL[i] == '/')
                    hostNotfinished = false;
                
                if(hostNotfinished)
                    ptrDestURL[i] = tolower(ptrSourceURL[i]);
                else
                    ptrDestURL[i] = ptrSourceURL[i];
                
                i++;
            }
            ptrDestURL[i] = '\0'; // Terminate destination string
            
            // Validate conversion OK only if not only an host URL has been copied in destination buffer
            if(! hostNotfinished)
                res = true;
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_convertHostURLtoLowerCase: Only host URL detected.");
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_convertHostURLtoLowerCase: Destination buffer too small.");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_convertHostURLtoLowerCase: Invalid Parameters.");
        
    return res;
}


/**
 * Send Callback notification for http errors
 * @param ptrLpaEventCallback Callback pointer
 * @param pErrorMessage message to return, string format. If null no Callback is send
 */
void _sendCallbackNotificationForHttpErrors(const LPA_EventCallback* ptrLpaEventCallback, const char * pErrorMessage)
{
    if ((ptrLpaEventCallback != NULL) && (pErrorMessage != NULL))
    {
        if (ptrLpaEventCallback->_lpaEventExecutionError != NULL)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Creating Callback Notification Error for http...");
            LPA_EVENT_EXECUTION_ERROR_INFO eventExecutionErrorInfo;
            memset(&eventExecutionErrorInfo, 0x00, sizeof(LPA_EVENT_EXECUTION_ERROR_INFO));

            eventExecutionErrorInfo.executionErrorType = LPA_EVENT_EXECUTION_HTTP_ERROR_TYPE;
            eventExecutionErrorInfo.detailErrorMask = LPA_EVENT_EXECUTION_ERROR_EXTRA_INFO_MASK;
            
            eventExecutionErrorInfo.ptrErrorExtraInfo = pErrorMessage;
             
            lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Sending Callback Notification Error to LPA application ...");
            ptrLpaEventCallback->_lpaEventExecutionError(ptrLpaEventCallback->_appParameter, &eventExecutionErrorInfo);
        }
    }
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES9Plus_InitiateAuthentication(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char * ptrEuiccChallenge, const char * ptrEuiccInfo1, const char* ptrSmdpAddress)
{
	cJSON* jsonObject = cJSON_CreateObject();
        cJSON* ptrcJSONItem_Retrieval = NULL;
	char * httpServerResp = NULL;
	char * bindata = NULL;
	size_t len = 0;
	bool isSuccess = false;
	bool res = false;
	size_t binDataDecodedMemorySize = 0;

	if (p_serverData != NULL && ptrEuiccChallenge != NULL && ptrEuiccInfo1 != NULL && ptrSmdpAddress != NULL)
	{
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES9Plus_InitiateAuthentication ...");
                
            if(jsonObject != NULL)
            {    
		cJSON_AddItemToObject(jsonObject, "euiccInfo1", cJSON_CreateString(ptrEuiccInfo1));
		cJSON_AddItemToObject(jsonObject, "euiccChallenge", cJSON_CreateString(ptrEuiccChallenge));
		cJSON_AddItemToObject(jsonObject, "smdpAddress", cJSON_CreateString(ptrSmdpAddress));

		char* buffer = cJSON_Print(jsonObject);
		cJSON_Delete(jsonObject);
		jsonObject = NULL;

		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, ">>  HTTP request %s\n", buffer);

		char initialAuthSmdpAddr[LPA_ADDRESS_MAX_SIZE];
		memset(&initialAuthSmdpAddr, 0, sizeof(initialAuthSmdpAddr));
		
		if ((strlen(ptrSmdpAddress) + strlen(INITIATE_AUTHENTICATION_PATH)) < LPA_ADDRESS_MAX_SIZE)
		{
			memcpy(initialAuthSmdpAddr, ptrSmdpAddress, strlen(ptrSmdpAddress));
			memcpy(initialAuthSmdpAddr + strlen(ptrSmdpAddress), INITIATE_AUTHENTICATION_PATH, strlen(INITIATE_AUTHENTICATION_PATH));
			
			long httpCode = 0;
			httpServerResp = lpaManagerES9Plus_ExecutePost(initialAuthSmdpAddr, buffer, &isSuccess, &httpCode, ptrLpaEventCallback);

			if (httpServerResp != NULL && isSuccess)
			{
				if (httpCode > 0)
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "initiateAuthentication() => HTTP Request return httpCode=%d", httpCode);
				else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "initiateAuthentication() => HTTP Request return invalid httpCode !");
                                    lpaSetErrorCode(LPA_ERROR_INVALID_SERVER_RESPONSE);
                                    isSuccess = false;
                                }
                                
                                if(isSuccess)
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "<<  HTTP response %s", httpServerResp);

                                    cJSON* cjson = cJSON_Parse(httpServerResp);
                                    if (cjson == NULL) 
                                    {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "json pack into cjson error...");
                                            lpaSetErrorCode(LPA_ERROR_CJSON_PARSE_FAILURE);

                                            // do memory cleanup
                                            if (buffer != NULL)
                                            {
                                                    lpaCoreMemoryFree(buffer);
                                                    buffer = NULL;
                                            }
                                            httpMediaManagerHttpExecuteCleanup();
                                            return res;
                                    }

                                    if (!_checkJSONresponseStatusSuccessful(cjson, ptrLpaEventCallback))
                                    {
                                            _clearJsonAndBuffer(cjson, buffer);
                                            // Turnaround because pointers are not set to NULL by _clearJsonAndBuffer()
                                            buffer = NULL;
                                            cjson = NULL;
                                            httpMediaManagerHttpExecuteCleanup();
                                            return res;
                                    }

                                    ptrcJSONItem_Retrieval = cJSON_GetObjectItem(cjson, "transactionId");
                                    bindata = NULL;
                                    if(ptrcJSONItem_Retrieval != NULL)
                                        bindata = cJSON_GetStringValue(ptrcJSONItem_Retrieval);

                                    if (bindata != NULL && strlen(bindata) < LPA_TRANSACTION_ID_MAX_SIZE)
                                    {
                                            if (p_serverData->_transactionId.val == NULL)
                                                    p_serverData->_transactionId.val = lpaCoreMemoryAlloc(LPA_TRANSACTION_ID_MAX_SIZE);

                                            if (p_serverData->_transactionId.val != NULL)
                                            {
                                                    memset(p_serverData->_transactionId.val, 0, LPA_TRANSACTION_ID_MAX_SIZE);
                                                    memcpy(p_serverData->_transactionId.val, bindata, strlen(bindata));
                                                    p_serverData->_transactionId.len = strlen(bindata);
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "transactionId : %s ", p_serverData->_transactionId.val);
                                            }
                                            else
                                            {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot to allocate memory for transactionId");
                                                    lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                                    p_serverData->_transactionId.len = 0;
                                                    isSuccess = false;
                                            }
                                    }
                                    else
                                    {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get transactionId from server");
                                            lpaSetErrorCode(LPA_ERROR_INVALID_TRANSACTIONID);
                                            p_serverData->_transactionId.len = 0;
                                            isSuccess = false;
                                    }

                                    if(isSuccess)
                                    {
                                        p_serverData->_serverSigned1.val = lpaCoreMemoryAlloc(LPA_INITIAL_AUTHENTICATE_SERVER_SIGNED1_MAX_SIZE);
                                        p_serverData->_serverSigned1.len = 0;
                                        if (p_serverData->_serverSigned1.val != NULL) 
                                        {
                                                memset(p_serverData->_serverSigned1.val, 0, sizeof(char) * LPA_INITIAL_AUTHENTICATE_SERVER_SIGNED1_MAX_SIZE);

                                                ptrcJSONItem_Retrieval = cJSON_GetObjectItem(cjson, "serverSigned1");
                                                bindata = NULL;
                                                if(ptrcJSONItem_Retrieval != NULL)
                                                    bindata = cJSON_GetStringValue(ptrcJSONItem_Retrieval);

                                                if(bindata != NULL)
                                                {
                                                    binDataDecodedMemorySize = (strlen(bindata) / 4 * 3 + 1);
                                                    if (binDataDecodedMemorySize< LPA_INITIAL_AUTHENTICATE_SERVER_SIGNED1_MAX_SIZE)
                                                    {
                                                            if (ffw_base64_decode(bindata, strlen(bindata), p_serverData->_serverSigned1.val, &len, LPA_INITIAL_AUTHENTICATE_SERVER_SIGNED1_MAX_SIZE))
                                                            {
                                                                    p_serverData->_serverSigned1.len = len;

                                                                    snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "serverSigned1 (%lu bytes) :", CAST_SIZET_PLATFORM(len));
                                                                    lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "serverSigned1", p_serverData->_serverSigned1.val, len);
                                                            }
                                                            else
                                                            {
                                                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to decode serverSigned1 from base64 !");
                                                                lpaSetErrorCode(LPA_ERROR_INVALID_SERVERSIGNED1);
                                                                isSuccess = false;
                                                            }
                                                    }
                                                    else
                                                    {
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_serverSigned1 size exceeds maximum allowed (Current size needed:%d - Maximum size allowed:%d)!", binDataDecodedMemorySize, LPA_INITIAL_AUTHENTICATE_SERVER_SIGNED1_MAX_SIZE - 1);
                                                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                                        isSuccess = false;
                                                    }
                                                }
                                                else
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get serverSigned1 from server");
                                                    lpaSetErrorCode(LPA_ERROR_INVALID_SERVERSIGNED1);
                                                    isSuccess = false;
                                                }
                                        }
                                        else
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot allocate memory for _serverSigned1 !");
                                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                            isSuccess = false;
                                        }
                                    }

                                    if(isSuccess)
                                    {
                                        binDataDecodedMemorySize = 0;
                                        p_serverData->_serverSignature1.val = lpaCoreMemoryAlloc(LPA_INITIAL_AUTHENTICATE_SERVER_SIGNATURE1_MAX_SIZE);
                                        p_serverData->_serverSignature1.len = 0;
                                        if (p_serverData->_serverSignature1.val != NULL) 
                                        {
                                                memset(p_serverData->_serverSignature1.val, 0, sizeof(char) * LPA_INITIAL_AUTHENTICATE_SERVER_SIGNATURE1_MAX_SIZE);

                                                ptrcJSONItem_Retrieval = cJSON_GetObjectItem(cjson, "serverSignature1");
                                                bindata = NULL;
                                                if(ptrcJSONItem_Retrieval != NULL)
                                                    bindata = cJSON_GetStringValue(ptrcJSONItem_Retrieval);

                                                if(bindata != NULL)
                                                {
                                                    binDataDecodedMemorySize = (strlen(bindata) / 4 * 3 + 1);
                                                    if (binDataDecodedMemorySize< LPA_INITIAL_AUTHENTICATE_SERVER_SIGNATURE1_MAX_SIZE)
                                                    {
                                                            if (ffw_base64_decode(bindata, strlen(bindata), p_serverData->_serverSignature1.val, &len, LPA_INITIAL_AUTHENTICATE_SERVER_SIGNATURE1_MAX_SIZE))
                                                            {
                                                                    p_serverData->_serverSignature1.len = len;

                                                                    snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "serverSignature1 (%lu bytes) :", CAST_SIZET_PLATFORM(len));
                                                                    lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "serverSignature1", p_serverData->_serverSignature1.val, len);
                                                            }
                                                            else
                                                            {
                                                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to decode serverSignature1 from base64 !");
                                                                lpaSetErrorCode(LPA_ERROR_INVALID_SERVERSIGNATURE1);
                                                                isSuccess = false;
                                                            }
                                                    }
                                                    else
                                                    {
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_serverSignature1 size exceeds maximum allowed (Current size needed:%d - Maximum size allowed:%d)!", binDataDecodedMemorySize, LPA_INITIAL_AUTHENTICATE_SERVER_SIGNATURE1_MAX_SIZE - 1);
                                                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                                        isSuccess = false;
                                                    }
                                                }
                                                else
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get serverSignature1 from server");
                                                    lpaSetErrorCode(LPA_ERROR_INVALID_SERVERSIGNATURE1);
                                                    isSuccess = false;
                                                }
                                        }
                                        else
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot allocate memory for _serverSignature1 !");
                                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                            isSuccess = false;
                                        }
                                    }

                                    if(isSuccess)
                                    {
                                        binDataDecodedMemorySize = 0;
                                        p_serverData->_euiccCiPKIdToBeUsed.val = lpaCoreMemoryAlloc(LPA_INITIAL_AUTHENTICATE_EUICC_CIPKID_TO_BE_USED_MAX_SIZE);
                                        p_serverData->_euiccCiPKIdToBeUsed.len = 0;
                                        if (p_serverData->_euiccCiPKIdToBeUsed.val != NULL) 
                                        {
                                                memset(p_serverData->_euiccCiPKIdToBeUsed.val, 0, sizeof(char) * LPA_INITIAL_AUTHENTICATE_EUICC_CIPKID_TO_BE_USED_MAX_SIZE);

                                                ptrcJSONItem_Retrieval = cJSON_GetObjectItem(cjson, "euiccCiPKIdToBeUsed");
                                                bindata = NULL;
                                                if(ptrcJSONItem_Retrieval != NULL)
                                                    bindata = cJSON_GetStringValue(ptrcJSONItem_Retrieval);

                                                if(bindata != NULL)
                                                {
                                                    binDataDecodedMemorySize = (strlen(bindata) / 4 * 3 + 1);
                                                    if (binDataDecodedMemorySize< LPA_INITIAL_AUTHENTICATE_EUICC_CIPKID_TO_BE_USED_MAX_SIZE)
                                                    {
                                                            if (ffw_base64_decode(bindata, strlen(bindata), p_serverData->_euiccCiPKIdToBeUsed.val, &len, LPA_INITIAL_AUTHENTICATE_EUICC_CIPKID_TO_BE_USED_MAX_SIZE))
                                                            {
                                                                    p_serverData->_euiccCiPKIdToBeUsed.len = len;

                                                                    snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "euiccCiPKIdToBeUsed (%lu bytes) :", CAST_SIZET_PLATFORM(len));
                                                                    lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "euiccCiPKIdToBeUsed", p_serverData->_euiccCiPKIdToBeUsed.val, len);
                                                            }
                                                            else
                                                            {
                                                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to decode euiccCiPKIdToBeUsed from base64 !");
                                                                lpaSetErrorCode(LPA_ERROR_INVALID_EUICCCIPKIDTOBEUSED);
                                                                isSuccess = false;
                                                            }
                                                    }
                                                    else
                                                    {
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_euiccCiPKIdToBeUsed size exceeds maximum allowed (Current size needed:%d - Maximum size allowed:%d)!", binDataDecodedMemorySize, LPA_INITIAL_AUTHENTICATE_EUICC_CIPKID_TO_BE_USED_MAX_SIZE - 1);
                                                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                                        isSuccess = false;
                                                    }
                                                }
                                                else
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get euiccCiPKIdToBeUsed from server");
                                                    lpaSetErrorCode(LPA_ERROR_INVALID_EUICCCIPKIDTOBEUSED);
                                                    isSuccess = false;
                                                }
                                        }
                                        else
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot allocate memory for _euiccCiPKIdToBeUsed !");
                                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                            isSuccess = false;
                                        }

                                    }

                                    if(isSuccess)
                                    {
                                        binDataDecodedMemorySize = 0;
                                        p_serverData->_serverCertificate.val = lpaCoreMemoryAlloc(LPA_INITIAL_AUTHENTICATE_SERVER_CERTIFICATE_MAX_SIZE);
                                        p_serverData->_serverCertificate.len = 0;
                                        if (p_serverData->_serverCertificate.val != NULL)
                                        {
                                                memset(p_serverData->_serverCertificate.val, 0, sizeof(char) * LPA_INITIAL_AUTHENTICATE_SERVER_CERTIFICATE_MAX_SIZE);

                                                ptrcJSONItem_Retrieval = cJSON_GetObjectItem(cjson, "serverCertificate");
                                                bindata = NULL;
                                                if(ptrcJSONItem_Retrieval != NULL)
                                                    bindata = cJSON_GetStringValue(ptrcJSONItem_Retrieval);

                                                if(bindata != NULL)
                                                {
                                                    binDataDecodedMemorySize = (strlen(bindata) / 4 * 3 + 1);
                                                    if (binDataDecodedMemorySize< LPA_INITIAL_AUTHENTICATE_SERVER_CERTIFICATE_MAX_SIZE)
                                                    {
                                                            if (ffw_base64_decode(bindata, strlen(bindata), p_serverData->_serverCertificate.val, &len, LPA_INITIAL_AUTHENTICATE_SERVER_CERTIFICATE_MAX_SIZE))
                                                            {
                                                                    p_serverData->_serverCertificate.len = len;

                                                                    snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "serverCertificate (%lu bytes) :", CAST_SIZET_PLATFORM(len));
                                                                    lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "serverCertificate", p_serverData->_serverCertificate.val, len);
                                                            }
                                                            else
                                                            {
                                                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to decode serverCertificate from base64 !");
                                                                lpaSetErrorCode(LPA_ERROR_INVALID_SERVERCERTIFICATE);
                                                                isSuccess = false;
                                                            }
                                                    }
                                                    else
                                                    {
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_serverCertificate size exceeds maximum allowed (Current size needed:%d - Maximum size allowed:%d)!", binDataDecodedMemorySize, LPA_INITIAL_AUTHENTICATE_SERVER_CERTIFICATE_MAX_SIZE - 1);
                                                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                                        isSuccess = false;
                                                    }
                                                }
                                                else
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Failed to get serverCertificate from server");
                                                    lpaSetErrorCode(LPA_ERROR_INVALID_SERVERCERTIFICATE);
                                                    isSuccess = false;
                                                }
                                        }
                                        else
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot allocate memory for _serverCertificate !");
                                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                            isSuccess = false;
                                        }
                                    }

                                    // Temporary pointers cleanup, no need to free
                                    if(ptrcJSONItem_Retrieval != NULL)
                                        ptrcJSONItem_Retrieval = NULL;
                                    if(bindata != NULL)
                                        bindata = NULL;

                                    // JSON / Buffer cleanup
                                    _clearJsonAndBuffer(cjson, buffer);
                                    // Turnaround because pointers are not set to NULL by _clearJsonAndBuffer(). Can cause locking issue at the end.
                                    buffer = NULL;
                                    cjson = NULL;

                                    // Return OK ONLY if no problem during execution!
                                    if(isSuccess)
                                        res = true;
                                }
			}
			else
			{
                            // Enter here match to an error case or response type not expected here (Example 204)
                            // Set an error if not already performed by lpaManagerES9Plus_ExecutePost()
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Server communication error detected!");
                            lpaSetErrorCode(LPA_ERROR_SERVER_COMMUNICATION_ISSUE);
			}

		}
		else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Invalid address");
                    lpaSetErrorCode(LPA_ERROR_INVALID_SERVER_ADDRESS);
                }
                
                // do memory cleanup
                if (buffer != NULL)
                {
                    lpaCoreMemoryFree(buffer);
                    buffer = NULL;
                }
                httpMediaManagerHttpExecuteCleanup();
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot create JSON object jsonObject!");
                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
            }
	}
	else
	{
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "initiateAuthentication() - Invalid parameter");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
	}

	return res;
}


/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES9Plus_GetBoundProfilePackage(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrTransactionId, const char* ptrSmdpAddress, const unsigned char* ptrPrepareDownloadResponse)
{
	bool res = false;
	char * bindata = NULL;
	size_t len = 0;
	bool isSuccess = false;
	char* httpServerResp = NULL;
	size_t binDataDecodedMemorySize = 0;

	if (p_serverData != NULL && ptrTransactionId != NULL && ptrSmdpAddress != NULL && ptrPrepareDownloadResponse != NULL)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerGetBoundProfilePackage ...");
		cJSON* jsonObject = cJSON_CreateObject();
                
                if(jsonObject != NULL)
                {
                    cJSON_AddItemToObject(jsonObject, "transactionId", cJSON_CreateString(ptrTransactionId));
                    cJSON_AddItemToObject(jsonObject, "prepareDownloadResponse", cJSON_CreateString((char*)ptrPrepareDownloadResponse));
                    char* buffer = cJSON_Print(jsonObject);
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, ">>  HTTP request %s\n ", buffer);
                    cJSON_Delete(jsonObject);
                    jsonObject = NULL;

                    char getBoundProfileSmdpAddr[LPA_ADDRESS_MAX_SIZE];
                    memset(&getBoundProfileSmdpAddr, 0, sizeof(getBoundProfileSmdpAddr));
                    if ((strlen(ptrSmdpAddress) + strlen(GET_BOUND_PROFILE_PACKAGE_PATH))< LPA_ADDRESS_MAX_SIZE)
                    {
                        memcpy(getBoundProfileSmdpAddr, ptrSmdpAddress, strlen(ptrSmdpAddress));
                        memcpy(getBoundProfileSmdpAddr + strlen(ptrSmdpAddress), GET_BOUND_PROFILE_PACKAGE_PATH, strlen(GET_BOUND_PROFILE_PACKAGE_PATH));

                        long httpCode = 0;
                        httpServerResp = lpaManagerES9Plus_ExecutePost(getBoundProfileSmdpAddr, buffer, &isSuccess, &httpCode, ptrLpaEventCallback);

                        if (httpServerResp != NULL && isSuccess)
                        {
                            if (httpCode > 0)
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerGetBoundProfilePackage() => HTTP Request return httpCode=%d", httpCode);
                            else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_lpaManagerAuthenticateClientSendRequest() => HTTP Request return invalid httpCode !");

                            lpaCoreLogAppendLongText(SDK_LOG_LEVEL_DEBUG, "<<  HTTP response:", httpServerResp, strlen(httpServerResp));
                            cJSON* cjson = cJSON_Parse(httpServerResp);
                            httpMediaManagerHttpExecuteCleanup();

                            if (cjson == NULL)
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "json pack into cjson error...");
                                lpaSetErrorCode(LPA_ERROR_CJSON_PARSE_FAILURE);

                                // do memory cleanup
                                if (buffer != NULL)
                                {
                                        lpaCoreMemoryFree(buffer);
                                        buffer = NULL;
                                }
                                //httpMediaManagerHttpExecuteCleanup();
                                return res;
                            }
                            else
                            {
                                if (!_checkJSONresponseStatusSuccessful(cjson, ptrLpaEventCallback))
                                {
                                        _clearJsonAndBuffer(cjson, buffer);
                                        //httpMediaManagerHttpExecuteCleanup();
                                        return res;
                                }

                                // check transactionId
                                cJSON* ptrcJSONItem_TransactionID = cJSON_GetObjectItem(cjson, "transactionId");
                                char* ptrServerTransactionId = NULL;
                                if(ptrcJSONItem_TransactionID != NULL)
                                {
                                    ptrServerTransactionId = cJSON_GetStringValue(ptrcJSONItem_TransactionID);
                                    ptrcJSONItem_TransactionID = NULL;  // Enough to free it
                                }

                                if (ptrServerTransactionId == NULL)
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Failed to get transactionId from server");
                                    lpaSetErrorCode(LPA_ERROR_INVALID_TRANSACTIONID);
                                }
                                else
                                {
                                    if (strcmp(ptrServerTransactionId, ptrTransactionId) != 0)
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Invalid transactionId from server");
                                        lpaSetErrorCode(LPA_ERROR_INVALID_TRANSACTIONID);
                                    }
                                    ptrServerTransactionId = NULL; // Not allocated => no need to free it !
                                }

                                //boundProfilePackage
                                cJSON* ptrcJSON_boundProfilePackage = cJSON_GetObjectItem(cjson, "boundProfilePackage");
                                if (ptrcJSON_boundProfilePackage != NULL)
                                {
                                    if (cJSON_IsString(ptrcJSON_boundProfilePackage) || cJSON_IsRaw(ptrcJSON_boundProfilePackage))
                                    {
                                        if (cJSON_IsString(ptrcJSON_boundProfilePackage))
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "boundProfilePackage cJSON object is String type");

                                        if (cJSON_IsRaw(ptrcJSON_boundProfilePackage))
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "boundProfilePackage cJSON object is Raw type");

                                        //bindata = ptrcJSON_boundProfilePackage->valuestring;
                                        bindata = cJSON_GetStringValue(ptrcJSON_boundProfilePackage);

                                        if(bindata != NULL)
                                        {
                                            size_t bindataSize = strlen(bindata);
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "bindata size : %llu bytes", bindataSize);
                                            binDataDecodedMemorySize = (bindataSize / 4 * 3 + 1);
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "bindata decoded size : %d bytes", binDataDecodedMemorySize);
                                            p_serverData->_boundProfilePackage.val = lpaCoreMemoryAlloc(binDataDecodedMemorySize);
                                        }
                                        else
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Cannot retrieve value from boundProfilePackage cJSON object");
                                    }
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "boundProfilePackage cJSON object not String or Raw type");

                                    ptrcJSON_boundProfilePackage = NULL; // Do not delete it
                                }

                                // Arghhhhhhh, DO NOT DELETE CJSON HERE, CJSON DATA ARE USED JUST BELOW ...

                                p_serverData->_boundProfilePackage.len = 0;
                                if (p_serverData->_boundProfilePackage.val != NULL && bindata != NULL)
                                {
                                    memset(p_serverData->_boundProfilePackage.val, 0, sizeof(char) * binDataDecodedMemorySize);

                                    if (binDataDecodedMemorySize< LPA_GET_BOUND_PROFILE_MAX_SIZE)
                                    {						
                                        if (ffw_base64_decode(bindata, strlen(bindata), p_serverData->_boundProfilePackage.val, &len, binDataDecodedMemorySize))
                                        {
                                            p_serverData->_boundProfilePackage.len = len;

                                            snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "boundProfilePackage HEX (%lu bytes) :", CAST_SIZET_PLATFORM(len));
                                            lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "boundProfilePackage", p_serverData->_boundProfilePackage.val, len);

                                            res = true;
                                        }                     
                                        else
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to decode boundProfilePackage from base64 !");
                                    }
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_boundProfilePackage size (%d) is greater than maximum allowed :%d !", binDataDecodedMemorySize, LPA_GET_BOUND_PROFILE_MAX_SIZE - 1);
                                }
                                else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid boundProfilePackage from server !");

                                cJSON_Delete(cjson);
                                cjson = NULL;
                            } //end cjson null
                        }
                        else
                        {
                            // Enter here match to an error case or response type not expected here (Example 204)
                            // Set an error if not already performed by lpaManagerES9Plus_ExecutePost()
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Server communication error detected!");
                            lpaSetErrorCode(LPA_ERROR_SERVER_COMMUNICATION_ISSUE);
                        }
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Invalid Address");

                    // do memory cleanup
                    if (buffer != NULL)
                    {
                        lpaCoreMemoryFree(buffer);
                        buffer = NULL;
                    }
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot create JSON object jsonObject!");
                    lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                }
	}
	else
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Invalid parameter");
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
	}

	//httpMediaManagerHttpExecuteCleanup();
	return res;
}

/////////////////////////////////////////////////
//
/////////////////////////////////////////////////

bool lpaManagerES9Plus_AuthenticateClient(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrTransactionId, const char* ptrSmdpAddress, const unsigned char* ptrAuthenticateServerResponse)
{
	bool res = false;
	LPA_API_ERROR lpaError = LPA_NO_ERROR;
	char* ptrHttpAuthClientResp = NULL;
	cJSON* ptrcjsonHttpAuthClientResp = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerAuthenticateClient ...");

	if(p_serverData != NULL && ptrTransactionId != NULL && ptrSmdpAddress != NULL && ptrAuthenticateServerResponse != NULL)
        {    
            // Step 1 : authenticate client
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerAuthenticateClient() => Step1 : authenticateClient ...");

            ptrHttpAuthClientResp = _lpaManagerAuthenticateClientSendRequest(&lpaError, ptrTransactionId, ptrSmdpAddress, (const char*)ptrAuthenticateServerResponse, ptrLpaEventCallback);
            if (ptrHttpAuthClientResp == NULL || lpaError != LPA_NO_ERROR)
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "lpaManagerAuthenticateClient() => ptrHttpAuthClientResp is NULL or lpaError not equal to LPA_NO_ERROR");
                    lpaError = LPA_ERROR_AUTHENTICATE_CLIENT_EXCHANGE;
            }

            if (lpaError == LPA_NO_ERROR)
            {
                    // Step 2 : manage HTTP response
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerAuthenticateClient() => Step2 : manage response ...");

                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "<<  HTTP response %s", ptrHttpAuthClientResp);
                    ptrcjsonHttpAuthClientResp = cJSON_Parse(ptrHttpAuthClientResp);
                    if (ptrcjsonHttpAuthClientResp != NULL)
                    {
                        if (_checkJSONresponseStatusSuccessful(ptrcjsonHttpAuthClientResp, ptrLpaEventCallback))
                            lpaError = _lpaManagerAuthenticateClientCheckTransactionId(ptrcjsonHttpAuthClientResp, ptrTransactionId);
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "cJSON content error !");
                            lpaError = LPA_ERROR_FAILED_AUTHENTICATE_CLIENT;
                        }
                    }
                    else
                    {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "json pack into cjson error...");
                            lpaError = LPA_ERROR_CJSON_PARSE_FAILURE;
                    }
            }

            if (lpaError == LPA_NO_ERROR)
            {
                    // Step 3 : Extract data from HTTP response
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerAuthenticateClient() => Step3 : extract data from HTTP response ...");

                    lpaError = _lpaManagerAuthenticateClientExtractAllDataFromCJSON(ptrcjsonHttpAuthClientResp, p_serverData);
            }

            // Memory cleanup
            ///////////////////////////////////
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Do memory cleanup ...");
            if (ptrHttpAuthClientResp != NULL)
            {
                    //		lpaCoreMemoryFree(ptrHttpAuthClientResp); // Do not cleanup here, will be done inside httpMediaManagerHttpExecuteCleanup() !!!
                    ptrHttpAuthClientResp = NULL;
            }

            if (ptrcjsonHttpAuthClientResp != NULL)
            {
                    cJSON_Delete(ptrcjsonHttpAuthClientResp);
                    ptrcjsonHttpAuthClientResp = NULL;
            }

            httpMediaManagerHttpExecuteCleanup();

            if (lpaError == LPA_NO_ERROR)
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerAuthenticateClient() =>  return TRUE");
                    res = true;
            }
            else
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerAuthenticateClient() => LPA Error : 0x%04X", lpaError);
                    lpaSetErrorCode(lpaError);
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Invalid parameter");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }

	return res;
}


/////////////////////////////////////////////////
//
/////////////////////////////////////////////////

char* _lpaManagerAuthenticateClientSendRequest(LPA_API_ERROR* ptrLpaError, const char* ptrTransactionId, const char* ptrSmdpAddress, const char* ptrAuthenticateServerResponse, const LPA_EventCallback* ptrLpaEventCallback)
{
	LPA_API_ERROR lpaError = LPA_NO_ERROR;
	char* ptrHttpAuthClientResp = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerAuthenticateClientSendRequest ...");

	if (ptrLpaError != NULL && ptrTransactionId != NULL && ptrSmdpAddress != NULL && ptrAuthenticateServerResponse != NULL)
	{
		cJSON* jsonObject = cJSON_CreateObject();
		if (jsonObject != NULL)
		{
			cJSON_AddItemToObject(jsonObject, "transactionId", cJSON_CreateString(ptrTransactionId));
			cJSON_AddItemToObject(jsonObject, "authenticateServerResponse", cJSON_CreateString((char*)ptrAuthenticateServerResponse));
			char* ptrBufferJSON = cJSON_Print(jsonObject);
			if (ptrBufferJSON != NULL)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, ">>  HTTP request %s", ptrBufferJSON);

				char authClientSmdpAddr[LPA_ADDRESS_MAX_SIZE];
				memset(&authClientSmdpAddr, 0, sizeof(authClientSmdpAddr));
				if ((strlen(ptrSmdpAddress) + strlen(AUTHENTICATE_CLIENT_PATH)) < LPA_ADDRESS_MAX_SIZE)
				{
					memcpy(authClientSmdpAddr, ptrSmdpAddress, strlen(ptrSmdpAddress));
					memcpy(authClientSmdpAddr + strlen(ptrSmdpAddress), AUTHENTICATE_CLIENT_PATH, strlen(AUTHENTICATE_CLIENT_PATH));

					bool isSuccess = false;
					long httpCode = 0;
					ptrHttpAuthClientResp = lpaManagerES9Plus_ExecutePost(authClientSmdpAddr, ptrBufferJSON, &isSuccess, &httpCode, ptrLpaEventCallback);
					//transactionId+profileMetadata+smdpSigned2+smdpSignature2+smdpCertificate

					if (ptrHttpAuthClientResp != NULL)
					{
						if (isSuccess)
						{
							if( httpCode > 0 )
								lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerAuthenticateClientSendRequest() => HTTP Request return httpCode=%d", httpCode);
							else
								lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_lpaManagerAuthenticateClientSendRequest() => HTTP Request return invalid httpCode !");
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, ">>  HTTP response : %s", ptrHttpAuthClientResp);
						}
						else
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Invalid response !");
							lpaError = LPA_ERROR_AUTHENTICATE_CLIENT_EXCHANGE;

							lpaCoreMemoryFree(ptrHttpAuthClientResp);
							ptrHttpAuthClientResp = NULL;
						}
					}
					else
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Server communication issue (ptrHttpAuthClientResp is NULL)");
						lpaError = LPA_ERROR_AUTHENTICATE_CLIENT_EXCHANGE;
					}
				}
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No enought memory to manage authClientSmdpAddr !");
					lpaError = LPA_ERROR_INSUFFICIENT_BUFFER;
				}
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Unable to create CJSON object !");
				lpaError = LPA_ERROR_INSUFFICIENT_BUFFER;
			}
                                               
			cJSON_Delete(jsonObject);
			jsonObject = NULL;
                        
            // do memory cleanup DAVY
            if (ptrBufferJSON != NULL)
            {
                    lpaCoreMemoryFree(ptrBufferJSON);
                    ptrBufferJSON = NULL;
            }
                 
			//httpMediaManagerHttpExecuteCleanup();
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No enough memory to create CJSON object !");
			lpaError = LPA_ERROR_INSUFFICIENT_BUFFER;
		}
	}
        else
            lpaError = LPA_ERROR_INVALID_PARAMETER;

	if (ptrLpaError != NULL)
		*ptrLpaError = lpaError;

	return ptrHttpAuthClientResp;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

LPA_API_ERROR _lpaManagerAuthenticateClientCheckTransactionId(cJSON* ptrcjsonHttpAuthClientResp, const char* ptrTransactionId)
{
	LPA_API_ERROR lpaError = LPA_NO_ERROR;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerAuthenticateClientCheckTransactionId ...");

	if (ptrcjsonHttpAuthClientResp != NULL && ptrTransactionId != NULL)
	{
		cJSON* ptrcJSONItem_TransactionID = cJSON_GetObjectItem(ptrcjsonHttpAuthClientResp, "transactionId");
                char* ptrServerTransactionId = NULL;
                if(ptrcJSONItem_TransactionID != NULL)
                {
                    ptrServerTransactionId = cJSON_GetStringValue(ptrcJSONItem_TransactionID);
                    ptrcJSONItem_TransactionID = NULL;  // Enough to free it
                }
                    
		if (ptrServerTransactionId != NULL)
		{
			if (strlen(ptrServerTransactionId) < LPA_TRANSACTION_ID_MAX_SIZE)
			{
				if (strcmp(ptrTransactionId, ptrServerTransactionId) != 0)
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "incorrect transactionId !");
					lpaError = LPA_ERROR_INVALID_TRANSACTIONID;
				}
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Failed to get transactionId from server");
				lpaError = LPA_ERROR_INVALID_TRANSACTIONID;
			}

			ptrServerTransactionId = NULL; // No need to free it !!!
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Failed to get transactionId from server");
			lpaError = LPA_ERROR_INVALID_TRANSACTIONID;
		}

	}
        else
            lpaError = LPA_ERROR_INVALID_PARAMETER;

	return lpaError;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

LPA_API_ERROR _lpaManagerAuthenticateClientExtractAllDataFromCJSON(cJSON* ptrcjsonHttpAuthClientResp, ptr_serverData p_serverData)
{
	LPA_API_ERROR lpaError = LPA_NO_ERROR;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerAuthenticateClientExtractAllDataFromCJSON ...");

	if (ptrcjsonHttpAuthClientResp != NULL && p_serverData != NULL)
	{
		if (lpaError == LPA_NO_ERROR)
		{
			if (p_serverData->_smdpSigned2.val == NULL)
				p_serverData->_smdpSigned2.val = lpaCoreMemoryAlloc(LPA_AUTHENTICATE_CLIENT_SMDP_SIGNED2_MAX_SIZE);

			p_serverData->_smdpSigned2.len = 0;

			if (p_serverData->_smdpSigned2.val != NULL)
			{
				lpaError = _lpaManagerAuthenticateClientExtractDataFromCJSON(ptrcjsonHttpAuthClientResp, p_serverData->_smdpSigned2.val,
					&(p_serverData->_smdpSigned2.len), LPA_AUTHENTICATE_CLIENT_SMDP_SIGNED2_MAX_SIZE, "smdpSigned2");
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to allocate p_serverData->_smdpSigned2.val (expected : %d bytes)!", LPA_AUTHENTICATE_CLIENT_SMDP_SIGNED2_MAX_SIZE);
				lpaError = LPA_ERROR_INSUFFICIENT_BUFFER;
			}
		}

		if (lpaError == LPA_NO_ERROR)
		{
			if (p_serverData->_smdpSignature2.val == NULL)
				p_serverData->_smdpSignature2.val = lpaCoreMemoryAlloc(LPA_AUTHENTICATE_CLIENT_SMDP_SIGNATURE2_MAX_SIZE);

			p_serverData->_smdpSignature2.len = 0;

			if (p_serverData->_smdpSignature2.val != NULL)
			{
				lpaError = _lpaManagerAuthenticateClientExtractDataFromCJSON(ptrcjsonHttpAuthClientResp, p_serverData->_smdpSignature2.val,
					&(p_serverData->_smdpSignature2.len), LPA_AUTHENTICATE_CLIENT_SMDP_SIGNATURE2_MAX_SIZE, "smdpSignature2");
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to allocate p_serverData->_smdpSignature2.val (expected : %d bytes)!", LPA_AUTHENTICATE_CLIENT_SMDP_SIGNATURE2_MAX_SIZE);
				lpaError = LPA_ERROR_INSUFFICIENT_BUFFER;
			}
		}

		if (lpaError == LPA_NO_ERROR)
		{
			if (p_serverData->_profileMetadata.val == NULL)
				p_serverData->_profileMetadata.val = lpaCoreMemoryAlloc(LPA_AUTHENTICATE_CLIENT_PROFILE_METADATA_MAX_SIZE);

			p_serverData->_profileMetadata.len = 0;

			if (p_serverData->_profileMetadata.val != NULL)
			{
				lpaError = _lpaManagerAuthenticateClientExtractDataFromCJSON(ptrcjsonHttpAuthClientResp, p_serverData->_profileMetadata.val,
					&(p_serverData->_profileMetadata.len), LPA_AUTHENTICATE_CLIENT_PROFILE_METADATA_MAX_SIZE, "profileMetadata");
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to allocate p_serverData->_profileMetadata.val (expected : %d bytes)!", LPA_AUTHENTICATE_CLIENT_PROFILE_METADATA_MAX_SIZE);
				lpaError = LPA_ERROR_INSUFFICIENT_BUFFER;
			}
		}

		if (lpaError == LPA_NO_ERROR)
		{
			if (p_serverData->_smdpCertificate.val == NULL)
				p_serverData->_smdpCertificate.val = lpaCoreMemoryAlloc(LPA_AUTHENTICATE_CLIENT_SMDP_CERTIFICATE_MAX_SIZE);

			p_serverData->_smdpCertificate.len = 0;

			if (p_serverData->_smdpCertificate.val != NULL)
			{
				lpaError = _lpaManagerAuthenticateClientExtractDataFromCJSON(ptrcjsonHttpAuthClientResp, p_serverData->_smdpCertificate.val,
					&(p_serverData->_smdpCertificate.len), LPA_AUTHENTICATE_CLIENT_SMDP_CERTIFICATE_MAX_SIZE, "smdpCertificate");
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to allocate p_serverData->_smdpCertificate.val (expected : %d bytes)!", LPA_AUTHENTICATE_CLIENT_SMDP_CERTIFICATE_MAX_SIZE);
				lpaError = LPA_ERROR_INSUFFICIENT_BUFFER;
			}
		}
	}
	else
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "ptrcjsonHttpAuthClientResp or/and p_serverData NULL !!");
		lpaError = LPA_ERROR_AUTHENTICATE_CLIENT_EXCHANGE;
	}


	return lpaError;
}

LPA_API_ERROR _lpaManagerAuthenticateClientExtractDataFromCJSON(cJSON* ptrcjsonHttpAuthClientResp, unsigned char* ptrBuffer, size_t* ptrBufferSize, size_t bufferSizeMax, const char* ptrName)
{
	LPA_API_ERROR lpaError = LPA_NO_ERROR;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerAuthenticateClientExtractDataFromCJSON ...");

	if (ptrcjsonHttpAuthClientResp != NULL && ptrBuffer != NULL && ptrBufferSize != NULL && bufferSizeMax > 0 && ptrName != NULL)
	{
		char * ptrBindata = NULL;
		size_t binDataDecodedMemorySize = 0;
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Extracting data for <%s> ...", ptrName);

		memset(ptrBuffer, 0, bufferSizeMax);
		cJSON* ptrcJsonName = cJSON_GetObjectItem(ptrcjsonHttpAuthClientResp, ptrName);
		if (ptrcJsonName != NULL)
		{
                        ptrBindata = cJSON_GetStringValue(ptrcJsonName);
			ptrcJsonName = NULL; // Enough to free it
                        
                        if(ptrBindata != NULL)
                        {
                            binDataDecodedMemorySize = (strlen(ptrBindata) / 4 * 3 + 1);
                            if (binDataDecodedMemorySize < bufferSizeMax)
                            {
                                    if (ffw_base64_decode(ptrBindata, strlen(ptrBindata), ptrBuffer, ptrBufferSize, bufferSizeMax))
                                    {
                                        if((*ptrBufferSize * 2) < sizeof(_bufferFormatLogMessage))
                                        {
                                            if (formatBytesToHexaString(ptrBuffer, *ptrBufferSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "%s : %s ", ptrName, _bufferFormatLogMessage);
                                            else
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "%s  ...", ptrName);
                                        }
                                        else
                                        {
                                            // For long data exceeding _bufferFormatLogMessage, avoid "Name ..." if conversion failed due to text buffer size
                                            lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, NULL, ptrName, ptrBuffer, *ptrBufferSize);
                                        }
                                    }
                                    else
                                    {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to decode %s from base64 !", ptrName);
                                            lpaError = LPA_ERROR_AUTHENTICATE_CLIENT_EXCHANGE;
                                    }
                            }
                            else
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "%s buffer too small (Expected : %d bytes, Maximum allowed : %d )!", ptrName, binDataDecodedMemorySize, bufferSizeMax - 1);
                                    lpaError = LPA_ERROR_INSUFFICIENT_BUFFER;
                            }
                            
                            ptrBindata = NULL;  // Enough to free
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to parse %s from JSON object !", ptrName);
                            lpaError = LPA_ERROR_AUTHENTICATE_CLIENT_EXCHANGE;
                        }
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "lpaManagerAuthenticateClientExtractDataFromCJSON () => '%s' not found on JSON response", ptrName);
			lpaError = LPA_ERROR_AUTHENTICATE_CLIENT_EXCHANGE;
		}
	}
	else
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "lpaManagerAuthenticateClientExtractDataFromCJSON() => one or more parameter NULL !!");
		lpaError = LPA_ERROR_AUTHENTICATE_CLIENT_EXCHANGE;
	}

	return lpaError;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES9Plus_HandleNotification(const char* ptrSmdpAddr, size_t smdpAddrSize, const unsigned char* ptrPendingNotification, const LPA_EventCallback* ptrLpaEventCallback)
{
	bool res = false;
	cJSON* jsonObject = cJSON_CreateObject();
	char * httpServerResp = NULL;
	bool isSuccess = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ES9plus handleNotification ...");
	if (ptrPendingNotification != NULL && ptrSmdpAddr != NULL && smdpAddrSize > 0)
	{
            if(jsonObject != NULL)
            {
                cJSON_AddItemToObject(jsonObject, "pendingNotification", cJSON_CreateString((char*)ptrPendingNotification));

                char* buffer = cJSON_Print(jsonObject);
                cJSON_Delete(jsonObject);
                jsonObject = NULL;

                if(buffer != NULL)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, ">>  HTTP request %s\n", buffer);

                    char pirNotiSmdpAddr[LPA_ADDRESS_MAX_SIZE];
                    memset(&pirNotiSmdpAddr, 0, sizeof(pirNotiSmdpAddr));
                    if (smdpAddrSize + strlen(HANDLE_NOTIFICATION_PATH) < LPA_ADDRESS_MAX_SIZE)
                    {
                        memcpy(pirNotiSmdpAddr, ptrSmdpAddr, smdpAddrSize);
                        memcpy(pirNotiSmdpAddr + smdpAddrSize, HANDLE_NOTIFICATION_PATH, strlen(HANDLE_NOTIFICATION_PATH));

                        long httpCode = 0;
                        httpServerResp = lpaManagerES9Plus_ExecutePost(pirNotiSmdpAddr, buffer, &isSuccess, &httpCode, ptrLpaEventCallback);

                        if (isSuccess)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES9Plus_ExecutePost() return true (httpCode=%d)", httpCode);

                            switch (httpCode)
                            {
                                case 204:
                                        // HTTP Code 204 => No data expected
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Success to send notification to server and get the status code (No data expected)");
                                        res = true;
                                break;

                                case 200:
                                        // HTTP Code 200 => data expected 
                                        if (httpServerResp != NULL)
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "HTTP Code 200 with data always present on error case (on notification management). Try to retrieve error code.");
                                            
                                            // Process data returned to retrieve error code from server
                                            cJSON* cjson = cJSON_Parse(httpServerResp);
                                            if (cjson != NULL) 
                                            {
                                                if (_checkJSONresponseStatusSuccessful(cjson, ptrLpaEventCallback))
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No error reported in JSON object...");
                                                
                                                cJSON_Delete(cjson);
                                                cjson = NULL;
                                            }
                                            else
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "json pack into cjson error...");
                                        }
                                        else
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "HTTP Code 200 without data !");
                                        lpaSetErrorCode(LPA_ERROR_FAILED_SEND_NOTIFICATION_OR_NOT_GET_STATUS_CODE);
                                break;

                                default:
                                        if (httpCode >= 200 && httpCode < 300)
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "handleNotification() => Other 2xx HTTP Code not authorized for Handle Notification !");
                                            lpaSetErrorCode(LPA_ERROR_FAILED_SEND_NOTIFICATION_OR_NOT_GET_STATUS_CODE);
                                        }
                                        else
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "handleNotification() => Not 2xx HTTP Code ");
                                            lpaSetErrorCode(LPA_ERROR_FAILED_SEND_NOTIFICATION_OR_NOT_GET_STATUS_CODE);
                                        }
                                break;
                            }
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to send successfully notification to server");
                            lpaSetErrorCode(LPA_ERROR_FAILED_SEND_NOTIFICATION_OR_NOT_GET_STATUS_CODE);
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No enough memory to update pirNotiSmdpAddr !");
                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                    }

                    // do memory cleanup
                    if (buffer != NULL)
                    {
                            lpaCoreMemoryFree(buffer);
                            buffer = NULL;
                    }
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve http request from JSON object !");
                    lpaSetErrorCode(LPA_ERROR_CJSON_PARSE_FAILURE);
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot create JSON object jsonObject!");
                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
            }    
	}
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Invalid parameter");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }
	
	httpMediaManagerHttpExecuteCleanup();
	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES9Plus_EventRetrieval(const char* ptrTransactionId, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrSmdsAddress, const unsigned char* ptrAuthenticateServerResponse, EVENT_RECORD_LIST* ptrEventRecordList)
{
	bool res = false;
	char* httpResp = NULL;
	bool isSuccess = false;

	if (ptrTransactionId != NULL && ptrSmdsAddress != NULL && ptrAuthenticateServerResponse != NULL && ptrEventRecordList != NULL) {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerEventRetrieval ...");

            cJSON* jsonObject = cJSON_CreateObject();
            if(jsonObject != NULL)
            {
		cJSON_AddItemToObject(jsonObject, "transactionId", cJSON_CreateString(ptrTransactionId));
		cJSON_AddItemToObject(jsonObject, "authenticateServerResponse", cJSON_CreateString((char*)ptrAuthenticateServerResponse));
		char* buffer = cJSON_Print(jsonObject);
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, ">>  HTTP request %s\n ", buffer);
		cJSON_Delete(jsonObject);
		jsonObject = NULL;

		char anthClientSmdsAddr[LPA_ADDRESS_MAX_SIZE];
		memset(&anthClientSmdsAddr, 0, sizeof(anthClientSmdsAddr));
		if ((strlen(ptrSmdsAddress) + strlen(AUTHENTICATE_CLIENT_PATH))< LPA_ADDRESS_MAX_SIZE)
		{
			memcpy(anthClientSmdsAddr, ptrSmdsAddress, strlen(ptrSmdsAddress));
			memcpy(anthClientSmdsAddr + strlen(ptrSmdsAddress), AUTHENTICATE_CLIENT_PATH, strlen(AUTHENTICATE_CLIENT_PATH));

			long httpCode = 0;
			httpResp = lpaManagerES9Plus_ExecutePost(anthClientSmdsAddr, buffer, &isSuccess, &httpCode, ptrLpaEventCallback);

			if (httpResp != NULL && isSuccess)
			{
				if (httpCode > 0)
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerEventRetrieval() => HTTP Request return httpCode=%d", httpCode);
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "lpaManagerEventRetrieval() => HTTP Request return invalid httpCode !");

                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "<<  HTTP response %s \n ", httpResp);
                                cJSON* cjson = cJSON_Parse(httpResp);

                                if (cjson == NULL)
                                {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "json pack into cjson error...");
                                        lpaSetErrorCode(LPA_ERROR_CJSON_PARSE_FAILURE);

                                        if (buffer != NULL)
                                        {
                                                lpaCoreMemoryFree(buffer);
                                                buffer = NULL;
                                        }
                                        httpMediaManagerHttpExecuteCleanup();
                                        return res;
                                }
                                else
                                {

                                        if (!_checkJSONresponseStatusSuccessful(cjson, ptrLpaEventCallback))
                                        {
                                                _clearJsonAndBuffer(cjson, buffer);
                                                httpMediaManagerHttpExecuteCleanup();
                                                return res;
                                        }

                                        cJSON *eventEntries = cJSON_GetObjectItem(cjson, "eventEntries");

                                        if (eventEntries != NULL)
                                        {
                                                int i;
                                                int count = cJSON_GetArraySize(eventEntries);
                                                ptrEventRecordList->countEvent = count;
                                                if (count == 0)
                                                {
                                                        //
                                                }
                                                else
                                                {
                                                        for (i = 0; i < count; i++)
                                                        {
                                                                cJSON *item = cJSON_GetArrayItem(eventEntries, i);

                                                                if (item != NULL)
                                                                {
                                                                        char* tempstr = NULL;
                                                                        cJSON* ptrcJSONItem_Retrieval = cJSON_GetObjectItem(item, "eventId");
                                                                        if(ptrcJSONItem_Retrieval != NULL)
                                                                            tempstr = cJSON_GetStringValue(ptrcJSONItem_Retrieval);

                                                                        if (tempstr != NULL && strlen(tempstr) < LPA_MATCHING_ID_SIZE)
                                                                        {
                                                                                memcpy(ptrEventRecordList->eventRecordList[i].eventId, tempstr, strlen(tempstr));
                                                                                lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "the eventId: %s", ptrEventRecordList->eventRecordList[i].eventId);
                                                                        }
                                                                        else
                                                                        {
                                                                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Failed to get eventId");
                                                                                lpaSetErrorCode(LPA_ERROR_INVALID_EVENT_ID);
                                                                        }

                                                                        tempstr = NULL;
                                                                        ptrcJSONItem_Retrieval = cJSON_GetObjectItem(item, "rspServerAddress");
                                                                        if(ptrcJSONItem_Retrieval != NULL)
                                                                        {
                                                                            tempstr = cJSON_GetStringValue(ptrcJSONItem_Retrieval);
                                                                            ptrcJSONItem_Retrieval = NULL;
                                                                        }

                                                                        if (tempstr != NULL && strlen(tempstr) < LPA_SMDP_ADDRESS_SIZE){
                                                                                memcpy(ptrEventRecordList->eventRecordList[i].rspServerAddress, tempstr, strlen(tempstr));
                                                                                lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "the rspServerAddress: %s", ptrEventRecordList->eventRecordList[i].rspServerAddress);
                                                                                tempstr = NULL; // Enough to free it
                                                                        }
                                                                        else
                                                                        {
                                                                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Failed to get rspServerAddress");
                                                                                lpaSetErrorCode(LPA_ERROR_INVALID_RSP_SERVER_ADDRESS);
                                                                        }

                                                                        // Cleanup
                                                                        item = NULL;
                                                                }
                                                                else
                                                                {
                                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Failed to get event entries list");
                                                                        lpaSetErrorCode(LPA_ERROR_INVALID_EVENT_ENTRIES);
                                                                }

                                                        }

                                                }
                                                eventEntries = NULL; // Enough to free it
                                        }
                                        else
                                        {
                                                lpaSetErrorCode(LPA_ERROR_INVALID_EVENT_ENTRIES);
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Not found eventEntries");
                                        }
                                        cJSON_Delete(cjson);
                                        cjson = NULL;

                                        res = true;
                                }// end of cjson not NULL

			}
			else
			{
                            // Enter here match to an error case or response type not expected here (Example 204)
                            // Set an error if not already performed by lpaManagerES9Plus_ExecutePost()
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Server communication error detected!");
                            lpaSetErrorCode(LPA_ERROR_SERVER_COMMUNICATION_ISSUE);
			}

		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, " Invalid Address");

		// do memory cleanup
		if (buffer != NULL)
		{
			lpaCoreMemoryFree(buffer);
			buffer = NULL;
		}
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot create JSON object jsonObject!");
                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
            }

	}
	else{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Invalid parameter");
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
	}

	httpMediaManagerHttpExecuteCleanup();
	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Return true if JSON object "status" key = "Executed-Success" or "eventEntries" key found
 * @param cjson JSON object to evaluate
 * @return Status of checking
 */
bool _checkJSONresponseStatusSuccessful(cJSON* cjson, const LPA_EventCallback* ptrLpaEventCallback)
{
	bool res = false;

	cJSON* header = NULL;
	cJSON* functionExecutionStatus = NULL;
	cJSON* statusCodeData = NULL;
	char* status = NULL;
	char* subcode = NULL;
	char* rescode = NULL;
        
        if(cjson != NULL)
        {
            if (cJSON_HasObjectItem(cjson, "header"))
            {
                    header = cJSON_GetObjectItem(cjson, "header");
                    functionExecutionStatus = cJSON_GetObjectItem(header, "functionExecutionStatus");

                    if (functionExecutionStatus != NULL)
                    {
                            cJSON* ptrcJSON_status = cJSON_GetObjectItem(functionExecutionStatus, "status");

                            if (ptrcJSON_status == NULL )
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "no Status");
                                    return res;
                            }
                            
                            status = cJSON_GetStringValue(ptrcJSON_status);
                            if (status != NULL)
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "_checkJSONresponseStatusSuccessful(): status : %s ", status);
                                    
                                    // Normally we shall no have case sensitive comparison problems, but some servers are not correctly programmed
                                    if (compareEqualStringIgnoringCase(status, "Executed-Success"))
                                        res = true;

                                    statusCodeData = cJSON_GetObjectItem(functionExecutionStatus, "statusCodeData");
                                    if (statusCodeData != NULL)
                                    {
                                            cJSON* ptrcJSON_subjectCode = cJSON_GetObjectItem(statusCodeData, "subjectCode");
                                            cJSON* ptrcJSON_reasonCode = cJSON_GetObjectItem(statusCodeData, "reasonCode");
                                            
                                            subcode = (ptrcJSON_subjectCode != NULL ? cJSON_GetStringValue(ptrcJSON_subjectCode) : NULL);
                                            rescode = (ptrcJSON_reasonCode != NULL ? cJSON_GetStringValue(ptrcJSON_reasonCode) : NULL);


                                            lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "_checkJSONresponseStatusSuccessful(): subjectCode: %s reasonCode : %s",
                                                    (subcode != NULL ? subcode : "N/A"), (rescode != NULL ? rescode : "N/A"));
                                            
                                            if (ptrLpaEventCallback != NULL && (subcode != NULL || rescode != NULL) )
                                            {
                                                if (ptrLpaEventCallback->_lpaEventExecutionError != NULL)
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Creating Notification Error ...");
                                                    LPA_EVENT_EXECUTION_ERROR_INFO eventExecutionErrorInfo;
                                                    memset(&eventExecutionErrorInfo, 0x00, sizeof(LPA_EVENT_EXECUTION_ERROR_INFO));

                                                    eventExecutionErrorInfo.executionErrorType = LPA_EVENT_EXECUTION_SERVER_ERROR_TYPE;
                                                    eventExecutionErrorInfo.detailErrorMask = LPA_EVENT_EXECUTION_ERROR_NO_DETAIL_MASK; // By default

                                                    if (subcode != NULL)
                                                    {
                                                            eventExecutionErrorInfo.detailErrorMask |= LPA_EVENT_EXECUTION_ERROR_SUBJECT_CODE_MASK;
                                                            eventExecutionErrorInfo.ptrErrorSubjectCode = subcode;
                                                    }

                                                    if (rescode != NULL)
                                                    {
                                                            eventExecutionErrorInfo.detailErrorMask |= LPA_EVENT_EXECUTION_ERROR_REASON_CODE_MASK;
                                                            eventExecutionErrorInfo.ptrErrorReasonCode = rescode;
                                                    }

                                                    if (eventExecutionErrorInfo.detailErrorMask != LPA_EVENT_EXECUTION_ERROR_NO_DETAIL_MASK)
                                                    {
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Sending Notification Error to LPA application ...");
                                                            ptrLpaEventCallback->_lpaEventExecutionError(ptrLpaEventCallback->_appParameter, &eventExecutionErrorInfo);
                                                    }
                                                }
                                            }

                                            // Cleanup
                                            ptrcJSON_subjectCode = NULL;
                                            ptrcJSON_reasonCode = NULL;
                                    }
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_checkJSONresponseStatusSuccessful(): \"status\" value string cannot be retrieved.");
                       
                        // Cleanup
                        functionExecutionStatus = NULL;
                        ptrcJSON_status = NULL;
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "_checkJSONresponseStatusSuccessful(): No functionExecutionStatus");
                    
                    // Cleanup
                    header = NULL;
 
            }
            else
                if (cJSON_HasObjectItem(cjson, "eventEntries"))
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "_checkJSONresponseStatusSuccessful(): eventEntries found => OK");
                    res = true;
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "_checkJSONresponseStatusSuccessful(): No header");
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_checkJSONresponseStatusSuccessful(): Passed JSON object NULL !");
	
	return res;
}


/**
 * Return true if error code returned in JSON object match Subject Code and Reason Code specified
 * @param p_cjson JSON object to evaluate
 * @param p_subjectCode Subject Code to compare, string format
 * @param p_reasonCode Reason Code to compare, string format
 * @return Status of checking
 */
bool _checkJSONresponseErrorCode(const cJSON* p_cjson, const char * p_subjectCode, const char * p_reasonCode)
{
	bool res = false;

	cJSON* header = NULL;
	cJSON* functionExecutionStatus = NULL;
	cJSON* statusCodeData = NULL;
	char* subcode = NULL;
	char* rescode = NULL;
        
        if((p_cjson != NULL) && (p_subjectCode != NULL) && (p_reasonCode != NULL))
        {
            if (cJSON_HasObjectItem(p_cjson, "header"))
            {
                    header = cJSON_GetObjectItem(p_cjson, "header");
                    functionExecutionStatus = cJSON_GetObjectItem(header, "functionExecutionStatus");

                    if (functionExecutionStatus != NULL)
                    {
                        statusCodeData = cJSON_GetObjectItem(functionExecutionStatus, "statusCodeData");
                        if (statusCodeData != NULL)
                        {
                            cJSON* ptrcJSON_subjectCode = cJSON_GetObjectItem(statusCodeData, "subjectCode");
                            cJSON* ptrcJSON_reasonCode = cJSON_GetObjectItem(statusCodeData, "reasonCode");
                            
                            subcode = (ptrcJSON_subjectCode != NULL ? cJSON_GetStringValue(ptrcJSON_subjectCode) : NULL);
                            rescode = (ptrcJSON_reasonCode != NULL ? cJSON_GetStringValue(ptrcJSON_reasonCode) : NULL);

                            if((subcode != NULL) && (rescode != NULL))
                            {
                                res = (strcmp(subcode, p_subjectCode) == 0) ? true : false;
                                if(res)
                                    res = (strcmp(rescode, p_reasonCode) == 0) ? true : false;
                            }
                            
                            lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "_checkJSONresponseErrorCode(): subjectCode: %s reasonCode : %s - Check status = %s",
                               (subcode != NULL ? subcode : "N/A"), (rescode != NULL ? rescode : "N/A"), (res ? "True" : "False"));
                            
                            // Cleanup
                            statusCodeData = NULL;
                            ptrcJSON_subjectCode = NULL;
                            ptrcJSON_reasonCode = NULL;
                        }
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "_checkJSONresponseErrorCode(): No statusCodeData");
                        
                        // Cleanup
                        functionExecutionStatus = NULL;
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "_checkJSONresponseErrorCode(): No functionExecutionStatus");
                    
                    // Cleanup
                    header = NULL;
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "_checkJSONresponseErrorCode(): No header");
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_checkJSONresponseErrorCode(): Parameter NULL detected!");
	
	return res;
}


void  _clearJsonAndBuffer(cJSON * ptrcjson, char *ptrBuffer)
{
	if (ptrcjson != NULL)
	{
		cJSON_Delete(ptrcjson);
		ptrcjson = NULL;
	}

	if (ptrBuffer != NULL)
	{
		lpaCoreMemoryFree(ptrBuffer);
		ptrBuffer = NULL;
	}
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Submit Cancel Session request to SM-DP server
 * @param transactionID TransactionID of current server session, string format
 * @param ptrCancelSessionResp Pointer on CANCEL_SESSION_RESPONSE object containing cancelSessionResponse issued by eUICC
 * @return True if operations OK and server response OK
 */
bool lpaManagerES9plus_CancelSession(const char * transactionID, CANCEL_SESSION_RESPONSE * ptrCancelSessionResp, const char* ptrSmdpAddress, const LPA_EventCallback* ptrLpaEventCallback)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES9plus_CancelSession...");
    
    if ((transactionID != NULL) && (strlen(transactionID) > 0) && (strlen(transactionID) < LPA_TRANSACTION_ID_MAX_SIZE) && (ptrCancelSessionResp != NULL) && (ptrSmdpAddress != NULL))
    {
        // Reminder: Request = BF41 L cancelSesionResponseOk
        
        // We use response data directly given by the eUICC instead of cancelSessionResponseOK object because it is already encapsulated in BF41 TLV
        if ((ptrCancelSessionResp->cancelSessionResponse_RawDataSize > 0) && (ptrCancelSessionResp->resultOK == true))
        {
            if(_lpaManagerES9SendCancelSession(transactionID, ptrCancelSessionResp, ptrSmdpAddress, ptrLpaEventCallback))
            {
                res = true;
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES9plus_CancelSession: Request sending is successful.");
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "lpaManagerES9plus_CancelSession: Request sending failed.");
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "lpaManagerES9plus_CancelSession: Invalid cancelSessionResponse object.");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "lpaManagerES9plus_CancelSession: Invalid parameter.");
    
    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Build server request (JSON object) and send it to server
 * @param transactionID Input, TransactionID of current server session, string format
 * @param ptrCancelSessionResp  Pointer on CANCEL_SESSION_RESPONSE object containing cancelSessionResponse issued by eUICC
 * @param ptrSmdpAddress Pointer on SM-DP server address, string format
 * @return true if sending is successful
 */
bool _lpaManagerES9SendCancelSession(const char * transactionID, CANCEL_SESSION_RESPONSE * ptrCancelSessionResp, const char* ptrSmdpAddress, const LPA_EventCallback* ptrLpaEventCallback)
{
    bool res = false;
    bool isExchangeSuccess = false;
    size_t cancelSessionResponseBase64size = 0;
    long httpCode = 0;
    char* httpServerResp = NULL;
    size_t sizeBase64 = 0;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerES9SendCancelSession...");
    
     // Check input parameters before building
    if ((transactionID != NULL) && (strlen(transactionID) > 0) && (strlen(transactionID) < LPA_TRANSACTION_ID_MAX_SIZE) && (ptrCancelSessionResp != NULL) && (ptrSmdpAddress != NULL))
    {
        if ((ptrCancelSessionResp->cancelSessionResponse_RawDataSize > 0) && (ptrCancelSessionResp->resultOK == true))
        {
            // Converts cancelSessionResponse in Base64
            // Size "original_byte_object * 2" is enough, Base64 will be always smaller than ASCII translation.
            sizeBase64 = ptrCancelSessionResp->cancelSessionResponse_RawDataSize * 2;
            char * cancelSessionResponseBase64 = lpaCoreMemoryAlloc(sizeBase64);
            if (cancelSessionResponseBase64 != NULL)
            {
                // Turnaround to avoid problems with Base64 conversion that does not add \0 character at the end of Base64 object
                // \0 is needed by cJSON_CreateString() that needs a string as entry parameter
                memset(cancelSessionResponseBase64, 0, sizeBase64);

                if (ffw_base64_encode(ptrCancelSessionResp->cancelSessionResponse_RawData, ptrCancelSessionResp->cancelSessionResponse_RawDataSize, cancelSessionResponseBase64, &cancelSessionResponseBase64size, sizeBase64))
                {
                    // Build JSON object request
                    cJSON* jsonObject = cJSON_CreateObject();
                    if (jsonObject != NULL){
                        cJSON_AddItemToObject(jsonObject, "transactionId", cJSON_CreateString(transactionID));
                        cJSON_AddItemToObject(jsonObject, "cancelSessionResponse", cJSON_CreateString(cancelSessionResponseBase64));
                        char* requestBuffer = cJSON_Print(jsonObject);
                        cJSON_Delete(jsonObject);
                        jsonObject = NULL;
                        if (requestBuffer != NULL)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, ">>  HTTP request %s\n ", requestBuffer);
                            
                            // Prepare sending
                            char smdpServerAddr[LPA_ADDRESS_MAX_SIZE];
                            memset(&smdpServerAddr, 0, sizeof(smdpServerAddr));
				
                            if ((strlen(ptrSmdpAddress) + strlen(CANCEL_SESSION_PATH)) < LPA_ADDRESS_MAX_SIZE)
                            {
                                memcpy(smdpServerAddr, ptrSmdpAddress, strlen(ptrSmdpAddress));
                                memcpy(smdpServerAddr + strlen(ptrSmdpAddress), CANCEL_SESSION_PATH, strlen(CANCEL_SESSION_PATH));

                                httpServerResp = lpaManagerES9Plus_ExecutePost(smdpServerAddr, requestBuffer, &isExchangeSuccess, &httpCode, ptrLpaEventCallback);

                                if (isExchangeSuccess)
                                {
                                    if (httpCode > 0)
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerES9SendCancelSession: HTTP Request return httpCode = %d", httpCode);
                                        
                                        // Final check of server response
                                        switch (httpCode)
                                        {
                                            case 204:
                                                // HTTP Code 204 => No data expected
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerES9SendCancelSession: Confirmation JSON is missing, but server return operation successfull ? (http 204).");
                                            break;

                                            case 200:
                                                // HTTP Code 200 => data expected 
                                                if (httpServerResp != NULL)
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerES9SendCancelSession: HTTP Code 200 with data, analysing server response.");

                                                    // Process data returned to final status from server
                                                    cJSON* cjson = cJSON_Parse(httpServerResp);
                                                    if (cjson != NULL) 
                                                    {
                                                        if (_checkJSONresponseStatusSuccessful(cjson, ptrLpaEventCallback))
                                                        {
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerES9SendCancelSession: Cancel Session confirmed successful by server.");
                                                            res = true;
                                                        }
                                                        else
                                                        {
                                                            lpaSetErrorCode(LPA_ERROR_FAILED_CANCEL_SESSION);
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerES9SendCancelSession: Server reported an error.");
                                                        }

                                                        cJSON_Delete(cjson);
                                                        cjson = NULL;
                                                    }
                                                    else
                                                    {
                                                        lpaSetErrorCode(LPA_ERROR_INVALID_SERVER_RESPONSE);
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_lpaManagerES9SendCancelSession: json pack into cjson error...");
                                                    }
                                                }
                                                else
                                                    // Error code already set by lpaManagerES9Plus_ExecutePost()
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "HTTP Code 200 without data !");
                                            break;

                                            default:
                                                // Error code already set by lpaManagerES9Plus_ExecutePost()
                                                if (httpCode >= 200 && httpCode < 300)
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_lpaManagerES9SendCancelSession: Other 2xx HTTP Code not authorized for Cancel Session !");
                                                else
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_lpaManagerES9SendCancelSession: Not 2xx HTTP Code ");
                                            break;
                                        }
                                    }
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_lpaManagerES9SendCancelSession: HTTP Request return invalid httpCode !");
                                }
                                else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_lpaManagerES9SendCancelSession: Invalid HTTP exchange !");

                                httpMediaManagerHttpExecuteCleanup();
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerES9SendCancelSession: HTTP address is too long.");
                                                        
                            // Memory clean
                            lpaCoreMemoryFree(requestBuffer);
                            requestBuffer = NULL;
                        }
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerES9SendCancelSession: Cannot build JSON object.");
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerES9SendCancelSession: Cannot create JSON object.");
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerES9SendCancelSession: Cannot convert cancelSessionResponse in Base64.");

                lpaCoreMemoryFree(cancelSessionResponseBase64);
                cancelSessionResponseBase64 = NULL;
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerES9SendCancelSession: Cannot allocate memory for cancelSessionResponseBase64.");
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerES9SendCancelSession: Invalid cancelSessionResponse object.");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerES9SendCancelSession: Invalid Parameters.");
    
    return res;
}

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

#include "lpasdk/lpasdk_internal_api.h"

#include "lpasdk/core/lpa_manager_api.h"		// For main API
#include "lpasdk/core/lpa_manager.h"		// For main API
#include "lpasdk/core/isdr_applet_manager.h"

#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_memory.h"
#include "lpasdk/core/semedia_manager.h"
#include "lpasdk/core/httpmedia_manager.h"
#include "lpasdk/core/bertlv_object.h"
#include "lpasdk/core/util.h"
#include "lpasdk/core/lpa_core.h"
#include "lpasdk/core/lpa_manager_helper.h"

#include "lpasdk/core/lpa_manager_es9plus.h"
#include "lpasdk/core/lpa_manager_es10b.h"
#include "lpasdk/core/lpa_manager_es10c.h"

#include "cJSON/cJSON.h"
#include "sha256/sha-256.h"

#include <string.h>
#include <ctype.h>

#ifdef _FFW_PCIOT_LPA_MODIFY_
//#include <Windows.h>
#endif

#define DEFAULT_SE_MEDIA_READER_NAME ""
#define DEFAULT_CERTFICATE_NAME      ""

#define DER_ATTRIBUTE_TAG			0x80
#define SEQUENCE_TAG				0xA0
#define NOTIFICATION_METADATA_TAG               0xBF2F

#define ASN1_PRIMITIVE_ATTRIBUTE_TAG		0x80
#define ASN1_CONSTRUCTED_ATTRIBUTE_TAG		0xA0

#define ASN1_SMDPSIGNED2_TAG                    0x30
#define ASN1_CCREQUIREDFLAG_TAG                 0x01

#define LIST_NOTIFICATION_TAG                   0xBF28
#define REMOVE_NOTIFICATION_FROM_LIST_TAG	0xBF30
#define RETRIEVE_NOTIFICATIONS_LIST_TAG		0xBF2B

#define PROFILE_METADATA_TAG                    0xBF25
#define EUICC_RAT_TAG                           0xBF43

#define LPA_SDK_MAX_NOTIFICATION_DATA_SIZE	MAX_LPA_MANAGER_APDU_BUFFER_SIZE


// Memory stress parameters
#define LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_MEM_COUNTER_EQ				"MEM_ERR_IF_MEM_COUNTER_EQ"
#define LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_MEM_COUNTER_GT				"MEM_ERR_IF_MEM_COUNTER_GT"
#define LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_MEM_COUNTER_GE				"MEM_ERR_IF_MEM_COUNTER_GE"

#define LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_SIZE_REQUESTED_EQ			"MEM_ERR_IF_SIZE_REQUESTED_EQ"
#define LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_SIZE_REQUESTED_GT			"MEM_ERR_IF_SIZE_REQUESTED_GT"
#define LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_SIZE_REQUESTED_GE			"MEM_ERR_IF_SIZE_REQUESTED_GE"


// SE Media
static char _seMediaReaderName[LPA_CFG_READER_NAME_MAX_SIZE];
static char _deviceInfo[LPA_CFG_DEVICE_INFO_TLV_MAX_SIZE];

static unsigned char _deviceInfoByteArray[LPA_CFG_DEVICE_INFO_TLV_BYTE_ARRAY_MAX_SIZE];
static size_t _deviceInfoByteArraySize = 0;


static bool _initializeDone = false;
static char _bufferFormatLogMessage[1024];	// 1Ko is enough (to increase it, use dynamic memory allocation)
static char _lpaFolder[LPA_MAX_PATH];

#define LPA_MANAGER_DATA_BUFFER_MAX_SIZE MAX_LPA_MANAGER_APDU_BUFFER_SIZE
static unsigned char _dataBuffer[LPA_MANAGER_DATA_BUFFER_MAX_SIZE]; // For managing data from/to SE Media

static bool _sendPIRDuringDownloadProfileOperation = true;

// for sending Event to Application
////////////////////////////////////////////

typedef struct
{
	// LPA SDK event parameter
	void* _appParameter;
	LPA_EVENT_EXECUTION_ERROR _lpaEventExecutionError;
} APP_EVENT_EXECUTION_ERROR_CALLBACK;

static APP_EVENT_EXECUTION_ERROR_CALLBACK _appEventExecutionErrorCallback;

void _registerAppEventExecutionCallback(const LPA_EventCallback* ptrEventCallback);
void _unregisterAppEventExecutionCallback();

#ifdef LPA_SDK__USING_EX_API
static bool _isExtendedApiActivated = true;
#else
static bool _isExtendedApiActivated = false;
#endif 

// PARAMETERS LIST DEFINITIONS
typedef enum
{
	PARAM_ID_READER_NAME = 1,
	PARAM_ID_SEND_PIR_DURING_DOWNLOAD_PROFILE,
	PARAM_ID_ADD_LE_TO_APDU_CASE_4,
	PARAM_ID_LOG_LEVEL,
	PARAM_ID_LOG_MAX_SIZE,

	// Profile Mangement
	PARAM_ID_PROFILE_REFRESH_FLAG,

	// CURL part (httpMedia)
	PARAM_ID_ACTIVATE_CURL_DEBUG_MODE,
	PARAM_ID_CURL_SSL_SSL_VERIFYPEER,
	PARAM_ID_CURL_SSL_SSL_VERIFYHOST,
	PARAM_ID_CURL_CONNECT_TIMEOUT,
	PARAM_ID_CURL_TIMEOUT,

	PARAM_ID_CERT_PATH,
	PARAM_ID_CONFIG_DEVICE_INFO_TLV,


	// Memory stress parameters
	PARAM_ID_MEM_ERR_IF_MEM_COUNTER_EQ,
	PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GT,
	PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GE,

	PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_EQ,
	PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GT,
	PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GE,

	// Latest entry
	PARAM_ID_NONE = 999
} LPA_PARAMETER_ID;

typedef struct
{
	LPA_PARAMETER_ID parameterId;
	char* parameterName;
	LPA_PARAMETER_TYPE parameterType;
	bool isRestricted;	// If yes, only available when using LPA_SDK__USING_EX_API build option
} LPA_PARAMETER_DEFINITION;

// PARAMETERS LIST BUILD
// NOTES: String constants defining names of parameters in second position are located in lpa_manager_api.h
//        true / false flag at the end defines a restriction mode. If true this parameter will be only usable when compiled / running in Extended API Mode
static LPA_PARAMETER_DEFINITION _lpaParameterDefinitionList[] =
{
	{ PARAM_ID_READER_NAME, LPA_SDK_CONFIG_PARAM_READER_NAME, LPA_PARAMETER_TYPE_STRING, false },
	{ PARAM_ID_SEND_PIR_DURING_DOWNLOAD_PROFILE, LPA_SDK_CONFIG_PARAM_SEND_PIR_DURING_DOWNLOAD_PROFILE, LPA_PARAMETER_TYPE_BOOL, true },

	{ PARAM_ID_ACTIVATE_CURL_DEBUG_MODE, LPA_SDK_CONFIG_PARAM_ACTIVATE_CURL_DEBUG_MODE, LPA_PARAMETER_TYPE_BOOL, true },
	{ PARAM_ID_CURL_SSL_SSL_VERIFYPEER, LPA_SDK_CONFIG_PARAM_CURL_SSL_SSL_VERIFYPEER, LPA_PARAMETER_TYPE_BOOL, true },
	{ PARAM_ID_CURL_SSL_SSL_VERIFYHOST, LPA_SDK_CONFIG_PARAM_CURL_SSL_SSL_VERIFYHOST, LPA_PARAMETER_TYPE_BOOL, true },
	{ PARAM_ID_ADD_LE_TO_APDU_CASE_4, LPA_SDK_CONFIG_PARAM_ADD_LE_TO_APDU_CASE_4, LPA_PARAMETER_TYPE_BOOL, false },

	{ PARAM_ID_LOG_LEVEL, LPA_SDK_CONFIG_PARAM_LOG_LEVEL, LPA_PARAMETER_TYPE_STRING, false },
	{ PARAM_ID_LOG_MAX_SIZE, LPA_SDK_CONFIG_PARAM_LOG_MAX_SIZE, LPA_PARAMETER_TYPE_LONG, false },
	{ PARAM_ID_PROFILE_REFRESH_FLAG, LPA_SDK_CONFIG_PARAM_PROFILE_REFRESH_FLAG, LPA_PARAMETER_TYPE_BOOL, false },

	{ PARAM_ID_CURL_CONNECT_TIMEOUT, LPA_SDK_CONFIG_PARAM_CURL_CONNECT_TIMEOUT, LPA_PARAMETER_TYPE_LONG, false },
	{ PARAM_ID_CURL_TIMEOUT, LPA_SDK_CONFIG_PARAM_CURL_TIMEOUT, LPA_PARAMETER_TYPE_LONG, false },

	{ PARAM_ID_CERT_PATH, LPA_SDK_CONFIG_PARAM_CERT_PATH, LPA_PARAMETER_TYPE_STRING, false },
	{ PARAM_ID_CONFIG_DEVICE_INFO_TLV, LPA_SDK_CONFIG_PARAM_DEVICE_INFO_TLV, LPA_PARAMETER_TYPE_STRING, false },

	// Memory stress parameters. Not restricted to allow to use them also out of Extended mode. Access not authorized if LPA_SDK__MEMORY is not set.
	{ PARAM_ID_MEM_ERR_IF_MEM_COUNTER_EQ, LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_MEM_COUNTER_EQ, LPA_PARAMETER_TYPE_LONG, false },
	{ PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GT, LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_MEM_COUNTER_GT, LPA_PARAMETER_TYPE_LONG, false },
	{ PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GE, LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_MEM_COUNTER_GE, LPA_PARAMETER_TYPE_LONG, false },

	{ PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_EQ, LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_SIZE_REQUESTED_EQ, LPA_PARAMETER_TYPE_LONG, false },
	{ PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GT, LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_SIZE_REQUESTED_GT, LPA_PARAMETER_TYPE_LONG, false },
	{ PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GE, LPA_SDK_CONFIG_PARAM_MEM_ERR_IF_SIZE_REQUESTED_GE, LPA_PARAMETER_TYPE_LONG, false },

	// Latest entry : do not remove it !!!!
	{ PARAM_ID_NONE, NULL, LPA_PARAMETER_TYPE_BOOL, false }
};

bool _lpaManagerGetBooleanParameterValue(LPA_PARAMETER_ID parameterId, bool* ptrParameterValue);
bool _lpaManagerSetBooleanParameterValue(const LPA_PARAMETER_DEFINITION * ptrParameterDefinition, bool parameterValue);

bool _lpaManagerGetLongParameterValue(LPA_PARAMETER_ID parameterId, long* ptrParameterValue);
bool _lpaManagerSetLongParameterValue(const LPA_PARAMETER_DEFINITION * ptrParameterDefinition, long parameterValue);

bool _lpaManagerGetStringParameterValue(LPA_PARAMETER_ID parameterId, char* ptrParameterValue, size_t parameterValueMaxSize);
bool _lpaManagerSetStringParameterValue(const LPA_PARAMETER_DEFINITION * ptrParameterDefinition, const char* ptrParameterValue);

#ifdef LPA_SDK__MEMORY
bool _lpaManagerGetLongMemoryErrorParameterValue(LPA_PARAMETER_ID parameterId, long* ptrParameterValue);
bool _lpaManagerSetLongMemoryErrorParameterValue(const LPA_PARAMETER_DEFINITION * ptrParameterDefinition, long parameterValue);
#endif // LPA_SDK__MEMORY

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _doExtractMetadataForGetProfileNotificationList(LPA_PROFILE_NOTIFICATION_LIST* ptrProfileNotificationList, unsigned char *ptrData, size_t dataSize);
LPA_DELETE_NOTIFICATION_STATUS _doExtractClearProfileNotificationResponse(unsigned char *ptrData, size_t dataSize);
LPA_NOTIFICATION_EVENT _doExtractNotificationEventValue(unsigned char* ptrDataValue, size_t dataLength);
bool _retrieveNotificationData(uint16_t sequenceNumber, RawDataObject* ptrRawDataNotification);
bool _sendNotificationToServer(const LPA_PROFILE_NOTIFICATION_METADATA* ptrProfileNotificationMetadata, const RawDataObject* ptrRawDataNotification, const LPA_EventCallback* ptrLpaEventCallback);
bool _extractNotificationData(const unsigned char* ptrDataBuffer, size_t dataBufferSize, RawDataObject* ptrRawDataNotification);
bool _sortNotificationList(LPA_PROFILE_NOTIFICATION_LIST * ptrNotificationList);
bool _copyNotificationListElement(LPA_PROFILE_NOTIFICATION_METADATA * ptrSource, LPA_PROFILE_NOTIFICATION_METADATA * ptrDestination);
bool _decodeActivationCodeStr(const char* ptrActivationCodeStr, ACTIVATION_CODE* ptrActivationCode);

bool _getEUICCconfiguredAddresses(EUICC_CONFIGURE_ADDR* ptrEuiccAddr);
LPA_PARAMETER_DEFINITION* _getParameterDefinition(const char* ptrParameterName);
void _sendEventCallbackProgressText(const LPA_EventCallback*, size_t eventType, const char* ptrText);
void _sendEventCallbackProgressValue(LPA_EventCallback*, size_t eventType, size_t valueMin, size_t currentValue, size_t valueMax);
bool _verifySMDPAddress(const char* ptrSmdpAddress, const char* ptrServerSignedTLV, size_t serverSignedTLVSize);
bool _handleNotification(const char* ptrSmdpAddr, size_t smdpAddrSize, const unsigned char* ptrPendingNotification, const LPA_EventCallback* ptrLpaEventCallback);

void _notifyAppliOwnerCancelResult(const LPA_EventCallback* ptrLpaEventCallback, const bool cancelResult, size_t callbackEventType);
static bool _manageDownloadProfile(const char * ptrActivationCodeStr, const char * ptrConfirmationCodeStr, const int confirmationCodeLength, 
                            const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult, bool * retryRequested);
bool _performProfileDownloadFromDefaultAddress(const LPA_EventCallback* ptrLpaEventCallback, const char* pSmdpAddr, const size_t pSmdpAddrSize, 
        const char* pEventID, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult, const bool requestFromDefaultSMDPload, bool * retryRequested);

//work with server
bool _lpaManagerAuthenticateClient(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrTransactionId, const char* ptrSmdpAddress, const unsigned char* ptrAuthenticateServerResponse);
bool _lpaManagerInitiateAuthentication(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char * ptrEuiccChallenge, const char * ptrEuiccInfo1, const char* ptrSmdpAddress);
bool _lpaManagerGetBoundProfilePackage(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrTransactionId, const char* ptrSmdpAddress, const unsigned char* ptrPrepareDownloadResponse);

bool _lpaManagerGetEuiccChallenge(LPA_GET_EUICC*);
bool _lpaManagerGetEuiccInfo(LPA_GET_EUICC*);
bool _lpaManagerAuthenticateServer(ptr_serverData, const LPA_EventCallback* ptrLpaEventCallback, RawDataObject *, AUTHENTICATE_SERVER_RESPONSE*);
bool _lpaManagerPrepareDownload(ptr_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char * ptrStringHashCC, PREPARE_DOWNLOAD_RESPONSE*);

//matching id+deviceinfoTlv
bool _lpaManagerPrepareCtxParam(const char* ptrMatchingId, const unsigned char* ptrDeviceInfoTlv, size_t deviceInfoTlvSize, RawDataObject **);
bool _lpaManagerCancelSession(const char * transactionID, const unsigned int p_reasonCode, const char* ptrSmdpAddress, const LPA_EventCallback* ptrLpaEventCallback);

bool _lpaManagerInitServerData(ptr_serverData p_serverData);
bool _lpaManagerFreeServerData(ptr_serverData p_serverData);

bool _extractFieldsFromProfileMetadata(const unsigned char * ptrProfileMetadata, const size_t profileMetadataSize, LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR * ptrExtractProfileFields);
bool _getProfilesInfoForPPR(LPA_GET_PROFILES_INFO* ptrLpaGetProfilesInfo);
bool _isPPR1definedInPPRflag(const unsigned int pprFlag);
bool _isPPR2definedInPPRflag(const unsigned int pprFlag);
bool _lpaManagerGetRAT(RawDataObject ** ptrGetRAT);
bool _checkIncomingProfilePPRconditionVersusInstalledProfiles(LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR* ptrIncomingProfileData, 
                                                                                                                const LPA_GET_PROFILES_INFO * ptrCurrentlyInstalledProfiles);
bool _checkIncomingProfilePPRattributesVersusRAT(LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR* ptrIncomingProfileData, const RawDataObject* ptrRATrules);
bool _analysePPARversusProfileAttribute(BeerTLV * ptrPPARrule, LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR * ptrIncomingProfileData, LPA_PPR_RAT_ANALYSIS_FLAGS * ptrCheckPPR1, 
                                                                                                                                LPA_PPR_RAT_ANALYSIS_FLAGS  * ptrCheckPPR2);
bool _analysePPARallowedOperatorVersusProfile(LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR * ptrIncomingProfileData, LPA_PPR_RAT_ANALYSIS_FLAGS * ptrCheckPPR, 
                                                                                                            BeerTLV * berAllowedOperators, const bool consentRequiredFlag);
bool _comparePPARallowedOperator_MCC_MNC(const unsigned char * ptrAllowOpMCCMNC, const unsigned char * ptrProfileMCCMNC, bool * ptrMatchingFlag, int * ptrMatchinglevel);

void* _hookForJSONAlloc(size_t size);
void _hookForJSONFree(void* ptrMemoryBlock);

// register EventError callback
void _lpaManagerEventExecutionErrorCallback(const void* ptrAppParameter, const LPA_EVENT_EXECUTION_ERROR_INFO* ptrEventExecutionErrorInfo);

void* _hookForJSONAlloc(size_t size)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "CJSON Alloc(%d bytes)", size);

	return lpaCoreMemoryAlloc(size);
}

void _hookForJSONFree(void* ptrMemoryBlock)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "CJSON Free(%08lX)", ptrMemoryBlock);
	lpaCoreMemoryFree(ptrMemoryBlock);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerInitialize(const char* ptrLpaFolder)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerInitialize(...)");

	if (!_initializeDone)
	{
		if (ptrLpaFolder != NULL)
		{
			// Update default seMediaReaderName and save lpaFolder
			if (snprintf(_seMediaReaderName, sizeof(_seMediaReaderName), "%s", DEFAULT_SE_MEDIA_READER_NAME) < sizeof(_seMediaReaderName) &&
				snprintf(_lpaFolder, sizeof(_lpaFolder), "%s", ptrLpaFolder) < sizeof(_lpaFolder))
			{
				_initializeDone = true;
			}

			
			memset(_deviceInfo, 0, sizeof(_deviceInfo));
			memset(_deviceInfoByteArray, 0, sizeof(_deviceInfoByteArray));
			_deviceInfoByteArraySize = 0;

			lpaManagerES9Plus_Init(); // To initialize certPath
		}

#if 0 
		// use cJSON Hook for using lpaMemory mechanism
		cJSON_Hooks jsonHook;
		jsonHook.malloc_fn = _hookForJSONAlloc;
		jsonHook.free_fn = _hookForJSONFree;
		cJSON_InitHooks(&jsonHook);
#endif

		// register EventError callback
		_appEventExecutionErrorCallback._appParameter = NULL;
		_appEventExecutionErrorCallback._lpaEventExecutionError = NULL;
	}

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerInitialize(...)");

	return _initializeDone;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

bool lpaManagerInitializeHttpMedia()
{
	bool res = false;
	
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerInitializeHttpMedia(...)");

	res = httpMediaManagerInitialize();
	if ( res )
		res = httpMediaManagerSetCallbackEventExecutionError(_lpaManagerEventExecutionErrorCallback);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerInitializeHttpMedia(...)");

	return res;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

bool lpaManagerInitializeSEMedia()
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerInitializeSEMedia(...)");

	res = seMediaManagerInitialize();
	if (res)
		res = seMediaManagerSetCallbackEventExecutionError(_lpaManagerEventExecutionErrorCallback);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerInitializeSEMedia(...)");

	return res;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

bool lpaManagerSetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, const void* ptrParameterValue, bool internalCall)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerSetConfigParameter(...)");

	if (ptrParameterName != NULL && ptrParameterValue != NULL)
	{
		if (internalCall )
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Internal call to this function");

                // Logging of parameter name / type / value
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter key name                            : %s", ptrParameterName);
                switch(parameterType)
                {
                    case LPA_PARAMETER_TYPE_BOOL:
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter type given                          : LPA_PARAMETER_TYPE_BOOL");
                        // No warranty that displaying will be correct because cannot effectively check parameter value type passed as void*
                        const bool* ptrLogBooleanValue = ptrParameterValue;
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter value (Type match cannot be checked): %s", (*ptrLogBooleanValue)?"true":"false");
                    }
                    break;
                    
                    case LPA_PARAMETER_TYPE_LONG:
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter type given                          : LPA_PARAMETER_TYPE_LONG");
                        // No warranty that displaying will be correct because cannot effectively check parameter value type passed as void*
                        const long* ptrLogLongValue = ptrParameterValue;
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter value (Type match cannot be checked): %ld", *ptrLogLongValue);
                    }
                    break;
                    
                    case LPA_PARAMETER_TYPE_STRING:
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter type given                          : LPA_PARAMETER_TYPE_STRING");
                        // No warranty that displaying will be correct because cannot effectively check parameter value type passed as void*
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter value (Type match cannot be checked): %s", (const char *)ptrParameterValue);
                    }
                    break;
                    
                    default:
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter type given not managed. Value will be not logged!");
                    break;
                }
                
		LPA_PARAMETER_DEFINITION *ptrParameterDefinition = _getParameterDefinition(ptrParameterName);
		if (ptrParameterDefinition != NULL)
		{
                    if (ptrParameterDefinition->isRestricted && !_isExtendedApiActivated)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter key not authorized!");
                        lpaSetErrorCode(LPA_ERROR_PARAMETER_NOT_AUTHORIZED);	// this parameter is only available for Extended API
                    }
                    else
                    {
                        switch (ptrParameterDefinition->parameterType)
                        {
                            case LPA_PARAMETER_TYPE_BOOL:
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter key identified, default type        : LPA_PARAMETER_TYPE_BOOL");

                                if (parameterType == LPA_PARAMETER_TYPE_BOOL)
                                {
                                    const bool* ptrParameterBooleanValue = ptrParameterValue;
                                    res = _lpaManagerSetBooleanParameterValue(ptrParameterDefinition, *ptrParameterBooleanValue);
                                }
                                else
                                {
                                    bool parameterBooleanValue = false;
                                    if (parameterType == LPA_PARAMETER_TYPE_STRING && convertStringToBoolean((const char*)ptrParameterValue, &parameterBooleanValue))
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Conversion from STRING to BOOL performed.");
                                        res = _lpaManagerSetBooleanParameterValue(ptrParameterDefinition, parameterBooleanValue);
                                    }
                                    else
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter type given is not compatible with parameter key!");
                                        lpaSetErrorCode(LPA_ERROR_INCORRECT_PARAMETER_TYPE);
                                    }
                                }
                            }
                            break;

                            case LPA_PARAMETER_TYPE_LONG:
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter key identified, default type        : LPA_PARAMETER_TYPE_LONG");

                                if (parameterType == LPA_PARAMETER_TYPE_LONG)
                                {
                                    const long* ptrParameterLongValue = ptrParameterValue;
                                    res = _lpaManagerSetLongParameterValue(ptrParameterDefinition, *ptrParameterLongValue);
                                }
                                else
                                {
                                    long parameterLongValue = 0L;
                                    if (parameterType == LPA_PARAMETER_TYPE_STRING && convertStringToLong((const char*)ptrParameterValue, &parameterLongValue))
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Conversion from STRING to LONG performed.");
                                        res = _lpaManagerSetLongParameterValue(ptrParameterDefinition, parameterLongValue);
                                    }
                                    else
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter type given is not compatible with parameter key!");
                                        lpaSetErrorCode(LPA_ERROR_INCORRECT_PARAMETER_TYPE);
                                    }
                                }
                            }
                            break;

                            case LPA_PARAMETER_TYPE_STRING:
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter key identified, default type        : LPA_PARAMETER_TYPE_STRING");

                                if (parameterType == LPA_PARAMETER_TYPE_STRING)
                                    res = _lpaManagerSetStringParameterValue(ptrParameterDefinition, ptrParameterValue);
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter type given is not compatible with parameter key!");
                                    lpaSetErrorCode(LPA_ERROR_INCORRECT_PARAMETER_TYPE);
                                }
                            }
                            break;

                            default:
                            {
                                // This case shall never occurs normally, except for memory / data corruption issue
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unknown parameter type returned from parameter list! DATA CORRUPTION ERROR CASE");
                                lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER_TYPE);
                            }
                            break;
                        }
                    }
		}
		else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter key name not identified!");
                    lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
                }
	}
	else
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerSetConfigParameter() return %s", (res ? "true" : "false"));
	return res;
}

bool lpaManagerGetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, void* ptrParameterValue, size_t parameterValueMaxSize)
{
	bool res = false;
        
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerGetConfigParameter(...)");

	if (ptrParameterName != NULL && ptrParameterValue != NULL && parameterValueMaxSize > 0)
	{
            // Logging of parameter name / type / value
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter key name                            : %s", ptrParameterName);
            switch(parameterType)
            {
                case LPA_PARAMETER_TYPE_BOOL:
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter type given                          : LPA_PARAMETER_TYPE_BOOL");
                break;

                case LPA_PARAMETER_TYPE_LONG:
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter type given                          : LPA_PARAMETER_TYPE_LONG");
                break;

                case LPA_PARAMETER_TYPE_STRING:
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter type given                          : LPA_PARAMETER_TYPE_STRING");
                break;

                default:
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter type given not managed, will be not displayed!");
                break;
            }

            LPA_PARAMETER_DEFINITION *ptrParameterDefinition = _getParameterDefinition(ptrParameterName);
            if (ptrParameterDefinition != NULL)
            {
                if (ptrParameterDefinition->isRestricted && !_isExtendedApiActivated)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter key not authorized!");
                    lpaSetErrorCode(LPA_ERROR_PARAMETER_NOT_AUTHORIZED);	// this parameter is only available for Extended API
                }
                else
                {
                    if (parameterType != ptrParameterDefinition->parameterType)
                    {
                        switch(ptrParameterDefinition->parameterType)
                        {
                            case LPA_PARAMETER_TYPE_BOOL:
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter type given does not match parameter key default type (LPA_PARAMETER_TYPE_BOOL)");
                            break;

                            case LPA_PARAMETER_TYPE_LONG:
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter type given does not match parameter key default type (LPA_PARAMETER_TYPE_LONG)");
                            break;

                            case LPA_PARAMETER_TYPE_STRING:
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter type given does not match parameter key default type (LPA_PARAMETER_TYPE_STRING)");
                            break;

                            default:
                                // This case shall never occurs normally, except for memory / data corruption issue
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unknown parameter type returned from parameter list! DATA CORRUPTION ERROR CASE");
                            break;
                        }
                        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER_TYPE);
                    }
                    else
                    {
                        switch (parameterType)
                        {
                            case LPA_PARAMETER_TYPE_BOOL:
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter key identified, default type        : LPA_PARAMETER_TYPE_BOOL");

                                if (sizeof(bool) == parameterValueMaxSize)
                                    res = _lpaManagerGetBooleanParameterValue(ptrParameterDefinition->parameterId, ptrParameterValue);
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Size of variable receiving value does not match value type!");
                                    lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER_TYPE);
                                }

                            }
                            break;

                            case LPA_PARAMETER_TYPE_STRING:
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter key identified, default type        : LPA_PARAMETER_TYPE_STRING");

                                res = _lpaManagerGetStringParameterValue(ptrParameterDefinition->parameterId, ptrParameterValue, parameterValueMaxSize);
                            }
                            break;

                            case LPA_PARAMETER_TYPE_LONG:
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter key identified, default type        : LPA_PARAMETER_TYPE_LONG");

                                if (sizeof(long) == parameterValueMaxSize)
                                    res = _lpaManagerGetLongParameterValue(ptrParameterDefinition->parameterId, ptrParameterValue);
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Size of variable receiving value does not match value type!");
                                    lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER_TYPE);
                                }
                            }
                            break;

                            default:
                            {
                                // Normally we shall never arrive here, but leaved as security
                                lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER_TYPE);
                            }
                            break;
                        }
                    }
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Parameter key name not identified!");
                lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
            }
	}
        else
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);

        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerGetConfigParameter() return %s", (res ? "true" : "false"));
	return res;
}

/**
 * Check if parameter exists in parameter list, if yes return parameter type and if access is allowed out of Extended Mode
 * @param ptrParameterName Pointer on parameter name to check, string type
 * @param ptrParameterType Pointer on parameter type to return, LPA_PARAMETER_TYPE type
 * @param ptrIsExist Pointer on boolean flag returning if parameter exists, if yes return true
 * @param ptrAccessGranted Pointer on boolean flag returning true if parameter use is allowed out of Extended Mode
 * @return True if no error occurred during check
 */
bool lpaManagerIsConfigParameterExist(const char* ptrParameterName, LPA_PARAMETER_TYPE* ptrParameterType, bool* ptrIsExist, bool* ptrAccessGranted)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerIsConfigParameterExist(...)");

	if (ptrParameterName != NULL && ptrParameterType != NULL && ptrIsExist != NULL)
	{
		*ptrIsExist = false;
		*ptrParameterType = LPA_PARAMETER_TYPE_UNKNOWN;

		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Searching parameter key '%s'", ptrParameterName);

		LPA_PARAMETER_DEFINITION *ptrParameterDefinition = _getParameterDefinition(ptrParameterName);
		if (ptrParameterDefinition != NULL)
		{
			*ptrIsExist = true;
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter key exists (isRestricted:%s)", (ptrParameterDefinition->isRestricted ? "yes" : "no"));

			*ptrParameterType = ptrParameterDefinition->parameterType;

			if (NULL != ptrAccessGranted)
				*ptrAccessGranted = (!ptrParameterDefinition->isRestricted || _isExtendedApiActivated);

			res = true;
		}
		else
                {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Parameter key does not exist.");
			res = true;	// Return true (Correct execution but parameter not found)
                }
	}
	else
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerIsConfigParameterExist() return %s", (res ? "true" : "false"));

	return res;
}


/**
 * Return list of settable parameters
 * @param ptrLpaParametersList List of parameters with type, LPA_PARAMETERS_LIST type.
 * @return true if operation successful
 */
bool lpaManagerGetFullParametersList(LPA_PARAMETERS_LIST * ptrLpaParametersList)
{
    bool res = false;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaGetFullParametersList(...)");
  
    if (ptrLpaParametersList != NULL)
    {
        memset(ptrLpaParametersList, 0, sizeof(LPA_PARAMETERS_LIST));
        ptrLpaParametersList->parametersCount = 0;
  
// Re-activate code block below in case of return of this function in Normal Mode
// Deactivated because generate a warning in compiler, syntax of use of #pragma is environment dependant
/*
        bool usingExtentedAPI = false;
#ifdef LPA_SDK__USING_EX_API
        usingExtentedAPI = true;
#endif //LPA_SDK__USING_EX_API
*/
    
        int parametersIndex = 0;
        int listIndex = 0;
        
        // Last test on parametersIndex is to avoid eventual infinite loop reading memory
        while((_lpaParameterDefinitionList[parametersIndex].parameterName != NULL) && (listIndex < LPA_MAX_PARAMETERS_LIST) && (parametersIndex < (LPA_MAX_PARAMETERS_LIST + 10)))
        {
            // Copy restricted parameters only if Extended API used mode ON
            // NOTE: Condition bypassed due to moving in Extended Mode. But kept here in case of returning of feature in Normal Mode
            //if(usingExtentedAPI || !_lpaParameterDefinitionList[parametersIndex].isRestricted)
            if(true)
            {
                // If LPA_MAX_PARAMETERS_LIST_ELEMENT_SIZE has been incorrectly set, parameters list elements will appear truncated
                strncpy(ptrLpaParametersList->parametersList[listIndex], _lpaParameterDefinitionList[parametersIndex].parameterName, (LPA_MAX_PARAMETERS_LIST_ELEMENT_SIZE - 1));
                ptrLpaParametersList->parametersTypeList[listIndex] = _lpaParameterDefinitionList[parametersIndex].parameterType;
                listIndex++;
            }
            
            parametersIndex++;
        }
        
        // Validated OK while list does not exceed parameters list array capacity. If empty list will return OK but no elements to display (parameterCount = 0)
        if(listIndex < LPA_MAX_PARAMETERS_LIST)
        {
            ptrLpaParametersList->parametersCount = listIndex;
            res = true;
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "lpaGetFullParametersList() - Invalid NULL parameter!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaGetFullParametersList() return %s", (res ? "true" : "false"));
    return res;
}


LPA_PARAMETER_DEFINITION* _getParameterDefinition(const char* ptrParameterName)
{
	LPA_PARAMETER_DEFINITION *ptrParameterDefinition = NULL;
	if (ptrParameterName != NULL)
	{
		int index = 0;
		while (ptrParameterDefinition == NULL)
		{
			if (_lpaParameterDefinitionList[index].parameterName == NULL)
				break;	// End of list : Entry not found

			if (compareEqualStringIgnoringCase(ptrParameterName, _lpaParameterDefinitionList[index].parameterName))
				ptrParameterDefinition = &_lpaParameterDefinitionList[index]; // Entry found
			else
				index++;
		}
	}

	return ptrParameterDefinition;
}


bool _lpaManagerGetBooleanParameterValue(LPA_PARAMETER_ID parameterId, bool* ptrParameterValue)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _lpaManagerGetBooleanParameterValue(parameterId=%d)", parameterId);

	if (ptrParameterValue != NULL)
	{
		switch (parameterId)
		{

		case PARAM_ID_SEND_PIR_DURING_DOWNLOAD_PROFILE:
			*ptrParameterValue = _sendPIRDuringDownloadProfileOperation;
			res = true;
			break;

		case PARAM_ID_PROFILE_REFRESH_FLAG:
			*ptrParameterValue = lpaManagerES10c_IsRefreshFlag();
			res = true;
			break;

			// CURL part (httpMedia)
		case PARAM_ID_ACTIVATE_CURL_DEBUG_MODE:
			res = httpMediaManagerGetBooleanOption(HTTP_MEDIA_OPTION_TYPE_CURL_VERBOSE, ptrParameterValue);
			break;

		case PARAM_ID_CURL_SSL_SSL_VERIFYPEER:
			res = httpMediaManagerGetBooleanOption(HTTP_MEDIA_OPTION_TYPE_CURL_SSL_VERIFYPEER, ptrParameterValue);
			break;

		case PARAM_ID_CURL_SSL_SSL_VERIFYHOST:
			res = httpMediaManagerGetBooleanOption(HTTP_MEDIA_OPTION_TYPE_CURL_SSL_VERIFYHOST, ptrParameterValue);
			break;

			// lpaManager Helper
		case PARAM_ID_ADD_LE_TO_APDU_CASE_4:
			*ptrParameterValue = lpaManagerHelperIsLeAddedToApduCase4();
			res = true;
			break;

		default:
			lpaSetErrorCode(LPA_ERROR_UNKNOWN_PARAMETER);
			break;
		}

		if (res)
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter value                               : %s", (*ptrParameterValue ? "true" : "false"));
	}

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _lpaManagerGetBooleanParameterValue() return %s", (res ? "true" : "false"));
	return res;
}

bool _lpaManagerSetBooleanParameterValue(const LPA_PARAMETER_DEFINITION * ptrParameterDefinition, bool parameterValue)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _lpaManagerSetBooleanParameterValue(...)");

    if(ptrParameterDefinition != NULL)
    {
        switch (ptrParameterDefinition->parameterId)
        {
        case PARAM_ID_SEND_PIR_DURING_DOWNLOAD_PROFILE:
                _sendPIRDuringDownloadProfileOperation = parameterValue;
                res = true;
                break;

        case PARAM_ID_PROFILE_REFRESH_FLAG:
                lpaManagerES10c_SetRefreshFlag(parameterValue);
                res = true;
                break;

                // CURL part (httpMedia)
        case PARAM_ID_ACTIVATE_CURL_DEBUG_MODE:
                res = httpMediaManagerSetBooleanOption(HTTP_MEDIA_OPTION_TYPE_CURL_VERBOSE, parameterValue);
                break;

        case PARAM_ID_CURL_SSL_SSL_VERIFYPEER:
                res = httpMediaManagerSetBooleanOption(HTTP_MEDIA_OPTION_TYPE_CURL_SSL_VERIFYPEER, parameterValue);
                break;

        case PARAM_ID_CURL_SSL_SSL_VERIFYHOST:
                res = httpMediaManagerSetBooleanOption(HTTP_MEDIA_OPTION_TYPE_CURL_SSL_VERIFYHOST, parameterValue);
                break;

                // SEMedia
        case PARAM_ID_ADD_LE_TO_APDU_CASE_4:
                lpaManagerHelperSetLeToAddApduCase4(parameterValue);
                res = true;
                break;

        default:
                lpaSetErrorCode(LPA_ERROR_UNKNOWN_PARAMETER);
                break;
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerSetBooleanParameterValue() : NULL parameter! Operation canceled.");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _lpaManagerSetBooleanParameterValue() return %s", (res ? "true" : "false"));
    return res;
}

bool _lpaManagerSetLongParameterValue(const LPA_PARAMETER_DEFINITION * ptrParameterDefinition, long parameterValue)
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _lpaManagerSetLongParameterValue(...)");
    
    if(ptrParameterDefinition != NULL)
    {
	switch (ptrParameterDefinition->parameterId)
	{
		// Log max size
		case PARAM_ID_LOG_MAX_SIZE:
			res = lpaCoreSetLogMaxSize(parameterValue);
		break;

		// CURL part (httpMedia)
		case PARAM_ID_CURL_CONNECT_TIMEOUT:
			res = httpMediaManagerSetLongOption(HTTP_MEDIA_OPTION_TYPE_CURL_CONNECT_TIMEOUT, parameterValue);
		break;

		case PARAM_ID_CURL_TIMEOUT:
			res = httpMediaManagerSetLongOption(HTTP_MEDIA_OPTION_TYPE_CURL_TIMEOUT, parameterValue);
		break;

		// Memory stress parameters
		case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_EQ:
		case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GT:
		case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GE:
		case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_EQ:
		case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GT:
		case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GE:
#ifdef LPA_SDK__MEMORY
                    res = _lpaManagerSetLongMemoryErrorParameterValue(ptrParameterDefinition, parameterValue);
#else
                    lpaSetErrorCode(LPA_ERROR_PARAMETER_NOT_AUTHORIZED);
#endif //LPA_SDK__MEMORY
		break;

		default:
			lpaSetErrorCode(LPA_ERROR_UNKNOWN_PARAMETER);
		break;
	}
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerSetLongParameterValue() : NULL parameter! Operation canceled.");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _lpaManagerSetLongParameterValue() return %s", (res ? "true" : "false"));
    return res;
}

#ifdef LPA_SDK__MEMORY
bool _lpaManagerSetLongMemoryErrorParameterValue(const LPA_PARAMETER_DEFINITION * ptrParameterDefinition, long parameterValue)
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _lpaManagerSetLongMemoryErrorParameterValue(...)");

    if(ptrParameterDefinition != NULL)
    {
	switch (ptrParameterDefinition->parameterId)
	{
            case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_EQ:
                    res = lpaCoreMemorySetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_EQ, parameterValue);
            break;

            case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GT:
                    res = lpaCoreMemorySetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GT, parameterValue);
            break;

            case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GE:
                    res = lpaCoreMemorySetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GE, parameterValue);
            break;

            case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_EQ:
                    res = lpaCoreMemorySetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_EQ, parameterValue);
            break;

            case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GT:
                    res = lpaCoreMemorySetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GT, parameterValue);
            break;

            case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GE:
                    res = lpaCoreMemorySetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GE, parameterValue);
            break;
                
            default:
            break;
	}
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerSetLongMemoryErrorParameterValue() : NULL parameter! Operation canceled.");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }    

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _lpaManagerSetLongMemoryErrorParameterValue() return %s", (res ? "true" : "false"));
    return res;
}
#endif //LPA_SDK__MEMORY

bool _lpaManagerGetLongParameterValue(LPA_PARAMETER_ID parameterId, long* ptrParameterValue)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _lpaManagerGetLongParameterValue(parameterId=%d)", parameterId);

	if (ptrParameterValue != NULL)
	{
		switch (parameterId)
		{
			// Log max size
			case PARAM_ID_LOG_MAX_SIZE:
				if (NULL != ptrParameterValue)
				{
					 long logMaxSize = lpaCoreGetLogMaxSize(ptrParameterValue);
					 (*ptrParameterValue) = logMaxSize;
					res = true;
				}
			break;

			// CURL part (httpMedia)
			case PARAM_ID_CURL_CONNECT_TIMEOUT:
				res = httpMediaManagerGetLongOption(HTTP_MEDIA_OPTION_TYPE_CURL_CONNECT_TIMEOUT, ptrParameterValue);
			break;

			case PARAM_ID_CURL_TIMEOUT:
				res = httpMediaManagerGetLongOption(HTTP_MEDIA_OPTION_TYPE_CURL_TIMEOUT, ptrParameterValue);
			break;

			// Memory stress parameters
			case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_EQ:
			case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GT:
			case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GE:
			case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_EQ:
			case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GT:
			case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GE:
#ifdef LPA_SDK__MEMORY
				res = _lpaManagerGetLongMemoryErrorParameterValue(parameterId, ptrParameterValue);
#else
				lpaSetErrorCode(LPA_ERROR_PARAMETER_NOT_AUTHORIZED);
#endif //LPA_SDK__MEMORY
			break;

			default:
				lpaSetErrorCode(LPA_ERROR_UNKNOWN_PARAMETER);
			break;
		}
                
                if (res)
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter value                               : %ld", *ptrParameterValue);
	}
	else
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _lpaManagerGetLongParameterValue() return %s", (res ? "true" : "false"));

	return res;
}

#ifdef LPA_SDK__MEMORY
bool _lpaManagerGetLongMemoryErrorParameterValue(LPA_PARAMETER_ID parameterId, long* ptrParameterValue)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _lpaManagerGetLongMemoryErrorParameterValue(parameterId=%d)", parameterId);

	if (ptrParameterValue != NULL)
	{
		switch (parameterId)
		{
			case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_EQ:
				res = lpaCoreMemoryGetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_EQ, ptrParameterValue);
			break;

			case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GT:
				res = lpaCoreMemoryGetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GT, ptrParameterValue);
			break;

			case PARAM_ID_MEM_ERR_IF_MEM_COUNTER_GE:
				res = lpaCoreMemoryGetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GE, ptrParameterValue);
			break;

			case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_EQ:
				res = lpaCoreMemoryGetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_EQ, ptrParameterValue);
			break;

			case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GT:
				res = lpaCoreMemoryGetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GT, ptrParameterValue);
			break;

			case PARAM_ID_MEM_ERR_IF_SIZE_REQUESTED_GE:
				res = lpaCoreMemoryGetParamGenerateErr(LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GE, ptrParameterValue);
			break;

			default:
				lpaSetErrorCode(LPA_ERROR_UNKNOWN_PARAMETER);
			break;
		}
	}

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _lpaManagerGetLongMemoryErrorParameterValue() return %s", (res ? "true" : "false"));
	return res;
}
#endif //LPA_SDK__MEMORY

bool _lpaManagerSetStringParameterValue(const LPA_PARAMETER_DEFINITION * ptrParameterDefinition, const char* ptrParameterValue)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _lpaManagerSetStringParameterValue(...)");

	if(ptrParameterDefinition != NULL && ptrParameterValue != NULL)
	{
            switch (ptrParameterDefinition->parameterId)
            {
                case PARAM_ID_READER_NAME:
                    if (strlen(ptrParameterValue) < sizeof(_seMediaReaderName))
                    {
                            sprintf(_seMediaReaderName, "%s", ptrParameterValue);
                            res = true;
                    }
                    else
                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                break;

                case PARAM_ID_LOG_LEVEL:
                {
                    res = lpaCoreSetLogLevelString(ptrParameterValue);
                }
                break;

                case PARAM_ID_CERT_PATH:
                {
                    if (strlen(ptrParameterValue) < LPA_CFG_CERT_PATH_MAX_SIZE)
                    {
                            res = lpaManagerES9Plus_setCertPath(ptrParameterValue);
                    }
                    else
                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                }
                break;

                case PARAM_ID_CONFIG_DEVICE_INFO_TLV:
                {
                    if (strlen(ptrParameterValue) < LPA_CFG_DEVICE_INFO_TLV_MAX_SIZE)
                    {
                        int deviceInfoByteArraySize = LPA_CFG_DEVICE_INFO_TLV_BYTE_ARRAY_MAX_SIZE;
                        _deviceInfoByteArraySize = 0;
                        if (hexStr2ByteArray((const unsigned char *)ptrParameterValue, strlen(ptrParameterValue), _deviceInfoByteArray, &deviceInfoByteArraySize) && deviceInfoByteArraySize > 0)
                        {
                            _deviceInfoByteArraySize = deviceInfoByteArraySize;
                            snprintf(_deviceInfo, sizeof(_deviceInfo), "%s", ptrParameterValue);
                            res = true;
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "-- Unable to convert DeviceInfoTLV element as ByteArray !!");
                            memset(_deviceInfo, 0, sizeof(_deviceInfo));
                            memset(_deviceInfoByteArray, 0, sizeof(_deviceInfoByteArray));
                        }
                    }
                    else
                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                }
                break;

                default:
                    lpaSetErrorCode(LPA_ERROR_UNKNOWN_PARAMETER);
                break;
            }
	}
	else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerSetStringParameterValue() : NULL parameter! Operation canceled.");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _lpaManagerSetStringParameterValue() return %s", (res ? "true" : "false"));
	return res;
}

bool _lpaManagerGetStringParameterValue(LPA_PARAMETER_ID parameterId, char* ptrParameterValue, size_t parameterValueMaxSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _lpaManagerGetStringParameterValue(parameterId=%d)", parameterId);

	if (ptrParameterValue != NULL && parameterValueMaxSize > 0)
	{
		switch (parameterId)
		{
			case PARAM_ID_READER_NAME:
				if (strlen(_seMediaReaderName) < parameterValueMaxSize)
				{
					sprintf(ptrParameterValue, "%s", _seMediaReaderName);
					res = true;
				}
				else
					lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
			break;

			case PARAM_ID_CERT_PATH:
			{
				if (lpaManagerES9Plus_getCertPathSize() < parameterValueMaxSize)
					res = lpaManagerES9Plus_getCertPath(ptrParameterValue, parameterValueMaxSize);
				else
					lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
			}
			break;

			case PARAM_ID_CONFIG_DEVICE_INFO_TLV:
			{
				if (strlen(_deviceInfo) < parameterValueMaxSize)
				{
					sprintf(ptrParameterValue, "%s", _deviceInfo);
					res = true;
				}
				else
					lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
			}
			break;

			case PARAM_ID_LOG_LEVEL:
			{
				LpaLogLevel logLevel = lpaCoreGetLogLevel();
				bool isLogLevelExist = false;
				const char* logLevelString = lpaCoreGetLogLevelName(logLevel, &isLogLevelExist);
				if (logLevelString != NULL && isLogLevelExist)
				{
					if (strlen(logLevelString) < parameterValueMaxSize)
					{
						sprintf(ptrParameterValue, "%s", logLevelString);
						res = true;
					}
					else
						lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
				}
				else
					lpaSetErrorCode(LPA_ERROR_PARAMETER_INTERNAL_ERROR);
			}
			break;

			default:
				lpaSetErrorCode(LPA_ERROR_UNKNOWN_PARAMETER);
			break;
		}
                
                if (res)
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameter value                               : %s", ptrParameterValue);
	}
	else
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _lpaManagerGetStringParameterValue() return %s", (res ? "true" : "false"));
	return res;
}


bool lpaManagerGetReaderList(LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader)
{
	return seMediaManagerListReader(ptrReaderNameInfoList, readerNameInfoMax, ptrCountReader);
}

// ES10c part
//////////////////////////////////////////////

bool lpaManagerGetProfilesInfo(LPA_GET_PROFILES_INFO* ptrLpaGetProfilesInfo)
{
    bool res = true;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;    // If retry deactivated, will be performed only one time
    bool continueRetry = true;  // Allow to know if have to give up performing GetResponse retry (Case of permanent issue for retrieve number of profile in eUICC). true = continue retrying
    
    // Execution / retry management
    while(nbExecGetResp > 0 && continueRetry)
    {
        res = lpaManagerES10c_GetProfilesInfo(ptrLpaGetProfilesInfo, &continueRetry, false);
        
        if(lpaGetErrorCodeNoClear() != SE_MEDIA_E_CHAINING_GET_RESPONSE)
        {
            // No chaining error detected, end execution loop
            nbExecGetResp = 0;
        }
        else
        {
            // Chaining error detected, continue on execution loop
            nbExecGetResp--;
            // If last loop not reached, clear error code to execute another attempt
            // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
            if(nbExecGetResp > 0 && continueRetry)
            {
                lpaResetErrorCode();
                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "lpaManagerGetProfilesInfo: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);                
            }
        }
    }
    
    return res;
}

bool lpaManagerGetProfilesInfo_ex(LPA_GET_PROFILES_INFO* ptrLpaGetProfilesInfo)
{
    bool res = true;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;    // If retry deactivated, will be performed only one time
    bool continueRetry = true;  // Allow to know if have to give up performing GetResponse retry (Case of permanent issue for retrieve number of profile in eUICC). true = continue retrying
    
    // Execution / retry management
    while(nbExecGetResp > 0 && continueRetry)
    {
        res = lpaManagerES10c_GetProfilesInfo_ex(ptrLpaGetProfilesInfo, &continueRetry, true);
        
        if(lpaGetErrorCodeNoClear() != SE_MEDIA_E_CHAINING_GET_RESPONSE)
        {
            // No chaining error detected, end execution loop
            nbExecGetResp = 0;
        }
        else
        {
            // Chaining error detected, continue on execution loop
            nbExecGetResp--;
            // If last loop not reached, clear error code to execute another attempt
            // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
            if(nbExecGetResp > 0 && continueRetry)
            {
                lpaResetErrorCode();
                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "lpaManagerGetProfilesInfo: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);                
            }
        }
    }
    
    return res;
}

bool lpaManagerGetProfilesNumber(size_t * ptrNumberOfProfiles)
{
    bool res = true;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;    // If retry deactivated, will be performed only one time
    
    // Execution / retry management
    while(nbExecGetResp > 0)
    {
        res = lpaManagerES10c_GetProfilesNumber(ptrNumberOfProfiles);
        
        if(lpaGetErrorCodeNoClear() != SE_MEDIA_E_CHAINING_GET_RESPONSE)
        {
            // No chaining error detected, end execution loop
            nbExecGetResp = 0;
        }
        else
        {
            // Chaining error detected, continue on execution loop
            nbExecGetResp--;
            // If last loop not reached, clear error code to execute another attempt
            // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
            if(nbExecGetResp > 0)
            {
                lpaResetErrorCode();
                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "lpaManagerGetProfilesNumber: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);
            }
        }
    }
    
    return res;
}

/**
 * Retrieve profile info for PPR processing
 * Note: This is for internal use, so memory allocation for ptrLpaGetProfilesInfo->profileInfoList will be performed here, so pointer should be NULL (De-allocated after if not)
 * @param ptrLpaGetProfilesInfo - Pointer on profile info structure. 
 * @return true if retrieve operation is successful
 */
bool _getProfilesInfoForPPR(LPA_GET_PROFILES_INFO* ptrLpaGetProfilesInfo)
{
    bool res = true;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;    // If retry deactivated, will be performed only one time
    bool continueRetry = true;  // Allow to know if have to give up performing GetResponse retry (Case of permanent issue for retrieve number of profile in eUICC). true = continue retrying
    
    if(ptrLpaGetProfilesInfo != NULL)
    {
        ptrLpaGetProfilesInfo->countProfileInfo = 0;
        ptrLpaGetProfilesInfo->numberProfileInfoFound = 0;
        
        // Execution / retry management
        while(nbExecGetResp > 0 && continueRetry)
        {
            res = lpaManagerES10c_GetProfilesInfo(ptrLpaGetProfilesInfo, &continueRetry, true);

            if(lpaGetErrorCodeNoClear() != SE_MEDIA_E_CHAINING_GET_RESPONSE)
            {
                // No chaining error detected, end execution loop
                nbExecGetResp = 0;
            }
            else
            {
                // Chaining error detected, continue on execution loop
                nbExecGetResp--;
                // If last loop not reached, clear error code to execute another attempt
                // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                if(nbExecGetResp > 0 && continueRetry)
                {
                    lpaResetErrorCode();
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_getProfilesInfoForPPR: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);
                }
            }
        }
    }
    else
    {
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_getProfilesInfoForPPR: NULL parameter!");
    }
    
    return res;
}


/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerGetEID(LPA_GET_EID* ptrGetEID)
{
	return lpaManagerES10c_GetEID(ptrGetEID);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerMemoryReset(const unsigned char* memoryResetOptionParameter, const size_t memoryResetOptionSize)
{
	return lpaManagerES10c_MemoryReset(memoryResetOptionParameter, memoryResetOptionSize);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerEnableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	return lpaManagerES10c_EnableProfileByIccid(ptrProfileId, profileIdSize);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerDisableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	return lpaManagerES10c_DisableProfileByIccid(ptrProfileId, profileIdSize);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerDeleteProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	return lpaManagerES10c_DeleteProfileByIccid(ptrProfileId, profileIdSize);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerSendPendingNotification(LPA_EventCallback* ptrLpaEventCallback, LPA_SENDING_NOTIFICATION_RESULT* ptrSendingNotificationResult)
{
    bool res = false;
    bool sendItSuccessfully = false;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;    // If Retry deactivated, will be performed only one time
    int notificationProcNumber = 0;
    
    LPA_API_ERROR firstEncounteredErrorCode = LPA_NO_ERROR; // To store first encountered error code if GetResponse Retry mechanism enabled

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerSendPendingNotification(...)");

    // Registering LPA_EVENT_EXECUTION_ERROR (if configured) for this call only
    _registerAppEventExecutionCallback(ptrLpaEventCallback);

    // ptrLpaEventCallback NULL managed in another function
    if (ptrSendingNotificationResult != NULL)
    {
        ptrSendingNotificationResult->countNotificationDetected = 0;
        ptrSendingNotificationResult->countNotificationSend = 0;

        // Step 1 : get Pending notification list
        LPA_PROFILE_NOTIFICATION_LIST profileNotificationList;
        _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Read pending notifications...");

        // If Retry enabled and failed during notification list retrieve, will return false so operation will end with SE_MEDIA_E_CHAINING_GET_RESPONSE error
        res = lpaManagerGetProfileNotificationList(&profileNotificationList);
        if (res)
        {
            if (profileNotificationList.countNotification > 0)
            {
                char bufferFormattingNotificationEventMessage[128];

                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "<%d> pending notification(s) detected", profileNotificationList.countNotification);

                snprintf(bufferFormattingNotificationEventMessage, sizeof(bufferFormattingNotificationEventMessage), "<%d> pending notification(s) detected", (unsigned int)profileNotificationList.countNotification);
                _sendEventCallbackProgressText(ptrLpaEventCallback, 0, bufferFormattingNotificationEventMessage);

                ptrSendingNotificationResult->countNotificationDetected = profileNotificationList.countNotification;

                // Sort list of notification by sequence numbers, ascending order
                if(_sortNotificationList(&profileNotificationList))
                {
                    size_t notificationManaged = 0;

                    RawDataObject* ptrRawDataNotification = rawDataObject_allocate();
                    if (ptrRawDataNotification != NULL)
                    {
                        // Step 2 : for each notification entry, get notification data and send them to the server
                        while (notificationManaged < profileNotificationList.countNotification)
                        {
                            sendItSuccessfully = false;
                            nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;

                            // Try to send notification to the server
                            uint16_t notificationSequenceNumber = profileNotificationList.notificationMetadataList[notificationManaged].seqNumber;
                            rawDataObject_clear(ptrRawDataNotification);

                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Processing notification #%d <%d>...", ++notificationProcNumber, notificationSequenceNumber);
                            snprintf(bufferFormattingNotificationEventMessage, sizeof(bufferFormattingNotificationEventMessage), "Processing notification #%d <%d>",
                                                                                                                            notificationProcNumber, notificationSequenceNumber);
                            _sendEventCallbackProgressText(ptrLpaEventCallback, 0, bufferFormattingNotificationEventMessage);

                            // Execution / retry management for notification data retrieval
                            while(nbExecGetResp > 0)
                            {
                                if (_retrieveNotificationData(notificationSequenceNumber, ptrRawDataNotification))
                                {
                                    // No chaining error detected, end execution loop
                                    nbExecGetResp = 0;

                                    sendItSuccessfully = _sendNotificationToServer(&profileNotificationList.notificationMetadataList[notificationManaged], ptrRawDataNotification, ptrLpaEventCallback);
                                }
                                else
                                {
                                    // Decrease execution loop. If Retry not enabled, will reach 0 so no retry
                                    nbExecGetResp--;

                                    // Retry management
                                    if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                                    {
                                        // If last loop not reached, clear error code to execute another attempt
                                        // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                                        if(nbExecGetResp > 0)
                                        {
                                            lpaResetErrorCode();
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "lpaManagerSendPendingNotification: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);

                                            rawDataObject_clear(ptrRawDataNotification);
                                        }
                                    }
                                }
                            }

                            if (sendItSuccessfully)
                            {
                                if(lpaManagerClearProfileNotification(notificationSequenceNumber))
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Notification <%d> successfully cleared", notificationSequenceNumber);
                                else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Failed to clear notification <%d>", notificationSequenceNumber);

                                ptrSendingNotificationResult->countNotificationSend++;

                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Notification <%d> successfully sent", notificationSequenceNumber);

                                snprintf(bufferFormattingNotificationEventMessage, sizeof(bufferFormattingNotificationEventMessage), "Notification <%d> successfully sent", notificationSequenceNumber);
                                _sendEventCallbackProgressText(ptrLpaEventCallback, 0, bufferFormattingNotificationEventMessage);
                            }
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Failed to send notification <%d>", notificationSequenceNumber);

                                snprintf(bufferFormattingNotificationEventMessage, sizeof(bufferFormattingNotificationEventMessage), "Failed to send notification <%d>", notificationSequenceNumber);
                                _sendEventCallbackProgressText(ptrLpaEventCallback, 0, bufferFormattingNotificationEventMessage);
                            }

                            notificationManaged++;

                            // Management of error code. Need to clear it at each notification read if GetResponse Retry mechanism is enabled
                            // So keep the first one encountered to deliver it at the end like previously
                            if(LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                            {
                                if(lpaGetErrorCodeNoClear() != LPA_NO_ERROR && firstEncounteredErrorCode == LPA_NO_ERROR)
                                {
                                    firstEncounteredErrorCode = lpaGetErrorCodeNoClear();
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerSendPendingNotification(): First encountered Error Code: 0x%06X", firstEncounteredErrorCode);
                                }

                                lpaResetErrorCode();
                            }
                        }

                        // Cleanup memory
                        rawDataObject_free(ptrRawDataNotification);
                        ptrRawDataNotification = NULL;
                    }
                    else
                    {
                        res = false;
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to allocate memory for ptrByteArrayNotificationData!");
                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                    }
                }
                else
                {
                    res = false;
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to sort list of notifications");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Failed to sort list of notifications");
                    lpaSetErrorCode(LPA_ERROR_PROCESSING_ERROR);
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No pending notification found");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "No pending notification found");
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve list of notifications!");
            _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Failed to retrieve list of notifications");
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter(s)!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

    // And unregistering LPA_EVENT_EXECUTION_ERROR callback
    _unregisterAppEventExecutionCallback();
    
    // Management of error code if GetResponse Retry mechanism enabled
    // If any error encountered while sending notification(s), restore it (First occurrence only)
    if(LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
    {
        if(firstEncounteredErrorCode != LPA_NO_ERROR)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerSendPendingNotification(): Restore first Error Code encountered: 0x%06X", firstEncounteredErrorCode);
            lpaResetErrorCode();
            lpaSetErrorCode(firstEncounteredErrorCode);
        }
    }

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerSendPendingNotification()");

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Sort notification list in ascending order
 * @param ptrNotificationList Pointer on notification list to sort, LPA_PROFILE_NOTIFICATION_LIST type
 * @return True if no error during sorting
 */
bool _sortNotificationList(LPA_PROFILE_NOTIFICATION_LIST * ptrNotificationList)
{
    bool res = false;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_sortNotificationList()...");
    
    if(ptrNotificationList != NULL)
    {
        if(ptrNotificationList->countNotification > 1)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_sortNotificationList(): Start sorting notification list...");
            
            // Declare here save heap operations if no need to sort
            int maxIterations = ptrNotificationList->countNotification - 1; // Up to N-1 iterations will be needed 
            int i = 0;
            int j = 0;
            bool swapPerformed = false;
            LPA_PROFILE_NOTIFICATION_METADATA listElementSwap;
            
            // Final status will be invalidated only if problem is detected
            res = true;
            
            for(i = 0; i < maxIterations; i++)
            {
                swapPerformed = false;
                for(j = 0; j < maxIterations; j++)
                {
                    // Sequence number of current notification is greater -> Perform swap of notifications in list
                    if(ptrNotificationList->notificationMetadataList[j].seqNumber > ptrNotificationList->notificationMetadataList[j+1].seqNumber)
                    {
                        swapPerformed = true;
                        res = _copyNotificationListElement(&ptrNotificationList->notificationMetadataList[j+1], &listElementSwap);
                        if(res)
                            res = _copyNotificationListElement(&ptrNotificationList->notificationMetadataList[j], &ptrNotificationList->notificationMetadataList[j+1]);
                        if(res)
                            res = _copyNotificationListElement(&listElementSwap, &ptrNotificationList->notificationMetadataList[j]);
                    }
                    
                    // If problem detected stop this loop
                    if(! res) break;
                }
                
                // No more swap performed => no need to perform more iterations, the list is sorted
                // Also exit in case of error
                if(! swapPerformed || ! res) break;
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_sortNotificationList(): Less than 2 elements reported in list, sorting is useless.");
            res = true;
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_sortNotificationList(): Invalid NULL parameter!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_sortNotificationList() return %s", (res ? "true" : "false"));
    return res;
}

/**
 * Copy notification list element from source to destination data structure
 * @param ptrSource Pointer on source element, LPA_PROFILE_NOTIFICATION_METADATA type
 * @param ptrDestination Pointer on destination element, LPA_PROFILE_NOTIFICATION_METADATA type
 * @return True if no error during copy
 */
bool _copyNotificationListElement(LPA_PROFILE_NOTIFICATION_METADATA * ptrSource, LPA_PROFILE_NOTIFICATION_METADATA * ptrDestination)
{
    bool res = false;
    
    if(ptrSource != NULL && ptrDestination != NULL)
    {
        ptrDestination->seqNumber = ptrSource->seqNumber;
        ptrDestination->profileManagementOperation = ptrSource->profileManagementOperation;
        ptrDestination->notificationAddressRawDataSize = ptrSource->notificationAddressRawDataSize;
        memcpy(ptrDestination->notificationAddressRawData, ptrSource->notificationAddressRawData, LPA_MAX_NOTIFICATION_ADDRESS_RAW_DATA_SIZE);
        
        res = true;
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_copyNotificationListElement(): Invalid NULL parameter(s)!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }
    
    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _retrieveNotificationData(uint16_t sequenceNumber, RawDataObject* ptrRawDataNotification)
{
	bool res = false;
	unsigned char byteArraySequenceNumber[16];
	size_t byteArraySequenceNumberSize = 0;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_retrieveNotificationData(sequenceNumber=%d) ...", sequenceNumber);

        if(ptrRawDataNotification != NULL)
        {
            if (writeIntegerValueToByteArray(sequenceNumber, byteArraySequenceNumber, sizeof(byteArraySequenceNumber), &byteArraySequenceNumberSize))
            {
                    RawDataObject* rawDataObjectSequenceNumber = NULL;
                    RawDataObject* rawDataObjectAttribute = NULL;
                    RawDataObject* rawDataObjectRetrieveNotificationsList = NULL;

                    rawDataObjectSequenceNumber = berTLV_createAndBuildRawDataObject(ASN1_PRIMITIVE_ATTRIBUTE_TAG, byteArraySequenceNumberSize, byteArraySequenceNumber);

                    if (rawDataObjectSequenceNumber != NULL)
                            rawDataObjectAttribute = berTLV_createAndBuildRawDataObject(ASN1_CONSTRUCTED_ATTRIBUTE_TAG, rawDataObjectSequenceNumber->rawDataSize, rawDataObjectSequenceNumber->rawData);

                    if (rawDataObjectAttribute != NULL)
                            rawDataObjectRetrieveNotificationsList = berTLV_createAndBuildRawDataObject(RETRIEVE_NOTIFICATIONS_LIST_TAG, rawDataObjectAttribute->rawDataSize, rawDataObjectAttribute->rawData);

                    if (rawDataObjectRetrieveNotificationsList != NULL)
                    {
                            // build and Send APDU
                            uint16_t apduSW = 0x0000;
                            size_t dataBufferSize = 0;

                            // Will be failed if unattended SW or GetResponse chaining issue is encountered
                            if (buildAndSendStoreDataCase4(rawDataObjectRetrieveNotificationsList, &apduSW, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
                            {
                                // Check SW = 90.00 or 91.xx
                                if((apduSW == 0x9000) || ((apduSW & 0xFF00) == 0x9100))
                                {
                                    res = _extractNotificationData(_dataBuffer, dataBufferSize, ptrRawDataNotification);
                                    if (res)
                                    {
                                        if (formatBytesToHexaString(ptrRawDataNotification->rawData, ptrRawDataNotification->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Notification rawData found ==> value (%d bytes) = %s", ptrRawDataNotification->rawDataSize, _bufferFormatLogMessage);
                                        else
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Notification rawData found ==> value (%d bytes) = ... ", ptrRawDataNotification->rawDataSize);
                                    }
                                }
                                else
                                    lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Problem encountered when trying to retrieve notification data: APDU execution failed!");
                    }
                    else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "rawDataObjectRetrieveNotificationsList is NULL !");
                    
                    // Do memory cleanup
                    ERASE_RAWDATAOBJECT(rawDataObjectSequenceNumber);
                    ERASE_RAWDATAOBJECT(rawDataObjectAttribute);
                    ERASE_RAWDATAOBJECT(rawDataObjectRetrieveNotificationsList);
            }
            else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to write sequenceNumber into byte array !");
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter!");


	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_retrieveNotificationData() return %s", (res ? "true" : "false"));
	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _extractNotificationData(const unsigned char* ptrDataBuffer, size_t dataBufferSize, RawDataObject* ptrRawDataNotification)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractNotificationData() ...");

	if (ptrDataBuffer != NULL && dataBufferSize > 0 && ptrRawDataNotification != NULL)
	{
		bool tagFound = false;
		BeerTLV* berTLVRetrieveNotificationsList = NULL;
		BeerTLV* berTLVConstructedAttribute = NULL;

		berTLVRetrieveNotificationsList = berTLV_extractTagUInt16(RETRIEVE_NOTIFICATIONS_LIST_TAG, ptrDataBuffer, dataBufferSize, &tagFound);
		if (berTLVRetrieveNotificationsList != NULL)
		{
			berTLVConstructedAttribute = berTLV_extractTagUInt8(ASN1_CONSTRUCTED_ATTRIBUTE_TAG, berTLVRetrieveNotificationsList->value, berTLVRetrieveNotificationsList->length, &tagFound);
			if (berTLVConstructedAttribute != NULL)
			{
				rawDataObject_update(ptrRawDataNotification, berTLVConstructedAttribute->value, berTLVConstructedAttribute->length);
				res = true;
			}
		}

		// Cleanup memory
		ERASE_BERTLV(berTLVRetrieveNotificationsList);
		ERASE_BERTLV(berTLVConstructedAttribute);
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractNotificationData() return %s", (res ? "true" : "false"));
	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _sendNotificationToServer(const LPA_PROFILE_NOTIFICATION_METADATA* ptrProfileNotificationMetadata, const RawDataObject* ptrRawDataNotification, const LPA_EventCallback* ptrLpaEventCallback)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_sendNotificationToServer(...) ...");

	if (ptrProfileNotificationMetadata != NULL && ptrRawDataNotification != NULL)
	{
                char * tmpInfo = NULL;
                size_t convBuffSize = ptrRawDataNotification->rawDataSize + (ptrRawDataNotification->rawDataSize / 2); // 150 % greater. A bit more than 137 % commonly described
                tmpInfo = lpaCoreMemoryAlloc(convBuffSize);
		
                if(tmpInfo != NULL)
                {
                    size_t outlen = 0;
                    memset(tmpInfo, 0, convBuffSize);

                    if (ffw_base64_encode(ptrRawDataNotification->rawData, ptrRawDataNotification->rawDataSize, tmpInfo, &outlen, convBuffSize))
                    {
                            if (_handleNotification((const char*)ptrProfileNotificationMetadata->notificationAddressRawData, ptrProfileNotificationMetadata->notificationAddressRawDataSize, (const unsigned char*)tmpInfo, ptrLpaEventCallback))
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Success to send notification data to server!");
                                    res = true;
                            }
                            else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send notification data to server!");
                    }
                    else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot convert Notification data in Base64 !");
                    
                    lpaCoreMemoryFree(tmpInfo);
                    tmpInfo = NULL;
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot allocate %d bytes for conversion buffer tmpInfo!", convBuffSize);
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_sendNotificationToServer() return %s", (res ? "true" : "false"));
	return res;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////


bool lpaManagerGetProfileNotificationList(LPA_PROFILE_NOTIFICATION_LIST* ptrProfileNotificationList)
{
    bool res = false;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;    // If retry deactivated, will be performed only one time

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerGetProfileNotificationList(...)");

    if (ptrProfileNotificationList != NULL)
    {
        RawDataObject* rawDataObjectGetProfileNotificationList = NULL;

        ptrProfileNotificationList->countNotification = 0;

        rawDataObjectGetProfileNotificationList = berTLV_createAndBuildRawDataObject(LIST_NOTIFICATION_TAG, 0, NULL);
        if (rawDataObjectGetProfileNotificationList != NULL)
        {
            // build and Send APDU
            uint16_t apduSW = 0x0000;
            size_t dataBufferSize = 0;

            // Execution / retry management
            while(nbExecGetResp > 0)
            {
                // In case of retry, better to re-initialize theses values                
                apduSW = 0x0000;
                dataBufferSize = 0;
                
                if (buildAndSendStoreDataCase4(rawDataObjectGetProfileNotificationList, &apduSW, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
                {
                    // No chaining error detected, end execution loop
                    nbExecGetResp = 0;

                    // Check SW = 90.00 or 91.xx
                    if((apduSW == 0x9000) || ((apduSW & 0xFF00) == 0x9100))
                    {
                        // APDU successfully sent
                        if (dataBufferSize == 0)
                        {
                            res = true; // no data
                        }
                        else
                        {
                            if (_doExtractMetadataForGetProfileNotificationList(ptrProfileNotificationList, _dataBuffer, dataBufferSize))
                                res = true;
                            else
                                lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNABLE_TO_EXTRACT_DATA);
                        }
                    }
                    else
                        lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                }
                else
                {
                    // Decrease execution loop. If Retry not enabled, will reach 0 so no retry
                    nbExecGetResp--;

                    // Retry management
                    if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                    {
                        // If last loop not reached, clear error code to execute another attempt
                        // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                        if(nbExecGetResp > 0)
                        {
                            lpaResetErrorCode();
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "lpaManagerGetProfileNotificationList: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);
                        }
                    }
                    else
                        lpaSetErrorCode(LPA_ERROR_NOTIFICATION_INCORRECT_CARD_RESPONSE);
                        
                }
            }

            // Do memory cleanup
            ERASE_RAWDATAOBJECT(rawDataObjectGetProfileNotificationList);
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot build Notification List raw data object!");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerGetProfileNotificationList() : return %s", (res ? "true" : "false"));
    return res;
}

bool _doExtractMetadataForGetProfileNotificationList(LPA_PROFILE_NOTIFICATION_LIST* ptrProfileNotificationList, unsigned char *ptrData, size_t dataSize)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _doExtractMetadataForGetProfileNotificationList(...)");

	if (ptrProfileNotificationList != NULL && ptrData != NULL && dataSize > 0)
	{
		bool tagFound = false;
		BeerTLV* berTLVListNotificationTag = NULL;
		BeerTLV* berTLVSequenceTag = NULL;
		BerTLVList* tlvListInsideSequenceTag = NULL;

		ptrProfileNotificationList->countNotification = 0;

		berTLVListNotificationTag = berTLV_extractTagUInt16(LIST_NOTIFICATION_TAG, ptrData, dataSize, &tagFound);
		if (berTLVListNotificationTag != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "BerTLV tag '%04lx' found", LIST_NOTIFICATION_TAG);

			berTLVSequenceTag = berTLV_extractTagUInt8(SEQUENCE_TAG, berTLVListNotificationTag->value, berTLVListNotificationTag->length, &tagFound);
		}

		if (berTLVSequenceTag != NULL)
		{
			uint8_t countTLVFound = 0;

			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "BerTLV tag '%02lx' found (data size:%d)", SEQUENCE_TAG, berTLVSequenceTag->length);
			if (berTLVSequenceTag->length > 0)
			{
				tlvListInsideSequenceTag = berTLV_extractList(berTLVSequenceTag->value, berTLVSequenceTag->length, &countTLVFound);

				if (countTLVFound > 0 && tlvListInsideSequenceTag != NULL)
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "%d tag found inside Ber TLV object", countTLVFound);

					BerTLVList* berTLVCurrentInsideSequenceTag = tlvListInsideSequenceTag;
					while (berTLVCurrentInsideSequenceTag != NULL)
					{
						if (berTLVCurrentInsideSequenceTag->berTLV != NULL && berTLVCurrentInsideSequenceTag->berTLV->tag == NOTIFICATION_METADATA_TAG)
						{
							bool tag80Found = false, tag81Found = false, tag0CFound = false;
							BeerTLV *ptrNotificationTag80 = NULL, *ptrNotificationTag81 = NULL, *ptrNotificationTag0C = NULL;
							bool isSequenceError = false;

							if (formatBytesToHexaString(berTLVCurrentInsideSequenceTag->berTLV->value, berTLVCurrentInsideSequenceTag->berTLV->length, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
								lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "NOTIFICATION_METADATA_TAG found : %s", _bufferFormatLogMessage);
							else
								lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "NOTIFICATION_METADATA_TAG found : ...");


							// extract metadata for current notification
							ptrNotificationTag80 = berTLV_extractTagUInt8(0x80, berTLVCurrentInsideSequenceTag->berTLV->value, berTLVCurrentInsideSequenceTag->berTLV->length, &tag80Found);
							ptrNotificationTag81 = berTLV_extractTagUInt8(0x81, berTLVCurrentInsideSequenceTag->berTLV->value, berTLVCurrentInsideSequenceTag->berTLV->length, &tag81Found);
							ptrNotificationTag0C = berTLV_extractTagUInt8(0x0C, berTLVCurrentInsideSequenceTag->berTLV->value, berTLVCurrentInsideSequenceTag->berTLV->length, &tag0CFound);

							// Actually, only Tag 80,81 & 0C mandatory and used
							if (ptrNotificationTag80 != NULL && ptrNotificationTag81 != NULL && ptrNotificationTag0C != NULL)
							{
								uint16_t seqNumber = 0;

								if (formatBytesToHexaString(ptrNotificationTag80->value, ptrNotificationTag80->length, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
									lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 80 found : %s", _bufferFormatLogMessage);
								else
									lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 80 found : ...");

								if (formatBytesToHexaString(ptrNotificationTag81->value, ptrNotificationTag81->length, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
									lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 81 found : %s", _bufferFormatLogMessage);
								else
									lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 81 found : ...");

								if (formatBytesToHexaString(ptrNotificationTag0C->value, ptrNotificationTag0C->length, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
									lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 0C found : %s", _bufferFormatLogMessage);
								else
									lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 0C found : ...");


								if (extractIntegerFromByteArray(ptrNotificationTag80->value, ptrNotificationTag80->length, &seqNumber))
								{
									LPA_NOTIFICATION_EVENT notificationEvent = LPA_NOTIFICATION_UNKNOWN; // Default value
									LPA_PROFILE_NOTIFICATION_METADATA* ptrCurrentProfileNotificationMetadata = &(ptrProfileNotificationList->notificationMetadataList[ptrProfileNotificationList->countNotification]);
                                                                        
                                                                        if(ptrCurrentProfileNotificationMetadata != NULL)
                                                                        {
                                                                            ptrCurrentProfileNotificationMetadata->seqNumber = seqNumber;
                                                                            ptrCurrentProfileNotificationMetadata->notificationAddressRawDataSize = 0;

                                                                            // Extract Notification Event value
                                                                            notificationEvent = _doExtractNotificationEventValue(ptrNotificationTag81->value, ptrNotificationTag81->length);
                                                                            ptrCurrentProfileNotificationMetadata->profileManagementOperation = notificationEvent;

                                                                            // Add Notification Address
                                                                            if (ptrNotificationTag0C->length <= LPA_MAX_NOTIFICATION_ADDRESS_RAW_DATA_SIZE)
                                                                            {
                                                                                    memcpy(ptrCurrentProfileNotificationMetadata->notificationAddressRawData, ptrNotificationTag0C->value, ptrNotificationTag0C->length);
                                                                                    ptrCurrentProfileNotificationMetadata->notificationAddressRawDataSize = ptrNotificationTag0C->length;
                                                                            }
                                                                            else
                                                                            {
                                                                                    isSequenceError = true;
                                                                                    lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                                                            }
                                                                        }
                                                                        else    // AK added this by security, but this error is more relevant from internal data structure error than a card error.
                                                                        {
                                                                            isSequenceError = true;
                                                                            lpaSetErrorCode(LPA_ERROR_NOTIFICATION_UNDEFINED_ERROR);
                                                                        }
								}
								else
								{
									isSequenceError = true;
									lpaSetErrorCode(LPA_ERROR_NOTIFICATION_INVALID_CARD_DATA);
								}
							}
							else
							{
								isSequenceError = true;
								lpaSetErrorCode(LPA_ERROR_NOTIFICATION_INVALID_CARD_DATA);
							}

							if (!isSequenceError)
								ptrProfileNotificationList->countNotification++;

							// Do cleanup for current notification
							ERASE_BERTLV(ptrNotificationTag80);
							ERASE_BERTLV(ptrNotificationTag81);
							ERASE_BERTLV(ptrNotificationTag0C);
						}

						berTLVCurrentInsideSequenceTag = berTLVCurrentInsideSequenceTag->ptrNext;
					}

					if (countTLVFound == ptrProfileNotificationList->countNotification)
						res = true;
				}
				else
					res = (countTLVFound == 0 && tlvListInsideSequenceTag == NULL); // If no data => OK
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No notification available");
				res = true; // Ok, APDU data response like "BF2802A000"
			}
		}


		// Do cleanup
		ERASE_BERTLV(berTLVListNotificationTag);
		ERASE_BERTLV(berTLVSequenceTag);
		ERASE_BERTLV_LIST(tlvListInsideSequenceTag);
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _doExtractMetadataForGetProfileNotificationList(...) : return %s", (res ? "true" : "false"));

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

LPA_NOTIFICATION_EVENT _doExtractNotificationEventValue(unsigned char* ptrDataValue, size_t dataLength)
{
	LPA_NOTIFICATION_EVENT notificationEvent = LPA_NOTIFICATION_UNKNOWN;

	if (dataLength == 2 && ptrDataValue != NULL)
	{
		uint16_t value = ptrDataValue[0] << 8 | ptrDataValue[1];
		switch (value)
		{
		case 0x0780:
			notificationEvent = LPA_NOTIFICATION_INSTALL;
			break;

		case 0x0640:
			notificationEvent = LPA_NOTIFICATION_ENABLE;
			break;

		case 0x0520:
			notificationEvent = LPA_NOTIFICATION_DISABLE;
			break;

		case 0x0410:
			notificationEvent = LPA_NOTIFICATION_DELETE;
			break;
		}
	}

	return notificationEvent;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool lpaManagerClearProfileNotification(uint16_t sequenceNumber)
{
	bool res = false;
	unsigned char byteArraySequenceNumber[16];
	size_t byteArraySequenceNumberSize = 0;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerClearProfileNotification(sequenceNumber=%d)", sequenceNumber);

	if (writeIntegerValueToByteArray(sequenceNumber, byteArraySequenceNumber, sizeof(byteArraySequenceNumber), &byteArraySequenceNumberSize))
	{
		RawDataObject* rawDataObjectRemoveNotificationFromList = NULL;
		RawDataObject* rawDataObjectSequenceNumber = NULL;

		rawDataObjectSequenceNumber = berTLV_createAndBuildRawDataObject(DER_ATTRIBUTE_TAG, byteArraySequenceNumberSize, byteArraySequenceNumber);

		if (rawDataObjectSequenceNumber != NULL)
			rawDataObjectRemoveNotificationFromList = berTLV_createAndBuildRawDataObject(REMOVE_NOTIFICATION_FROM_LIST_TAG, rawDataObjectSequenceNumber->rawDataSize, rawDataObjectSequenceNumber->rawData);

		if (rawDataObjectRemoveNotificationFromList != NULL)
		{
			// build and Send APDU
			uint16_t apduSW = 0x0000;
			size_t dataBufferSize = 0;

			if (buildAndSendStoreDataCase4(rawDataObjectRemoveNotificationFromList, &apduSW, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
			{
				// APDU successfully sent => analyze SW
				if((apduSW == 0x9000) || ((apduSW & 0xFF00) == 0x9100))
				{
					LPA_DELETE_NOTIFICATION_STATUS deleteNotificationStatus = _doExtractClearProfileNotificationResponse(_dataBuffer, dataBufferSize);
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerClearProfileNotification() : deleteNotificationStatus = %d", deleteNotificationStatus);

					switch (deleteNotificationStatus)
					{
					case LPA_DELETE_NOTIFICATION_OK:
						// operation done successfully
						res = true;
						break;

					case LPA_DELETE_NOTIFICATION_NOTHING_TO_DELETE:
						lpaSetErrorCode(LPA_ERROR_NOTIFICATION_NOTHING_TO_DELETE);
						break;

					case LPA_DELETE_NOTIFICATION_UNDEFINED_ERROR:
						lpaSetErrorCode(LPA_ERROR_NOTIFICATION_UNDEFINED_ERROR);
						break;

					default:
						lpaSetErrorCode(LPA_ERROR_NOTIFICATION_UNKNOWN_ERROR);
					}
				}
				else
					lpaSetErrorCode(LPA_ERROR_INVALID_SW);
			}

			// Do memory cleanup
			ERASE_RAWDATAOBJECT(rawDataObjectRemoveNotificationFromList);
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "rawDataObjectRemoveNotificationFromList is NULL !");
                
                // Moved here, can be defined out of rawDataObjectRemoveNotificationFromList
                ERASE_RAWDATAOBJECT(rawDataObjectSequenceNumber);

	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to write sequenceNumber into byte array !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerClearProfileNotification() : return %s", (res ? "true" : "false"));
	return res;
}


LPA_DELETE_NOTIFICATION_STATUS _doExtractClearProfileNotificationResponse(unsigned char *ptrData, size_t dataSize)
{
	LPA_DELETE_NOTIFICATION_STATUS deleteNotificationStatus = LPA_DELETE_NOTIFICATION_UNKNOWN;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _doExtractClearProfileNotificationResponse(...)");

	if (ptrData != NULL && dataSize > 0)
	{
		bool tagFound = false;
		BeerTLV* berTLVAttributeTag = NULL;
		BeerTLV* berTLVRemoveNotificationTag = NULL;

		berTLVRemoveNotificationTag = berTLV_extractTagUInt16(REMOVE_NOTIFICATION_FROM_LIST_TAG, ptrData, dataSize, &tagFound);
		if (berTLVRemoveNotificationTag != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "BerTLV tag '%04lx' found", REMOVE_NOTIFICATION_FROM_LIST_TAG);
			berTLVAttributeTag = berTLV_extractTagUInt16(DER_ATTRIBUTE_TAG, berTLVRemoveNotificationTag->value, berTLVRemoveNotificationTag->length, &tagFound);
		}

		if (berTLVAttributeTag != NULL && berTLVAttributeTag->length == 1)
		{
			switch (berTLVAttributeTag->value[0])
			{
			case 0:
				deleteNotificationStatus = LPA_DELETE_NOTIFICATION_OK;
				break;

			case 1:
				deleteNotificationStatus = LPA_DELETE_NOTIFICATION_NOTHING_TO_DELETE;
				break;

			case 127:
				deleteNotificationStatus = LPA_DELETE_NOTIFICATION_UNDEFINED_ERROR;
				break;

			default:
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " -> Incorrect deleteNotificationStatus returned by the card : 0x%02x", berTLVAttributeTag->value[0]);
				break;
			}
		}

		// Do cleanup
		ERASE_BERTLV(berTLVRemoveNotificationTag);
		ERASE_BERTLV(berTLVAttributeTag);

	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _doExtractClearProfileNotificationResponse(...) : return %d", deleteNotificationStatus);

	return deleteNotificationStatus;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

void _sendEventCallbackProgressText(const LPA_EventCallback* ptrEventCallback, size_t eventType, const char* ptrText)
{
    if ((ptrEventCallback != NULL) && (ptrText != NULL))
    {
        if (ptrEventCallback->_lpaEventProgressText != NULL)
            ptrEventCallback->_lpaEventProgressText(ptrEventCallback->_appParameter, eventType, ptrText);
    }
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

void _sendEventCallbackProgressValue(LPA_EventCallback* ptrEventCallback, size_t eventType, size_t valueMin, size_t currentValue, size_t valueMax)
{
	if (ptrEventCallback != NULL)
	{
		if (ptrEventCallback->_lpaEventProgressValue != NULL)
			ptrEventCallback->_lpaEventProgressValue(ptrEventCallback->_appParameter, eventType, valueMin, currentValue, valueMax);
	}
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _lpaManagerGetEuiccChallenge(LPA_GET_EUICC* ptrGetEUICC)
{
	return lpaManagerES10b_GetEuiccChallenge(ptrGetEUICC);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _lpaManagerPrepareCtxParam(const char* ptrMatchingId, const unsigned char* ptrDeviceInfoTlv, size_t deviceInfoTlvSize, RawDataObject ** ptrCtxParam)
{
	bool res = false;
	bool isError = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerPrepareCtxParam() ...");
        
        // Note : Given parameters pointers are checked not NULL below in the code, not at the beginning

	RawDataObject* ptrRawDataObjectMatchingId = NULL;

	if ((ptrDeviceInfoTlv != NULL) && (deviceInfoTlvSize > 0) && (ptrCtxParam != NULL))
	{
		// Matching Id is optional
		if (ptrMatchingId != NULL)
		{
			if (strlen(ptrMatchingId) > 0)
				ptrRawDataObjectMatchingId = berTLV_createAndBuildRawDataObject(0x80, strlen(ptrMatchingId), (const unsigned char*)ptrMatchingId);
			else
			{
				ptrRawDataObjectMatchingId = berTLV_createAndBuildRawDataObject(0x80, 0, NULL);
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Matching Id is empty");
			}

			if (ptrRawDataObjectMatchingId == NULL)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to create RawDataObjectMatchingId !");
				isError = true;
			}
		}
		else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Matching Id is missing (NULL pointer");

		// Create now CtxParams
		if (!isError)
		{
                        ERASE_RAWDATAOBJECT(*ptrCtxParam);

			if (ptrRawDataObjectMatchingId != NULL)
			{
				RawDataObject* rawDataObjectDeviceInfoTlv = rawDataObject_allocate();

				// Update reference without new allocation => do not remove it
				rawDataObjectDeviceInfoTlv->rawData = (unsigned char*)ptrDeviceInfoTlv;
				rawDataObjectDeviceInfoTlv->rawDataSize = deviceInfoTlvSize;

				RawDataObject* rawDataObjectPart1And2 = rawDataObject_concat(ptrRawDataObjectMatchingId, rawDataObjectDeviceInfoTlv);

				rawDataObjectDeviceInfoTlv->rawData = NULL;	// Reset memory pointer

				// Allocate new buffer that contains new BERT TLV created
				if (rawDataObjectPart1And2 != NULL)
				{
                                    if(rawDataObjectPart1And2->rawDataSize < LPA_GET_EUICC_BUFFER_MAX_SIZE)
                                        *ptrCtxParam = berTLV_createAndBuildRawDataObject(0xA0, rawDataObjectPart1And2->rawDataSize, rawDataObjectPart1And2->rawData);
                                    else
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Not enough memory allowed for ptrCtxParam: % bytes needed, %d max allowed!", rawDataObjectPart1And2->rawDataSize, LPA_GET_EUICC_BUFFER_MAX_SIZE - 1);
                                        lpaSetErrorCode(LPA_ERROR_INVALID_CTX_PARAM);
                                    }
                                    rawDataObject_free(rawDataObjectPart1And2);
                                    rawDataObjectPart1And2 = NULL;
				}
				rawDataObject_free(rawDataObjectDeviceInfoTlv);
			}
			else
			{
                            if(deviceInfoTlvSize < LPA_GET_EUICC_BUFFER_MAX_SIZE)
                                *ptrCtxParam = berTLV_createAndBuildRawDataObject(0xA0, deviceInfoTlvSize, ptrDeviceInfoTlv);
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Not enough memory allowed for ptrCtxParam: % bytes needed, %d max allowed!", deviceInfoTlvSize, LPA_GET_EUICC_BUFFER_MAX_SIZE - 1);
                                lpaSetErrorCode(LPA_ERROR_INVALID_CTX_PARAM);
                            }
			}

			if (*ptrCtxParam != NULL)
			{
                            if (formatBytesToHexaString((const unsigned char *)(*ptrCtxParam)->rawData, (*ptrCtxParam)->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "CtxParam (%d bytes) :<%s>", (*ptrCtxParam)->rawDataSize, _bufferFormatLogMessage);
                            else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "CtxParam (%d bytes) : ...", (*ptrCtxParam)->rawDataSize);

                            res = true;
			}
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to allocate ptrCtxParam.");
                            lpaSetErrorCode(LPA_ERROR_INVALID_CTX_PARAM);
                        }
		}

                ERASE_RAWDATAOBJECT(ptrRawDataObjectMatchingId);
	}
	else
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter !");
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
	}

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _lpaManagerGetEuiccInfo(LPA_GET_EUICC* ptrGetEUICC)
{
	return lpaManagerES10b_GetEuiccInfo(ptrGetEUICC);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerInitiateAuthentication(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char * ptrEuiccChallenge, const char * ptrEuiccInfo1, const char* ptrSmdpAddress)
{
	return lpaManagerES9Plus_InitiateAuthentication(p_serverData, ptrLpaEventCallback, ptrEuiccChallenge, ptrEuiccInfo1, ptrSmdpAddress);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _lpaManagerAuthenticateServer(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, RawDataObject * ptrCtxParam, AUTHENTICATE_SERVER_RESPONSE* ptrAuthServerResp)
{
	return lpaManagerES10b_AuthenticateServer(p_serverData, ptrLpaEventCallback, ptrCtxParam, ptrAuthServerResp);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerAuthenticateClient(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrTransactionId, const char* ptrSmdpAddress, const unsigned char* ptrAuthenticateServerResponse)
{
	return lpaManagerES9Plus_AuthenticateClient(p_serverData, ptrLpaEventCallback, ptrTransactionId, ptrSmdpAddress, ptrAuthenticateServerResponse);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerGetBoundProfilePackage(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrTransactionId, const char* ptrSmdpAddress, const unsigned char* ptrPrepareDownloadResponse)
{
	return lpaManagerES9Plus_GetBoundProfilePackage(p_serverData, ptrLpaEventCallback, ptrTransactionId, ptrSmdpAddress, ptrPrepareDownloadResponse);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _lpaManagerPrepareDownload(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char * ptrStringHashCC, PREPARE_DOWNLOAD_RESPONSE* ptrPrepareDownloadResp)
{
	return lpaManagerES10b_PrepareDownload(p_serverData, ptrLpaEventCallback, ptrStringHashCC, ptrPrepareDownloadResp);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
* Cancel download session on card and server side
* @param p_serverData Server data containing transactionID (structure pointer, ptr_serverData type)
* @param p_reasonCode Cancel reason code as defined in SGP.22
* @param ptrSmdpAddress SM-DP address, string format
* @return True if cancel is successful
*/
bool _lpaManagerCancelSession(const char * transactionID, const unsigned int p_reasonCode, const char* ptrSmdpAddress, const LPA_EventCallback* ptrLpaEventCallback)
{
    bool res = false;
    CANCEL_SESSION_RESPONSE *ptrCancelSessionResp = NULL;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerCancelSession...");

    if ((ptrSmdpAddress != NULL) && (transactionID != NULL) && (strlen(transactionID) > 0) && (strlen(transactionID) < LPA_TRANSACTION_ID_MAX_SIZE) &&
                                                        isElementPresentInArrayUInt(LPA_ALLOWED_CANCEL_SESSION_CODE_LIST, LPA_ALLOWED_CANCEL_SESSION_CODE_LIST_SIZE, p_reasonCode))
    {
        ptrCancelSessionResp = lpaCoreMemoryAlloc(sizeof(CANCEL_SESSION_RESPONSE));
        if (ptrCancelSessionResp != NULL)
        {
            memset(ptrCancelSessionResp, 0x00, sizeof(CANCEL_SESSION_RESPONSE));
            res = lpaManagerES10b_CancelSession(transactionID, p_reasonCode, ptrCancelSessionResp);
            if(res) res = lpaManagerES9plus_CancelSession(transactionID, ptrCancelSessionResp, ptrSmdpAddress, ptrLpaEventCallback);
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerCancelSession: Cannot allocate memory for response buffer.");
    }
    else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerCancelSession: Parameters error.");
    
    if(res)
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_lpaManagerCancelSession: Operation is successful.");
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_lpaManagerCancelSession: Operation failed.");
    
    if (ptrCancelSessionResp != NULL)
    {
        lpaCoreMemoryFree(ptrCancelSessionResp);
	ptrCancelSessionResp = NULL;
    }

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
* Extract element at position x (After '$' delimiter number x) in Activation Code. So SM-DP adress is at postion 1
* Note: If size of element to extract exceeds maximum output buffer size, function result will be false but output buffer will contain truncated value of found element
* @param ptrActivationCodeStr Activation code string
* @param elementNumber Position of element to found. Start at 1
* @param ptrElement Buffer that will receive element data, string format
* @param maxElementSize  Maximum size of buffer data, including '0' at the end of string. So shall be at least 2
* @return True if element x successfully extracted
*/
bool _extractElementFromActivationCode(const char * ptrActivationCodeStr, const int elementNumber, char * ptrElement, const int maxElementSize)
{
    bool res = false;
    int i;
    int j;
    int len;
    int countDollar = 0;

    if((ptrActivationCodeStr != NULL) && (elementNumber > 0) && (ptrElement != NULL) && (maxElementSize > 1))
    {
        // Security in case of asking an element above what is available in activation code   
        if (countCharOccurencesInString(ptrActivationCodeStr, '$') >= elementNumber)
        {
            len = strlen(ptrActivationCodeStr);
            i = 0;
            // Stop if end of activation string reached or element found or found but overflow detected
            while ((i < len) && (!res) && (countDollar < elementNumber))
            {
                if (ptrActivationCodeStr[i] == '$') countDollar++;

                // Increment "i" here allow to go directly to next element after '$' separator
                i++;

                // Element # found
                if (countDollar == elementNumber)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractElementFromActivationCode: Target element found");
                    
                    j = 0;
                    // Stop to copy in output buffer if end of string reached, max size of output buffer reached or another delimiter reached
                    while (((i + j) < len) && (j < maxElementSize) && (ptrActivationCodeStr[i + j] != '$'))
                    {
                        *(ptrElement + j) = ptrActivationCodeStr[i + j];
                        j++;
                    }
                    // Check if no overflow of reception buffer. If overflow, extraction will be considered as failed.
                    if (j < maxElementSize)
                    {
                        // Terminate output buffer to have a string
                        *(ptrElement + j) = 0;
                        res = true;
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractElementFromActivationCode: Element extracted successfully");
                    }
                    else
                    {
                        // In case of overflow terminate output buffer to avoid undefined length string. This truncated value will allow to know if overflow occured.
                        *(ptrElement + maxElementSize - 1) = 0;
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractElementFromActivationCode: Element exceeds output buffer size. Return it truncated.");
                    }
                }
            }
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_extractElementFromActivationCode: Not enough '$' separators in Activation Code to find target element");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractElementFromActivationCode: Invalid Parameter(s)!");

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
* Check if Activation Code is correct (String valid / size, syntax...)
* @param ptrActivationCodeStr Activation Code string to analyze
* @return true if Activation Code is correct, else false
*/
bool _checkActivationCodeValid(const char* ptrActivationCodeStr)
{
	bool res = false;
	int nbDollars;

	// At least 5 characters shall be present
	if (ptrActivationCodeStr != NULL && (strlen(ptrActivationCodeStr) > 4) && (strlen(ptrActivationCodeStr) < LPA_ACTIVATION_CODE_MAX_STRING_BUFFER_SIZE))
	{
		// Activation code contains 2 to 4 '$' signs
		nbDollars = countCharOccurencesInString(ptrActivationCodeStr, '$');
		if ((nbDollars > 1) && (nbDollars < 5))
		{
			// Check Activation Code begin with "1$"
			if ((ptrActivationCodeStr[0] == '1') && (ptrActivationCodeStr[1] == '$'))
				res = true;
		}
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_checkActivationCodeValid: Invalid Parameter!");

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
* Extract SM-DP Address and MatchingID from Activation Code
*
* @param ptrActivationCodeStr - Activation Code to check, String format
* @param ptrActivationCode - Data structure that will store SM-DP Address and MatchingID, ACTIVATION_CODE type
* @return True if extraction is successful
*/
bool _decodeActivationCodeStr(const char* ptrActivationCodeStr, ACTIVATION_CODE* ptrActivationCode)
{
    bool res = false;
    bool checkOID_OK = true;    // Checking job can be done fully on "res", but allow to isolate OID check result from main result for future needs
    size_t lastOIDcharacter = 0;
    size_t i = 0;
    

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Decode and parse Activation Code ...");

    if ((ptrActivationCodeStr != NULL) && (_checkActivationCodeValid(ptrActivationCodeStr)) && (ptrActivationCode != NULL))
    {
        // Try to extract server address, 1st position
        res = _extractElementFromActivationCode(ptrActivationCodeStr, 1, ptrActivationCode->smdxAddr, LPA_SMDP_ADDRESS_SIZE);
        if (res)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Activation Code parsing : SM-DP server address found: \"%s\".", ptrActivationCode->smdxAddr);
            // Check if not empty
            if (strlen(ptrActivationCode->smdxAddr) == 0)
            {
                res = false;
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Activation Code parsing : SM-DP server address empty!");
                lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Activation Code parsing : SM-DP server address cannot be found or size exceeds limits.");
            lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
        }

        // If OK for address, try with Matching ID, at 2nd position
        if (res)
        {
            res = _extractElementFromActivationCode(ptrActivationCodeStr, 2, ptrActivationCode->matchingId, LPA_MATCHING_ID_SIZE);
            if (res)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Activation Code parsing : MatchingID found: \"%s\".", ptrActivationCode->matchingId);
                // Note: Empty Matching ID shall be supported
            }
            else
            {
                // Missing delimiter for Matching ID is considered as an error
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Activation Code parsing : MatchingID cannot be found or size exceeds limits.");
                lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
            }
        }

        // If OK for address & Matching ID, try with OID, at 3rd position
        if (res)
        {
            // If not found, will be not a blocking point and will set empty string on it    
            if (_extractElementFromActivationCode(ptrActivationCodeStr, 3, ptrActivationCode->oid, LPA_OID_SIZE))
            {
                if(strlen(ptrActivationCode->oid) > 0)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Activation Code parsing : OID found: \"%s\".", ptrActivationCode->oid);
                    // Perform basic checking on OID given in Activation Code. If invalid, exit on error.
                    lastOIDcharacter = strlen(ptrActivationCode->oid) - 1;
                    // Must have at least 3 characters (Minimum format is x.y)
                    if(strlen(ptrActivationCode->oid) < 3)
                        checkOID_OK = false;
                    // Must start with digit 0, 1 or 2
                    if(ptrActivationCode->oid[0] < '0' || ptrActivationCode->oid[0] > '2')
                        checkOID_OK = false;
                    // Second character must be a dot '.'
                    if(ptrActivationCode->oid[1] != '.')
                        checkOID_OK = false;
                    // Third and last character must be a numeric digit (0..9)$
                    if(ptrActivationCode->oid[2] < '0' || ptrActivationCode->oid[2] > '9' || ptrActivationCode->oid[lastOIDcharacter] < '0' || ptrActivationCode->oid[lastOIDcharacter] > '9')
                        checkOID_OK = false;
                    // Nothing else than (0..9) and '.' must be present inside
                    // 3 first characters and last one already check
                    for(i = 3; i < lastOIDcharacter; i++)
                    {
                        if(! ((ptrActivationCode->oid[i] >= '0' && ptrActivationCode->oid[i] <= '9') || ptrActivationCode->oid[i] == '.'))
                            checkOID_OK = false;
                    }

                    if(checkOID_OK)        
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Activation Code parsing : OID format check OK");
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Activation Code parsing : OID format check failed.");
                        lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
                        res = false;
                    }
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Activation Code parsing : OID is empty.");
            }
            else
            {
                // If OID returned by _extractElementFromActivationCode() has a length, it means that OID was too big, so exit on error
                if(strlen(ptrActivationCode->oid) > 0)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Activation Code parsing : OID size exceeds limits.");
                    lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
                    res = false;
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Activation Code parsing : OID cannot be found.");
                
                ptrActivationCode->oid[0] = 0;
            }
        }

        // If OK for Address, Matching ID & OID, try to extract Confirmation Code required flag at 4th position.
        if (res)
        {
            // If not found, will be not a blocking point and will set empty string on it
            if (_extractElementFromActivationCode(ptrActivationCodeStr, 4, ptrActivationCode->ccRequiredFlag, LPA_CC_REQUIRED_FLAG_SIZE))
            {
                // If the Confirmation Code required flag is not set, it SHALL not be defined, so the delimiter shall not exist (Condition SHALL in SGP.22 chapter 4.1 Table 8)
                if(strlen(ptrActivationCode->ccRequiredFlag) > 0)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Activation Code parsing : Confirmation Code Required flag found: \"%s\".", ptrActivationCode->ccRequiredFlag);
                    
                    // Confirmation Code required Flag Format check:
                    // Lengh must be = 1, value must be '1' (Condition SHALL in SGP.22 chapter 4.1 Table 8)
                    if((strlen(ptrActivationCode->ccRequiredFlag) != 1) || (ptrActivationCode->ccRequiredFlag[0] != '1'))
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Activation Code parsing : Confirmation Code Required flag is invalid.");
                        lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
                        res = false;
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Activation Code parsing : Confirmation Code Required flag check OK.");
                }
                else
                {
                    // Empty value is an error considering that delimiter SHALL not be defined if flag is not set
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Activation Code parsing : Confirmation Code Required flag is empty.");
                    lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
                    res = false;
                }
            }
            else
            {
                // If flag returned by _extractElementFromActivationCode() has a length, it means that OID was too big, so exit on error
                if(strlen(ptrActivationCode->ccRequiredFlag) > 0)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Activation Code parsing : Confirmation Code Required flag size exceeds limits.");
                    lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
                    res = false;
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Activation Code parsing : Confirmation Code Required flag cannot be found.");
                
                ptrActivationCode->ccRequiredFlag[0] = 0;
            }
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid Activation Code!");
        lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
    }

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
* Check if Activation Code contains Confirmation Code request
*
* @param ptrActivationCodeStr - Activation Code to check, String format
* @return True if contains Confirmation Code request
*/
bool _checkConfirmationCodeRequestInActivationCodeStr(const char* ptrActivationCodeStr)
{
	bool res = false;
	char readFlag[LPA_CC_REQUIRED_FLAG_SIZE];

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Check if Confirmation Code required flag present in Activation Code and check value...");

	if ((ptrActivationCodeStr != NULL) && _checkActivationCodeValid(ptrActivationCodeStr))
	{
		// Confirmation Code required flag is at 4th position in Activation Code
		if (_extractElementFromActivationCode(ptrActivationCodeStr, 4, readFlag, LPA_CC_REQUIRED_FLAG_SIZE))
		{
			res = (strcmp(readFlag, "1") == 0) ? true : false;
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code required flag found, status is %s", (res) ? "\"Required.\"" : "\"Not required.\"");
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No Confirmation Code required flag found in Activation Code");
		}
	}
	else
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid activation code!");
		lpaSetErrorCode(LPA_ERROR_INVALID_ACTIVATION_CODE);
	}

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerloadBoundProfilePackage(ptr_serverData p_serverData, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors)
{
	return lpaManagerES10b_LoadBoundProfilePackage(p_serverData, pir, cancelForBPPerrors);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool extractSeqNumbFromPIR(PROFILE_INSTALLATION_RESULT* ptrPir, uint16_t * ptrSeqNumb)
{
	bool res = false;
	bool isTagFound_BF37 = false, isTagFound_BF27 = false, isTagFound_BF2F = false, isTagFound_80 = false;
	BeerTLV *ptrBerTLV_BF37 = NULL, *ptrBerTLV_BF27 = NULL, *ptrBerTLV_BF2F = NULL, *ptrBerTLV_80 = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "extractSeqNumbFromPIR() ...");

	if ((ptrPir != NULL) && ptrPir->hasResult && (ptrPir->ptrProfileInstallationResultTlv != NULL) && (ptrPir->ptrProfileInstallationResultTlv->rawDataSize > 0) && (ptrSeqNumb != NULL))
	{
		ptrBerTLV_BF37 = berTLV_extractTagUInt16(0xBF37, ptrPir->ptrProfileInstallationResultTlv->rawData, ptrPir->ptrProfileInstallationResultTlv->rawDataSize, &isTagFound_BF37);

		if (ptrBerTLV_BF37 != NULL && ptrBerTLV_BF37->length > 0)
		{
			ptrBerTLV_BF27 = berTLV_extractTagUInt16(0xBF27, ptrBerTLV_BF37->value, ptrBerTLV_BF37->length, &isTagFound_BF27);
			if (ptrBerTLV_BF27 != NULL && ptrBerTLV_BF27->length > 0)
			{
				ptrBerTLV_BF2F = berTLV_extractTagUInt16(0xBF2F, ptrBerTLV_BF27->value, ptrBerTLV_BF27->length, &isTagFound_BF2F);
				if (ptrBerTLV_BF2F != NULL && ptrBerTLV_BF2F->length > 0)
				{
					ptrBerTLV_80 = berTLV_extractTagUInt8(0x80, ptrBerTLV_BF2F->value, ptrBerTLV_BF2F->length, &isTagFound_80);
					if (ptrBerTLV_80 != NULL && ptrBerTLV_80->length > 0)
					{
						//seqNumber
						if (extractIntegerFromByteArray(ptrBerTLV_80->value, ptrBerTLV_80->length, ptrSeqNumb))
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "The sequence number is : %02x", *ptrSeqNumb);
							res = true;
						}
						else
							lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get sequence number from installation result! ");
					}
					else
						lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " Not Found Tag <80> ! ");
				}
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " Not Found Tag <BF2F> ! ");
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " Not Found Tag <BF27> ! ");
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " Not Found Tag <BF37> ! ");
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " Invalid installation result! ");

	// Cleanup memory
	ERASE_BERTLV(ptrBerTLV_80);
	ERASE_BERTLV(ptrBerTLV_BF2F);
	ERASE_BERTLV(ptrBerTLV_BF27);
	ERASE_BERTLV(ptrBerTLV_BF37);

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _handleNotification(const char* ptrSmdpAddr, size_t smdpAddrSize, const unsigned char* ptrPendingNotification, const LPA_EventCallback* ptrLpaEventCallback)
{
	return lpaManagerES9Plus_HandleNotification(ptrSmdpAddr, smdpAddrSize, ptrPendingNotification, ptrLpaEventCallback);
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
* Calculate hash of Confirmation Code with Transaction ID as defined in SGP.22 chapter 3.1.3
*
* @param confirmationCode - Confirmation Code data, String
* @param confirmationCodeLength - Confirmation Code length, integer, must be > 0 else error
* @param transactionID Transaction ID, - String Format, 32 CHARACTERS (String conversion of TransactionID)
* @param hashConfirmationCode - Returned calculated value of Confirmation Code hash, String format, MUST BE AT LEAST 68 CHARACTERS
* @return True if processing OK
*/
bool _calculateConfirmationCodeHash(const char * confirmationCode, const int confirmationCodeLength, const char * transactionID, char * hashConfirmationCode)
{
	bool res = false;

	uint8_t bufferCalcHash[32] = {0};
	unsigned char tabTransactionID[16] = { 0 };
	char concatHashCCTransationID[48] = { 0 };
	int convLen = 16;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code Hash calculation...");

	if ((confirmationCode != NULL) && (confirmationCodeLength > 0) && (transactionID != NULL) && (strlen(transactionID) == 32) &&
		(hashConfirmationCode != NULL))
	{
		// Convert Transaction ID string to values tab
		if (hexStr2ByteArray((unsigned char *)transactionID, 32, tabTransactionID, &convLen) && (convLen == 16))
		{
			calc_sha_256(bufferCalcHash, confirmationCode, confirmationCodeLength);

			// Concatenate Hash Confirmation Code + Transaction ID
			memcpy(concatHashCCTransationID, bufferCalcHash, 32);
			memcpy(concatHashCCTransationID + 32, tabTransactionID, 16);

			calc_sha_256(bufferCalcHash, &concatHashCCTransationID, 48);

			// Format response in String + check final length
			if (formatBytesToHexaString(bufferCalcHash, 32, hashConfirmationCode, 68) == 64)
			{
				res = true;
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code Hash calculation done.");
			}
		}
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Confirmation Code Hash calculation: Cannot convert TransactionID in hex.");
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Confirmation Code Hash calculation : Parameters problem.");

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Return if PPR1 is defined in PPR flag
 * @param pprFlag - PPR flag to evaluate
 * @return true if PPR1 is defined in PPR flag
 */
bool _isPPR1definedInPPRflag(const unsigned int pprFlag)
{
    bool pprStatus = false;
    
    switch(pprFlag)
    {
        case LPA_PPRDEF_PPR1_PPR2:
        case LPA_PPRDEF_PPRUC_PPR1_PPR2:
        case LPA_PPRDEF_PPR1:
        case LPA_PPRDEF_PPRUC_PPR1:
            pprStatus = true;
            break;
        default:
            break;
    }
    
    return pprStatus;
}

/**
 * Return if PPR2 is defined in PPR flag
 * @param pprFlag - PPR flag to evaluate
 * @return true if PPR2 is defined in PPR flag
 */
bool _isPPR2definedInPPRflag(const unsigned int pprFlag)
{
    bool pprStatus = false;
    
    switch(pprFlag)
    {
        case LPA_PPRDEF_PPR2:
        case LPA_PPRDEF_PPR1_PPR2:
        case LPA_PPRDEF_PPRUC_PPR2:
        case LPA_PPRDEF_PPRUC_PPR1_PPR2:
            pprStatus = true;
            break;
        default:
            break;
    }
    
    return pprStatus;
}


/**
 * Get RAT (Rules Access Table) from eUICC
 * @param ptrGetRATA - Address of pointer that will receive the RawdataObject who will store RAT value
 * @return true if operation is correct
 */
bool _lpaManagerGetRAT(RawDataObject ** ptrGetRAT)
{
    return lpaManagerES10b_GetRAT(&*ptrGetRAT);
}


/**
 * Check if profile can be installed or user must be warned considering its PPR flags and profiles already installed in card
 * @param incomingProfileFields - Pointer on incoming profile data's
 * @param currenltyInstalledProfiles - Pointer on simplified (PPR check) profiles list installed on eUICC
 * @return true if operation was successful
 */
bool _checkIncomingProfilePPRconditionVersusInstalledProfiles(LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR* ptrIncomingProfileData, 
                                                                                                                const LPA_GET_PROFILES_INFO * ptrCurrentlyInstalledProfiles)
{
    bool res = false;
    size_t indexProfile = 0;
    bool checkForEnabledPPR1 = true;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_checkIncomingProfilePPRconditionVersusInstalledProfiles()...");
        
    if(ptrIncomingProfileData != NULL && ptrCurrentlyInstalledProfiles != NULL)
    {
        // Profiles retrieval check for warning
        if(ptrCurrentlyInstalledProfiles->countProfileInfo < ptrCurrentlyInstalledProfiles->numberProfileInfoFound)
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Found more profiles available on eUICC than retrieved in list. SHALL not happen! eUICC profile parsing may be incomplete.");

        // Init main values by security
        ptrIncomingProfileData->userCallBackType = LPA_USR_CONSENT_DISABLED;
        ptrIncomingProfileData->performCancelSession = false;
        
        // No need to parse if no profile installed in eUICC
        if(ptrCurrentlyInstalledProfiles->countProfileInfo > 0)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Found %d profiles on eUICC", ptrCurrentlyInstalledProfiles->countProfileInfo);

            // CASE 1: Incoming profile has PPR1
            if(ptrIncomingProfileData->hasPPR1)
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Incoming profile has PPR1 defined, will check if an Operational profile is already loaded.");
            
            for (indexProfile = 0; indexProfile < ptrCurrentlyInstalledProfiles->countProfileInfo; indexProfile++)
            {
                LPA_PROFILE_INFO_FOR_PPR* ptrProfileInfo = (LPA_PROFILE_INFO_FOR_PPR*)(ptrCurrentlyInstalledProfiles->profileInfoList + (indexProfile * sizeof(LPA_PROFILE_INFO_FOR_PPR)));

                // CASE 1: Incoming profile has PPR1 defined, check if an Operational profile is already loaded (Abort download)
                if(ptrIncomingProfileData->hasPPR1)
                {
                    // Check profile Class
                    if (ptrProfileInfo->profileClassSize > 0)
                    {
                        // If Operational profile, download if not granted
                        if (ptrProfileInfo->profileClass[0] == 0x02)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile #%d is an Operational profile, download is not allowed due to PPR1 set on incoming profile.", indexProfile);
                            ptrIncomingProfileData->performCancelSession = true;
                            ptrIncomingProfileData->cancelSessionReason = LPA_CANCEL_SESSION_PPR_NOT_ALLOWED;
                            ptrIncomingProfileData->userCallBackType = LPA_USR_CONSENT_DISABLED;    // No more need to query User Consent if already set
                            
                            break;  // Here no need to parse another profile or check something else
                        }
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile #%d: Is not an Operational profile.", indexProfile);
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Profile #%d: No profile Class found.", indexProfile);
                }

                // CASE 2: All profiles class, check if installed profile has PPR1 set and is Enabled. If yes User Consent shall be required
                // Note: This User Consent request is defined in SGP.21, requirement LPA46. Used in some SGP.23 testing.
                if(checkForEnabledPPR1)
                {
                    // Check profile PPR status
                    if (ptrProfileInfo->profilePolicyRulesSize > 0)
                    {
                        // Check if profile has PPR1
                        if (_isPPR1definedInPPRflag((ptrProfileInfo->profilePolicyRules[0] << 8) + (ptrProfileInfo->profilePolicyRules[1])))
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile #%d: Has PPR1 defined.", indexProfile);
                            // Check profile State
                            if (ptrProfileInfo->profileStateSize > 0)
                            {
                                // Check if profile is Enabled
                                if (ptrProfileInfo->profileState[0] == 0x01)
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile #%d: Is in Enabled state, user consent is required.", indexProfile);
                                    // Note: This condition may be mixed with warnings about PPRs during RAT rules parsing
                                    ptrIncomingProfileData->userCallBackType = LPA_USR_CONSENT_PROFILE_WITH_PPR1_ENABLED_PRESENT;
                                    
                                    checkForEnabledPPR1 = false;  // Here no need to parse another profile for that condition
                                }
                                else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile #%d: Is in Disabled state.", indexProfile);
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Profile #%d: No profile State found.", indexProfile);
                        }
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile #%d: Has not PPR1 defined.", indexProfile);
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile #%d: No Profile Policy Rules (PPR) found.", indexProfile);
                }
            }
            
            res = true;
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Did not find profiles on eUICC, no limitation due to installed profile.");
            res = true;
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_checkIncomingProfilePPRconditionVersusInstalledProfiles(): return %s", (res ? "true" : "false"));
    
    return res;
}

/**
 * Check profile PPR elements wersus RAT to determine if user download is authorized, blocked or used must be warned
 * @param ptrIncomingProfileData - Pointer on incoming profile data's - Will also contain action to be performed
 * @param ptrRATrules - Pointer on Rawdata object containing RAT retrieved from eUICC
 * @return true if RAT analysis is OK, else false if RAT data error detected or other issue encountered
 */
bool _checkIncomingProfilePPRattributesVersusRAT(LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR* ptrIncomingProfileData, const RawDataObject* ptrRATrules)
{
    bool res = false;
    LPA_PPR_RAT_ANALYSIS_FLAGS checkPPR1;   // Analysis flags for PPR1
    LPA_PPR_RAT_ANALYSIS_FLAGS checkPPR2;   // Analysis flags for PPR2
    
    BeerTLV * berEUICC_RAT = NULL;
    BeerTLV * berRATmainSequenceA0 = NULL;
    BerTLVList * berTLVlistA0base = NULL;
    uint8_t tlvListA0BaseCount = 0;
    BerTLVList * berTLVlistA0parser = NULL;
    BeerTLV * berTLVlistA0current = NULL;
        
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_checkIncomingProfilePPRattributesVersurRAT()...");
    
    // Valid RAT rules object must be at least 5 bytes (Tag BF43 + Length + Tag A0 + Length)
    if(ptrIncomingProfileData != NULL && ptrRATrules != NULL && ptrRATrules->rawDataSize > 4)
    {
        // Check first if there is something to check in incoming profile
        if(ptrIncomingProfileData->hasPPR1 || ptrIncomingProfileData->hasPPR2)
        {
            if (formatBytesToHexaString(ptrRATrules->rawData, ptrRATrules->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "RAT retrieved from eUICC: %d bytes: %s", ptrRATrules->rawDataSize, _bufferFormatLogMessage);
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "RAT retrieved from eUICC: %d bytes", ptrRATrules->rawDataSize);
            
            // Analysis flags initialization
            checkPPR1.pprValidated = false;
            checkPPR1.matchLevelMCC_MNC = 0;
            checkPPR1.matchedGID1 = false;
            checkPPR1.matchedGID2 = false;
            checkPPR1.matchLevelGID = 0;
            checkPPR1.userConsentRequired = false;
            
            checkPPR2.pprValidated = false;
            checkPPR2.matchLevelMCC_MNC = 0;
            checkPPR2.matchedGID1 = false;
            checkPPR2.matchedGID2 = false;
            checkPPR2.matchLevelGID = 0;
            checkPPR2.userConsentRequired = false;
            
            // Check main tag BF43
            berEUICC_RAT = berTLV_extractTagUInt16(EUICC_RAT_TAG, ptrRATrules->rawData, ptrRATrules->rawDataSize, NULL);
            if(berEUICC_RAT != NULL)
            {
                // Check main sequence tag A0
                berRATmainSequenceA0 = berTLV_extractTagUInt8(0xA0, berEUICC_RAT->value, berEUICC_RAT->length, NULL);
                if(berRATmainSequenceA0 != NULL)
                {
                    // If rules are not empty continue, else cancel download
                    if(berRATmainSequenceA0->length > 0)
                    {
                        berTLVlistA0base = berTLV_extractList(berRATmainSequenceA0->value, berRATmainSequenceA0->length, &tlvListA0BaseCount);
                        
                        // List shall contain at least one element
                        if(berTLVlistA0base != NULL && tlvListA0BaseCount > 0)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Found %d PPAR (rule) in RAT", tlvListA0BaseCount);
                            // At this step we consider parsing OK, except if something invalidate this status
                            res = true;
                            
                            berTLVlistA0parser = berTLVlistA0base;
                            // Parse list of PPR rules
                            while(berTLVlistA0parser != NULL)
                            {
                                berTLVlistA0current = berTLVlistA0parser->berTLV;
                                if(berTLVlistA0current != NULL)
                                {
                                    // Check PPAR (RAT list element) tag before going next level of analysis
                                    if(berTLVlistA0current->tag == 0x30)
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Launch analysis of PPAR...");
                                        res = _analysePPARversusProfileAttribute(berTLVlistA0current, ptrIncomingProfileData, &checkPPR1, &checkPPR2);
                                        
                                        // If error detected while analysing PPAR, stop parsing and cancel profile download
                                        if(! res)
                                            break;
                                    }
                                    else
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid tag found for PPAR entry (Not 0x30), profile download is not allowed");
                                        ptrIncomingProfileData->performCancelSession = true;
                                        lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
                                        
                                        break; // No need to continue parsing
                                    }
                                }
                                else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "RAT rules parser: Current BerTLV list element appear to be NULL ? Go to next one...");
                                
                                berTLVlistA0parser = berTLVlistA0parser->ptrNext;
                            }
                            
                            // Final validation of loading authorization
                            // PPR1
                            if(ptrIncomingProfileData->hasPPR1)
                            {
                                if(checkPPR1.pprValidated)
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile PPR1 validated by RAT");
                                    // Check if user consent required for this PPR
                                    if(checkPPR1.userConsentRequired)
                                    {
                                        ptrIncomingProfileData->userCallBackType = ptrIncomingProfileData->userCallBackType | LPA_USR_CONSENT_PPR1;
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "User Consent required");
                                    }
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "User Consent not required");
                                }
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile PPR1 not validated by RAT, cancel download");
                                    ptrIncomingProfileData->performCancelSession = true;
                                }
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile has not having PPR1 defined");
                            
                            // PPR2
                            if(ptrIncomingProfileData->hasPPR2)
                            {
                                if(checkPPR2.pprValidated)
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile PPR2 validated by RAT");
                                    // Check if user consent required for this PPR
                                    if(checkPPR2.userConsentRequired)
                                    {
                                        ptrIncomingProfileData->userCallBackType = ptrIncomingProfileData->userCallBackType | LPA_USR_CONSENT_PPR2;
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "User Consent required");
                                    }
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "User Consent not required");
                                }
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile PPR2 not validated by RAT, cancel download");
                                    ptrIncomingProfileData->performCancelSession = true;
                                }
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile has not having PPR2 defined");
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Problem with RAT rules list, profile download is not allowed");
                            lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
                        }

                        // Memory cleanup
                        ERASE_BERTLV_LIST(berTLVlistA0base);
                        berTLVlistA0parser = NULL;
                        berTLVlistA0current = NULL;
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Empty RAT rules list found, profile download is not allowed");
                        ptrIncomingProfileData->performCancelSession = true;

                        res = true; // Empty rules but parsing OK
                    }

                    // Memory cleanup
                    ERASE_BERTLV(berRATmainSequenceA0);
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid RAT, main sequence tag A0 not found!");
                    lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
                }
                
                // Memory cleanup
                ERASE_BERTLV(berEUICC_RAT);
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid RAT, main tag %X not found!", EUICC_RAT_TAG);
                lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Submitted profile has no PPR defined, checking stop here.");
            res = true;     // This is not an error but a useless checking
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }
    
    // If any problem during parsing, cancel profile loading
    if(! res)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error reported during RAT rules parsing, canceling current download session");
        ptrIncomingProfileData->performCancelSession = true;
    }
    // Set cancel reason (Only one possible here) and invalidate eventual already set User Consent if session download is canceled
    if(ptrIncomingProfileData->performCancelSession)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Cancel Session required, setting cancel reason and disabling any User Consent request");
        ptrIncomingProfileData->cancelSessionReason = LPA_CANCEL_SESSION_PPR_NOT_ALLOWED;
        ptrIncomingProfileData->userCallBackType = LPA_USR_CONSENT_DISABLED;
    }
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_checkIncomingProfilePPRattributesVersurRAT(): return %s", (res ? "true" : "false"));
    
    return res;
}

/**
 * Analyze PPAR (RAT rule) versus profile PPR attributes. Update LPA_PPR_RAT_ANALYSIS_FLAGS structures if rule found is more accurate.
 * @param ptrPPARrule - Pointer on current PPAR rule to evaluate
 * @param ptrIncomingProfileData - Pointer on incoming profile data, so here PPR status, MCC/MNC and GID1 / GID2
 * @param ptrCheckPPR1 - Pointer on LPA_PPR_RAT_ANALYSIS_FLAGS structure for PPR1
 * @param ptrCheckPPR2 - Poitner on LPA_PPR_RAT_ANALYSIS_FLAGS structure for PPR2
 * @return true if analysis is OK, else false if PPAR data error detected or other issue encountered
 */
bool _analysePPARversusProfileAttribute(BeerTLV * ptrPPARrule, LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR * ptrIncomingProfileData, LPA_PPR_RAT_ANALYSIS_FLAGS * ptrCheckPPR1, 
                                                                                                                                LPA_PPR_RAT_ANALYSIS_FLAGS  * ptrCheckPPR2)
{
    bool res = false;
    
    // PPAR main elements
    BeerTLV * berPprIds = NULL;
    BeerTLV * berAllowedOperators = NULL;
    BeerTLV * berConsentRequired = NULL;
    
    bool ppr1InPPAR = false;
    bool ppr2InPPAR = false;
    bool consentRequiredFlag = false;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_analysePPARversusProfileAttribute()...");
    
    if(ptrPPARrule != NULL && ptrIncomingProfileData != NULL && ptrCheckPPR1 != NULL && ptrCheckPPR2 != NULL)
    {
        // Extract PprIDs TLV, Tag 0x80
        berPprIds = berTLV_extractTagUInt8(0x80, ptrPPARrule->value, ptrPPARrule->length, NULL);
        // Extract allowedOperators list, Tag 0xA1
        berAllowedOperators = berTLV_extractTagUInt8(0xA1, ptrPPARrule->value, ptrPPARrule->length, NULL);
        // Extract consentResuired flag, Tag 0x82
        berConsentRequired = berTLV_extractTagUInt8(0x82, ptrPPARrule->value, ptrPPARrule->length, NULL);
        
        // All these fields are mandatory, if any is missing RAT structure is damaged, exit on error
        if(berPprIds != NULL && berAllowedOperators != NULL && berConsentRequired)
        {
            // All these fields cannot be empty, defined lengths for PprIds & consentRequired
            //  allowedOperator must be at least 7 bytes (At least one operator with MCC/MNC object defined in (A1 + L + 30 + L + 80 + L + MCCMNC)
            if(berPprIds->length == 2 && berAllowedOperators->length >= 7 && (berConsentRequired->length == 0x02 || berConsentRequired->length == 0x01))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PPAR TLV objects validated. Now retrieving PPR(s) and consentRequired defined in PPAR");
                // At this step we consider result OK until something invalidate it
                res = true;
                
                // Check if PPR1 / PPR2 defined in PPAR (Extract from 2 bytes Bitsring Value)
                ppr1InPPAR = _isPPR1definedInPPRflag((berPprIds->value[0] << 8) + berPprIds->value[1]);
                ppr2InPPAR = _isPPR2definedInPPRflag((berPprIds->value[0] << 8) + berPprIds->value[1]);
                
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PPAR - PPR1 status: PPAR: %s - Profile: %s", (ppr1InPPAR)?"Defined":"Not defined",
                                                                                                                    (ptrIncomingProfileData->hasPPR1)?"Defined":"Not defined");
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PPAR - PPR2 status: PPAR: %s - Profile: %s", (ppr2InPPAR)?"Defined":"Not defined",
                                                                                                                    (ptrIncomingProfileData->hasPPR2)?"Defined":"Not defined");
                
                // Recover consentRequired flag. If fag set bitstring value = 0x780 else 0
                if(berConsentRequired->length == 0x02)
                {
                    if(((berConsentRequired->value[0] << 8) + berConsentRequired->value[1]) == 0x0780)
                        consentRequiredFlag = true;
                    else
                        res = false;
                }
                else
                {
                    // Length 0x01 already checked
                    if(berConsentRequired->value[0] == 0)
                        consentRequiredFlag = false;
                    else
                        res = false;
                }
                    
                // Continue only if consentRequired correctly encoded
                if(res)
                {
                    // Launch operator analysis for PPR1 & PPR2
                    if(ppr1InPPAR && ptrIncomingProfileData->hasPPR1)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PPAR - Launch allowedOperator analysis for PPR1");
                        res = _analysePPARallowedOperatorVersusProfile(ptrIncomingProfileData, ptrCheckPPR1, berAllowedOperators, consentRequiredFlag);
                    }

                    if(res && ppr2InPPAR && ptrIncomingProfileData->hasPPR2)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PPAR - Launch allowedOperator analysis for PPR2");
                        res = _analysePPARallowedOperatorVersusProfile(ptrIncomingProfileData, ptrCheckPPR2, berAllowedOperators, consentRequiredFlag);
                    }
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect value detected in TLV consentRequired, profile download is not allowed");
                    lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect length detected in TLV object in current PPAR, profile download is not allowed");
                lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Missing TLV object in current PPAR, profile download is not allowed");
            lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
        }
        
        // Memory cleanup
        ERASE_BERTLV(berPprIds);
        ERASE_BERTLV(berAllowedOperators);
        ERASE_BERTLV(berConsentRequired);
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect NULL parameter(s)!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }
        
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_analysePPARversusProfileAttribute(): return %s", (res ? "true" : "false"));
    
    return res;
}

/**
 * Analyze PPAR Allowed Operator list versus profile PPR attributes. Update LPA_PPR_RAT_ANALYSIS_FLAGS structures if rule found is more accurate.
 * @param ptrIncomingProfileData - Pointer on incoming profile data, so here MCC/MNC and GID1 / GID2
 * @param ptrCheckPPR - Pointer on LPA_PPR_RAT_ANALYSIS_FLAGS structure for currently evaluated PPR
 * @param berAllowedOperators - Pointer on BeerTLV TLV containing Allowed Operator list
 * @param consentRequiredFlag - Consent Required flag for current PPAR, already decoded as boolen flag
 * @return true if analysis is OK, else false if Allowed Operator data error detected or other issue encountered
 */
bool _analysePPARallowedOperatorVersusProfile(LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR * ptrIncomingProfileData, LPA_PPR_RAT_ANALYSIS_FLAGS * ptrCheckPPR, 
                                                                                                            BeerTLV * berAllowedOperators, const bool consentRequiredFlag)
{
    bool res = false;
    
    BerTLVList * berTLVlistOperatorsBase = NULL;
    uint8_t tlvListOperatorsCount = 0;
    BerTLVList * berTLVlistOperatorsParser = NULL;
    BeerTLV * berTLVlistOperatorsCurrent = NULL;
    
    BeerTLV * berTLVmccMnc = NULL;
    BeerTLV * berTLVgid1 = NULL;
    BeerTLV * berTLVgid2 = NULL;
    
    bool compareMCCMNCmatch = false;
    int compareMCCMNClevel = 0;
    bool definedGID1inPPAR = false;
    bool compareGID1match = false;
    bool definedGID2inPPAR = false;
    bool compareGID2match = false;
    bool profileMatchRule = false;
    int ruleGIDlevel = 0;
        
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_analysePPARallowedOperatorVersusProfile()...");
    
    if(ptrIncomingProfileData != NULL && ptrCheckPPR != NULL && berAllowedOperators != NULL)
    {
        // Extract allowedOperators list from PPAR
        berTLVlistOperatorsBase = berTLV_extractList(berAllowedOperators->value, berAllowedOperators->length, &tlvListOperatorsCount);
        
        // PPAR allowedOperators list shall contain at least one element
        if(berTLVlistOperatorsBase != NULL && tlvListOperatorsCount > 0)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Found %d allowedOperator in PPAR", tlvListOperatorsCount);
            // At this step we consider parsing OK, except if something invalidate this status
            res = true;
            
            berTLVlistOperatorsParser = berTLVlistOperatorsBase;
            // Parse list of Allowed Operators defined in PPAR
            while(berTLVlistOperatorsParser != NULL)
            {
                berTLVlistOperatorsCurrent = berTLVlistOperatorsParser->berTLV;
                if(berTLVlistOperatorsCurrent != NULL)
                {
                    // Operator tag entry is 0x30
                    if(berTLVlistOperatorsCurrent->tag == 0x30)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TLV allowedOperator tag correct, extracting MCC/MNC");
                        // Extract MCC/MNC from allowedOperator entry. This is a mandatory tag, always 3 bytes long
                        berTLVmccMnc = berTLV_extractTagUInt8(0x80, berTLVlistOperatorsCurrent->value, berTLVlistOperatorsCurrent->length, NULL);
                        if(berTLVmccMnc != NULL && berTLVmccMnc->length == 3)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Compare allowedOperator MCC/MNC with profile MCC/MNC");
                            // Compare MCC/MNC, with possible wildcards
                            res = _comparePPARallowedOperator_MCC_MNC(berTLVmccMnc->value, ptrIncomingProfileData->mccMnc, &compareMCCMNCmatch, &compareMCCMNClevel);
                                    
                            if(res)
                            {
                                // If there is a somewhat matching between MCC/MNC with allowedOperator, continue with comparison
                                if(compareMCCMNCmatch)
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "AllowedOperator MCC/MNC and profile MCC/MNC can match, now extracting GID's from rule");
                                    
                                    // Initialize allowedOperator GID level
                                    ruleGIDlevel = 0;
                                    
                                    // If GID1 defined in allowedOperator, compare with incoming profile
                                    compareGID1match = false;
                                    berTLVgid1 = berTLV_extractTagUInt8(0x81, berTLVlistOperatorsCurrent->value, berTLVlistOperatorsCurrent->length, NULL);
                                    if(berTLVgid1 != NULL)
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID1 is defined");
                                        // GID1 exists even if length = 0
                                        definedGID1inPPAR = true;
                                        // Useless to compare length = 0 because becomes a wildcard value
                                        if(berTLVgid1->length > 0)
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID1 contain data");
                                            if(berTLVgid1->length == ptrIncomingProfileData->gid1Size)
                                            {
                                                if(0 == memcmp(berTLVgid1->value, ptrIncomingProfileData->gid1, berTLVgid1->length))
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID1 from allowedOperator and profile are equal");
                                                    compareGID1match = true;
                                                }
                                            }
                                        }
                                        else
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID1 is empty");
                                    }
                                    else
                                        definedGID1inPPAR = false;
                                    
                                    // If GID2 defined in allowedOperator, compare with incoming profile
                                    compareGID2match = false;
                                    berTLVgid2 = berTLV_extractTagUInt8(0x82, berTLVlistOperatorsCurrent->value, berTLVlistOperatorsCurrent->length, NULL);
                                    if(berTLVgid2 != NULL)
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID2 is defined");
                                        // GID1 exists even if length = 0
                                        definedGID2inPPAR = true;
                                        // Useless to compare length = 0 because becomes a wildcard value
                                        if(berTLVgid2->length > 0)
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID2 contain data");
                                            if(berTLVgid2->length == ptrIncomingProfileData->gid2Size)
                                            {
                                                if(0 == memcmp(berTLVgid2->value, ptrIncomingProfileData->gid2, berTLVgid2->length))
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID2 from allowedOperator and profile are equal");
                                                    compareGID2match = true;
                                                }
                                            }
                                        }
                                        else
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID2 is empty");
                                    }
                                    else
                                        definedGID2inPPAR = false;
                                    
                                    // Log status of incoming profile GID's
                                    // Incoming GID1
                                    if(ptrIncomingProfileData->gid1Defined)
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Incoming profile GID1 is defined, length = %d", ptrIncomingProfileData->gid1Size);
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Incoming profile GID1 is not defined");
                                    // Incoming GID2
                                    if(ptrIncomingProfileData->gid2Defined)
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Incoming profile GID2 is defined, length = %d", ptrIncomingProfileData->gid2Size);
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Incoming profile GID2 is not defined");

                                    
                                    // Final comparison with evaluated PPR, considering MMC/MNC and GID
                                    profileMatchRule = true;

                                    // At least same matching level for MCC / MNC is needed
                                    if(ptrCheckPPR->matchLevelMCC_MNC <= compareMCCMNClevel)
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Level of allowedOperator MCC/MNC is higher or equal than previously evaluated one");
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Now check allowedOperator GID's compatibility with profile GID's");
                                        // GID1 defined in PPAR allowedOperator
                                        if(definedGID1inPPAR)
                                        {
                                            // If length = 0 becomes a wildcard so no need to compare
                                            if(berTLVgid1->length > 0)
                                            {
                                                // If GID match increase matching level else profile does not match rule
                                                if(compareGID1match)
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID1's are the same, increasing GID matching level");
                                                    ruleGIDlevel++;
                                                }
                                                else
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID1's are different, PPAR allowedOperator discarded");
                                                    profileMatchRule = false;
                                                }
                                            }
                                            else
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Empty GID1 defined in allowedOperator, compatible with any profile GID1");
                                        }
                                        else
                                        {
                                            // If GID not defined in PPAR, must also be not defined in profile
                                            if(ptrIncomingProfileData->gid1Defined)
                                            {
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID1 not defined in allowedOperator while defined in profile, PPAR allowedOperator discarded");
                                                profileMatchRule = false;
                                            }
                                            else
                                            {
                                                // GID not defined in PPAR and incoming profile is also an exact match
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID1 not defined in allowedOperator and profile, increasing GID matching level");
                                                compareGID1match = true;
                                                ruleGIDlevel++;
                                            }
                                        }

                                        if(profileMatchRule)
                                        {
                                            // GID2 defined in PPAR allowedOperator
                                            if(definedGID2inPPAR)
                                            {
                                                // If length = 0 becomes a wildcard so no need to compare
                                                if(berTLVgid2->length > 0)
                                                {
                                                    // If GID match increase matching level else profile does not match rule
                                                    if(compareGID2match)
                                                    {
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID2's are the same, increasing GID matching level");
                                                        ruleGIDlevel++;
                                                    }
                                                    else
                                                    {
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID2's are different, PPAR allowedOperator discarded");
                                                        profileMatchRule = false;
                                                    }
                                                }
                                                else
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Empty GID2 defined in allowedOperator, compatible with any profile GID2");
                                            }
                                            else
                                            {
                                                // If GID not defined in PPAR, must also be not defined in profile
                                                if(ptrIncomingProfileData->gid2Defined)
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID2 not defined in allowedOperator while defined in profile, PPAR allowedOperator discarded");
                                                    profileMatchRule = false;
                                                }
                                                else
                                                {
                                                    // GID not defined in PPAR and incoming profile is also an exact match
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "GID2 not defined in allowedOperator and profile, increasing GID matching level");
                                                    compareGID2match = true;
                                                    ruleGIDlevel++;
                                                }
                                            }
                                        }
                                    }
                                    else
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Level of rule MCC/MNC is lower than previously evaluated allowedOperator, PPAR allowedOperator discarded");
                                        profileMatchRule = false;
                                    }

                                    // If matched all conditions and GID matching "weight" is bigger than previous, update status of evaluated PPR
                                    if(profileMatchRule)
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PPAR allowedOperator match with incoming profile");
                                        if(ruleGIDlevel >= ptrCheckPPR->matchLevelGID)
                                        {

                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Level of GID's is higher or equal than previously evaluated one, PPR validated with actual levels and use PPAR condition for User Consent");
                                            ptrCheckPPR->pprValidated = true;                           // PPR has been validated at least one time
                                            ptrCheckPPR->matchLevelMCC_MNC = compareMCCMNClevel;          // Matching level for MCC / MNC
                                            ptrCheckPPR->matchedGID1 = compareGID1match;
                                            ptrCheckPPR->matchedGID2 = compareGID2match;
                                            ptrCheckPPR->matchLevelGID = ruleGIDlevel;
                                            ptrCheckPPR->userConsentRequired = consentRequiredFlag;
                                        }
                                        else
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Level of allowedOperator GID's is lower than previously evaluated rule, PPAR allowedOperator discarded");
                                        
                                    }
                                }
                                else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "AllowedOperator MCC/MNC and profile MCC/MNC cannot match, PPAR allowedOperator does not fit with profile");
                                      
                            }
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid MCC/MNC in allowedOperator or profile, profile download is not allowed");
                                lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
                                res = false;
                                break;  // No need to parse another operator
                            }
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Mandatory MCC/MNC tag missing or incorrect length in allowedOperator entry, profile download is not allowed");
                            lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
                            res = false;
                            break;  // No need to parse another operator
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid tag detected for allowedOperator list element, profile download is not allowed");
                        lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
                        res = false;
                        break;  // No need to parse another operator
                    }
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "PPAR allowedOperator parser: Current BerTLV list element appear to be NULL ? Go to next one...");
                
                berTLVlistOperatorsParser = berTLVlistOperatorsParser->ptrNext;
            }
            
            // Memory cleanup
            ERASE_BERTLV(berTLVmccMnc);
            ERASE_BERTLV(berTLVgid1);
            ERASE_BERTLV(berTLVgid2);
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Operator list not found or empty, profile download is not allowed");
            lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
        }
        
        // Memory cleanup
        ERASE_BERTLV_LIST(berTLVlistOperatorsBase);
        berTLVlistOperatorsParser = NULL;
        berTLVlistOperatorsCurrent = NULL;
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect NULL parameter(s)!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }    
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_analysePPARallowedOperatorVersusProfile(): return %s", (res ? "true" : "false"));
    
    return res;
}

/**
 * Compare allowedOperator and profile MCC/MNC, taking account of wildcard digits 'E' in allowedOperator. Also return matching level (0 if all wildcard, 6 if all match)
 * @param ptrAllowOpMCCMNC - Pointer on allowedOperator MCC/MNC, 3 bytes long
 * @param ptrProfileMCCMNC - Pointer on incoming profile MCC/MNC, 3 bytes long
 * @param ptrMatchingFlag - Pointer on flag confirming if MCC/MNC match
 * @param ptrMatchinglevel - Pointer on Matching level value to return
 * @return true if operation OK and MCC/MNC digits are correct
 */
bool _comparePPARallowedOperator_MCC_MNC(const unsigned char * ptrAllowOpMCCMNC, const unsigned char * ptrIncomingProfileMCCMNC, bool * ptrMatchingFlag, int * ptrMatchinglevel)
{
    bool res = false;
    uint32_t allowedOperatorMCCMNC32 = 0;
    uint32_t profileMCCMNC32 = 0;
    unsigned char checkDigitAllOp = 0;
    unsigned char checkDigitProf = 0;
    int i = 0;
        
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_comparePPARallowedOperator_MCC_MNC()...");
    
    if(ptrAllowOpMCCMNC != NULL && ptrIncomingProfileMCCMNC != NULL && ptrMatchingFlag != NULL && ptrMatchinglevel != NULL)
    {
        // Transfer the 3 bytes of MCC MNC in 32 bytes values
        allowedOperatorMCCMNC32 = (ptrAllowOpMCCMNC[0] << 16) + (ptrAllowOpMCCMNC[1] << 8) + ptrAllowOpMCCMNC[2];
        profileMCCMNC32 = (ptrIncomingProfileMCCMNC[0] << 16) + (ptrIncomingProfileMCCMNC[1] << 8) + ptrIncomingProfileMCCMNC[2];
        
        // For the moment we consider that all digits will match and comparison is OK
        *ptrMatchinglevel = 6;
        *ptrMatchingFlag = true;
        res = true;
        
        // Check MCC/MNC digits and build comparison mask
        while(i < 6 && res && *ptrMatchingFlag)
        {
            // Extract lower digit
            checkDigitAllOp = allowedOperatorMCCMNC32 & 0x0000000F;
            checkDigitProf = profileMCCMNC32 & 0x0000000F;
            
            // Check digits
            // allowedOperator: 0-9 + E + F
            // profile: 0-9 + F
            if(! ((checkDigitAllOp >= 0x00 && checkDigitAllOp <= 0x09) || checkDigitAllOp == 0x0E || checkDigitAllOp == 0x0F))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid digit detected in allowedProfile MCC/MNC");
                res = false;
            }
            if(! ((checkDigitProf >= 0x00 && checkDigitProf <= 0x09) || checkDigitProf == 0x0F))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid digit detected in incoming profile MCC/MNC");
                res = false;
            }
            
            if(res)
            {
                // If wildcard in allowedOperator, digit is validated and matching level decreased
                if(checkDigitAllOp == 0x0E)
                    *ptrMatchinglevel = *ptrMatchinglevel - 1;
                else
                {
                    // check if digits are matching. If not stop here
                    if(checkDigitAllOp != checkDigitProf)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Digit %d is different, allowedOperator and incoming profile does not match", (6 - i));
                        *ptrMatchingFlag = false;
                    }
                }
            }
            
            // Go to next digit to check
            allowedOperatorMCCMNC32 = allowedOperatorMCCMNC32 >> 4;
            profileMCCMNC32 = profileMCCMNC32 >> 4;
            i++;
        }
        
        if(res && *ptrMatchingFlag)
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "allowedOperator and incoming profile MCC/MNC are compatible, with level %d", *ptrMatchinglevel);
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect NULL parameter(s)!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }  
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_comparePPARallowedOperator_MCC_MNC(): return %s", (res ? "true" : "false"));
    
    return res;
}
    
/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Notify application owner about Cancel Session result through callback
 * Avoid to repeat that 3 times in _manageDownloadProfile()
 * 
 * @param ptrLpaEventCallback - Event Callback, type LPA_EventCallback
 * @param cancelResult - Result of the Cancel Session operation
 * @param callbackLevel - Level / event type of notification for Callback
 */
void _notifyAppliOwnerCancelResult(const LPA_EventCallback* ptrLpaEventCallback, const bool cancelResult, size_t callbackEventType)
{
    if (ptrLpaEventCallback != NULL && ptrLpaEventCallback->_lpaEventProgressText != NULL)
    {
        if(cancelResult)
            ptrLpaEventCallback->_lpaEventProgressText(ptrLpaEventCallback->_appParameter, callbackEventType, "Cancel Session done");
        else
            ptrLpaEventCallback->_lpaEventProgressText(ptrLpaEventCallback->_appParameter, callbackEventType, "Cancel Session failed");
    }
}

/**
* Perform profile download operation from calling entry point (With or without Confirmation Code provided).
*
* @param ptrActivationCodeStr - Activation Code, String format
* @param ptrConfirmationCodeStr - Confirmation Code, String format. Set it as "" if not used
* @param confirmationCodeLength - Length of Confirmation Code, Integer. Set it to 0 if not used.
* @param ptrLpaEventCallback - Event Callback, type LPA_EventCallback
* @param ptrDownloadProfileResult - Download profile result, type LPA_DOWNLOAD_PROFILE_RESULT
* @param retryRequested - Pointer to bool flag. If returned "true" ask to caller to perform another retry due to error (Chained GetResponse,...)
* @return True if profile download is successful
*/

static bool _manageDownloadProfile(const char * ptrActivationCodeStr, const char * ptrConfirmationCodeStr, const int confirmationCodeLength, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult, bool * retryRequested)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _manageDownloadProfile(...)");

	LPA_GET_EUICC getEuiccChallenge;
	LPA_GET_EUICC getEuiccInfo;
	RawDataObject * prepareCtxParam = NULL;
	LPA_SERVER_DATA serverData;
	PROFILE_INSTALLATION_RESULT pir;
	ACTIVATION_CODE activationCode;
	char transactionId[LPA_TRANSACTION_ID_MAX_SIZE];
        
        unsigned char certificateOIDrawASN1[LPA_OID_SIZE];  // By all the way Raw ASN1 will be always smaller than text version
        size_t certificateOIDrawASN1Length = 0;
        char certificateOIDtext[LPA_OID_SIZE];

        LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR incomingProfileData;
        LPA_GET_PROFILES_INFO currentlyInstalledProfiles;
        RawDataObject* eUICC_RATrules = NULL;
        LPA_REQUEST_USER_CONSENT_FOR_LOADING_PROFILE lpaRequestUserConsent;

	AUTHENTICATE_SERVER_RESPONSE authentServerResp;
	PREPARE_DOWNLOAD_RESPONSE prepareDownloadResp;

        bool cancelForBPPerrors = false;
        bool isNeedToSendPIR = false;
	LPA_API_ERROR lpaApiError = LPA_NO_ERROR;
	bool res = false;

	char hashConfirmCode[70];
        bool cancelResult = false;
	BeerTLV* ptrBerTLVsmdpSigned2Objects = NULL;
	BeerTLV* ptrBerTLVccRequiredFlag = NULL;
	bool tagFound;
        
        int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
        *retryRequested = false;    // For the moment, do not request retry in case of GetResponseChaining failure
        
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Start to Download Profile ...");

	// Step 1 : Init and allocate memory
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 1 -> Initialization ...");

	memset(transactionId, 0x00, sizeof(transactionId));
	memset(&serverData, 0x00, sizeof(LPA_SERVER_DATA));
	memset(&activationCode, 0x00, sizeof(ACTIVATION_CODE));
        memset(&incomingProfileData, 0x00, sizeof(LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR));
        memset(&currentlyInstalledProfiles, 0x00, sizeof(LPA_GET_PROFILES_INFO));
        memset(&certificateOIDrawASN1, 0x00, LPA_OID_SIZE);
        memset(&certificateOIDtext, 0x00, LPA_OID_SIZE);

        getEuiccChallenge.ptrEUICC = NULL;
        getEuiccChallenge.prtEUICC_Base64 = NULL;
        
        getEuiccInfo.ptrEUICC = NULL;
        getEuiccInfo.prtEUICC_Base64 = NULL;
        
	_lpaManagerInitServerData(&serverData);
        
	pir.hasResult = false;
        pir.ptrProfileInstallationResultTlv = NULL;
        pir.ptrProfileInstallationResultTlv_Base64 = NULL;
        
        authentServerResp.ptrAuthenticateServerResponse = NULL;
        authentServerResp.ptrAuthenticateServerResponse_Base64 = NULL;
        
        prepareDownloadResp.ptrPrepareDownloadResponse = NULL;
        prepareDownloadResp.ptrPrepareDownloadResponse_Base64 = NULL;

	// Parameters checks
        // ptrLpaEventCallback is checked each time it has to be used
        if (ptrActivationCodeStr != NULL && ptrDownloadProfileResult != NULL && ptrConfirmationCodeStr != NULL)
	{
            ptrDownloadProfileResult->countProfileInstalled = 0;
            ptrDownloadProfileResult->countProfileTotal = 0;
	}
	else
            lpaApiError = LPA_ERROR_INVALID_PARAMETER;

	// Step2 : Select ISDR if not yet selected
	// -> Now do nothing

	// Step 3 : GetEuiccChallenge
	if (lpaApiError == LPA_NO_ERROR)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 3 -> GetEuiccChallenge ...");

		if (_lpaManagerGetEuiccChallenge(&getEuiccChallenge))
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Success to get EuiccChallenge");
		else
			lpaApiError = LPA_ERROR_INVALID_GET_UICC_CHALLENGE;
	}

	// Step 4 : GetEuiccInfo
	if (lpaApiError == LPA_NO_ERROR)
	{
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 4 -> GetEuiccInfo ...");
            
            nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;

            // Management of execution / retry loop for GetEuiccInfo1
            while(nbExecGetResp > 0)
            {
                // If GetResponse Chaining issue occur, _lpaManagerGetEuiccInfo() will return false
                if (_lpaManagerGetEuiccInfo(&getEuiccInfo))
                {
                    // No chaining error detected, end execution loop
                    nbExecGetResp = 0;
                    
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Success to get UiccInfo");
                }
                else
                {
                    // Decrease execution loop. If Retry not enabled, will reach 0 so no retry
                    nbExecGetResp--;

                    // Retry management
                    if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                    {
                        // If last loop not reached, clear error code to execute another attempt
                        // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                        if (nbExecGetResp > 0)
                        {
                            lpaResetErrorCode();
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "GetEuiccInfo1: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);

                            // Avoid memory leak because assigned by _lpaManagerGetEuiccInfo()
                            ERASE_RAWDATAOBJECT(getEuiccInfo.ptrEUICC);
                            ERASE_RAWDATAOBJECT(getEuiccInfo.prtEUICC_Base64);
                        }
                        else
                            lpaApiError = SE_MEDIA_E_CHAINING_GET_RESPONSE;  // Prevent going on next steps for nothing
                    }
                    else
                    {
                        lpaApiError = LPA_ERROR_INVALID_GET_EUICC_INFO;
                        nbExecGetResp = 0; // End retry loop, another error encountered
                    }
                }
            }
	}


	// Step 5 : Manage activation code
	if (lpaApiError == LPA_NO_ERROR)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 5 -> Manage activation code...");

		if (_decodeActivationCodeStr(ptrActivationCodeStr, &activationCode))
		{
                        _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Activation code decoded");
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to extract data from Activation code.");
			lpaApiError = LPA_ERROR_FAILED_GET_DATA_FROM_ACTIVATION_CODE;
		}
	}

	// Step 6 : Initiate Authentication
	if (lpaApiError == LPA_NO_ERROR)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 6 -> Initiate Authentication ...");
		if ( getEuiccChallenge.prtEUICC_Base64 != NULL && getEuiccInfo.prtEUICC_Base64 != NULL &&
			lpaManagerInitiateAuthentication(&serverData, ptrLpaEventCallback, (const char*)getEuiccChallenge.prtEUICC_Base64->rawData, (const char*)getEuiccInfo.prtEUICC_Base64->rawData, activationCode.smdxAddr))
		{
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Initiate authentication done");
                        _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "Initiate authentication done");

			// Check and copy transactionId
			if (serverData._transactionId.val != NULL && serverData._transactionId.len > 0 && serverData._transactionId.len < LPA_TRANSACTION_ID_MAX_SIZE)
			{
				sprintf(transactionId, "%s", serverData._transactionId.val);
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : TransactionId : <%s>", transactionId);
			}
			else
				lpaApiError = LPA_ERROR_INVALID_TRANSACTIONID;
		}
		else
		{
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Initiate authentication");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "Failed to Initiate authentication");
                        
                    lpaApiError = LPA_ERROR_FAILED_INITIAL_AUTHENTICATION;
		}
	}
        
        // Memory cleanup
        ERASE_RAWDATAOBJECT(getEuiccChallenge.ptrEUICC);
        ERASE_RAWDATAOBJECT(getEuiccChallenge.prtEUICC_Base64);
        
        ERASE_RAWDATAOBJECT(getEuiccInfo.ptrEUICC);
        ERASE_RAWDATAOBJECT(getEuiccInfo.prtEUICC_Base64);

	// Step 7 : Check SM-DP+ Address 
	if (lpaApiError == LPA_NO_ERROR)
	{
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 7 -> Check SM-DP+ Address ...");
            if (serverData._serverSigned1.val != NULL && _verifySMDPAddress(activationCode.smdxAddr, (const char*)serverData._serverSigned1.val, serverData._serverSigned1.len))
            {
                _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "SM-DP+ address checking done");
                
                if (activationCode.matchingId != NULL && strlen(_deviceInfo) > 0 && _lpaManagerPrepareCtxParam(activationCode.matchingId, _deviceInfoByteArray, _deviceInfoByteArraySize, &prepareCtxParam))
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Success to prepareCtxParam");
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to prepareCtxParam");
                    lpaApiError = LPA_ERROR_INVALID_CTX_PARAM;
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid SMDP address");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "Failed to check SM-DP+ address, invalid");
                lpaApiError = LPA_ERROR_INVALID_SERVER_ADDRESS;
            }
	}
        
        // Step 8 : Check OID. If error at this step no need to perform Cancel Session because Authenticate client not yet performed.
        if(lpaApiError == LPA_NO_ERROR)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 8 -> Check Activation Code OID versus OID in certificate returned by server...");
            
            // Note: Activation Code OID format has already been checked by _decodeActivationCodeStr() function

            // If OID is not defined in Activation Code, OID test will be not performed
            if(strlen(activationCode.oid) > 0)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "OID provided in Activation Code, extracting OID from certificate...");
                
                // Extract OID from certificate. If not defined or error in extraction, exit on error
                if(extractOIDfromCertificate(serverData._serverCertificate.val, serverData._serverCertificate.len, certificateOIDrawASN1, &certificateOIDrawASN1Length, LPA_OID_SIZE))
                {
                    convertASN1_OIDtoText(certificateOIDrawASN1, certificateOIDrawASN1Length, certificateOIDtext, LPA_OID_SIZE);
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "OID extracted from Activation Code: %s", activationCode.oid);
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "OID extracted from certificate    : %s", certificateOIDtext);
                    
                    // Compare OID's. Stop download if mismatch
                    if(strcmp(activationCode.oid, certificateOIDtext) == 0)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "OID check done successfully. Match with Activation Code");
                        _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "OID comparison done");
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "OIDs do not match. Check failed. Abort download");
                        _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "Failed to OID comparison, does not match");
                        lpaApiError = LPA_ERROR_OID_MISMATCH;
                    }
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "An issue has been encountered while extracting OID from certificate returned by server or OID not found");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "Failed to retrieve OID from certificate");
                    lpaApiError = LPA_ERROR_INVALID_SERVERCERTIFICATE;
                }
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No OID defined in Activation Code, checking canceled.");
        }

	// Step 9 : Authenticate Server 
	if (lpaApiError == LPA_NO_ERROR)
	{
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 9 -> Authenticate server ...");

            // If GetResponse chaining error, _lpaManagerAuthenticateServer() will return false
            if(_lpaManagerAuthenticateServer(&serverData, ptrLpaEventCallback, prepareCtxParam, &authentServerResp) && authentServerResp.ptrAuthenticateServerResponse_Base64 != NULL)
            {
                _sendEventCallbackProgressText(ptrLpaEventCallback, 2, "Authenticate server done");

                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Authenticate server done");
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Authenticate server response: %s ", authentServerResp.ptrAuthenticateServerResponse_Base64->rawData);

                _lpaManagerFreeServerData(&serverData);
                memset(&serverData, 0, sizeof(serverData));
            }
            else
            {
                // Management of GetResponse chaining issue. Command retry is not possible at this step, cancel current session and inform caller that
                // retry of entire process is requested
                if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                {
                    *retryRequested = true;
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "AuthenticateServer: GetResponse chaining issue detected, stop current profile download");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 2,  "AuthenticateServer: GetResponse chaining issue detected, stop current profile download");
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Authenticate server.");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 2, "Failed to Authenticate server");
                }
                
                lpaApiError = LPA_ERROR_FAILED_AUTHENTICATE_SERVER;
            }
	}
        
        // Memory cleanup
        ERASE_RAWDATAOBJECT(prepareCtxParam);
      
	// Step 10 : Authenticate Client 
	if (lpaApiError == LPA_NO_ERROR)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 10 -> Authenticate client ...");

		if (lpaManagerAuthenticateClient(&serverData, ptrLpaEventCallback, transactionId, activationCode.smdxAddr, authentServerResp.ptrAuthenticateServerResponse_Base64->rawData))
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Authenticate client done");
			_sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Authenticate client done");
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Authenticate client!");
                        _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Failed to Authenticate client");
                        
			lpaApiError = LPA_ERROR_FAILED_AUTHENTICATE_CLIENT;
		}
	}

	// Do memory cleanup
        ERASE_RAWDATAOBJECT(authentServerResp.ptrAuthenticateServerResponse);
        ERASE_RAWDATAOBJECT(authentServerResp.ptrAuthenticateServerResponse_Base64);
        
        // Total Profile - At this step (Authenticate Client OK) we consider we have a profile available
        if (lpaApiError == LPA_NO_ERROR)
        {
            // No need to increase because we always start from 0
            ptrDownloadProfileResult->countProfileTotal = 1;
        }
        
        // Step 11 : Extract data linked to PPR filtering from profile Metadata retrieved after Authenticate Client: PPR, MCC/MNC, GID1/GID2 & Profile Name
        // Also check if PPR1 or PPR2 is defined in profile
        if (lpaApiError == LPA_NO_ERROR)
        {
            // Icoming profile data initialization
            incomingProfileData.profilePPR = 0;
            incomingProfileData.hasPPR1 = false;
            incomingProfileData.hasPPR2 = false;
            // mccMnc already set to 00 by memset
            // gid1  already set to 00 by memset
            // gid1Size already set to 00 by memset
            incomingProfileData.gid1Defined = false;
            // gid2  already set to 00 by memset
            // gid2Size already set to 00 by memset
            incomingProfileData.gid2Defined = false;
            // profileName already set to 00 by memset
            incomingProfileData.userCallBackType = LPA_USR_CONSENT_DISABLED;
            incomingProfileData.performCancelSession = false;
            // cancelSessionReason already set to 00 by memset, no meaning while performCancelSession = false
                        
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 11 -> Extract Metadata linked to PPR management from incoming profile...");
            // Launch extraction only if metadata have been retrieved from profile (Optional in Authenticate Client response)
            if(serverData._profileMetadata.len > 0)
            {
                if(! _extractFieldsFromProfileMetadata(serverData._profileMetadata.val, serverData._profileMetadata.len, &incomingProfileData))
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to extract Metadata from incoming profile!");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Failed to extract Metadata from incoming profile");
                    lpaApiError = LPA_ERROR_LOCAL_PROFILE_UNABLE_TO_EXTRACT_DATA;
                                    
                    // This error will drive to Cancel Session with reason "Undefined" (Case not covered)
                    incomingProfileData.performCancelSession = true;
                    incomingProfileData.cancelSessionReason = LPA_CANCEL_SESSION_UNDEFINED_REASON;
                }
            }
        }
                
        // Step 12 Get RAT from eUICC if PPR1 or PPR2 is defined
        if (lpaApiError == LPA_NO_ERROR && (incomingProfileData.hasPPR1 || incomingProfileData.hasPPR2))
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 12 -> Retrieve RAT from eUICC...");
            if(! _lpaManagerGetRAT(&eUICC_RATrules))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve RAT from eUICC!");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Failed to retrieve RAT from eUICC");
                lpaApiError = LPA_ERROR_INVALID_GET_RAT;
                
                // Error in this process will drive to Cancel Session with "PPR not allowed"
                incomingProfileData.performCancelSession = true;
                incomingProfileData.cancelSessionReason = LPA_CANCEL_SESSION_PPR_NOT_ALLOWED;
            }
        }
        
        // Step 13 : Extract already installed profiles info, list apply for PPR management
        if (lpaApiError == LPA_NO_ERROR)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 13 -> Retrieve information of already installed profiles...");
            // Launch retrieve of information for profiles already installed on eUICC
            if(! _getProfilesInfoForPPR(&currentlyInstalledProfiles))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve installed profiles informations from eUICC!");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Failed to retrieve installed profiles informations from eUICC");
                lpaApiError = LPA_ERROR_INVALID_GET_PROFILES_INFO_EXCHANGE;
                
                // This error will drive to Cancel Session with reason "Undefined" (Case not covered)
                incomingProfileData.performCancelSession = true;
                incomingProfileData.cancelSessionReason = LPA_CANCEL_SESSION_UNDEFINED_REASON;
            }
        }

        // Step 14 : PPR management: Check if downloaded profile can be installed versus profiles already installed on eUICC
        // If profile has PPR some conditions apply
        if (lpaApiError == LPA_NO_ERROR)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 14 -> Checking incoming profile PPR versus already installed profiles...");
            if(! _checkIncomingProfilePPRconditionVersusInstalledProfiles(&incomingProfileData, &currentlyInstalledProfiles))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to check PPR of incoming profile versus already installed profiles!");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Failed to check PPR of incoming profile versus already installed profiles");
                lpaApiError = LPA_ERROR_INVALID_GET_PROFILES_INFO_EXCHANGE;
                
                // This error will drive to Cancel Session with reason "Undefined" (Case not covered)
                incomingProfileData.performCancelSession = true;
                incomingProfileData.cancelSessionReason = LPA_CANCEL_SESSION_UNDEFINED_REASON;
            }
            else
            {
                // Set lpaApiError in case cancel is due to PPR not allowed
                if(incomingProfileData.performCancelSession)
                {
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Profile download not allowed (has PPR1 set whereas at least one Operational profile is already loaded on eUICC)");
                    lpaApiError = LPA_ERROR_PPR_NOT_ALLOWED;
                }
            }
        }

        // Step 15 : Check incoming profile PPR (If defined) versus RAT rules. 
        // Does not apply if already Canceled by checking versus installed profiles (So lpaApiError = LPA_ERROR_PPR_NOT_ALLOWED so not entering here)
        if (lpaApiError == LPA_NO_ERROR && (incomingProfileData.hasPPR1 || incomingProfileData.hasPPR2))
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 15 -> Checking incoming profile PPR versus RAT rules...");
            if(! _checkIncomingProfilePPRattributesVersusRAT(&incomingProfileData, eUICC_RATrules))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to check incoming profile PPR versus PPR rules stored in RAT!");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Failed to check incoming profile PPR versus PPR rules stored in RAT");
                
                // Error in this process will drive to Cancel Session with "PPR not allowed"
                incomingProfileData.performCancelSession = true;
                incomingProfileData.cancelSessionReason = LPA_CANCEL_SESSION_PPR_NOT_ALLOWED;
            }
            
            // If Cancel Session required (All due to PPR not allowed) set lpaApiError to cancel remaining of procedure
            if(incomingProfileData.performCancelSession)
            {
                _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Profile download not allowed by RAT");
                lpaApiError = LPA_ERROR_PPR_NOT_ALLOWED;
            }
        }
        
        // Memory cleanup: Profile list / RAT used for PPR management
        if(currentlyInstalledProfiles.profileInfoList != NULL)
        {
            lpaCoreMemoryFree(currentlyInstalledProfiles.profileInfoList);
            currentlyInstalledProfiles.profileInfoList = NULL;
            // currentlyInstalledProfiles is a local member structure, will be removed from heap at the end of function
        }
        ERASE_RAWDATAOBJECT(eUICC_RATrules);
       
        // Step 16 : Perform User Consent through Callback if required and loading not already refused by PPR conditions (lpaApiError = LPA_ERROR_PPR_NOT_ALLOWED)
        if (lpaApiError == LPA_NO_ERROR && incomingProfileData.userCallBackType != LPA_USR_CONSENT_DISABLED)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 16 -> User Consent for PPR management...");

            // Query User Consent only if Callback is defined
            if(ptrLpaEventCallback != NULL && ptrLpaEventCallback->_lpaEventRequestUserConsentForLoadingProfile != NULL )
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "User Consent required, initiating Callback...");
                // To be sure to initialize it a each request
                memset(&lpaRequestUserConsent, 0x00, sizeof(LPA_REQUEST_USER_CONSENT_FOR_LOADING_PROFILE));
                
                lpaRequestUserConsent.userCallBackType = incomingProfileData.userCallBackType;
                lpaRequestUserConsent.downloadAllowed = false;
                lpaRequestUserConsent.cancelSessionReason = LPA_CANCEL_SESSION_UNDEFINED_REASON;

                // Copy profile name in callback structure. If problem during copy, set "..." as profile name
                if(NULL == strncpy(lpaRequestUserConsent.profileName, incomingProfileData.profileName, LPA_PROFILE_NAME_MAX_SIZE + 1))
                {
                    lpaRequestUserConsent.profileName[0] = '.';
                    lpaRequestUserConsent.profileName[1] = '.';
                    lpaRequestUserConsent.profileName[2] = '.';
                    lpaRequestUserConsent.profileName[3] = '\0';
                }

                if ( !ptrLpaEventCallback->_lpaEventRequestUserConsentForLoadingProfile(ptrLpaEventCallback->_appParameter, &lpaRequestUserConsent))
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Callback defined to query User Consent, return false => Cancel session");
                    incomingProfileData.performCancelSession = true;
                    incomingProfileData.cancelSessionReason = LPA_CANCEL_SESSION_POSTPONED;
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Callback execution is correct, get user response");
                    incomingProfileData.performCancelSession = ! lpaRequestUserConsent.downloadAllowed;
                    incomingProfileData.cancelSessionReason = lpaRequestUserConsent.cancelSessionReason;
                }
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No Callback defined to query User Consent, download will be granted by default.");

            // If Cancel Session required due to User choice or no response (Time out case), set lpaApiError to cancel remaining of procedure
            if(incomingProfileData.performCancelSession)
                lpaApiError = LPA_ERROR_DOWNLOAD_SESSION_CANCELED_BY_USER;
        }

        // Step 17 : Perform Cancel Session if required
        if (incomingProfileData.performCancelSession)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 17 -> Cancel Session required, initiating it with reason code %d...", incomingProfileData.cancelSessionReason);

            cancelResult = _lpaManagerCancelSession(transactionId, incomingProfileData.cancelSessionReason, activationCode.smdxAddr, ptrLpaEventCallback);
            // Notify application owner (Callback)
            _notifyAppliOwnerCancelResult(ptrLpaEventCallback, cancelResult, 3);
        }
        
        // Step 18 : Confirm PPR check has been done
	if (lpaApiError == LPA_NO_ERROR)
	{
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 18 -> PPR conditions check performed successfully, download granted...");
            _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "PPR conditions check done");
        }

	// Step 19 : prepareDownload
	if (lpaApiError == LPA_NO_ERROR)
	{
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 19 -> prepare Download ...");
            if (lpaApiError == LPA_NO_ERROR)
            {
                if (serverData._smdpSigned2.val != NULL)
                {
                    hashConfirmCode[0] = 0; // Init empty string for Confirmation Code hash, not yet checked if required

                    // Check if server requires Confirmation Code
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Check if Confirmation Code requested by server ...");

                    if (serverData._smdpSigned2.len > 0){
                        ptrBerTLVsmdpSigned2Objects = berTLV_extractTagUInt8(ASN1_SMDPSIGNED2_TAG, serverData._smdpSigned2.val, serverData._smdpSigned2.len, &tagFound);

                        if (ptrBerTLVsmdpSigned2Objects != NULL){
                            ptrBerTLVccRequiredFlag = berTLV_extractTagUInt8(ASN1_CCREQUIREDFLAG_TAG, ptrBerTLVsmdpSigned2Objects->value, ptrBerTLVsmdpSigned2Objects->length, &tagFound);

                            if (ptrBerTLVccRequiredFlag != NULL){
                                if ((ptrBerTLVccRequiredFlag->length == 1) && (*ptrBerTLVccRequiredFlag->value != 0))
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code required by server: ccRequiredFlag = Required");
                                    _sendEventCallbackProgressText(ptrLpaEventCallback, 4, "Confirmation Code required by server");

                                    // Generate Confirmation Code hash if Confirmation Code provided, else error
                                    if (confirmationCodeLength > 0)
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code provided.");
                                        if (_calculateConfirmationCodeHash(ptrConfirmationCodeStr, confirmationCodeLength, transactionId, hashConfirmCode))
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code Hash : %s", hashConfirmCode);
                                        }
                                        else
                                        {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to calculate Confirmation Code Hash.");
                                            lpaApiError = LPA_ERROR_FAILED_PREPARE_DOWNLOAD;
                                        }
                                    }
                                    else
                                    {
                                        // Confirmation Code not yet present
                                        if (ptrLpaEventCallback != NULL && ptrLpaEventCallback->_lpaEventRequestConfirmationCode != NULL)
                                        {
                                            // CallBack for query of Confirmation Code exist => call it

                                            LPA_REQUEST_CONFIRMATION_CODE lpaRequestConfirmationCode;
                                            lpaRequestConfirmationCode.confirmationCodeMaxBufferSize = LPA_CONFIRMATION_CODE_MAX_SIZE;
                                            memset(lpaRequestConfirmationCode.confirmationCode, 0x00, LPA_CONFIRMATION_CODE_MAX_SIZE);
                                            lpaRequestConfirmationCode.reasonCodeNoCC = LPA_CANCEL_SESSION_POSTPONED;  // In case callback does not set it when return false.

                                            bool resultCallbackCC = ptrLpaEventCallback->_lpaEventRequestConfirmationCode(ptrLpaEventCallback->_appParameter, &lpaRequestConfirmationCode);
                                            if (resultCallbackCC)
                                            {
                                                if (strlen(lpaRequestConfirmationCode.confirmationCode) > 0)
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code recovered from callback: %s", lpaRequestConfirmationCode.confirmationCode);
                                                    
                                                    // Generate Confirmation Code hash
                                                    if (_calculateConfirmationCodeHash(lpaRequestConfirmationCode.confirmationCode, strlen(lpaRequestConfirmationCode.confirmationCode), transactionId, hashConfirmCode))
                                                    {
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code Hash: %s", hashConfirmCode);
                                                    }
                                                    else
                                                    {
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to calculate Confirmation Code Hash");
                                                        lpaApiError = LPA_ERROR_FAILED_PREPARE_DOWNLOAD;
                                                        lpaSetErrorCode(lpaApiError);
                                                    }
                                                }
                                                else
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Empty Confirmation Code returned. Canceling session...");
                                                    _sendEventCallbackProgressText(ptrLpaEventCallback, 4, "Empty Confirmation Code returned. Canceling session...");
                                                    
                                                    lpaApiError = LPA_ERROR_CONFIRMATION_CODE_MISSING_OR_EMPTY;
                                                    lpaSetErrorCode(lpaApiError);	// Set LPA ErrorCode before doing lpaManagerCancelSession

                                                    // Send Cancel Session to SM-DP server, reason "Postponed" (1)
                                                    cancelResult = _lpaManagerCancelSession(transactionId, LPA_CANCEL_SESSION_POSTPONED, activationCode.smdxAddr, ptrLpaEventCallback);

                                                    // Notify application owner (Callback)
                                                    _notifyAppliOwnerCancelResult(ptrLpaEventCallback, cancelResult, 4);
                                                }
                                            }
                                            else
                                            {
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to query Confirmation Code or operation canceled from application. Canceling session with reason code %d.", lpaRequestConfirmationCode.reasonCodeNoCC);
                                                
                                                _sendEventCallbackProgressText(ptrLpaEventCallback, 4, "Failed to query Confirmation Code or operation canceled from application. Canceling session...");

                                                // Send Cancel Session to SM-DP server, reason specified by application
                                                // If reason code incorrect or other error, will use "Postponed" reason
                                                if(isElementPresentInArrayUInt(LPA_ALLOWED_CANCEL_SESSION_CODE_LIST, LPA_ALLOWED_CANCEL_SESSION_CODE_LIST_SIZE, lpaRequestConfirmationCode.reasonCodeNoCC))
                                                    cancelResult = _lpaManagerCancelSession(transactionId, lpaRequestConfirmationCode.reasonCodeNoCC, activationCode.smdxAddr, ptrLpaEventCallback);
                                                else
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "cancelSession reasonCode unknown, \"Postponed\" (0x01) will be used instead!");
                                                    cancelResult = _lpaManagerCancelSession(transactionId, LPA_CANCEL_SESSION_POSTPONED, activationCode.smdxAddr, ptrLpaEventCallback);
                                                }

                                                // Notify application owner (Callback)
                                                _notifyAppliOwnerCancelResult(ptrLpaEventCallback, cancelResult, 4);

                                                // Set error code after in case Cancel session return its own Error Code due to failed operation
                                                lpaApiError = LPA_ERROR_CONFIRMATION_CODE_MISSING_OR_EMPTY;
                                                lpaSetErrorCode(lpaApiError);
                                            }
                                        }
                                        else
                                        {
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Confirmation Code not provided. Canceling session...");
                                                _sendEventCallbackProgressText(ptrLpaEventCallback, 4, "Confirmation Code not provided. Canceling session...");
                                                
                                                lpaApiError = LPA_ERROR_CONFIRMATION_CODE_MISSING_OR_EMPTY;
                                                lpaSetErrorCode(lpaApiError);	// Set LPA ErrorCode before doing lpaManagerCancelSession

                                                // Send Cancel Session to SM-DP server, reason "Postponed" (1)
                                                cancelResult = _lpaManagerCancelSession(transactionId, LPA_CANCEL_SESSION_POSTPONED, activationCode.smdxAddr, ptrLpaEventCallback);

                                                // Notify application owner (Callback)
                                                _notifyAppliOwnerCancelResult(ptrLpaEventCallback, cancelResult, 4);
                                        }
                                    }
                                }
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code not required by server: ccRequiredFlag = Not required");
                                    // Confirmation Code not required by server so do nothing, hashConfirmCode will be empty so not added
                                    // in prepareDownload command
                                }

                                berTLV_freeBerTLV(ptrBerTLVccRequiredFlag);
                            }
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error: Server data invalid, cannot find ccRequiredFlag in smdpSigned2.");
                                lpaApiError = LPA_ERROR_FAILED_PREPARE_DOWNLOAD;
                            }

                            berTLV_freeBerTLV(ptrBerTLVsmdpSigned2Objects);
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error: Server data invalid, cannot find smdpSigned2 objects data.");
                            lpaApiError = LPA_ERROR_FAILED_PREPARE_DOWNLOAD;
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "smdpSigned2 length empty.");
                        lpaApiError = LPA_ERROR_FAILED_PREPARE_DOWNLOAD;
                    }

                    // If preparing steps OK send "prepareDownload" to eUICC
                    if (lpaApiError == LPA_NO_ERROR)
                    {
                        nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
                        
                        // Management of execution / retry loop for PrepareDownload
                        while(nbExecGetResp > 0)
                        {    
                            // If GetResponse Chaining issue occur, _lpaManagerPrepareDownload() will also return false
                            if (_lpaManagerPrepareDownload(&serverData, ptrLpaEventCallback, hashConfirmCode, &prepareDownloadResp))
                            {
                                // No chaining error detected, end execution loop
                                nbExecGetResp = 0;
                                
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PrepareDownload success.");

                                _lpaManagerFreeServerData(&serverData);
                                memset(&serverData, 0, sizeof(serverData));
                            }
                            else
                            {
                                // Decrease execution loop. If Retry not enabled, will reach 0 so no retry
                                nbExecGetResp--;

                                // Retry management
                                if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                                {
                                    // If last loop not reached, clear error code to execute another attempt
                                    // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                                    if(nbExecGetResp > 0)
                                    {
                                        lpaResetErrorCode();
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "prepareDownload: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);
                                        
                                        // Avoid memory leak because assigned by _lpaManagerPrepareDownload()
                                        ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse);
                                        ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse_Base64);
                                    }
                                    else
                                        lpaApiError = SE_MEDIA_E_CHAINING_GET_RESPONSE;    // Prevent going on next step when error state is permanent
                                }
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to prepareDownload. ");
                                    lpaApiError = LPA_ERROR_FAILED_PREPARE_DOWNLOAD;
                                    
                                    nbExecGetResp = 0; // End execution loop, another error encountered
                                }
                            }
                        }
                    }
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Server data is invalid, smdpSigned2 value = NULL. ");
                    lpaApiError = LPA_ERROR_FAILED_PREPARE_DOWNLOAD;
                }
            }
	}

	// Step 20 : Manage Bound Profile Package
	if (lpaApiError == LPA_NO_ERROR)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 20 -> Manage Bound Profile Package ...");
		if (lpaManagerGetBoundProfilePackage(&serverData, ptrLpaEventCallback, transactionId, activationCode.smdxAddr, prepareDownloadResp.ptrPrepareDownloadResponse_Base64->rawData))
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Get Bound Profile Package done.");
                        _sendEventCallbackProgressText(ptrLpaEventCallback, 5, "Get Bound Profile Package done");

			isNeedToSendPIR = true;

			//loadBoundProfilePackage
			if (lpaManagerloadBoundProfilePackage(&serverData, &pir, &cancelForBPPerrors))
			{
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Load Bound Profile Package done - Profile download successful");
				_sendEventCallbackProgressText(ptrLpaEventCallback, 6, "Load Bound Profile Package done - Profile download successful");

				ptrDownloadProfileResult->countProfileInstalled = 1;
			}
			else
			{
                            // Change error message in case Chained GetResponse issue for loadBoundProfilePackage result and PIR retrieve
                            if (LPA_RETRY_CHAINED_GET_RESPONSE_MGT && lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE)
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve final result of loadBoundProfilePackage due to Chained GetResponse issue");
                                _sendEventCallbackProgressText(ptrLpaEventCallback, 6, "Failed to retrieve final result of load Bound Profile Package");
                            }
                            else
                            {
                                // If issues have been detected in Bound Profile Package, cancel current download
                                if(cancelForBPPerrors)
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Load Bound Profile Package due to BPP error, canceling session.");
                                    _sendEventCallbackProgressText(ptrLpaEventCallback, 6, "Failed to Load Bound Profile Package due to BPP error, canceling session");

                                    cancelResult = _lpaManagerCancelSession(transactionId, LPA_CANCEL_SESSION_LOAD_BPP_EXECUTION_ERROR, activationCode.smdxAddr, ptrLpaEventCallback);
                                    // Notify application owner (Callback)
                                    _notifyAppliOwnerCancelResult(ptrLpaEventCallback, cancelResult, 6);
                                    
                                    // Due to Cancel Session, no have to send PIR
                                    isNeedToSendPIR = false;
                                }
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Load Bound Profile Package.");
                                    _sendEventCallbackProgressText(ptrLpaEventCallback, 6, "Failed to Load Bound Profile Package");
                                }

                                lpaApiError = LPA_ERROR_FAILED_LOAD_BPP;
                            }
			}
		}
		else
		{
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Get Bound Profile Package.");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 5, "Failed to Get Bound Profile Package");

                    lpaApiError = LPA_ERROR_FAILED_GET_BOUND_PROFILE_PACKAGE;
		}
	}

        // Memory cleanup
	_lpaManagerFreeServerData(&serverData);

        ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse);
        ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse_Base64);

	// Step 21 : Send PIR is needed
	if (isNeedToSendPIR)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : Step 21 -> Send PIR ...");

		//PIR management
		if (pir.hasResult && pir.ptrProfileInstallationResultTlv != NULL)
		{
			if (_sendPIRDuringDownloadProfileOperation)
			{
				// handle notification
				if (activationCode.smdxAddr != NULL && _handleNotification(activationCode.smdxAddr, strlen(activationCode.smdxAddr), pir.ptrProfileInstallationResultTlv_Base64->rawData, ptrLpaEventCallback))
				{
                                    // Report Successful overall execution if profile successfully loaded AND PIR successfully send to server)
                                    if(ptrDownloadProfileResult->countProfileInstalled == 1)
                                        res = true;
                                    
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Send PIR notification to server done");
                                    _sendEventCallbackProgressText(ptrLpaEventCallback, 7, "Send PIR notification to server done");

                                    //get seq number for remove notification
                                    uint16_t seqNumber = 0;
                                    if (extractSeqNumbFromPIR(&pir, &seqNumber))
                                    {
                                            //success to get the seq number
                                            if (lpaManagerClearProfileNotification(seqNumber))
                                            {
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Success to clear the PIR notification");
                                                _sendEventCallbackProgressText(ptrLpaEventCallback, 8, "Clear PIR notification done");
                                            }
                                            else
                                            {
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Clear PIR notification");
                                                _sendEventCallbackProgressText(ptrLpaEventCallback, 8, "Failed to Clear PIR notification");
                                            }
                                    }
                                    else
                                    {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get the PIR notification sequence number");
                                            _sendEventCallbackProgressText(ptrLpaEventCallback, 8, "Failed to get PIR notification sequence number");
                                    }
				}
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Send PIR notification to server");
                                        _sendEventCallbackProgressText(ptrLpaEventCallback, 7, "Failed to Send PIR notification to server");
                                        
                                        lpaSetErrorCode(LPA_ERROR_FAILED_SEND_NOTIFICATION_OR_NOT_GET_STATUS_CODE);
				}
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "PIR notification sending feature deactivated");
                                _sendEventCallbackProgressText(ptrLpaEventCallback, 7, "PIR notification sending feature deactivated");
			}
		}
		else
		{
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve PIR notification from eUICC");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 7, "Failed to retrieve PIR notification from eUICC");
                    
                    lpaSetErrorCode(LPA_ERROR_INVALID_PIR_RESPONSE);
		}
	}
     
        // Memory cleanup
        ERASE_RAWDATAOBJECT(pir.ptrProfileInstallationResultTlv);
        ERASE_RAWDATAOBJECT(pir.ptrProfileInstallationResultTlv_Base64);

	if (lpaApiError != LPA_NO_ERROR)
		lpaSetErrorCode(lpaApiError);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _manageDownloadProfile(...) return %s", (res ? "true" : "false"));

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
* Profile Download entry point, without Confirmation Code provided
* Note: Do not specify "Confirmation Code required" in Activation code, else request will be rejected.
*
* @param ptrActivationCodeStr - Activation Code, String format
* @param ptrLpaEventCallback - Event Callback, type LPA_EventCallback
* @param ptrDownloadProfileResult - Download profile result, type LPA_DOWNLOAD_PROFILE_RESULT
* @return True if profile download is successful
*/

bool lpaManagerDownloadProfile(const char * ptrActivationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
    bool res = false;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
    bool retryRequested = false;

    // Registering LPA_EVENT_EXECUTION_ERROR (if configured) for this call only
    _registerAppEventExecutionCallback(ptrLpaEventCallback);

    // ptrLpaEventCallback can be NULL, managed after
    if((ptrActivationCodeStr != NULL) && (ptrDownloadProfileResult != NULL))
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerDownloadProfile(...)");

        // Check if Confirmation Code required. If yes exits
        if (!_checkConfirmationCodeRequestInActivationCodeStr(ptrActivationCodeStr))
        {
            // Management execution / retry for GetResponse chained issue
            // Here manages issue when occurring at AuthenticateServer
            while(nbExecGetResp > 0)
            {
                res = _manageDownloadProfile(ptrActivationCodeStr, "", 0, ptrLpaEventCallback, ptrDownloadProfileResult, &retryRequested);
                if(res)
                {
                    // No error detected, end execution loop anyway
                    nbExecGetResp = 0;
                }
                else
                {
                    // Manage retry from return issued by _manageDownloadProfile()
                    if(retryRequested && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                    {
                        nbExecGetResp--;
                        // If last loop not reached, clear error code to execute another attempt
                        if(nbExecGetResp > 0)
                        {
                            lpaResetErrorCode();
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Restart another download attempt from SM-DP, nbExecGetResp=%d", nbExecGetResp);
                            _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Restart another download attempt from SM-DP server...");
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Maximum download attempts reached, do not retry anymore.");
                            _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Maximum download attempts reached, do not retry anymore");
                        }
                    }
                    else
                    {
                        // Issue not caused by GetReponse chaining error or no management, end execution loop
                        nbExecGetResp = 0;
                    }
                }
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error : Confirmation Code request in Activation Code. Not supported in this entry point.");
            lpaSetErrorCode(LPA_ERROR_DOWNLOAD_PROFILE_PARAMETER_ERROR);
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");
        lpaSetErrorCode(LPA_ERROR_DOWNLOAD_PROFILE_PARAMETER_ERROR);
    }

    // And unregistering LPA_EVENT_EXECUTION_ERROR callback
    _unregisterAppEventExecutionCallback();

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerDownloadProfile(...) return %s", (res ? "true" : "false"));

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
* Profile Download entry point, with Confirmation Code provided.
* Note: If server does not confirm use of Confirmation Code, it will be ignored.
*
* @param ptrActivationCodeStr - Activation Code, String format
* @param ptrConfirmationCodeStr - Confirmation Code, String format
* @param ptrLpaEventCallback - Event Callback, type LPA_EventCallback
* @param ptrDownloadProfileResult - Download profile result, type LPA_DOWNLOAD_PROFILE_RESULT
* @return True if profile download is successful
*/
bool lpaManagerDownloadProfileWithConfirmationCode(const char * ptrActivationCodeStr, const char * ptrConfirmationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
    bool res = false;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
    bool retryRequested = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerDownloadProfileWithConfirmationCode(...)");

    // Registering LPA_EVENT_EXECUTION_ERROR (if configured) for this call only
    _registerAppEventExecutionCallback(ptrLpaEventCallback);

    // ptrLpaEventCallback can be NULL, managed after
    if ((ptrActivationCodeStr != NULL) && (ptrConfirmationCodeStr != NULL) && (ptrDownloadProfileResult != NULL))
    {
        int confirmationCodeLength = strlen(ptrConfirmationCodeStr);

        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parameters check ...");
        if (confirmationCodeLength <= 0)
        {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error : Confirmation Code length <= 0");
                lpaSetErrorCode(LPA_ERROR_DOWNLOAD_PROFILE_PARAMETER_ERROR);
        }
        else
        {
            // Management of execution / retry for GetResponse chained issue
            // Here manages issue when occurring at AuthenticateServer
            while(nbExecGetResp > 0)
            {
                res = _manageDownloadProfile(ptrActivationCodeStr, ptrConfirmationCodeStr, confirmationCodeLength, ptrLpaEventCallback, ptrDownloadProfileResult, &retryRequested);
                if(res)
                {
                    // No error detected, end execution loop anyway
                    nbExecGetResp = 0;
                }
                else
                {
                    // Manage retry from return issued by _manageDownloadProfile()
                    if(retryRequested && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                    {
                        nbExecGetResp--;
                        // If last loop not reached, clear error code to execute another attempt
                        if(nbExecGetResp > 0)
                        {
                            lpaResetErrorCode();
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Restart another download attempt from SM-DP, nbExecGetResp=%d", nbExecGetResp);
                            _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Restart another download attempt from SM-DP server...");
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Maximum download attempts reached, do not retry anymore.");
                            _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Maximum download attempts reached, do not retry anymore");
                        }

                    }
                    else
                    {
                        // Issue not caused by GetReponse chaining error or no management, end execution loop
                        nbExecGetResp = 0;
                    }
                }
            }
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");
        lpaSetErrorCode(LPA_ERROR_DOWNLOAD_PROFILE_PARAMETER_ERROR);
    }

    // And unregistering LPA_EVENT_EXECUTION_ERROR callback
    _unregisterAppEventExecutionCallback();

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerDownloadProfileWithActivationCode(...) return %s", (res ? "true" : "false"));

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _lpaManagerInitServerData(ptr_serverData p_serverData)
{
	bool res = false;
	if (p_serverData) {
		memset(p_serverData, 0, sizeof(struct LPA_SERVER_DATA));
		res = true;
	}
	return res;
}
/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _lpaManagerFreeServerData(ptr_serverData p_serverData)
{
	if (p_serverData != NULL)
	{
		FREEIF_LPAMEMORYBLOCK(p_serverData->_transactionId.val);
		p_serverData->_transactionId.len = 0;

		FREEIF_LPAMEMORYBLOCK(p_serverData->_serverSigned1.val);
		p_serverData->_serverSigned1.len = 0;

		FREEIF_LPAMEMORYBLOCK(p_serverData->_serverSignature1.val);
		p_serverData->_serverSignature1.len = 0;

		FREEIF_LPAMEMORYBLOCK(p_serverData->_euiccCiPKIdToBeUsed.val);
		p_serverData->_euiccCiPKIdToBeUsed.len = 0;

		FREEIF_LPAMEMORYBLOCK(p_serverData->_serverCertificate.val);
		p_serverData->_serverCertificate.len = 0;

		FREEIF_LPAMEMORYBLOCK(p_serverData->_smdpSignature2.val);
		p_serverData->_smdpSignature2.len = 0;

		FREEIF_LPAMEMORYBLOCK(p_serverData->_smdpSigned2.val);
		p_serverData->_smdpSigned2.len = 0;

		FREEIF_LPAMEMORYBLOCK(p_serverData->_profileMetadata.val);
		p_serverData->_profileMetadata.len = 0;

		FREEIF_LPAMEMORYBLOCK(p_serverData->_smdpCertificate.val);
		p_serverData->_smdpCertificate.len = 0;

		FREEIF_LPAMEMORYBLOCK(p_serverData->_boundProfilePackage.val);
		p_serverData->_boundProfilePackage.len = 0;
	}

	return true;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////


/**
 * Extract useful fields from currently downloaded profile metadata. Also checks that mandatory fields are present (iccid, serviceProviderName and profileName)
 * Also initialize extra fields of LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR structure
 * @param profileMetadata - Pointer on profile metadata, bytes
 * @param profileMetadataSize - Profile metatadata size. Must be > 2 (At least tag + length)
 * @param extractProfileFields - Pointer on LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR structure who will receive extracted data
 * @return true if OK, else false if parsing failed (Metadata anomaly / missing fields detected)
 */
bool _extractFieldsFromProfileMetadata(const unsigned char * ptrProfileMetadata, const size_t profileMetadataSize, LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR * ptrExtractProfileFields)
{
    bool res = false;
    
    // Main Metadata object
    BeerTLV * berProfileMetadata = NULL;
    // Metadata main objects / fields parsing
    BerTLVList * berProfileFieldList = NULL;
    uint8_t profileFieldListCount = 0;
    BerTLVList * berTLVlistParser = NULL;
    BeerTLV * berTLVlistCurrent = NULL;
    // Profile Owner objects / fields parsing
    BerTLVList * berProfileOwnerList = NULL;
    uint8_t profileOwnerListCount = 0;
    BerTLVList* berTLVProfileOwnerCurrent = NULL;
    BeerTLV* usedProfileOwnerBerTLV = NULL;
            
    bool iccidTagDetectedValidated = false;
    bool serviceProviderNameTagDetected = false;
    bool profileNameTagDetected = false;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractFieldsFromProfileMetadata()...");
    
    if((ptrProfileMetadata != NULL) && (profileMetadataSize > 2) && (ptrExtractProfileFields))
    {
        berProfileMetadata = berTLV_extractTagUInt16(PROFILE_METADATA_TAG, (unsigned char *)ptrProfileMetadata, profileMetadataSize, NULL);
        
        if(berProfileMetadata != NULL)
        {
            // No need to process a empty list with not enough bytes to make at least one TL object
            if(berProfileMetadata->length > 1)
            {
                berProfileFieldList = berTLV_extractList(berProfileMetadata->value, berProfileMetadata->length, &profileFieldListCount);
                
                // Metadata shall contain at least 3 elements: iccid, serviceProviderName and profileName
                if(berProfileFieldList != NULL && profileFieldListCount > 2)
                {
                    res = true;
                    
                    // Extracted profile fields initialization
                    ptrExtractProfileFields->gid1Defined = false;
                    ptrExtractProfileFields->gid2Defined = false;
                    
                    // Parse Metadata main fields
                    berTLVlistParser = berProfileFieldList;
                    while (berTLVlistParser != NULL)
                    {
                        berTLVlistCurrent = berTLVlistParser->berTLV;
                        
                        if(berTLVlistCurrent != NULL)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Found profile Metadata field tag: %X", berTLVlistCurrent->tag);
                            
                            switch(berTLVlistCurrent->tag)
                            {
                                case 0x99:      // PPR definition
                                    // Recover ASN1 tag on 2 bytes and convert it to unsigned int
                                    ptrExtractProfileFields->profilePPR = (berTLVlistCurrent->value[0] << 8) + berTLVlistCurrent->value[1];
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 99 -> PPR ASN1 value: %X", ptrExtractProfileFields->profilePPR);
                                    
                                    ptrExtractProfileFields->hasPPR1 = _isPPR1definedInPPRflag(ptrExtractProfileFields->profilePPR);
                                    ptrExtractProfileFields->hasPPR2 = _isPPR2definedInPPRflag(ptrExtractProfileFields->profilePPR);
                                    
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PPR defined in profile:%s%s%s", ((ptrExtractProfileFields->hasPPR1)?" PPR1":""),
                                        ((ptrExtractProfileFields->hasPPR2)?" PPR2":""), (!(ptrExtractProfileFields->hasPPR1 || ptrExtractProfileFields->hasPPR2))?" None or PPRUC only":"");
                                    break;
                                    
                                case 0xB7:      // Profile Owner object
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile Owner tag list found, parsing it...");
                                    // Profile Owner integrate mandatory mccMnc and optional gid1 / gid2 in a list
                                    berProfileOwnerList = berTLV_extractList(berTLVlistCurrent->value, berTLVlistCurrent->length, &profileOwnerListCount);
                                    
                                    if(berProfileOwnerList != NULL && profileOwnerListCount > 0)
                                    {
                                        berTLVProfileOwnerCurrent = berProfileOwnerList;
                                        
                                        // Parse Profile Owner fields
                                        while(berTLVProfileOwnerCurrent != NULL && res)
                                        {
                                            usedProfileOwnerBerTLV = berTLVProfileOwnerCurrent->berTLV;
                                            
                                            if(usedProfileOwnerBerTLV != NULL)
                                            {
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Found Profile Owner tag: %X", usedProfileOwnerBerTLV->tag);
                                                
                                                switch(usedProfileOwnerBerTLV->tag)
                                                {
                                                    case 0x80:      // MCC / MNC (Mandatory)
                                                        if(usedProfileOwnerBerTLV->length == 3)
                                                        {
                                                            memcpy(ptrExtractProfileFields->mccMnc, usedProfileOwnerBerTLV->value, 3);
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG B7 -> 80 -> MCC/MNC value: %X%X%X", usedProfileOwnerBerTLV->value[0], 
                                                                                                        usedProfileOwnerBerTLV->value[1], usedProfileOwnerBerTLV->value[2]);
                                                        }
                                                        else
                                                        {
                                                            res = false;
                                                            lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Profile Owner list: Incorrect length for MCC/MNC!");
                                                        }
                                                        
                                                        break;
                                                    
                                                    case 0x81:      // GID1
                                                        ptrExtractProfileFields->gid1Defined = true;        // Field exists, even if length = 0 (Used for PPR conditions)
                                                        if(usedProfileOwnerBerTLV->length <= LPA_GID_MAX_SIZE)  // If empty no copy done, length = 0
                                                        {
                                                            memcpy(ptrExtractProfileFields->gid1, usedProfileOwnerBerTLV->value, usedProfileOwnerBerTLV->length);
                                                            ptrExtractProfileFields->gid1Size = usedProfileOwnerBerTLV->length;
                                                            lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, NULL, "TAG B7 -> 81 -> GID1", ptrExtractProfileFields->gid1,
                                                                                                                                        ptrExtractProfileFields->gid1Size);
                                                        }
                                                        else
                                                        {
                                                            res = false;
                                                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Profile Owner list: GID1 size bigger (%d) than maximum allowed (%d)!",
                                                                                                                        usedProfileOwnerBerTLV->length, LPA_GID_MAX_SIZE);
                                                        }    
                                                        
                                                        break;
                                                        
                                                    case 0x82:      // GID2
                                                        ptrExtractProfileFields->gid2Defined = true;        // Field exists, even if length = 0 (Used for PPR conditions)
                                                        if(usedProfileOwnerBerTLV->length <= LPA_GID_MAX_SIZE)  // If empty no copy done, length = 0
                                                        {
                                                            memcpy(ptrExtractProfileFields->gid2, usedProfileOwnerBerTLV->value, usedProfileOwnerBerTLV->length);
                                                            ptrExtractProfileFields->gid2Size = usedProfileOwnerBerTLV->length;
                                                            lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, NULL, "TAG B7 -> 82 -> GID2", ptrExtractProfileFields->gid2,
                                                                                                                                        ptrExtractProfileFields->gid2Size);
                                                        }
                                                        else
                                                        {
                                                            res = false;
                                                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Profile Owner list: GID2 size bigger (%d) than maximum allowed (%d)!",
                                                                                                                        usedProfileOwnerBerTLV->length, LPA_GID_MAX_SIZE);
                                                        }    
                                                        
                                                        break;

                                                    default:
                                                        res = false;
                                                        lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Profile Owner list: Unattended element detected!");

                                                        break;
                                                }
                                            }
                                            else
                                            {
                                                res = false;
                                                lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Profile Owner: Current BerTLV list element appear to be NULL ?");
                                            }
                                            
                                            berTLVProfileOwnerCurrent = berTLVProfileOwnerCurrent->ptrNext;
                                        }
                                        
                                        // MCC / MNC is a mandatory field of Profile Owner object, so if missing goes on error
                                        // Note: Value "000000" shall not exist (Also variable contain if not updated)
                                        if((ptrExtractProfileFields->mccMnc[0] == 0) && (ptrExtractProfileFields->mccMnc[1] == 0) && (ptrExtractProfileFields->mccMnc[2] == 0))
                                        {
                                            res = false;
                                            lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Profile Owner: MCC / MNC missing or invalid value \"000000\"!");
                                        }
                                    }
                                    else
                                    {
                                        res = false;
                                        lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Profile Metadata: Corrupted data detected in Profile Owner!");
                                    }
                                    
                                    // Memory cleanup
                                    berTLV_freeBerTLVList(berProfileOwnerList);
                                    
                                    break;
                                
                                case 0x92:      // Profile Name
                                    profileNameTagDetected = true;
                                    // Recover profile name and store it as string. If too long generate error
                                    if(berTLVlistCurrent->length <= (LPA_PROFILE_NAME_MAX_SIZE))
                                    {
                                        memcpy(ptrExtractProfileFields->profileName, berTLVlistCurrent->value, berTLVlistCurrent->length);
                                        ptrExtractProfileFields->profileName[berTLVlistCurrent->length] = 0;
                                        
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 92 -> Profile Name: %s", ptrExtractProfileFields->profileName);
                                    }
                                    else
                                    {
                                        res = false;
                                        lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Profile Metadata: Too big length found (%d) for Profile Name!", berTLVlistCurrent->length);                                        
                                    }

                                    break;
                                        
                                case 0x91:      // Service Provider Name. Just tested for Metadata integrity check
                                    serviceProviderNameTagDetected = true;
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 91 -> Service Provider Name validated. Not used here.");
                                    
                                    break;
                                
                                case 0x5A:      // ICCID. Just tested for Metadata integrity check
                                    // We can also validate length, that is fixed.
                                    if(berTLVlistCurrent->length == 0x0A)
                                    {
                                        iccidTagDetectedValidated = true;
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 5A -> ICCID validated. Not used here.");
                                    }
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "TAG 5A -> ICCID. Not used here but length invalid. Metadata invalid!");
                                    
                                    break;

                                default:
                                    // No processing for other tags
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Not used here.");
                                    break;
                            }
                        }
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Profile Metadata: Current BerTLV list element appear to be NULL ? Go to next one...");
                        
                        berTLVlistParser = berTLVlistParser->ptrNext;
                    }
                    
                    // Check if mandatory fields of profile Metadata have been all checked OK
                    if(!profileNameTagDetected || !serviceProviderNameTagDetected ||!iccidTagDetectedValidated)
                    {
                        res = false;
                        lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Missing or invalid Metadata field (ICCID, Service Provider Name or Profile Name)");
                    }
                }
                else
                {
                    lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractFieldsFromProfileMetadata: Failed to extract field list or corrupted Metadata!");
                }
                
                // Memory cleanup
                berTLV_freeBerTLVList(berProfileFieldList);
            }
            else
            {
                // Empty metadata container.
                // Generate an error because object StoreMetadataRequest defining it shall at least contain: iccid, serviceProviderName and profileName
                lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractFieldsFromProfileMetadata: Metadata container is empty.");
            }
                
            
            // Memory cleanup
            berTLV_freeBerTLV(berProfileMetadata);
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractFieldsFromProfileMetadata: Profile Metadata main tag not found!");
            lpaSetErrorCode(LPA_ERROR_INVALID_PROFILE_METADATA);
        }
        
        // Initialize general purpose fields whatever is result
        ptrExtractProfileFields->userCallBackType = LPA_USR_CONSENT_DISABLED;
        ptrExtractProfileFields->performCancelSession = false;
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractFieldsFromProfileMetadata: Invalid parameter!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractFieldsFromProfileMetadata(): Return %s", (res)?"True":"False");
    
    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Set default SM-DP address in eUICC
 * @param ptrSMDPAddr SM-DP address to set, string format, length smaller than LPA_SMDP_ADDRESS_SIZE
 * @return true if address setting OK
 */

bool lpaManagerSetDefaultSMDPAddress(const char* ptrSMDPAddr)
{
	bool res = false;
        
	const int responseBufferSize = 10; // Response object contains only TLV header and a flag on one byte , so normally 6 bytes shall be enough

        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerSetDefaultSMDPAddress()...");
        
	if (ptrSMDPAddr != NULL && strlen(ptrSMDPAddr) < LPA_SMDP_ADDRESS_SIZE) // ptrSMDPAddr can contains an empty string
	{
		unsigned char *ptrResponseBuffer = lpaCoreMemoryAlloc(responseBufferSize);
		if (ptrResponseBuffer != NULL)
		{
			size_t dataBufferSize = 0;
			uint16_t sw = 0x0000;
			RawDataObject* rawDataObjectSDMDAddr_80 = NULL;
			rawDataObjectSDMDAddr_80 = berTLV_createAndBuildRawDataObject(0x80, strlen(ptrSMDPAddr), (const unsigned char*)ptrSMDPAddr);

			if (rawDataObjectSDMDAddr_80 != NULL && rawDataObjectSDMDAddr_80->rawDataSize > 0)
			{
				RawDataObject* rawDataObjectSDMDAddr = NULL;
				rawDataObjectSDMDAddr = berTLV_createAndBuildRawDataObject(0xBF3F, rawDataObjectSDMDAddr_80->rawDataSize, rawDataObjectSDMDAddr_80->rawData);
				if (rawDataObjectSDMDAddr != NULL && rawDataObjectSDMDAddr->rawDataSize > 0)
				{
					if (buildAndSendStoreDataCase4(rawDataObjectSDMDAddr, &sw, ptrResponseBuffer, responseBufferSize, &dataBufferSize))
					{
						// Check SW = 90.00 or 91.xx
						if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
						{
							if (dataBufferSize > 1)
							{
								if (ptrResponseBuffer[dataBufferSize - 1] == 0)
								{
									res = true;
								}
								else
								{
									lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to set default SMDP address");
									lpaSetErrorCode(LPA_ERROR_INVALID_SET_DEFAULT_SMDP_ADDRESS);
								}
							}
							else
							{
								lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No Raw data available !");
								lpaSetErrorCode(LPA_ERROR_INVALID_SET_DEFAULT_SMDP_ADDRESS);
							}
						}
						else
						{
							// No or Invalid SW
							lpaSetErrorCode(LPA_ERROR_INVALID_SW);
						}
					}
					else
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to set default SMDP address");
						lpaSetErrorCode(LPA_ERROR_INVALID_SET_DEFAULT_SMDP_ADDRESS);
					}
				}
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to build data object!");
					lpaSetErrorCode(LPA_ERROR_INVALID_SET_DEFAULT_SMDP_ADDRESS);
				}

				ERASE_RAWDATAOBJECT(rawDataObjectSDMDAddr);
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to build data object!");
				lpaSetErrorCode(LPA_ERROR_INVALID_SET_DEFAULT_SMDP_ADDRESS);
			}

			ERASE_RAWDATAOBJECT(rawDataObjectSDMDAddr_80);

			lpaCoreMemoryFree(ptrResponseBuffer);
			ptrResponseBuffer = NULL;
		}
		else
		{
			lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
		}
	}
	else
        {
            if (ptrSMDPAddr == NULL)
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect NULL parameter!");
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "SM-DP address too long, maximum %d characters allowed!", LPA_SMDP_ADDRESS_SIZE - 1);
            
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _getEUICCconfiguredAddresses(EUICC_CONFIGURE_ADDR* ptrEuiccAddr)
{
    bool res = false;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;    // If retry deactivated, will be performed only one time
        
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_getEUICCconfiguredAddresses()...");

    if (ptrEuiccAddr != NULL) 
    {
        size_t dataBufferSize = 0;
        uint16_t sw = 0x0000;
        RawDataObject* rawDataObjectGetAddr = NULL;
        rawDataObjectGetAddr = berTLV_createAndBuildRawDataObject(0xBF3C, 0, 0x00);
        if (rawDataObjectGetAddr != NULL && rawDataObjectGetAddr->rawDataSize > 0)
        {
            // Execution / retry management
            while(nbExecGetResp > 0)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "do buildAndSendStoreDataCase4(GetEUICCConfiguredAddress) ...");
                if (buildAndSendStoreDataCase4(rawDataObjectGetAddr, &sw, _dataBuffer, LPA_MANAGER_DATA_BUFFER_MAX_SIZE, &dataBufferSize))
                {
                    // No chaining error detected, end execution loop
                    nbExecGetResp = 0;

                    // Check SW = 90.00 or 91.xx
                    if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                    {
                        if (dataBufferSize > 0)
                        {
                            if (dataBufferSize <= LPA_GET_EUICC_BUFFER_MAX_SIZE)
                            {
                                // Copy Raw Data
                                memcpy(ptrEuiccAddr->eUICCConfiguredAddr_RawData, _dataBuffer, dataBufferSize);
                                ptrEuiccAddr->eUICCConfiguredAddr_RawDataSize = dataBufferSize;
                                res = true;
                            }
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Buffer too small for copying raw data !");
                                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                            }
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No Raw data available !");
                            lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_ADDRESS);
                        }
                    }
                    else
                    {
                        // No or Invalid SW
                        lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                    }
                }
                else
                {
                    // Decrease execution loop. If Retry not enabled, will reach 0 so no retry
                    nbExecGetResp--;

                    // Retry management
                    if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                    {
                        // If last loop not reached, clear error code to execute another attempt
                        // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                        if(nbExecGetResp > 0)
                        {
                            lpaResetErrorCode();
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_getEUICCconfiguredAddresses: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);
                        }
                    }
                    else
                    {
                        lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_ADDRESS);
                        nbExecGetResp = 0; // End execution loop, another error encountered
                    }
                }    
            }
        }
        else
            lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_ADDRESS);

        ERASE_RAWDATAOBJECT(rawDataObjectGetAddr);
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect address parameter NULL!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////
bool lpaManagerGetSMDPAddress(ADDRESS_DATA* ptrAddressData)
{
	bool res = false;
	EUICC_CONFIGURE_ADDR euiccAddr;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerGetSMDPAddress()...");
        
	if (ptrAddressData != NULL)
	{
                memset(&euiccAddr, 0, sizeof(EUICC_CONFIGURE_ADDR));
                
		if (_getEUICCconfiguredAddresses(&euiccAddr))
		{
			if (euiccAddr.eUICCConfiguredAddr_RawData != NULL && euiccAddr.eUICCConfiguredAddr_RawDataSize >0)
			{
				bool isTagFound_BF3C = false;
				BeerTLV* berTLV_BF3C = berTLV_extractTagUInt16(0xBF3C, (const unsigned char*)euiccAddr.eUICCConfiguredAddr_RawData, euiccAddr.eUICCConfiguredAddr_RawDataSize, &isTagFound_BF3C);

				if (berTLV_BF3C != NULL)
				{
					bool isTagFound_80 = false;
					BeerTLV* berTLV_80 = berTLV_extractTagUInt8(0x80, berTLV_BF3C->value, berTLV_BF3C->length, &isTagFound_80);
					if (berTLV_80 != NULL)
					{
						if (berTLV_80->length < LPA_SMDP_ADDRESS_SIZE)
						{
							memcpy(ptrAddressData->address_Data, berTLV_80->value, berTLV_80->length);
							ptrAddressData->address_DataSize = berTLV_80->length;
							res = true;
						}
						else
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Buffer too small for copying raw data !");
							lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
						}
						berTLV_freeBerTLV(berTLV_80);
						berTLV_80 = NULL;
					}
					else
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "defaultDpAddress is not present (optional)");
						ptrAddressData->address_DataSize = 0;
						res = true;
					}
				}
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get SM-DP address");
					lpaSetErrorCode(LPA_ERROR_INVALID_GET_SMDP_ADDRESS);
				}
				berTLV_freeBerTLV(berTLV_BF3C);
				berTLV_BF3C = NULL;
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid eUICC address");
			}
		}
		else
			lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_ADDRESS);
	}
	else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter!");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }

	return res;
}


/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Load profile using Default SM-DP defined in eUICC
 * @param ptrLpaEventCallback       Pointer on callback for calling layer information messages. If NULL no callback performed
 * @param ptrDownloadProfileResult  Pointer for profile download results return, LPA_DOWNLOAD_PROFILE_RESULT type.
 * @return Return "true" if profile download in eUICC is successful
 */
bool lpaManagerDownloadProfileWithDefaultSMDPAddress(const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerDownloadProfileWithDefaultSMDPAddress(...)");

    bool res = false;
    ADDRESS_DATA smdpAddr;
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
    bool retryRequested = false;

	// Registering LPA_EVENT_EXECUTION_ERROR (if configured) for this call only
	_registerAppEventExecutionCallback(ptrLpaEventCallback);

    // ptrLpaEventCallback can be NULL, managed below
    if (ptrDownloadProfileResult != NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Start to Download Profile with default SMDP Address...");

        ptrDownloadProfileResult->countProfileInstalled = 0;
        ptrDownloadProfileResult->countProfileTotal = 0;
        
        memset(&smdpAddr, 0, sizeof(ADDRESS_DATA));
		
        // If failed due to unsuccessful Retry on getting default SMDP address, will exit with code related to Chained GetResponse issue.
        if (lpaManagerGetSMDPAddress(&smdpAddr))
        {
            if((smdpAddr.address_Data != NULL) && (smdpAddr.address_DataSize > 0))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Success to get default SM-DP + address: %s", smdpAddr.address_Data);
                _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Get default SM-DP + Address done");
                
                // Management of execution / retry loop for GetResponse chained issue
                // Here manages issue when occurring at AuthenticateServer
                while(nbExecGetResp > 0)
                {
                    // Load profile. If GetResponse chaining issue occur, will also return false
                    if(_performProfileDownloadFromDefaultAddress(ptrLpaEventCallback, smdpAddr.address_Data, smdpAddr.address_DataSize, "", ptrDownloadProfileResult, true, &retryRequested))
                    {
                        // No error detected, end execution loop anyway
                        nbExecGetResp = 0;
                        
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile loading is successful.");
                        res = true;
                    }
                    else
                    {
                        // Manage retry from return issued by _performProfileDownloadFromDefaultAddress()
                        if(retryRequested && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                        {
                            nbExecGetResp--;
                            // If last loop not reached, clear error code to execute another attempt
                            if(nbExecGetResp > 0)
                            {
                                lpaResetErrorCode();
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Restart another download attempt from SM-DP, nbExecGetResp=%d", nbExecGetResp);
                                _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Restart another download attempt from SM-DP server...");
                            }
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Maximum download attempts reached, do not retry anymore.");
                                _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Maximum download attempts reached, do not retry anymore");
                            }
                        }
                        else
                        {
                            // Issue not caused by GetReponse chaining error or no management, end execution loop
                            nbExecGetResp = 0;

                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Profile loading failed.");
                        }
                    }
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Default SM-DP Address invalid");
                lpaSetErrorCode(LPA_ERROR_INVALID_GET_SMDP_ADDRESS);
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get default SM-DP Address");
            lpaSetErrorCode(LPA_ERROR_INVALID_GET_SMDP_ADDRESS);
        }

    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

	// And unregistering LPA_EVENT_EXECUTION_ERROR callback
	_unregisterAppEventExecutionCallback();

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerDownloadProfileWithDefaultSMDPAddress(...)");

    return res;
}


/**
 * Perform profile download from an SM-DP when loading using SM-DS or default SM-DP defined in eUICC
 * @param ptrLpaEventCallback         Pointer on callback for calling layer information messages. If NULL no callback performed
 * @param pSmdpAddr                   Address of SM-DP server, string format
 * @param pSmdpAddrSize               SM-DP address length
 * @param pEventID                    EventID for SM-DS operations. Must be an empty string "" for SM-DP operations.
 * @param ptrDownloadProfileResult    Pointer for profile download results return, LPA_DOWNLOAD_PROFILE_RESULT type.
 * @param requestFromDefaultSMDPload  Boolean flag, must me set to "true" for SM-DP operation, "false" for SM-DS operations
 * @param retryRequested              Pointer to boolean flag, if returned "true" means to caller that it is requested to retry request due to error (GetResponse chaining...)
 * @return Return "true" if profile download in eUICC is successful, including successful sending of PIR to server
 */
bool _performProfileDownloadFromDefaultAddress(const LPA_EventCallback* ptrLpaEventCallback, const char* pSmdpAddr, const size_t pSmdpAddrSize, 
        const char* pEventID, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult, const bool requestFromDefaultSMDPload, bool * retryRequested)
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _performProfileDownloadFromDefaultAddress(...)");

    bool res = false;
    bool isProcessOK = true;
    bool cancelForBPPerrors = false;
    bool cancelResult = false;
        
    LPA_GET_EUICC getEuiccChallenge;
    LPA_GET_EUICC getEuiccInfo;
    RawDataObject * prepareCtxParam = NULL;
    LPA_SERVER_DATA serverData;
    AUTHENTICATE_SERVER_RESPONSE authentServerResp;
    PREPARE_DOWNLOAD_RESPONSE prepareDownloadResp;
    PROFILE_INSTALLATION_RESULT pir;
    char transactionId[LPA_TRANSACTION_ID_MAX_SIZE];
    
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
    *retryRequested = false;    // For the moment, do not request retry in case of GetResponseChaining failure

    // Initialize variable
    getEuiccChallenge.ptrEUICC = NULL;
    getEuiccChallenge.prtEUICC_Base64 = NULL;

    getEuiccInfo.ptrEUICC = NULL;
    getEuiccInfo.prtEUICC_Base64 = NULL;

    authentServerResp.ptrAuthenticateServerResponse = NULL;
    authentServerResp.ptrAuthenticateServerResponse_Base64 = NULL;

    prepareDownloadResp.ptrPrepareDownloadResponse = NULL;
    prepareDownloadResp.ptrPrepareDownloadResponse_Base64 = NULL;

    pir.hasResult = false;
    pir.ptrProfileInstallationResultTlv = NULL;
    pir.ptrProfileInstallationResultTlv_Base64 = NULL;

    memset(transactionId, 0x00, sizeof(transactionId));

    // STEP 1: Parameters check. ptrLpaEventCallback can be NULL, managed below depending needs.
    if ((ptrDownloadProfileResult == NULL) || (pSmdpAddr == NULL) || (pSmdpAddrSize <= 0) || (pEventID == NULL) || ((strlen(pEventID) != 0) && requestFromDefaultSMDPload) ||
                                                                                                                        ((strlen(pEventID) <= 0) && !requestFromDefaultSMDPload))
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        isProcessOK = false;
    }

    // STEP 2: Get UiccChallenge and UiccInfo
    if(isProcessOK)
    {
        _lpaManagerInitServerData(&serverData);

        if(_lpaManagerGetEuiccChallenge(&getEuiccChallenge))
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Get UiccChallenge OK.");
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get UiccChallenge!");
            // Error code lpaSetErrorCode() has to be set by _lpaManagerGetEuiccChallenge()
            isProcessOK = false;
        }
        
        if(isProcessOK)
        {
            nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
            
            // Manage execution / retry loop
            while(nbExecGetResp > 0)
            {
                // If GetResponse Chaining issue occur, _lpaManagerGetEuiccInfo() will return false
                if(_lpaManagerGetEuiccInfo(&getEuiccInfo))
                {
                    // No chaining error detected, end execution loop
                    nbExecGetResp = 0;

                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Get UiccInfo OK.");
                }
                else
                {
                    // Decrease execution loop. If Retry not enabled, will reach 0 so no retry
                    nbExecGetResp--;

                    // Retry management
                    if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                    {
                        // If last loop not reached, clear error code to execute another attempt
                        // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                        if (nbExecGetResp > 0)
                        {
                            lpaResetErrorCode();
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "GetEuiccInfo1: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);

                            // Avoid memory leak because assigned by _lpaManagerGetEuiccInfo()
                            ERASE_RAWDATAOBJECT(getEuiccInfo.ptrEUICC);
                            ERASE_RAWDATAOBJECT(getEuiccInfo.prtEUICC_Base64);
                        }
                        else
                            isProcessOK = false;    // Prevent going on next steps for nothing
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get UiccInfo!");
                        // Error code lpaSetErrorCode() has to be set by _lpaManagerGetEuiccInfo()
                        isProcessOK = false;
                        nbExecGetResp = 0; // End execution loop, another error encountered
                    }
                }
            }
        }
    }
      
    // STEP 3: Perform initiateAuthentication
    if(isProcessOK)
    {
        if (getEuiccChallenge.prtEUICC_Base64 != NULL && getEuiccInfo.prtEUICC_Base64 != NULL &&
            lpaManagerInitiateAuthentication(&serverData, ptrLpaEventCallback, (const char*)getEuiccChallenge.prtEUICC_Base64->rawData, (const char*)getEuiccInfo.prtEUICC_Base64->rawData, pSmdpAddr))
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Initiate authentication done.");
            _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "Initiate authentication done");
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Initiate authentication!");
            _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "Failed to Initiate authentication");
            
            lpaSetErrorCode(LPA_ERROR_FAILED_INITIAL_AUTHENTICATION);
            isProcessOK = false;
        }
    }
    
    // Memory cleanup
    ERASE_RAWDATAOBJECT(getEuiccChallenge.ptrEUICC);
    ERASE_RAWDATAOBJECT(getEuiccChallenge.prtEUICC_Base64);
    
    ERASE_RAWDATAOBJECT(getEuiccInfo.ptrEUICC);
    ERASE_RAWDATAOBJECT(getEuiccInfo.prtEUICC_Base64);

    // STEP 4: Check transactionID
    if(isProcessOK)
    {
        //if ((serverData._transactionId.val != NULL) && (serverData._transactionId.len > 0) && (serverData._transactionId.len < LPA_TRANSACTION_ID_MAX_SIZE))
        if ((serverData._transactionId.val == NULL) || (serverData._transactionId.len < 1) || (serverData._transactionId.len >= LPA_TRANSACTION_ID_MAX_SIZE))
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid TransactionID!");
            lpaSetErrorCode(LPA_ERROR_INVALID_TRANSACTIONID);
            isProcessOK = false;
        }
    }

    // STEP 5: Copy transactionId and check SM-DP+ Address 
    if(isProcessOK)
    {
        sprintf(transactionId, "%s", serverData._transactionId.val);
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : TransactionId : <%s>", transactionId);

        if ((serverData._serverSigned1.val != NULL) && _verifySMDPAddress(pSmdpAddr, (const char*)serverData._serverSigned1.val, serverData._serverSigned1.len))
        {
            _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "SM-DP+ address checking done");
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid SM-DP+ Address");
            _sendEventCallbackProgressText(ptrLpaEventCallback, 1, "Failed to check SM-DP+ address, invalid");
            lpaSetErrorCode(LPA_ERROR_INVALID_SERVER_ADDRESS);
            isProcessOK = false;
        }
    }
    
    // STEP 6: Check CTX param
    if(isProcessOK)
    {
        if ((strlen(_deviceInfo) > 0) && _lpaManagerPrepareCtxParam(pEventID, _deviceInfoByteArray, _deviceInfoByteArraySize, &prepareCtxParam))
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PrepareCtxParam OK");
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid CTX Params");
            lpaSetErrorCode(LPA_ERROR_INVALID_CTX_PARAM);
            isProcessOK = false;
        }
    }
    
    // STEP 7: Perform and check authenticateServer
    if(isProcessOK)
    {    
        // If GetResponse chaining error, _lpaManagerAuthenticateServer() will return false
        if (_lpaManagerAuthenticateServer(&serverData, ptrLpaEventCallback, prepareCtxParam, &authentServerResp) && authentServerResp.ptrAuthenticateServerResponse_Base64 != NULL)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Authenticate server done");
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "AuthenticateServer response: %s ", authentServerResp.ptrAuthenticateServerResponse_Base64->rawData);
            
            _sendEventCallbackProgressText(ptrLpaEventCallback, 2, "Authenticate server done");
        }
        else
        {
            // Management of GetResponse chaining issue. Command retry is not possible at this step, cancel current session and inform caller that
            // retry of entire process is requested
            if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
            {
                *retryRequested = true;
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "AuthenticateServer: GetResponse chaining issue detected, stop current profile download.");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 2, "AuthenticateServer: GetResponse chaining issue detected, stop current profile download");
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Authenticate server!");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 2, "Failed to Authenticate server");
            }

            lpaSetErrorCode(LPA_ERROR_FAILED_AUTHENTICATE_SERVER);
            isProcessOK = false;
        }
    }
    
    // Memory cleanup
    ERASE_RAWDATAOBJECT(prepareCtxParam);

    // STEP 8: Perform and check authenticateClient
    if(isProcessOK)
    {                                  
        _lpaManagerFreeServerData(&serverData);
        memset(&serverData, 0, sizeof(serverData));

        if (lpaManagerAuthenticateClient(&serverData, ptrLpaEventCallback, transactionId, pSmdpAddr, authentServerResp.ptrAuthenticateServerResponse_Base64->rawData))
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Authenticate client done");

            _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Authenticate client done");
            
            // FOR DEFAULT SM-DP LOAD ONLY:
            // Total Profile management - At this step (Authenticate Client OK) we consider we have a profile available
            // No need to increase because we always start from 0
            if(requestFromDefaultSMDPload)
                ptrDownloadProfileResult->countProfileTotal = 1;

        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Authenticate client!");
            _sendEventCallbackProgressText(ptrLpaEventCallback, 3, "Failed to Authenticate client");
            
            lpaSetErrorCode(LPA_ERROR_FAILED_AUTHENTICATE_CLIENT); 
            isProcessOK = false;
        }
        
        ERASE_RAWDATAOBJECT(authentServerResp.ptrAuthenticateServerResponse);
        ERASE_RAWDATAOBJECT(authentServerResp.ptrAuthenticateServerResponse_Base64);
    }
    
    // Step 9: Check smdpSigned2 for prepareDownload
    if(isProcessOK)
    { 
        if (serverData._smdpSigned2.val == NULL)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Server data smdpSigned2 is invalid!");
            lpaSetErrorCode(LPA_ERROR_FAILED_PREPARE_DOWNLOAD);
            isProcessOK = false;
        }
    }
    
    // STEP 10: Perform and check prepareDownload
    if(isProcessOK)
    {
        nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
        
        // Management of execution / retry loop for PrepareDownload
        while(nbExecGetResp > 0)
        { 
            // If GetResponse Chaining issue occur, _lpaManagerPrepareDownload() will also return false
            if (!_lpaManagerPrepareDownload(&serverData, ptrLpaEventCallback, "", &prepareDownloadResp))
            {
                // Decrease execution loop. If Retry not enabled, will reach 0 so no retry
                nbExecGetResp--;

                // Retry management
                if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                {
                    // If last loop not reached, clear error code to execute another attempt
                    // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                    if(nbExecGetResp > 0)
                    {
                        lpaResetErrorCode();
                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "prepareDownload: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);

                        // Avoid memory leak because assigned by _lpaManagerPrepareDownload()
                        ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse);
                        ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse_Base64);
                    }
                    else
                        isProcessOK = false;
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to prepareDownload!");
                    lpaSetErrorCode(LPA_ERROR_FAILED_PREPARE_DOWNLOAD);
                    isProcessOK = false;

                    nbExecGetResp = 0; // End execution loop, another error encountered
                }
            }
            else
            {
                nbExecGetResp = 0; // No chaining error detected, end execution loop
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PrepareDownload success.");
            }
            
        }
    }
    
    // STEP 11: Perform and check getBoundProfilePackage
    if(isProcessOK)
    { 
        _lpaManagerFreeServerData(&serverData);
        memset(&serverData, 0, sizeof(serverData));
        
        if (prepareDownloadResp.ptrPrepareDownloadResponse_Base64 != NULL && lpaManagerGetBoundProfilePackage(&serverData, ptrLpaEventCallback, transactionId, pSmdpAddr, prepareDownloadResp.ptrPrepareDownloadResponse_Base64->rawData))
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Get Bound Profile Package OK");
            _sendEventCallbackProgressText(ptrLpaEventCallback, 5, "Get Bound Profile Package done");
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Get Bound Profile Package. ");
            _sendEventCallbackProgressText(ptrLpaEventCallback, 5, "Failed to Get Bound Profile Package");
            
            lpaSetErrorCode(LPA_ERROR_FAILED_GET_BOUND_PROFILE_PACKAGE);
            isProcessOK = false;
        }
        
        ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse);
        ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse_Base64);
    }
    
    // STEP 12: Perform and check loadBoundProfilePackage, then process PIR
    if(isProcessOK)
    {
        if (lpaManagerloadBoundProfilePackage(&serverData, &pir, &cancelForBPPerrors))
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Load Bound Profile Package done - Profile download successful");
            _sendEventCallbackProgressText(ptrLpaEventCallback, 6, "Load Bound Profile Package done - Profile download successful");

            // At this step we confirm that profile is installed, increase installation counter (INCREASE APPLY FOR SM-DS MULTIPLE PROFILES OPERATIONS)
            ptrDownloadProfileResult->countProfileInstalled++;
            
            // Report overall success of download operation, but status can be invalidated in case PIR sending is failed
            res = true;
        }
        else
        {
            // Change error message in case Chained GetResponse issue for loadBoundProfilePackage result and PIR retrieve
            if (LPA_RETRY_CHAINED_GET_RESPONSE_MGT && lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve final result of loadBoundProfilePackage due to Chained GetResponse issue");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 6, "Failed to retrieve final result of load Bound Profile Package");
            }
            else
            {
                // If issues have been detected in Bound Profile Package, cancel current download
                if(cancelForBPPerrors)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Load Bound Profile Package due to BPP error, canceling session.");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 6, "Failed to Load Bound Profile Package due to BPP error, canceling session");

                    cancelResult = _lpaManagerCancelSession(transactionId, LPA_CANCEL_SESSION_LOAD_BPP_EXECUTION_ERROR, pSmdpAddr, ptrLpaEventCallback);
                    // Notify application owner (Callback)
                    _notifyAppliOwnerCancelResult(ptrLpaEventCallback, cancelResult, 6);
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Load Bound Profile Package.");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 6, "Failed to Load Bound Profile Package");
                }

                lpaSetErrorCode(LPA_ERROR_FAILED_LOAD_BPP);
            }
        }

        //PIR management, if no Cancel Session due to BPP error initiated before
        if(! cancelForBPPerrors)
        {
            if (pir.hasResult && pir.ptrProfileInstallationResultTlv->rawData != NULL)
            {
                if (_sendPIRDuringDownloadProfileOperation)
                {
                    // handle notification
                    if (_handleNotification(pSmdpAddr, pSmdpAddrSize, pir.ptrProfileInstallationResultTlv_Base64->rawData, ptrLpaEventCallback))
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Send PIR notification to server done.");
                        _sendEventCallbackProgressText(ptrLpaEventCallback, 7, "Send PIR notification to server done");

                        //get seq number for remove notification
                        uint16_t seqNumber = 0;
                        if (extractSeqNumbFromPIR(&pir, &seqNumber))
                        {
                            //success to get the seq number
                            if (lpaManagerClearProfileNotification(seqNumber))
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Clear the PIR notification OK");
                                _sendEventCallbackProgressText(ptrLpaEventCallback, 8, "Clear PIR notification done");
                            }
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Clear PIR notification");
                                _sendEventCallbackProgressText(ptrLpaEventCallback, 8, "Failed to Clear PIR notification");
                            }
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get the PIR notification sequence number");
                            _sendEventCallbackProgressText(ptrLpaEventCallback, 8, "Failed to get PIR notification sequence number");
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Send PIR notification to server");
                        _sendEventCallbackProgressText(ptrLpaEventCallback, 7, "Failed to Send PIR notification to server");

                        // PIR sending failed, so invalidate eventual overall success
                        res = false;

                        lpaSetErrorCode(LPA_ERROR_FAILED_SEND_NOTIFICATION_OR_NOT_GET_STATUS_CODE);
                    }
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "PIR notification sending feature deactivated.");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 7, "PIR notification sending feature deactivated");
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve PIR notification from eUICC!");
                _sendEventCallbackProgressText(ptrLpaEventCallback, 7, "Failed to retrieve PIR notification from eUICC");

                // No PIR available invalidates eventual overall success
                res = false;            

                lpaSetErrorCode(LPA_ERROR_INVALID_PIR_RESPONSE);
            }
        }
    }

    // Do not display / log this message if retry requested (GetResponse Chaining issue management) or if download from default SM-DP
    // Allow to recognize more easily failed download(s) when loading multiple profiles from SM-DS Event List
    if(((! *retryRequested) && (! requestFromDefaultSMDPload)) && ((! isProcessOK) || (! res)))
    {
        // Report failed status to be sure to notify user when issue occurred during a step without application owner notification, before final Load Bound Profile Package
        // Especially not visible when SM-DS is used

        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Problem occurred during download!");
        _sendEventCallbackProgressText(ptrLpaEventCallback, 9, "Problem occurred during download");
    }

    
    // Memory cleanup
    ERASE_RAWDATAOBJECT(authentServerResp.ptrAuthenticateServerResponse);
    ERASE_RAWDATAOBJECT(authentServerResp.ptrAuthenticateServerResponse_Base64);
    
    ERASE_RAWDATAOBJECT(pir.ptrProfileInstallationResultTlv);
    ERASE_RAWDATAOBJECT(pir.ptrProfileInstallationResultTlv_Base64);

    ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse);
    ERASE_RAWDATAOBJECT(prepareDownloadResp.ptrPrepareDownloadResponse_Base64);

    _lpaManagerFreeServerData(&serverData);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _performProfileDownloadFromDefautlAddress(...)");

    return res;
}


bool _verifySMDPAddress(const char* ptrSmdpAddress, const char* ptrServerSignedTLV, size_t serverSignedTLVSize)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_verifySMDPAddress()...");
        
        if (ptrSmdpAddress != NULL && ptrServerSignedTLV != NULL && serverSignedTLVSize > 0)
	{
		if (formatBytesToHexaString((const unsigned char *)ptrServerSignedTLV, serverSignedTLVSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ptrServerSignedTLV (%d bytes) :<%s>", serverSignedTLVSize, _bufferFormatLogMessage);
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ptrServerSignedTLV (%d bytes) : ...", serverSignedTLVSize);

		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ptrSmdpAddress (%d bytes) :<%s>", strlen(ptrSmdpAddress), ptrSmdpAddress);

		char* ptrServerSmdpAddress = NULL;
		BeerTLV* berTLV_30 = NULL;
		BerTLVList* BerTLVListInsideTag30 = NULL;

		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Extracting SMDP Address from ptrServerSignedTLV ...");

		bool isTagFound_30 = false;
		berTLV_30 = berTLV_extractTagUInt16(0x30, (const unsigned char*)ptrServerSignedTLV, serverSignedTLVSize, &isTagFound_30);
		if (berTLV_30 != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TAG 0x30 Present");

			uint8_t countTLVFound = 0;
			BerTLVListInsideTag30 = berTLV_extractList(berTLV_30->value, berTLV_30->length, &countTLVFound);

			if (countTLVFound > 0 && BerTLVListInsideTag30 != NULL)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "%d tag found inside Ber TLV object", countTLVFound);

				BerTLVList* berTLVCurrentInsideSequenceTag = BerTLVListInsideTag30;
				while (berTLVCurrentInsideSequenceTag != NULL)
				{
					if (formatBytesToHexaString(berTLVCurrentInsideSequenceTag->berTLV->value, berTLVCurrentInsideSequenceTag->berTLV->length, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
						lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Tag 0x%04X (%d bytes) :<%s>", berTLVCurrentInsideSequenceTag->berTLV->tag,
						berTLVCurrentInsideSequenceTag->berTLV->length, _bufferFormatLogMessage);
					else
						lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Tag 0x%04X (%d bytes) : ...", berTLVCurrentInsideSequenceTag->berTLV->tag, berTLVCurrentInsideSequenceTag->berTLV->length);


					if (berTLVCurrentInsideSequenceTag->berTLV->tag == 0x83)
					{
						if (berTLVCurrentInsideSequenceTag->berTLV->length > 0)
						{
							// The RSP Server address as an FQDN
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, " ServerAdress item found  => extract string from rawdata");

							ptrServerSmdpAddress = lpaCoreMemoryAlloc(berTLVCurrentInsideSequenceTag->berTLV->length + 1);
							if (ptrServerSmdpAddress != NULL)
							{
								memcpy(ptrServerSmdpAddress, berTLVCurrentInsideSequenceTag->berTLV->value, berTLVCurrentInsideSequenceTag->berTLV->length);
								ptrServerSmdpAddress[berTLVCurrentInsideSequenceTag->berTLV->length] = 0x00;
								lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, " ServerAdress extracted : <%s>", ptrServerSmdpAddress);
							}
						}
					}

					berTLVCurrentInsideSequenceTag = berTLVCurrentInsideSequenceTag->ptrNext;
				}
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No tag found inside Ber TLV object !");
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "TAG 0x30 absent !");



		if (ptrServerSmdpAddress != NULL && strcmp(ptrServerSmdpAddress, ptrSmdpAddress) == 0)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SM-DP Address is valid");
			res = true;
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "SM-DP Address is invalid!");
			lpaSetErrorCode(LPA_ERROR_INVALID_SERVER_ADDRESS);
		}

		// Cleanup memory
		ERASE_BERTLV_LIST(BerTLVListInsideTag30);
		ERASE_BERTLV(berTLV_30);
		if (ptrServerSmdpAddress != NULL)
		{
			lpaCoreMemoryFree(ptrServerSmdpAddress);
			ptrServerSmdpAddress = NULL;
		}

	}
	else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s)!");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerGetSMDSAddress(ADDRESS_DATA* ptrAddressData)
{
	bool res = false;
	EUICC_CONFIGURE_ADDR euiccAddr;
		
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerGetSMDSAddress()...");
	
        if (ptrAddressData != NULL){
                memset(&euiccAddr, 0, sizeof(EUICC_CONFIGURE_ADDR));
                
                if (_getEUICCconfiguredAddresses(&euiccAddr))
		{
			if (euiccAddr.eUICCConfiguredAddr_RawData != NULL && euiccAddr.eUICCConfiguredAddr_RawDataSize >0)
			{
				bool isTagFound_BF3C = false;
				BeerTLV* berTLV_BF3C = berTLV_extractTagUInt16(0xBF3C, (const unsigned char*)euiccAddr.eUICCConfiguredAddr_RawData, euiccAddr.eUICCConfiguredAddr_RawDataSize, &isTagFound_BF3C);

				if (berTLV_BF3C != NULL)
				{
					bool isTagFound_81 = false;
					BeerTLV* berTLV_81 = berTLV_extractTagUInt8(0x81, berTLV_BF3C->value, berTLV_BF3C->length, &isTagFound_81);
					if (berTLV_81 != NULL)
					{
						if (berTLV_81->length <= LPA_SMDS_ADDRESS_SIZE)
						{
							memcpy(ptrAddressData->address_Data, berTLV_81->value, berTLV_81->length);
							ptrAddressData->address_DataSize = berTLV_81->length;
							res = true;
						}
						else
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Buffer too small for copying raw data !");
							lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
						}
						berTLV_freeBerTLV(berTLV_81);
						berTLV_81 = NULL;
					}
					else
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get Default SM-DS address (Root) address");
						lpaSetErrorCode(LPA_ERROR_INVALID_GET_SMDS_ADDRESS);
					}
				}
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get Default SM-DS address (Root) address");
					lpaSetErrorCode(LPA_ERROR_INVALID_GET_SMDS_ADDRESS);
				}
				
				berTLV_freeBerTLV(berTLV_BF3C);
				berTLV_BF3C = NULL;
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid euicc address");
				lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_ADDRESS);
			}
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to retrieve euicc address");
			lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_ADDRESS);
		}
	}
	else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter!");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }
	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////
/**
 * Load profile(s) using Default SM-DS defined in eUICC. Manage multiple profile list that could be returned by server.
 * @param ptrLpaEventCallback       Pointer on callback for calling layer information messages. If NULL no callback performed
 * @param ptrDownloadProfileResult  Pointer for profile download results return, LPA_DOWNLOAD_PROFILE_RESULT type.
 * @return Return "true" if profile download in eUICC is successful
  */
bool lpaManagerDownloadProfileWithSMDSAddress(const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerDownloadProfileWithSDMSAddress(...)");

    bool res = false;
    bool isDSprocessOK = true;
    LPA_GET_EUICC getEuiccChallenge;
    LPA_GET_EUICC getEuiccInfo;
    RawDataObject * prepareCtxParam = NULL;
    LPA_SERVER_DATA serverData;

    AUTHENTICATE_SERVER_RESPONSE ptrAuthServerResp;

    EVENT_RECORD_LIST eventRecordList;
    ADDRESS_DATA smdsAddr;

    char transactionId_DS[LPA_TRANSACTION_ID_MAX_SIZE];
    
    char callbackDisplayBuffer[100];
    
    int nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
    int nbExecGetRespAuthServDS = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE; // Need another loop because other retry processes runs inside
    bool retryRequested = false;
    
    LPA_API_ERROR firstEncounteredErrorCode = LPA_NO_ERROR; // To store first encountered profile download error code if GetResponse Retry mechanism enabled
	
    getEuiccChallenge.ptrEUICC = NULL;
    getEuiccChallenge.prtEUICC_Base64 = NULL;

    getEuiccInfo.ptrEUICC = NULL;
    getEuiccInfo.prtEUICC_Base64 = NULL;

    ptrAuthServerResp.ptrAuthenticateServerResponse = NULL;
    ptrAuthServerResp.ptrAuthenticateServerResponse_Base64 = NULL;
        
    // Registering LPA_EVENT_EXECUTION_ERROR (if configured) for this call only
    _registerAppEventExecutionCallback(ptrLpaEventCallback);

    // ptrLpaEventCallback can be NULL, managed below
    if (ptrDownloadProfileResult != NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Start to Download Profile list (Event list) with SMDS address...");
        
        // STEP 1: Data structures initialization
        memset(transactionId_DS, 0x00, sizeof(transactionId_DS));
        memset(&smdsAddr, 0, sizeof(ADDRESS_DATA));
        memset(&eventRecordList, 0x00, sizeof(EVENT_RECORD_LIST));
        
        // Install counters init
        ptrDownloadProfileResult->countProfileInstalled = 0;
        ptrDownloadProfileResult->countProfileTotal = 0;

        // STEP 1: Get default SM-DS address from eUICC
        // If failed due to unsuccessful Retry on getting default SMDS address, will exit with code related to Chained GetResponse issue.
        if(isDSprocessOK)
        {
            if (lpaManagerGetSMDSAddress(&smdsAddr))
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Get SM-DS address from eUICC OK");
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get SM-DS address from eUICC!");
                lpaSetErrorCode(LPA_ERROR_FAILED_GET_DATA_FROM_ACTIVATION_CODE);
                isDSprocessOK = false;
            }
        }
        
        // Restart point for management of Retry on GetResponse chaining issue for AuthenticateServer - SM-DS Events retrieval exucution loop
        while(nbExecGetRespAuthServDS > 0)
        {
            // STEP 2: Get and check successful retrieve of EuiccChallenge and EuiccInfo (SM-DS operations)
            if(isDSprocessOK)
            {
                _lpaManagerInitServerData(&serverData);

                if(_lpaManagerGetEuiccChallenge(&getEuiccChallenge))
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Get UiccChallenge for SM-DS Events retrieval OK");
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get UiccChallenge for SM-DS Events retrieval!");
                    lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_INFO);
                    isDSprocessOK = false;
                }

                if(isDSprocessOK)
                {
                    nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;

                    while(nbExecGetResp > 0)
                    {
                        // If GetResponse Chaining issue occur, _lpaManagerGetEuiccInfo() will return false
                        if(_lpaManagerGetEuiccInfo(&getEuiccInfo))
                        {
                            // No chaining error detected, end execution loop
                            nbExecGetResp = 0;

                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Get UiccInfo for SM-DS Events retrieval OK");
                        }
                        else
                        {
                            // Decrease execution loop. If Retry not enabled, will reach 0 so no retry
                            nbExecGetResp--;

                            // Retry management
                            if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                            {
                                // If last loop not reached, clear error code to execute another attempt
                                // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                                if(nbExecGetResp > 0)
                                {
                                    lpaResetErrorCode();
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "GetEuiccInfo1: GetResponse chaining issue detected, try another time, nbExecGetResp = %d", nbExecGetResp);

                                    // Avoid memory leak because assigned by _lpaManagerGetEuiccInfo()
                                    ERASE_RAWDATAOBJECT(getEuiccInfo.ptrEUICC);
                                    ERASE_RAWDATAOBJECT(getEuiccInfo.prtEUICC_Base64);
                                }
                                else
                                    isDSprocessOK = false;  // Last attempt reached, stop load by SM-DS
                            }
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to get UiccInfo for SM-DS Events retrieval!");
                                lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_INFO);
                                isDSprocessOK = false;
                                nbExecGetResp = 0; // End execution loop, another error encountered
                            }
                        }
                    }
                }
            }

            // STEP 3: Perform and check initiateAuthentication execution (SM-DS operations)
            if(isDSprocessOK)
            {
                if (lpaManagerInitiateAuthentication(&serverData, ptrLpaEventCallback, (const char*)getEuiccChallenge.prtEUICC_Base64->rawData, (const char*)getEuiccInfo.prtEUICC_Base64->rawData, smdsAddr.address_Data))
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Execution of initiateAuthentication for SM-DS Events retrieval OK");
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to process initiateAuthentication for SM-DS Events retrieval!");
                    lpaSetErrorCode(LPA_ERROR_FAILED_INITIAL_AUTHENTICATION);
                    isDSprocessOK = false;
                }
            }

            // Memory cleanup
            ERASE_RAWDATAOBJECT(getEuiccChallenge.ptrEUICC);
            ERASE_RAWDATAOBJECT(getEuiccChallenge.prtEUICC_Base64);

            ERASE_RAWDATAOBJECT(getEuiccInfo.ptrEUICC);
            ERASE_RAWDATAOBJECT(getEuiccInfo.prtEUICC_Base64);

            // STEP 4: Check serverSigned1, SM-DS address and transactionID then confirm successful initiateAuthentication (SM-DS operations)
            if(isDSprocessOK)
            {
                if (serverData._serverSigned1.val != NULL)
                {
                    if(_verifySMDPAddress(smdsAddr.address_Data, (const char*)serverData._serverSigned1.val, serverData._serverSigned1.len))
                    {
                        // Check and copy transactionId
                        if (serverData._transactionId.val != NULL && serverData._transactionId.len > 0 && serverData._transactionId.len < LPA_TRANSACTION_ID_MAX_SIZE)
                        {
                            sprintf(transactionId_DS, "%s", serverData._transactionId.val);
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "LPA Download : TransactionId (DS) : <%s>", transactionId_DS);
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid transactionId  (SM-DS Events retrieval)!");
                            lpaSetErrorCode(LPA_ERROR_INVALID_TRANSACTIONID);
                            isDSprocessOK = false;
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid SM-DS address (SM-DS Events retrieval)!");
                        lpaSetErrorCode(LPA_ERROR_INVALID_SERVER_ADDRESS);
                        isDSprocessOK = false;
                    }
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid serverSigned1 (SM-DS Events retrieval)!");
                    lpaSetErrorCode(LPA_ERROR_FAILED_INITIAL_AUTHENTICATION);
                    isDSprocessOK = false;
                }
            }

            // STEP 5: Perform and check prepareCtxParam (SM-DS operations)
            if(isDSprocessOK)
            {                                        
                if (strlen(_deviceInfo) > 0 && _lpaManagerPrepareCtxParam("", _deviceInfoByteArray, _deviceInfoByteArraySize, &prepareCtxParam))
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PrepareCtxParam for SM-DS Events retrieval OK");
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to prepareCtxParam for SM-DS Events retrieval!");
                    lpaSetErrorCode(LPA_ERROR_INVALID_CTX_PARAM);
                    isDSprocessOK = false;
                }
            }

            // STEP 6: Perform and check authenticateServer (SM-DS operations)
            if(isDSprocessOK)
            { 
                // If GetResponse chaining error, _lpaManagerAuthenticateServer() will return false
                if (_lpaManagerAuthenticateServer(&serverData, ptrLpaEventCallback, prepareCtxParam, &ptrAuthServerResp) && ptrAuthServerResp.ptrAuthenticateServerResponse_Base64 != NULL)
                {
                    // No error detected, end execution loop anyway
                    nbExecGetRespAuthServDS = 0;
                    
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "AuthenticateServer for SM-DS Events retrieval OK");

                    if(ptrAuthServerResp.ptrAuthenticateServerResponse_Base64->rawData != NULL )
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "AuthenticateServer for Events retrieval response: %s ", ptrAuthServerResp.ptrAuthenticateServerResponse_Base64->rawData);
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "AuthenticateServer for Events retrieval response: N/A ");
                }
                else
                {
                    // Decrease execution loop. If Retry not enabled, will reach 0 so no retry
                    nbExecGetRespAuthServDS--;

                    // Retry management
                    if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                    {
                        // If last loop not reached, clear error code to execute another attempt
                        // Else stay on error SE_MEDIA_E_CHAINING_GET_RESPONSE
                        if(nbExecGetRespAuthServDS > 0)
                        {
                            memset(transactionId_DS, 0x00, sizeof(transactionId_DS));
                            lpaResetErrorCode();
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "AuthenticateServer for Events retrieval: GetResponse chaining issue detected, try another time, nbExecGetRespAuthServDS = %d", nbExecGetRespAuthServDS);
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "AuthenticateServer for Events retrieval: GetResponse chaining issue detected, no more attempt performed");
                            isDSprocessOK = false;
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to Authenticate server for SM-DS Events retrieval!");
                        lpaSetErrorCode(LPA_ERROR_FAILED_AUTHENTICATE_SERVER);
                        isDSprocessOK = false;
                        
                        nbExecGetRespAuthServDS = 0; // End execution loop, another error encountered
                    }
                }

                _lpaManagerFreeServerData(&serverData);
                memset(&serverData, 0, sizeof(serverData));
            }
            else
                nbExecGetRespAuthServDS = 0; // If error other than GetResponse Chaining issue stop execution loop for AuthenticateServer - SM-DS Events retrieval
        }// End of execution loop for AuthenticateServer - SM-DS Events retrieval
        
        // Memory cleanup
        ERASE_RAWDATAOBJECT(prepareCtxParam);
        
        // STEP 7: Retrieve Events list (Profile list)
        if(isDSprocessOK)
        {
            if (lpaManagerES9Plus_EventRetrieval(transactionId_DS, ptrLpaEventCallback, smdsAddr.address_Data, ptrAuthServerResp.ptrAuthenticateServerResponse_Base64->rawData, &eventRecordList))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Event(s) retrieval done. SM-DS reported %lu profile(s) to download", CAST_SIZET_PLATFORM(eventRecordList.countEvent));

                // Notify application owner
                if (ptrLpaEventCallback != NULL && ptrLpaEventCallback->_lpaEventProgressText != NULL)
                {
                    sprintf(callbackDisplayBuffer, "Event(s) retrieval done. SM-DS reported %lu profile(s) to download", CAST_SIZET_PLATFORM(eventRecordList.countEvent));
                    ptrLpaEventCallback->_lpaEventProgressText(ptrLpaEventCallback->_appParameter, 0, callbackDisplayBuffer);
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve event(s) list (Authenticate client).");
                
                lpaSetErrorCode(LPA_ERROR_FAILED_AUTHENTICATE_CLIENT);
                isDSprocessOK = false;
            }
            
            ERASE_RAWDATAOBJECT(ptrAuthServerResp.ptrAuthenticateServerResponse);
            ERASE_RAWDATAOBJECT(ptrAuthServerResp.ptrAuthenticateServerResponse_Base64);
        }
        
        // STEP 8: Process Event list -> Download profile(s) in eUICC if Event list contains elements
        if(isDSprocessOK)
        {
            if (eventRecordList.countEvent > 0)
            {
                // start to download one by one
                size_t i;

                ptrDownloadProfileResult->countProfileTotal = eventRecordList.countEvent;
                // Note: Installed profiles counter ptrDownloadProfileResult->countProfileInstalled is managed by _performProfileDownloadFromDefautAddress()
                
                for (i = 0; i < eventRecordList.countEvent; i++)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "=============================================");
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Start to process the Event record #%d", i + 1);

                    // Notify application owner
                    if (ptrLpaEventCallback != NULL && ptrLpaEventCallback->_lpaEventProgressText != NULL)
                    {
                        sprintf(callbackDisplayBuffer, "====== Start to process Event record #%lu ======", CAST_SIZET_PLATFORM(i + 1));
                        ptrLpaEventCallback->_lpaEventProgressText(ptrLpaEventCallback->_appParameter, 1, callbackDisplayBuffer);
                    }

                    // Management execution / retry loop for GetResponse chained issue
                    // Here manages issue when occurring at AuthenticateServer
                    nbExecGetResp = LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE;
                    while(nbExecGetResp > 0)
                    {
                        // Load profile. If GetResponse chaining issue occur, will also return false
                        if(_performProfileDownloadFromDefaultAddress(ptrLpaEventCallback, eventRecordList.eventRecordList[i].rspServerAddress, strlen(eventRecordList.eventRecordList[i].rspServerAddress), eventRecordList.eventRecordList[i].eventId, ptrDownloadProfileResult, false, &retryRequested))
                        {
                            // No error detected, end execution loop anyway
                            nbExecGetResp = 0;
                            
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile loading is successful.");
                        }
                        else
                        {
                            // Manage retry from return issued by _performProfileDownloadFromDefaultAddress()
                            if(retryRequested && LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                            {
                                nbExecGetResp--;
                                // If last loop not reached, clear error code to execute another attempt
                                if(nbExecGetResp > 0)
                                {
                                    lpaResetErrorCode();
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Restart another download attempt from SM-DP for Event record #%d, nbExecGetResp=%d", i + 1, nbExecGetResp);

                                    // Notify application owner
                                    if (ptrLpaEventCallback != NULL && ptrLpaEventCallback->_lpaEventProgressText != NULL)
                                    {
                                        sprintf(callbackDisplayBuffer, "Restart another download attempt from SM-DP server for Event record #%lu...", CAST_SIZET_PLATFORM(i + 1));
                                        ptrLpaEventCallback->_lpaEventProgressText(ptrLpaEventCallback->_appParameter, 0, callbackDisplayBuffer);
                                    }
                                }
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Maximum download attempts reached, do not retry anymore for Event record #%d", i + 1);

                                    // Notify application owner
                                    if (ptrLpaEventCallback != NULL && ptrLpaEventCallback->_lpaEventProgressText != NULL)
                                    {
                                        sprintf(callbackDisplayBuffer, "Maximum download attempts reached, do not retry anymore for Event record #%lu", CAST_SIZET_PLATFORM(i + 1));
                                        ptrLpaEventCallback->_lpaEventProgressText(ptrLpaEventCallback->_appParameter, 0, callbackDisplayBuffer);
                                    }
                                }

                            }
                            else
                            {
                                // Issue not caused by GetReponse chaining error (AuthenticateServer) or no management, end execution loop
                                nbExecGetResp = 0;

                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Profile loading failed.");
                            }
                        }
                    }
                    
                    // Management of error code. Need to clear it at each profile download if GetResponse Retry mechanism is enabled
                    // So keep the first one encountered to deliver it at the end like previously
                    if(LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                    {
                        if(lpaGetErrorCodeNoClear() != LPA_NO_ERROR && firstEncounteredErrorCode == LPA_NO_ERROR)
                        {
                            firstEncounteredErrorCode = lpaGetErrorCodeNoClear();
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerDownloadProfileWithSDMSAddress(): First encountered Error Code while download profiles: 0x%06X", firstEncounteredErrorCode);
                        }

                        lpaResetErrorCode();
                    }
                }
                
                // Management of error code if GetResponse Retry mechanism enabled
                // If any error encountered while download profiles(s), restore it (First occurrence only)
                if(LPA_RETRY_CHAINED_GET_RESPONSE_MGT)
                {
                    if(firstEncounteredErrorCode != LPA_NO_ERROR)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerDownloadProfileWithSDMSAddress(): Restore first Error Code encountered while download profiles: 0x%06X", firstEncounteredErrorCode);
                        lpaResetErrorCode();
                        lpaSetErrorCode(firstEncounteredErrorCode);
                    }
                }

                if (ptrDownloadProfileResult->countProfileInstalled == eventRecordList.countEvent)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "==== All Events (Profiles) defined by SM-DS were successfully loaded in eUICC");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 2, "All Events (Profiles) defined by SM-DS were successfully loaded in eUICC");
                    res = true;
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "==== Some issues occurred during Events (Profiles) loading in eUICC");
                    _sendEventCallbackProgressText(ptrLpaEventCallback, 2, "Some issues occurred during Events (Profiles) loading in eUICC");

                    // No longer set error when one or more profile(s) failed
                    res = true;
                }

            }
            else
            {
                // No event record => Result OK with empty counters
                res = true;
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "=== No pending event record!");
            }
        }
        else
            _sendEventCallbackProgressText(ptrLpaEventCallback, 0, "Failed to retrieve Events list from SM-DS");
 
        ERASE_RAWDATAOBJECT(ptrAuthServerResp.ptrAuthenticateServerResponse);
        ERASE_RAWDATAOBJECT(ptrAuthServerResp.ptrAuthenticateServerResponse_Base64);
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

    _lpaManagerFreeServerData(&serverData);
    
    // And unregistering LPA_EVENT_EXECUTION_ERROR callback
    _unregisterAppEventExecutionCallback();
        
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerDownloadProfileWithSDMSAddress(...)");

    return res;
}

/////////////////////////////////////////////
// Redirection
/////////////////////////////////////////////

/**
 * Set new Nickname field in profile identified by ICCID.
 * @param ptrProfileID Profile ICCI size, raw format
 * @param profileIdSize Profile ICCID size, size_t
 * @param ptrNickname New Nickname to set, raw hex format
 * @param nickNameSize New Nickname size, size_t
 * @return true is nickname change operation successful
 */
bool lpaManagerSetNickname(const unsigned char* ptrProfileId, size_t profileIdSize, const unsigned char* ptrNickname, size_t nickNameSize)
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerSetNickname(...)");
    
    bool res = lpaManagerES10c_SetNickname(ptrProfileId, profileIdSize, ptrNickname, nickNameSize);
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerSetNickname(...)");
    
    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

void _registerAppEventExecutionCallback(const LPA_EventCallback* ptrEventCallback)
{
	if (ptrEventCallback != NULL && ptrEventCallback->_lpaEventExecutionError != NULL )
	{
		// Registering LPA_EVENT_EXECUTION_ERROR
		_appEventExecutionErrorCallback._appParameter = ptrEventCallback->_appParameter;
		_appEventExecutionErrorCallback._lpaEventExecutionError = ptrEventCallback->_lpaEventExecutionError;
	}
}

void _unregisterAppEventExecutionCallback()
{
	// Unregistering LPA_EVENT_EXECUTION_ERROR
	if (_appEventExecutionErrorCallback._lpaEventExecutionError != NULL)
	{
		_appEventExecutionErrorCallback._lpaEventExecutionError = NULL;
		_appEventExecutionErrorCallback._appParameter = NULL;
	}
}


void _lpaManagerEventExecutionErrorCallback(const void* ptrAppParameter, const LPA_EVENT_EXECUTION_ERROR_INFO* ptrEventExecutionErrorInfo)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _lpaManagerEventExecutionErrorCallback(...)");

	if (_appEventExecutionErrorCallback._lpaEventExecutionError != NULL )
	{
		// ptrAppParameter must be NULL if called internally
		if (ptrEventExecutionErrorInfo != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "send Event to appEventExecutionErrorCallback ...");
			_appEventExecutionErrorCallback._lpaEventExecutionError(_appEventExecutionErrorCallback._appParameter, ptrEventExecutionErrorInfo);
		}
	}

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _lpaManagerEventExecutionErrorCallback(...)");
}


/////////////////////////////////////////////
// Redirection
/////////////////////////////////////////////

bool lpaManagerSEMediaManagerIsInitialized()
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerSEMediaManagerIsInitialized()");
    
    return seMediaManagerIsInitialized();
}

/////////////////////////////////////////////
// Redirection
/////////////////////////////////////////////

bool lpaManagerSEMediaManagerUninitialize()
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerSEMediaManagerUninitialize()");
    
    return seMediaManagerUninitialize();
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////


bool lpaManagerSEMediaCardReset()
{
	bool reset = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerSEMediaCardReset()");

	// In first, establish SEMedia
	if (!seMediaManagerIsContextEstablished())
	{
		if (seMediaManagerEstablishContext())
		{
			if (seMediaManagerConnect(_seMediaReaderName))
				reset = seMediaManagerDisconnectWithReset();
			else
				lpaSetErrorCode(LPA_ERROR_SE_MEDIA_READER_CONNECTION);
		}
		else
			lpaSetErrorCode(LPA_ERROR_SE_MEDIA_CONTEXT_NOT_ESTABLISHED);
	}
	else
	{
		// Manage abnormal use case : context already established
		if (isISDRAppletSelected() && !unselectISDRApplet())
			lpaSetErrorCode(LPA_ERROR_SE_MEDIA_UNABLE_TO_UNSELECT_ISDR);

		// If ISDR applet is now unselected, do Disconnect
		if (!isISDRAppletSelected())
		{
			if (!seMediaManagerIsConnected() && !seMediaManagerConnect(_seMediaReaderName) )
				lpaSetErrorCode(LPA_ERROR_SE_MEDIA_READER_CONNECTION);

			if (seMediaManagerIsConnected())
				reset = seMediaManagerDisconnectWithReset();
		}
	}

	if (seMediaManagerIsContextEstablished())
	{
		if (!seMediaManagerReleaseContext())
			lpaSetErrorCode(LPA_ERROR_SE_MEDIA_READER_NOT_DISCONNECTED);
	}

	return (reset && !lpaIsError()); // Reset must be done without LPA error
}

//////////////////////////////////////////////
// Manage SEMedia connection and Select ISDR
//////////////////////////////////////////////

bool lpaManagerConnectReaderAndSelectISDR()
{
	bool res = false;

	if (seMediaManagerIsInitialized())
	{
		if (seMediaManagerIsContextEstablished())
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMediaManagerIsContextEstablished() = true -> do _lpaManagerUnselectISDRAndDisconnectReader() ...");
			lpaManagerUnselectISDRAndDisconnectReader();
		}

		if (!seMediaManagerIsContextEstablished())
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Try to established SEMedia context ...");

			if (seMediaManagerEstablishContext())
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SEMedia context established => connect reader ...");
				if (seMediaManagerConnect(_seMediaReaderName))
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Reader connected => select ISDR ...");
                        res = selectISDRApplet();
                        }
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to connect reader !");
					lpaSetErrorCode(LPA_ERROR_SE_MEDIA_READER_CONNECTION);
				}
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to establish SEMedia context !");
				lpaSetErrorCode(LPA_ERROR_SE_MEDIA_CONTEXT_NOT_ESTABLISHED);
			}
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "SEMedia context not released !");
			lpaSetErrorCode(LPA_ERROR_SE_MEDIA_CONTEXT_NOT_RELEASED);
		}
	}
	else
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "SEMedia not initialized !");
		lpaSetErrorCode(LPA_ERROR_SE_MEDIA_NOT_INITIALIZED);
	}

	return res;
}

////////////////////////////////////////////////
// Unselect ISDR and release SEMedia connection
////////////////////////////////////////////////

bool lpaManagerUnselectISDRAndDisconnectReader()
{
	bool res = false;

	if (isISDRAppletSelected())
	{
		if (!unselectISDRApplet())
			lpaSetErrorCode(LPA_ERROR_SE_MEDIA_UNABLE_TO_UNSELECT_ISDR);
	}

	if (!isISDRAppletSelected() && seMediaManagerIsConnected())
	{
		if (!seMediaManagerDisconnect())
			lpaSetErrorCode(LPA_ERROR_SE_MEDIA_READER_NOT_DISCONNECTED);
	}

	if (!seMediaManagerIsConnected() && seMediaManagerIsContextEstablished())
	{
		if (!seMediaManagerReleaseContext())
			lpaSetErrorCode(LPA_ERROR_SE_MEDIA_READER_NOT_DISCONNECTED);
	}

	if (!seMediaManagerIsContextEstablished())
		res = true;

	return res;
}

/////////////////////////////////////////////
// Redirection
/////////////////////////////////////////////

bool lpaManagerHttpMediaManagerIsInitialized()
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerHttpMediaManagerIsInitialized()");
    
    return httpMediaManagerIsInitialized();
}

/////////////////////////////////////////////
// Redirection
/////////////////////////////////////////////

bool lpaManagerHttpMediaManagerDelete()
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerHttpMediaManagerDelete()");
    
    return httpMediaManagerDelete();
}

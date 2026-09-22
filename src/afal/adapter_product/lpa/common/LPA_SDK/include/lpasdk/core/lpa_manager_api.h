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

#ifndef LPA_SDK__LPA_MANAGER_API_H
#define LPA_SDK__LPA_MANAGER_API_H

#include "lpasdk/api/lpasdk_api.h"
#include "lpasdk/lpasdk_internal_api.h"

#ifdef LPA_SDK__USING_EX_API
#include "lpasdk/api/lpasdk_ex_api.h"
#endif // LPA_SDK__USING_EX_API

#include "lpasdk/core/lpa_core.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

// PARAMETER KEY NAMES
// Parameter list itself is defined in lpa_manager.c

#define LPA_SDK_CONFIG_PARAM_READER_NAME							"readerName"
#define LPA_SDK_CONFIG_PARAM_DEVICE_INFO_TLV						"deviceInfoTlv"
#define LPA_SDK_CONFIG_PARAM_CERT_PATH								"certPath"


#define LPA_SDK_CONFIG_PARAM_SEND_PIR_DURING_DOWNLOAD_PROFILE		"sendPIRDuringDownloadProfile"
#define LPA_SDK_CONFIG_PARAM_USING_HTTPS_REQUEST					"usingHTTPSRequest"
#define LPA_SDK_CONFIG_PARAM_ACTIVATE_CURL_DEBUG_MODE				"activateCURLDebugMode"
#define LPA_SDK_CONFIG_PARAM_ADD_LE_TO_APDU_CASE_4					"addLeToApduCase4"
#define LPA_SDK_CONFIG_PARAM_LOG_LEVEL								"logLevel"
#define LPA_SDK_CONFIG_PARAM_LOG_MAX_SIZE							"logMaxSize"
#define LPA_SDK_CONFIG_PARAM_PROFILE_REFRESH_FLAG					"profileRefreshFlag"

#define LPA_SDK_CONFIG_PARAM_CURL_SSL_SSL_VERIFYPEER				"CURL_SSL_VERIFYPEER"
#define LPA_SDK_CONFIG_PARAM_CURL_SSL_SSL_VERIFYHOST				"CURL_SSL_VERIFYHOST"

#define LPA_SDK_CONFIG_PARAM_CURL_CONNECT_TIMEOUT					"CURL_CONNECT_TIMEOUT"
#define LPA_SDK_CONFIG_PARAM_CURL_TIMEOUT							"CURL_TIMEOUT"

    
UT_EXPORT_DLL bool lpaManagerApiInitialize(const char* ptrLpaFolder);

UT_EXPORT_DLL bool lpaManagerApiSetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, const void* ptrParameterValue, bool internalCall);
UT_EXPORT_DLL bool lpaManagerApiGetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, void* ptrParameterValue, size_t parameterValueMaxSize);
UT_EXPORT_DLL bool lpaManagerApiIsConfigParameterExist(const char* ptrParameterName, LPA_PARAMETER_TYPE* ptrParameterType, bool* ptrIsExist);
UT_EXPORT_DLL bool lpaManagerApiGetFullParametersList(LPA_PARAMETERS_LIST * ptrLpaParametersList);

// Reconnect SEMedia
UT_EXPORT_DLL bool lpaManagerApiSEMediaCardReset();


// Exchange with ISDR applet
UT_EXPORT_DLL bool lpaManagerApiGetReaderList(LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader);
UT_EXPORT_DLL bool lpaManagerApiGetProfilesInfo(LPA_GET_PROFILES_INFO*);
UT_EXPORT_DLL bool lpaManagerApiGetProfilesInfo_ex(LPA_GET_PROFILES_INFO*);
UT_EXPORT_DLL bool lpaManagerApiGetProfilesNumber(size_t*);
UT_EXPORT_DLL bool lpaManagerApiGetEID(LPA_GET_EID*);
UT_EXPORT_DLL bool lpaManagerApiMemoryReset(const unsigned char* memoryResetOptionParameter, const size_t memoryResetOptionSize);
UT_EXPORT_DLL bool lpaManagerApiSendPendingNotification(LPA_EventCallback* ptrLpaEventCallback, LPA_SENDING_NOTIFICATION_RESULT* ptrSendingNotificationResult);


// Manage Enable/Disable/Delete Profile
UT_EXPORT_DLL bool lpaManagerApiEnableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);
UT_EXPORT_DLL bool lpaManagerApiDisableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);
UT_EXPORT_DLL bool lpaManagerApiDeleteProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);

// Manage notification list
UT_EXPORT_DLL bool lpaManagerApiGetProfileNotificationList(LPA_PROFILE_NOTIFICATION_LIST* ptrProfileNotificationList);
UT_EXPORT_DLL bool lpaManagerApiClearProfileNotification(uint16_t sequenceNumber);

UT_EXPORT_DLL bool lpaManagerApiSetDefaultSMDPAddress(const char* ptrSMDPAddr);
UT_EXPORT_DLL bool lpaManagerApiGetSMDPAddress(ADDRESS_DATA* ptrAddressData);
UT_EXPORT_DLL bool lpaManagerApiGetSMDSAddress(ADDRESS_DATA* ptrAddressData);

UT_EXPORT_DLL bool lpaManagerApiDownloadProfile(const char * ptrActivationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);
UT_EXPORT_DLL bool lpaManagerApiDownloadProfileWithConfirmationCode(const char * ptrActivationCodeStr, const char * ptrConfirmationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);
UT_EXPORT_DLL bool lpaManagerApiDownloadProfileWithDefaultSMDPAddress(const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);
UT_EXPORT_DLL bool lpaManagerApiDownloadProfileWithSDMSAddress(const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);

UT_EXPORT_DLL bool lpaManagerApiSetNickname(const unsigned char* ptrProfileId, size_t profileIdSize, const unsigned char* ptrNickname, size_t nickNameSize);

UT_EXPORT_DLL bool lpaManagerApiSEMediaManagerIsInitialized();
UT_EXPORT_DLL bool lpaManagerApiSEMediaManagerUninitialize();
UT_EXPORT_DLL bool lpaManagerApiHttpMediaManagerIsInitialized();
UT_EXPORT_DLL bool lpaManagerApiHttpMediaManagerDelete();

#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif // LPA_SDK__LPA_MANAGER_API_H
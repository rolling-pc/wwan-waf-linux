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

#ifndef LPA_SDK__LPA_MANAGER_H
#define LPA_SDK__LPA_MANAGER_H

#include "lpasdk/api/lpasdk_api.h"
#include "lpasdk/lpasdk_internal_api.h"

#ifdef LPA_SDK__USING_EX_API
//#include "lpasdk/lpasdk_ex_api.h"
#endif // LPA_SDK__USING_EX_API

#include "lpasdk/core/lpa_core.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define MAX_LPA_MANAGER_APDU_BUFFER_SIZE	8192
	
bool lpaManagerInitialize(const char* ptrLpaFolder);
bool lpaManagerInitializeSEMedia();
bool lpaManagerInitializeHttpMedia();

bool lpaManagerSetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, const void* ptrParameterValue, bool internalCall);
bool lpaManagerGetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, void* ptrParameterValue, size_t parameterValueMaxSize);
bool lpaManagerIsConfigParameterExist(const char* ptrParameterName, LPA_PARAMETER_TYPE* ptrParameterType, bool* ptrIsExist, bool* ptrAccessGranted);
bool lpaManagerGetFullParametersList(LPA_PARAMETERS_LIST * ptrLpaParametersList);

// SEMedia manager redirections through lpa_manager.c
bool lpaManagerSEMediaManagerIsInitialized();
bool lpaManagerSEMediaManagerUninitialize();
// Reconnect SEMedia
bool lpaManagerSEMediaCardReset();

// httpMedia manager redirections through lpa_manager.c
bool lpaManagerHttpMediaManagerIsInitialized();
bool lpaManagerHttpMediaManagerDelete();


// Exchange with ISDR applet
bool lpaManagerGetReaderList(LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader);
bool lpaManagerGetProfilesInfo(LPA_GET_PROFILES_INFO* );
bool lpaManagerGetProfilesInfo_ex(LPA_GET_PROFILES_INFO*);
bool lpaManagerGetProfilesNumber(size_t* );
bool lpaManagerGetEID(LPA_GET_EID* );
bool lpaManagerMemoryReset(const unsigned char* memoryResetOptionParameter, const size_t memoryResetOptionSize);
bool lpaManagerSendPendingNotification(LPA_EventCallback* ptrLpaEventCallback, LPA_SENDING_NOTIFICATION_RESULT* ptrSendingNotificationResult);


// Manage Enable/Disable/Delete Profile
bool lpaManagerEnableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);
bool lpaManagerDisableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);
bool lpaManagerDeleteProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);

// Manage notification list
bool lpaManagerGetProfileNotificationList(LPA_PROFILE_NOTIFICATION_LIST* ptrProfileNotificationList);
bool lpaManagerClearProfileNotification(uint16_t sequenceNumber);

bool lpaManagerSetDefaultSMDPAddress(const char* ptrSMDPAddr);
bool lpaManagerGetSMDPAddress(ADDRESS_DATA* ptrAddressData);
bool lpaManagerGetSMDSAddress(ADDRESS_DATA* ptrAddressData);

bool lpaManagerDownloadProfile(const char * ptrActivationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);
bool lpaManagerDownloadProfileWithConfirmationCode(const char * ptrActivationCodeStr, const char * ptrConfirmationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);
bool lpaManagerDownloadProfileWithDefaultSMDPAddress(const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);
bool lpaManagerDownloadProfileWithSMDSAddress( const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);

bool lpaManagerGetProfileNotificationList(LPA_PROFILE_NOTIFICATION_LIST* ptrProfileNotificationList);
bool lpaManagerClearProfileNotification(uint16_t sequenceNumber);

// Nickname management
bool lpaManagerSetNickname(const unsigned char* ptrProfileId, size_t profileIdSize, const unsigned char* ptrNickname, size_t nickNameSize);

bool lpaManagerConnectReaderAndSelectISDR();
bool lpaManagerUnselectISDRAndDisconnectReader();

#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif // LPA_SDK__LPA_MANAGER_H
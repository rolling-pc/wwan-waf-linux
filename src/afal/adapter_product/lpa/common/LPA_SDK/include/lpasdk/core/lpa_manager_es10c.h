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

#ifndef LPA_SDK__LPA_MANAGER_ES10C_H
#define LPA_SDK__LPA_MANAGER_ES10C_H

#include "lpasdk/api/lpasdk_api.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

void lpaManagerES10c_SetRefreshFlag(bool refreshFlagActivated);
bool lpaManagerES10c_IsRefreshFlag();

bool lpaManagerES10c_GetProfilesInfo(LPA_GET_PROFILES_INFO* ptrLpaGetProfilesInfo, bool * continueRetry, bool requestForPPRmanagement);
bool lpaManagerES10c_GetProfilesInfo_ex(LPA_GET_PROFILES_INFO* ptrLpaGetProfilesInfo, bool * continueRetry, bool requestForPPRmanagement);
bool lpaManagerES10c_GetProfilesNumber(size_t*);
bool lpaManagerES10c_EnableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);
bool lpaManagerES10c_DisableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);
bool lpaManagerES10c_DeleteProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);
bool lpaManagerES10c_MemoryReset(const unsigned char* memoryResetOptionParameter, const size_t memoryResetOptionSize);
bool lpaManagerES10c_GetEID(LPA_GET_EID*);
bool lpaManagerES10c_SetNickname(const unsigned char* ptrProfileId, size_t profileIdSize, const unsigned char* ptrNickname, size_t nickNameSize);

#endif // LPA_SDK__LPA_MANAGER_ES10C_H


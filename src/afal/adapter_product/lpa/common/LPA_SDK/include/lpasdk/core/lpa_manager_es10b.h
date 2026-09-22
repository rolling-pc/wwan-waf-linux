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

#ifndef LPA_SDK__LPA_MANAGER_ES10B_H
#define LPA_SDK__LPA_MANAGER_ES10B_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lpasdk/api/lpasdk_api.h"
#include "lpasdk/lpasdk_internal_api.h"

#include "lpasdk/core/rawdata_object.h"

typedef struct
{
	RawDataObject* ptrRawDataObjectTLV_transactionId;
	RawDataObject* ptrRawDataObjectTLV_ccRequiredFlag;
	RawDataObject* ptrRawDataObjectTLV_bppEuiccOtpk;
} SMDP_SIGNED2_DATA;


bool lpaManagerES10b_PrepareDownload(ptr_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char * ptrStringHashCC, PREPARE_DOWNLOAD_RESPONSE*);
bool lpaManagerES10b_LoadBoundProfilePackage(ptr_serverData p_serverData, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors);
bool lpaManagerES10b_GetEuiccChallenge(LPA_GET_EUICC* ptrGetEUICC);
bool lpaManagerES10b_GetEuiccInfo(LPA_GET_EUICC* ptrGetEUICC);
bool lpaManagerES10b_AuthenticateServer(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, RawDataObject * ptrCtxParam, AUTHENTICATE_SERVER_RESPONSE* ptrAnthServerResp);
bool lpaManagerES10b_CancelSession(const char * transactionID, const unsigned int p_reasonCode, CANCEL_SESSION_RESPONSE * ptrCancelSessionResp);
bool lpaManagerES10b_GetRAT(RawDataObject ** ptrGetRAT);

#endif // LPA_SDK__LPA_MANAGER_ES10C_H

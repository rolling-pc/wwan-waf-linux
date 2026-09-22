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

#ifndef LPA_SDK__LPA_MANAGER_ES9_PLUS_H
#define LPA_SDK__LPA_MANAGER_ES9_PLUS_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lpasdk/api/lpasdk_api.h"
#include "lpasdk/lpasdk_internal_api.h"

bool lpaManagerES9Plus_InitiateAuthentication(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char * ptrEuiccChallenge, const char * ptrEuiccInfo1, const char* ptrSmdpAddress);
bool lpaManagerES9Plus_GetBoundProfilePackage(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrTransactionId, const char* ptrSmdpAddress, const unsigned char* ptrPrepareDownloadResponse);
bool lpaManagerES9Plus_AuthenticateClient(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrTransactionId, const char* ptrSmdpAddress, const unsigned char* ptrAuthenticateServerResponse);

bool lpaManagerES9Plus_HandleNotification(const char* ptrSmdpAddr, size_t smdpAddrSize, const unsigned char* ptrPendingNotification, const LPA_EventCallback* ptrLpaEventCallback);
bool lpaManagerES9Plus_EventRetrieval(const char* ptrTransactionId, const LPA_EventCallback* ptrLpaEventCallback, const char* ptrSmdsAddress, const unsigned char* ptrAuthenticateServerResponse, EVENT_RECORD_LIST* ptrEventRecordList);

bool lpaManagerES9plus_CancelSession(const char * transactionID, CANCEL_SESSION_RESPONSE * ptrCancelSessionResp, const char* ptrSmdpAddress, const LPA_EventCallback* ptrLpaEventCallback);

// From lpa_manager.c
char* lpaManagerES9Plus_ExecutePost(const char * ptrTargetURL, const char * ptrJsonRequest, bool* ptrIsSuccess, long *ptrHttpCode, const LPA_EventCallback* ptrLpaEventCallback);

// certPath support
void lpaManagerES9Plus_Init();
bool lpaManagerES9Plus_setCertPath(const char* ptrCertPath);
size_t lpaManagerES9Plus_getCertPathSize();
bool lpaManagerES9Plus_getCertPath(char*ptrCertPath, size_t ptrCertPathMaxSize);

#endif // LPA_SDK__LPA_MANAGER_ES9_PLUS_H
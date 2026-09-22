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

#ifndef LPA_SDK__CORE_SEMEDIA_BASE_H
#define LPA_SDK__CORE_SEMEDIA_BASE_H

#include <stdio.h>
#include <stdbool.h>

#include "lpasdk/api/lpasdk_api.h"
#include "lpasdk/api/semedia/semedia.h"


typedef struct TSEMedia
{
	// parameter part
	/////////////////////////

	void* _childStruct;

	// function part
	/////////////////////////

	bool (*seMediaEstablishContext) (const struct TSEMedia*);
	bool (*seMediaReleaseContext) (const struct TSEMedia*);
	bool (*seMediaIsValidContext) (const struct TSEMedia*);
	bool (*seMediaIsContextEstablished) (const struct TSEMedia*);

	bool (*seMediaSetCallbackEventExecutionError) (const struct TSEMedia*, LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback);

	bool (*seMediaListReader) (const struct TSEMedia*, LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader);
	bool (*seMediaConnect) (const struct TSEMedia*, const char *ptrReaderName);
	bool (*seMediaIsConnected) (const struct TSEMedia*);
	bool (*seMediaTransmitApdu) (const struct TSEMedia*, const unsigned char* ptrApduCommandBytes, size_t apduCommandSize, unsigned char* ptrApduResponseBytes, size_t* ptrApduResponseMaxSize);
	bool (*seMediaDisconnect) (const struct TSEMedia*);
	bool (*seMediaDisconnectWithReset) (const struct TSEMedia*);

	bool (*seMediaGetStatus) (const struct TSEMedia*, SE_MEDIA_CARD_STATUS* ptrStatus);

} TSEMedia;

TSEMedia* New_SEMediaBase();

#endif // LPA_SDK__CORE_SEMEDIA_BASE_H
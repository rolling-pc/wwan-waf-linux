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

#ifndef LPA_SDK__DRIVER_SEMEDIA_GENERIC_MODEM_H
#define LPA_SDK__DRIVER_SEMEDIA_GENERIC_MODEM_H

// This driver is compiled only if LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM build option exist
#ifdef LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM

#if defined(LPA_SDK__PLATFORM_WIN) || defined(LPA_SDK__PLATFORM_CYGWIN) || defined(LPA_SDK__PLATFORM_RASPBIAN)

#include <stdio.h>
//#include <windows.h>

#include "lpasdk/core/semedia_manager.h"
#include "lpasdk/core/semedia_base.h"

typedef struct
{
	// Base part
	/////////////////////////
	TSEMedia* _ptrBase;

	// Specific part
	/////////////////////////
#ifdef LPA_SDK__PLATFORM_WIN
	int _modemHandle;
#else
	int		_modemFD;
#endif // LPA_SDK__PLATFORM_WIN

	uint8_t	_apduChannel;
    char _apduChannelString[3];
	bool _contextEstablished;
} TSEMediaGenericModem;

// Simulate object constructor
TSEMedia* New_SEMediaGenericModem();

// Simulate object destructor
void Delete_SEMediaGenericModem(TSEMedia* ptrTSEMedia);

#endif //def LPA_SDK__PLATFORM_WIN || LPA_SDK__PLATFORM_CYGWIN ||LPA_SDK__PLATFORM_RASPBIAN

#endif // LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM

#endif // LPA_SDK__DRIVER_SEMEDIA_GENERIC_MODEM_H


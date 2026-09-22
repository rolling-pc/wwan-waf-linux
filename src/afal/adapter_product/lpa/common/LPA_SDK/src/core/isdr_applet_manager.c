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

#include "lpasdk/core/isdr_applet_manager.h"
#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_manager_helper.h"

static bool _isSelectedISDR = false;
static unsigned char _apduCommandSelectISDR[] = { 0x00, 0xA4, 0x04, 0x00, 0x10, 0xA0, 0x00, 0x00, 0x05, 0x59, 0x10, 0x10, 0xFF, 0xFF, 0xFF, 0xFF, 0x89, 0x00, 0x00, 0x01, 0x00 };
static unsigned char _apduResponseBytes[256]; // Max APDU response size when Selecting ISDR

bool selectISDRApplet()
{
	if (!_isSelectedISDR)
	{
		size_t apduResponseSize = 0;
		uint16_t sw = 0x00;

		if (buildAndSendApduCase4Ex(_apduCommandSelectISDR, sizeof(_apduCommandSelectISDR), &sw, _apduResponseBytes, sizeof(_apduResponseBytes), &apduResponseSize))
		{
			if (apduResponseSize >= 2)
			{
				if (sw == (uint16_t)0x9000 || ((sw & (uint16_t)0x9100) == (uint16_t)0x9100) || ((sw & (uint16_t)0x6100) == (uint16_t)0x6100) )
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "ISDR applet selected successfully");
					_isSelectedISDR = true;
				}
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect SW received !");
					lpaSetErrorCode(LPA_ERROR_SE_MEDIA_UNABLE_TO_SELECT_ISDR);
				}
			}
			else
			{
                            // SW is empty / incomplete or incorrect data (Too short to be a SW, can be data corruption or error detected at lower layer)
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect data or invalid SW or error! (Too short response < 2)");
                            lpaSetErrorCode(LPA_ERROR_SE_MEDIA_UNABLE_TO_SELECT_ISDR);
			}
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to send successfully APDU command !");
			lpaSetErrorCode(LPA_ERROR_SE_MEDIA_UNABLE_TO_SELECT_ISDR);
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "ISDR applet already selected");

	return _isSelectedISDR;
}

bool isISDRAppletSelected()
{
	return _isSelectedISDR;
}

bool unselectISDRApplet()
{
	_isSelectedISDR = false;

	return true;
}

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


#include "lpasdk/core/semedia_manager.h"
#include "lpasdk/core/lpa_log.h"
#include "lpasdk/lpasdk_internal_api.h"

#ifdef LPA_SDK__SEMEDIA_DRIVER_EXTERNAL
	#if defined(LPA_SDK__PLATFORM_WIN)
		#include "lpasdk/driver/semedia_external.h" // External (dynamic) driver
	#else
		#error "SE Media external driver only supported on WIN platform"
	#endif // LPA_SDK__PLATFORM_WIN
#elif LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM
	#include "lpasdk/driver/semedia_genericmodem.h" // Generic Modem driver
#elif LPA_SDK__SEMEDIA_DRIVER_WINSCARD
	#if defined(LPA_SDK__PLATFORM_WIN) || defined(LPA_SDK__PLATFORM_CYGWIN) || defined(LPA_SDK__PLATFORM_RASPBIAN)
		#include "lpasdk/driver/semedia_winscard.h" // SE driver for Windows (using Winscard API) or Cygwin (using also Winscard API)
	#endif // LPA_SDK__PLATFORM_WIN || LPA_SDK__PLATFORM_CYGWIN || defined(LPA_SDK__PLATFORM_RASPBIAN)
#else
	#error "No SEMedia compilation option defined"
#endif // LPA_SDK__SEMEDIA_DRIVER_EXTERNAL


static TSEMedia* _seMedia = NULL;
static bool _manageAutomatically61XX = true;
static bool _manageAutomatically6CXX = false;

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

UT_EXPORT_DLL bool seMediaManagerInitialize()
{
	bool initOk = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerInitialize()");

	if (_seMedia == NULL)
	{
#ifdef LPA_SDK__SEMEDIA_DRIVER_EXTERNAL
		_seMedia = New_SEMediaExternal();
#elif LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM
		_seMedia = New_SEMediaGenericModem();
#elif LPA_SDK__SEMEDIA_DRIVER_WINSCARD
	#if defined(LPA_SDK__PLATFORM_WIN) || defined(LPA_SDK__PLATFORM_CYGWIN) || defined(LPA_SDK__PLATFORM_RASPBIAN)
		_seMedia = New_SEMediaWinSCard();
	#endif // LPA_SDK__PLATFORM_WIN || LPA_SDK__PLATFORM_CYGWIN
#endif // LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM

		initOk = _seMedia != NULL;
	}

	return initOk;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

UT_EXPORT_DLL bool seMediaManagerSetCallbackEventExecutionError(LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerSetCallbackEventExecutionError()");

	if (_seMedia != NULL)
		res = _seMedia->seMediaSetCallbackEventExecutionError(_seMedia, lpaEventExecutionErrorCallback);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- seMediaManagerSetCallbackEventExecutionError() : return %s", (res ? "true" : "false"));
	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

UT_EXPORT_DLL bool seMediaManagerIsInitialized()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerIsInitialized()");

	return (_seMedia != NULL);
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

UT_EXPORT_DLL bool seMediaManagerUninitialize()
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerUninitialize()");

	if (_seMedia != NULL)
	{
#ifdef LPA_SDK__SEMEDIA_DRIVER_EXTERNAL
		Delete_SEMediaExternal(_seMedia);
#elif LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM
		Delete_SEMediaGenericModem(_seMedia);
#elif LPA_SDK__SEMEDIA_DRIVER_WINSCARD
	#if defined(LPA_SDK__PLATFORM_WIN) || defined(LPA_SDK__PLATFORM_CYGWIN)|| defined(LPA_SDK__PLATFORM_RASPBIAN)
		Delete_SEMediaWinSCard(_seMedia);
	#endif // LPA_SDK__PLATFORM_WIN || LPA_SDK__PLATFORM_CYGWIN
#endif // LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM		
		_seMedia = NULL;
		res = true;
	}

	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerEstablishContext()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerEstablishContext()");
	bool res = false;

	if (_seMedia != NULL)
	{
		if( _seMedia->seMediaEstablishContext != NULL  )
			res = _seMedia->seMediaEstablishContext(_seMedia);
	}
	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerReleaseContext()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerReleaseContext()");
	bool res = false;

	if (_seMedia != NULL)
	{
		if( _seMedia->seMediaReleaseContext != NULL  )
			res = _seMedia->seMediaReleaseContext(_seMedia);
	}
	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerIsContextEstablished()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerIsContextEstablished()");
	bool res = false;

	if (_seMedia != NULL)
	{
		if( _seMedia->seMediaIsContextEstablished != NULL  )
			res = _seMedia->seMediaIsContextEstablished(_seMedia);
	}
	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerIsValidContext()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerIsValidContext()");
	bool res = false;

	if (_seMedia != NULL)
	{
		if( _seMedia->seMediaIsValidContext != NULL  )
			res = _seMedia->seMediaIsValidContext(_seMedia);
	}
	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerListReader(LPA_SE_MEDIA_READER_NAME_INFO * readerNameInfoList, size_t readerNameInfoMax, size_t* countReader)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerListReader()");
	bool res = false;

	if (_seMedia != NULL && readerNameInfoList != NULL && countReader != NULL)
	{
		if( _seMedia->seMediaListReader != NULL  )
			res = _seMedia->seMediaListReader(_seMedia, readerNameInfoList, readerNameInfoMax, countReader);
	}
	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerConnect(const char *readerName)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerConnect()");
	bool res = false;

	if (_seMedia != NULL && readerName != NULL)
	{
		if( _seMedia->seMediaConnect != NULL  )
			res = _seMedia->seMediaConnect(_seMedia, readerName);
	}
	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerIsConnected()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerIsConnected()");
	bool res = false;

	if (_seMedia != NULL)
	{
		if( _seMedia->seMediaIsConnected != NULL  )
			res = _seMedia->seMediaIsConnected(_seMedia);
	}
	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerTransmitApdu(const unsigned char* apduCommandBytes, size_t apduCommandSize, unsigned char* apduResponseBytes, size_t* apduResponseMaxSize)
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerTransmitApdu()");
    bool res = false;

    if (_seMedia != NULL && apduCommandBytes != NULL && apduResponseBytes != NULL && apduResponseMaxSize != NULL)
    {
        size_t apduResponseSize = *apduResponseMaxSize;

        if (_seMedia->seMediaTransmitApdu != NULL && seMediaManagerIsConnected() )
        {
            res = _seMedia->seMediaTransmitApdu(_seMedia, apduCommandBytes, apduCommandSize, apduResponseBytes, &apduResponseSize);

            if (res)
            {
                if (apduResponseSize >= 2)
                {
                    if (apduResponseBytes[apduResponseSize - 2] == 0x61)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU R - SW is 61.XX type");

                        // Manage 61.XX
                        if (_manageAutomatically61XX)
                        {
                            bool errorManaging61XX = false;
                            bool endOfData = false;
                            size_t totalDataSize = 0;
                            size_t partialApduResponseSize = *apduResponseMaxSize;

                            unsigned char dataSizeRequested = apduResponseBytes[apduResponseSize - 1];
                            unsigned char apduGetResponse[] = { 0x00, 0xC0, 0x00, 0x00, 0x00 };

                            // Manage data available on first APDU
                            if (apduResponseSize > 2)
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Data available on first 61.XX APDU");
                                totalDataSize += apduResponseSize - 2;
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Partial data size : %zd", totalDataSize);
                            }

                            // Management of chained 61XX begin here
                            while (!errorManaging61XX && !endOfData)
                            {
                                apduGetResponse[4] = dataSizeRequested;
                                partialApduResponseSize = (*apduResponseMaxSize - totalDataSize);

                                res = _seMedia->seMediaTransmitApdu(_seMedia, apduGetResponse, sizeof(apduGetResponse), &apduResponseBytes[totalDataSize], &partialApduResponseSize);
                                if (res && partialApduResponseSize >= 2)
                                {
                                    // check if another SW=61.XX
                                    if (apduResponseBytes[totalDataSize + partialApduResponseSize - 2] == 0x61)
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU R - SW is again 61.XX type");
                                        dataSizeRequested = apduResponseBytes[totalDataSize + partialApduResponseSize - 1];
                                        totalDataSize += (partialApduResponseSize - 2); // No SW included

                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Partial data size : %ld bytes", (long) totalDataSize);
                                    }
                                    else
                                    {
                                        // Accept responses 0x90 00 or 0x91 xx
                                        if ((apduResponseBytes[totalDataSize + partialApduResponseSize - 2] == 0x90 &&
                                             apduResponseBytes[totalDataSize + partialApduResponseSize - 1] == 0x00) ||
                                            (apduResponseBytes[totalDataSize + partialApduResponseSize - 2] == 0x91))    
                                        {
                                            totalDataSize += partialApduResponseSize;
                                            apduResponseSize = totalDataSize;

                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Final data size : %ld bytes", (long) totalDataSize);
                                            endOfData = true;
                                        }
                                        else
                                        {
                                            // Detection of 0x6D00 in chained response loop, if detection is enabled
                                            if(LPA_RETRY_CHAINED_GET_RESPONSE_MGT &&
                                               (apduResponseBytes[totalDataSize + partialApduResponseSize - 2] == 0x6D) &&
                                               (apduResponseBytes[totalDataSize + partialApduResponseSize - 1] == 0x00))
                                            {
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SW 0x6D00 detected in chained GetResponse()!");
                                                errorManaging61XX = true;
                                                apduResponseSize = 0;   // With length=0 will be see as "Incorrect data / Invalid SW / error" on lpa_manager_helper layer
                                                lpaSetErrorCode(SE_MEDIA_E_CHAINING_GET_RESPONSE);
                                            }
                                            else
                                            {
                                                // Other error cases will produce an error
                                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "90.00 or 91.xx or 61.XX Status Word expected !");
                                                errorManaging61XX = true;
                                                apduResponseSize = 0;   // With length=0 will be see as "Incorrect data / Invalid SW / error" on lpa_manager_helper layer
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    errorManaging61XX = true;
                                    apduResponseSize = 0;   // With length=0 will be see as "Incorrect data / Invalid SW / error" on lpa_manager_helper layer
                                }
                            }
                        }
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Automatic GetResponse for 61.XX not activated => do nothing");
                    }
                    else
                    {
                        if (apduResponseBytes[apduResponseSize - 2] == 0x6C)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU R - SW is 6C.XX type");

                            // Manage 6C.XX
                            if (_manageAutomatically6CXX)
                            {
                                // Implement 6C.XX if needed
                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "6C.XX not yet implemented  => do nothing");
                            }
                        }
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Automatic GetResponse for 6C.XX not activated => do nothing");
                    }
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Apdu response size < 2 bytes!");
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMedia->seMediaTransmitApdu return false!");
                apduResponseSize = 0;   // With length=0 will be see as "Incorrect data / Invalid SW / error" on lpa_manager_helper layer
            }

            if (!res)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Error during transmitAPDU process => Check Status...");
                if (_seMedia->seMediaGetStatus != NULL)
                {
                    // try to get SCard Status
                    SE_MEDIA_CARD_STATUS cardStatus = SE_MEDIA_STATUS_SCARD_UNKNOWN;
                    if (_seMedia->seMediaGetStatus(_seMedia, &cardStatus))
                    {
                        if (cardStatus == SE_MEDIA_STATUS_REMOVED_CARD)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Card removed -> do disconnect !");
                            seMediaManagerDisconnect();
                        }
                    }
                }
            }
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMedia->seMediaTransmitApdu NULL or failed at seMediaManagerIsConnected()!");

        *apduResponseMaxSize = apduResponseSize;
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid NULL parameter !");

    return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerDisconnect()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerDisconnect()");
	bool res = false;

	if (_seMedia != NULL)
	{
		if( _seMedia->seMediaDisconnect != NULL  )
			res = _seMedia->seMediaDisconnect(_seMedia);
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_seMedia->seMediaDisconnect is NULL !");
	}

	return res;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

bool seMediaManagerDisconnectWithReset()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+seMediaManagerDisconnectWithReset()");
	bool res = false;

	if (_seMedia != NULL)
	{
		if (_seMedia->seMediaDisconnect != NULL)
			res = _seMedia->seMediaDisconnectWithReset(_seMedia);
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_seMedia->seMediaDisconnectWithReset is NULL !");
	}

	return res;
}

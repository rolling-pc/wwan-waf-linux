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


#include "lpasdk/api/lpasdk_api.h"
#include "lpasdk/lpasdk_internal_api.h"

#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_memory.h"

#include "lpasdk/core/lpa_manager_api.h"		// For main API
#include "lpasdk/core/lpa_manager.h"

#include "lpasdk/core/lpa_core.h"
#include "lpasdk/core/lpa_config_file.h"

// Since API 1.5, Extended API is always compiled
// check done on API entry point code to verify access right
#include "lpasdk/api/lpasdk_ex_api.h"

#include "lpasdk/lpasdk_version.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h> 

static bool _lpaInit = false;
static LPA_API_ERROR _errorCode = LPA_NO_ERROR;
static char _lpaLogFileName[LPA_MAX_PATH];
static char _lpaLogBackupFileName[LPA_MAX_PATH];
static char _lpaConfigFileName[LPA_MAX_PATH];

#define LPA_FUNCTION_NAME_MAX_SIZE	64


/**
* \fn const LPA_API_VERSION* lpaGetApiVersion()
* \brief Managing API Versionning.
*
* \return Structure that contains all API version information (Major, Minor).
*
* Do not free this structure
*/

EXPORT_DLL const LPA_API_VERSION* lpaGetApiVersion()
{
	static LPA_API_VERSION	_lpaApiVersion = { LPA_API_MAJOR_VERSION, LPA_API_MINOR_VERSION };
	return &_lpaApiVersion;
}

////////////////////////////////////////////
// Managing API function

void _lpaBeginApiFunction(const char* ptrApiFunctionName, bool resetErrorCode);
void _lpaEndApiFunction(bool displayAPIError);

static char _currentAPIFunctionName[LPA_FUNCTION_NAME_MAX_SIZE];

void _lpaBeginApiFunction(const char* ptrApiFunctionName, bool resetErrorCode)
{
	if (ptrApiFunctionName != NULL)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKAPI] <%s> started", ptrApiFunctionName);
		snprintf(_currentAPIFunctionName, LPA_FUNCTION_NAME_MAX_SIZE, "%s", ptrApiFunctionName);
	}
	else
		memset(_currentAPIFunctionName, 0x0, LPA_FUNCTION_NAME_MAX_SIZE);

	if (resetErrorCode)
		lpaResetErrorCode();
}

void _lpaEndApiFunction(bool displayAPIError)
{
	if (strlen(_currentAPIFunctionName) > 0)
	{
		if (!displayAPIError || _errorCode == LPA_NO_ERROR)
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKAPI] <%s> terminated", _currentAPIFunctionName);
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKAPI] <%s> terminated (API error: 0x%lx) ", _currentAPIFunctionName, _errorCode);
	}
	lpaCoreLogFlush();
}

/**
* \fn bool lpaInitialize(const char* ptrLpaFolder)
* \brief Initialize LPA SDK module (log module, lpaManager).
*
* \param ptrLpaFolder must contain full path of LPA as a text
*
* \return false (but GetLastErrorCode() may return LPA_NO_ERROR if called), if an error occurred, otherwise return true.
*
* This function, or lpaInitializeWithInputOutputFolder() function, must be called one time before using the LPA SDK.
* SE Media and HTTP Media components are initialized by this function.
*/

EXPORT_DLL bool lpaInitialize(const char* ptrLpaFolder)
{
    return lpaInitializeWithInputOutputFolder(ptrLpaFolder, ptrLpaFolder);
}

/**
* \fn bool lpaInitializeWithInputOutputFolder(const char* ptrLpaIntputFolder, const char* ptrLpaOutputFolder)
* \since API 1.6
* \brief Initialize LPA SDK module (log module, lpaManager).
*
* \param ptrLpaIntputFolder must contain full path, as a text, of LPA Input folder (configuration file ...) 
* \param ptrLpaOutputFolder must contain full path, as a text, of LPA Output folder (log files)
*
* \return false (but GetLastErrorCode() return LPA_NO_ERROR is called), if an error occured, otherwise return true.
*
* This function,or lpaInitialize() function, must be called one time before using the LPA SDK.
* SE Media and HTTP Media components are initialized by this function.
*/

EXPORT_DLL bool lpaInitializeWithInputOutputFolder(const char* ptrLpaIntputFolder, const char* ptrLpaOutputFolder)
{
	bool res = false;

	if (!_lpaInit)
	{
		LPA_API_ERROR lpaErrorCode = LPA_NO_ERROR;

		memset(_lpaLogFileName, 0x00, sizeof(_lpaLogFileName));
		memset(_lpaLogBackupFileName, 0x00, sizeof(_lpaLogBackupFileName));

		if (ptrLpaIntputFolder == NULL || strlen(ptrLpaIntputFolder) >= LPA_MAX_PATH)
			lpaErrorCode = LPA_ERROR_INVALID_PARAMETER;

        if (ptrLpaOutputFolder == NULL || strlen(ptrLpaOutputFolder) >= LPA_MAX_PATH)
            lpaErrorCode = LPA_ERROR_INVALID_PARAMETER;

		if (lpaErrorCode == LPA_NO_ERROR)
		{
#ifdef LPA_SDK__PLATFORM_RASPBIAN
			if (snprintf(_lpaLogFileName, sizeof(_lpaLogFileName), "%s//%s", ptrLpaOutputFolder, "./lpa.log") >= LPA_MAX_PATH ||
				snprintf(_lpaLogBackupFileName, sizeof(_lpaLogBackupFileName), "%s//%s", ptrLpaOutputFolder, "./lpa.backup.log") >= LPA_MAX_PATH)
#else
			if (snprintf( _lpaLogFileName, sizeof( _lpaLogFileName), "%s//%s", ptrLpaOutputFolder, "lpa.log") >= LPA_MAX_PATH ||
				snprintf(_lpaLogBackupFileName, sizeof(_lpaLogBackupFileName), "%s//%s", ptrLpaOutputFolder, "lpa.backup.log") >= LPA_MAX_PATH)
#endif			
				lpaErrorCode = LPA_ERROR_INSUFFICIENT_BUFFER;
		}

		if (lpaErrorCode == LPA_NO_ERROR)
		{
			// Initialize Memory Manager
			lpaCoreMemoryInitialize();

            // Check if LPA log already initialized (UT use case ...)
            bool lpaLogInitialized = lpaCoreLogIsInitialized();
            if (!lpaLogInitialized)
                lpaLogInitialized = lpaCoreLogInit();

			if (lpaLogInitialized)
			{
                if( !lpaCoreLogIsOpen())
				    lpaCoreLogOpen(_lpaLogFileName, _lpaLogBackupFileName);

				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "---==========================---");
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "--------------------------------");
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "LPA Module release %d.%d.%d.%d",
					LPA_SDK_VERSION_MAJOR, LPA_SDK_VERSION_MINOR,
					LPA_SDK_VERSION_PATCH, LPA_SDK_VERSION_BUILD);

#if LPA_SDK_VERSION_BUILD == 0
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "LPA Module build : %s", LPA_SDK_EXTRA_VERSION );
#endif // LPA_SDK_VERSION_BUILD

				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "LPA API : %d.%d", LPA_API_MAJOR_VERSION, LPA_API_MINOR_VERSION);
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Build option(s):");

#ifdef LPA_SDK__PLATFORM_CYGWIN
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__PLATFORM_CYGWIN");
#endif // LPA_SDK__PLATFORM_CYGWIN

#ifdef LPA_SDK__PLATFORM_RASPBIAN
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__PLATFORM_RASPBIAN");
#endif // LPA_SDK__PLATFORM_RASPBIAN

#ifdef LPA_SDK__PLATFORM_WIN
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__PLATFORM_WIN");
#endif // LPA_SDK__PLATFORM_WIN

#ifdef LPA_SDK__USING_EX_API
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__USING_EX_API");
#endif // LPA_SDK__USING_EX_API

#ifdef LPA_SDK__MEMORY
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__MEMORY");
#endif // LPA_SDK__USING_EX_API

#ifdef LPA_SDK__MEMORY_MONITORING
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__MEMORY_MONITORING");
#endif // LPA_SDK__MEMORY_MONITORING

#ifdef LPA_SDK__UT_MODE
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__UT_MODE");
#endif // LPA_SDK__UT_MODE

#ifdef LPA_SDK__PAMPERS
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__PAMPERS");
#endif // LPA_SDK__PAMPERS

#ifdef LPA_SDK__CURL_MEMORY
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__CURL_MEMORY");
#endif // LPA_SDK__CURL_MEMORY

#ifdef LPA_SDK__CURL_MEMORY_MONITORING
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__CURL_MEMORY_MONITORING");
#endif // LPA_SDK__CURL_MEMORY_MONITORING

#ifdef LPA_SDK__SEMEDIA_DRIVER_WINSCARD
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__SEMEDIA_DRIVER_WINSCARD");
#endif // LPA_SDK__SEMEDIA_DRIVER_WINSCARD
				
#ifdef LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM");
#endif // LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM

#ifdef LPA_SDK__SEMEDIA_DRIVER_EXTERNAL
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__SEMEDIA_DRIVER_EXTERNAL");
#endif // LPA_SDK__SEMEDIA_DRIVER_EXTERNAL

#ifdef LPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE
                                lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE - Value: %d", LPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE);
#endif // LPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE

#ifdef LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU
                                lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU - Value: %d", LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU);
#endif // LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU
                                
#ifdef LPA_SDK__LOG_MAX_SIZE
                                lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - LPA_SDK__LOG_MAX_SIZE - Value: %ld", LPA_SDK__LOG_MAX_SIZE);
#endif // LPA_SDK__LOG_MAX_SIZE

#ifdef GM_SERIAL_PORT_NAME
                                lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - GM_SERIAL_PORT_NAME - Value: %s", GM_SERIAL_PORT_NAME);
#endif // GM_SERIAL_PORT_NAME

#ifdef _DEBUG
                                lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "  - _DEBUG");
#endif // _DEBUG
                                
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "--------------------------------");
			}
			else
				lpaErrorCode = LPA_ERROR_UNABLE_TO_INIT_LOG;
		}

		if (lpaErrorCode == LPA_NO_ERROR)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "------ LPA Initialize ------");
			lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "LPAManager initialization ...");

			if (!lpaManagerApiInitialize(ptrLpaIntputFolder))
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error occured during lpaManagerInitialize() " );
				lpaErrorCode = LPA_ERROR_UNABLE_TO_INITIALIZE_LPA_MANAGER;
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "LPAManager initialization done successfully");
		}

		// Initialize SE Media component
		if (lpaErrorCode == LPA_NO_ERROR)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "---- =====================  ----");
			lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Initialize SE Media () ...");
			if(!lpaManagerInitializeSEMedia())
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error occured during SE Media initialization");
				lpaErrorCode = LPA_ERROR_INVALID_PARAMETER;
			}
		}

		if (lpaErrorCode == LPA_NO_ERROR)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "---- =====================  ----");
			lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Initialize HTTP Media () ...");

			if (!lpaManagerInitializeHttpMedia())
				lpaErrorCode = LPA_ERROR_INVALID_PARAMETER;
		}

		// Load and apply configuration file
		if (lpaErrorCode == LPA_NO_ERROR)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "---- =====================  ----");
			lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Manage Configuration file ...");

			if (snprintf(_lpaConfigFileName, sizeof(_lpaConfigFileName), "%s%s%s", ptrLpaIntputFolder, LPA_PATH_SEPARATOR, "lpa_config.ini") >= sizeof(_lpaConfigFileName))
				lpaErrorCode = LPA_ERROR_INSUFFICIENT_BUFFER;
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Loading configuration file (if present) ...");
				bool isConfigurationFilePresent = false;

				if (lpaConfigFileLoad(_lpaConfigFileName, &isConfigurationFilePresent))
					lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Configuration file loaded");
				else
				{
					if (isConfigurationFilePresent)
						lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Unable to load configuration file");
					else
						lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Configuration file not present");
				}
			}
		}

		if (lpaErrorCode == LPA_NO_ERROR)
		{
			_lpaInit = true;
			res = true;
		}
		else
			lpaSetErrorCode(lpaErrorCode);

		// At the end of init process, activate log limitation mechanism
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Activate log limitation mechanism ...");
		lpaCoreActivateLogLimitation(true);
	}

	lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "--------------------------------");
	lpaCoreLogFlush();
	return res;
}

////////////////////////////////////////////////////
//
////////////////////////////////////////////////////


/**
* \fn bool lpaUninitialize()
* \brief Uninitialize LPA SDK module.
*
* \return false (but GetLastErrorCode() return LPA_NO_ERROR is called), if an error occured, otherwise return true.
*
* After calling this function, LPA function are unvailable until calling again lpaInitialize() function.
* If SEMedia is initialized, uninitialize it.
* If log is opened, close it.
*/

EXPORT_DLL bool lpaUninitialize()
{
	bool res = false;

	if (_lpaInit)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "------ LPA Uninitialize ------");
		bool isError = false;

		if (lpaManagerApiSEMediaManagerIsInitialized())
		{
			if (!lpaManagerApiSEMediaManagerUninitialize())
				isError = true;
		}

		if (lpaManagerApiHttpMediaManagerIsInitialized())
		{
			if ( !lpaManagerApiHttpMediaManagerDelete() )
				isError = true;
		}

		// Closing Log
		lpaCoreLogClose();

		_lpaInit = false;
		
		if (!isError)
			res = true;
	}

	return res;
}

/**
* \fn bool lpaIsInitialized()
* \brief Return LPA SDK module initialization status.
*
* \return true if initialized, FALSE otherwise.
*
*/

EXPORT_DLL bool lpaIsInitialized()
{
	return _lpaInit;
}

void lpaResetErrorCode()
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaResetErrorCode()...");
    _errorCode = LPA_NO_ERROR;
}

bool lpaIsError()
{
	return _errorCode != LPA_NO_ERROR;
}

LPA_API_ERROR lpaGetErrorCodeNoClear()
{
    // Internal function that does not reset ErrorCode compared to lpaGetErrorCode()
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaGetErrorCodeNoClear(): ErrorCode=0x%06X", _errorCode);

    return _errorCode;
}

// this function is not available from application
// only LPA SDK library (LPA Manager) could use it

void lpaSetErrorCode(LPA_API_ERROR errorCode)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaSetLastErrorCode(0x%06X)", errorCode);

	// Only first error code managed
	if (_errorCode == LPA_NO_ERROR)
	{
		_errorCode = errorCode;
		const char* ptrErrorCodeDescription = NULL;

		if (_errorCode != LPA_NO_ERROR)
			ptrErrorCodeDescription = lpaGetErrorCodeDescription(_errorCode);

		if (ptrErrorCodeDescription != NULL)
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_errorCode=0x%06X => %s", _errorCode, ptrErrorCodeDescription);
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_errorCode=0x%06X", _errorCode);
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "errorCode already defined to 0x%06X (error code requested:0x%06X)", _errorCode, errorCode);
}

// this function is not available from application
// only LPA SDK library (LPA Manager) could use it

void lpaWriteErrorMessageOnLog(LPA_API_ERROR errorCode)
{
	if (LPA_NO_ERROR != errorCode)
	{
		const char* ptrErrorCodeDescription = lpaGetErrorCodeDescription(errorCode);

		if (ptrErrorCodeDescription != NULL)
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "%s (errorCode 0x%06X)", ptrErrorCodeDescription, errorCode);
	}
}

/**
* \fn LPA_API_ERROR lpaGetErrorCode()
* \brief Get LPA error.
*
* \return lPA_API_ERROR.
*
* This function is used to get LPA_API_ERROR after using a LPA function() that return a boolean type.
* Calling it reset LPA_API_ERROR to LPA_NO_ERROR.
*/

EXPORT_DLL LPA_API_ERROR lpaGetErrorCode()
{
	// Reset ErrorCode not requested when calling _lpaBeginApiFunction(), else error code will be lost
	_lpaBeginApiFunction("lpaGetErrorCode", false);

	LPA_API_ERROR errorCode = _errorCode;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "return _errorCode=0x%06X", _errorCode);
	_errorCode = LPA_NO_ERROR;

	_lpaEndApiFunction(false);
        
        
	return errorCode;
}


EXPORT_DLL bool lpaGetVersion(LPA_VERSION* ptrLpaVersion)
{
	bool res = false;
	_lpaBeginApiFunction("lpaGetVersion", true);

	if (ptrLpaVersion != NULL)
	{
		ptrLpaVersion->major = LPA_SDK_VERSION_MAJOR;
		ptrLpaVersion->minor = LPA_SDK_VERSION_MINOR;
		ptrLpaVersion->patch = LPA_SDK_VERSION_PATCH;
		ptrLpaVersion->build = LPA_SDK_VERSION_BUILD;

		res = true;
	}

	_lpaEndApiFunction(true);
	return res;
}



EXPORT_DLL bool lpaSetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, const void* ptrParameterValue)
{
	bool res = false;
	_lpaBeginApiFunction("lpaSetConfigParameter", true);

	res = lpaManagerApiSetConfigParameter(ptrParameterName, parameterType, ptrParameterValue, false);

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaGetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, void* ptrParameterValue, size_t parameterValueMaxSize)
{
	bool res = false;
	_lpaBeginApiFunction("lpaGetConfigParameter", true);

	res = lpaManagerApiGetConfigParameter(ptrParameterName, parameterType, ptrParameterValue, parameterValueMaxSize);

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaIsConfigParameterExist(const char* ptrParameterName, LPA_PARAMETER_TYPE* ptrParameterType, bool* ptrIsExist)
{
	bool res = false;
	_lpaBeginApiFunction("lpaIsConfigParameterExist", true);

	res = lpaManagerApiIsConfigParameterExist(ptrParameterName, ptrParameterType, ptrIsExist);

	_lpaEndApiFunction(true);
	return res;
}

/**
* \fn bool lpaGetReaderList(LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader)
* \brief Get list of reader available.
*
* \param ptrReaderNameInfoList pointer on LPA_SE_MEDIA_READER_NAME_INFO array
* \param readerNameInfoMax Number of LPA_SE_MEDIA_READER_NAME_INFO item
* \param ptrCountReader Number of reader detected
*
* \return true if no error detected during reader list generation otherwise return false.
*
*/

EXPORT_DLL bool lpaGetReaderList(LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader)
{
	bool res = false;
	_lpaBeginApiFunction("lpaGetReaderList", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaGetReaderList()");
	res = lpaManagerApiGetReaderList(ptrReaderNameInfoList, readerNameInfoMax, ptrCountReader);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaGetReaderList() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

/**
 * Perform ES10C GetProfilesInfo request and fill LPA_GET_PROFILES_INFO structure with profiles informations returned by eUICC
 * @param ptrLpaGetProfilesInfo Pointer on LPA_GET_PROFILES_INFO structure to fill with profiles informations
 * @return true if operation is successful
 */
EXPORT_DLL bool lpaGetProfilesInfo(LPA_GET_PROFILES_INFO* ptrGetProfilesInfo)
{
	bool res = false;
	_lpaBeginApiFunction("lpaGetProfilesInfo", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaGetProfilesInfo()");
	res = lpaManagerApiGetProfilesInfo(ptrGetProfilesInfo);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaGetProfilesInfo() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaGetProfilesInfo_ex(LPA_GET_PROFILES_INFO* ptrGetProfilesInfo)
{
	bool res = false;
	_lpaBeginApiFunction("lpaGetProfilesInfo", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaGetProfilesInfo()");
	res = lpaManagerApiGetProfilesInfo_ex(ptrGetProfilesInfo);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaGetProfilesInfo() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

/**
 * Uses ES10C GetProfilesInfo request to retrieve number of profiles available on eUICC
 * @param ptrLpaGetProfilesInfo Pointer on variable were number of profile will be returned, size_t type
 * @return true if operation is successful
 */
EXPORT_DLL bool lpaGetProfilesNumber(size_t * ptrNumberOfProfiles)
{
	bool res = false;
	_lpaBeginApiFunction("lpaGetProfilesNumber", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaGetProfilesNumber()");
	res = lpaManagerApiGetProfilesNumber(ptrNumberOfProfiles);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaGetProfilesNumber() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaGetEID(LPA_GET_EID* ptrGetEID)
{
	bool res = false;
	_lpaBeginApiFunction("lpaGetEID", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaGetEID()");
	res = lpaManagerApiGetEID(ptrGetEID);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaGetEID() : return %s", (res ? "true" : "false"));
	
	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaMemoryReset(const unsigned char* memoryResetOptionParameter, const size_t memoryResetOptionSize)
{
	bool res = false;
	_lpaBeginApiFunction("lpaMemoryReset", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaMemoryReset()");
	res = lpaManagerApiMemoryReset(memoryResetOptionParameter, memoryResetOptionSize);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaMemoryReset() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaSendPendingNotification(LPA_EventCallback* ptrLpaEventCallback, LPA_SENDING_NOTIFICATION_RESULT* ptrSendingNotificationResult)
{
	bool res = false;
	_lpaBeginApiFunction("lpaSendPendingNotification", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaSendPendingNotification()");
	res = lpaManagerApiSendPendingNotification(ptrLpaEventCallback, ptrSendingNotificationResult);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaSendPendingNotification() : return %s", (res ? "true" : "false"));
	
	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaEnableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;
	_lpaBeginApiFunction("lpaEnableProfileByIccid", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaEnableProfileByIccid()");
	res = lpaManagerApiEnableProfileByIccid(ptrProfileId, profileIdSize);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaEnableProfileByIccid() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaDisableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;
	_lpaBeginApiFunction("lpaDisableProfileByIccid", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaDisableProfileByIccid()");
	res = lpaManagerApiDisableProfileByIccid(ptrProfileId, profileIdSize);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaDisableProfileByIccid() : return %s", (res ? "true" : "false"));
	
	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaDeleteProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;
	_lpaBeginApiFunction("lpaDeleteProfileByIccid", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaDeleteProfileByIccid()");
	res = lpaManagerApiDeleteProfileByIccid(ptrProfileId, profileIdSize);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaDeleteProfileByIccid() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaDownloadProfile(const char * ptrActivationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
    bool res = false;
	_lpaBeginApiFunction("lpaDownloadProfile", true);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaDownloadProfile()");
	res = lpaManagerApiDownloadProfile(ptrActivationCodeStr, ptrLpaEventCallback, ptrDownloadProfileResult);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaDownloadProfile() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true); 
	return res;
}


EXPORT_DLL bool lpaDownloadProfileWithConfirmationCode(const char * ptrActivationCodeStr, const char * ptrConfirmationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
    bool res = false;
	_lpaBeginApiFunction("lpaDownloadProfileWithConfirmationCode", true);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaDownloadProfileWithConfirmationCode()");
	res = lpaManagerApiDownloadProfileWithConfirmationCode(ptrActivationCodeStr, ptrConfirmationCodeStr, ptrLpaEventCallback, ptrDownloadProfileResult);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaDownloadProfileWithConfirmationCode() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true); 
	return res;
}


EXPORT_DLL bool lpaDownloadProfileWithDefaultSMDPAddress(const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
    bool res = false;
	_lpaBeginApiFunction("lpaDownloadProfileWithDefaultSMDPAddress", true);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaDownloadProfileWithDefaultSMDPAddress()");
	res = lpaManagerApiDownloadProfileWithDefaultSMDPAddress(ptrLpaEventCallback, ptrDownloadProfileResult);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaDownloadProfileWithDefaultSMDPAddress() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true); 
	return res;
}


EXPORT_DLL bool lpaDownloadProfileWithSMDSAddress( const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
    bool res = false;
	_lpaBeginApiFunction("lpaDownloadProfileWithSMDSAddress", true);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaDownloadProfileWithSMDSAddress()");
	res = lpaManagerApiDownloadProfileWithSDMSAddress( ptrLpaEventCallback, ptrDownloadProfileResult);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaDownloadProfileWithSMDSAddress() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true); 
	return res;
}



EXPORT_DLL bool lpaSetDefaultSMDPAddress(const char* ptrSMDPAddr)
{
    bool res = false;
	_lpaBeginApiFunction("lpaSetDefaultSMDPAddress", true);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaSetDefaultSMDPAddress()");
    res = lpaManagerApiSetDefaultSMDPAddress(ptrSMDPAddr);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaSetDefaultSMDPAddress() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true); 
	return res;
}


EXPORT_DLL bool lpaGetSMDPAddress(ADDRESS_DATA* ptrAddressData)
{
    bool res = false;
	_lpaBeginApiFunction("lpaGetSMDPAddress", true);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaGetSMDPAddress()");
    res = lpaManagerApiGetSMDPAddress(ptrAddressData);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaGetSMDPAddress() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true); 
	return res;
}


EXPORT_DLL bool lpaGetSMDSAddress(ADDRESS_DATA* ptrAddressData)
{
    bool res = false;
	_lpaBeginApiFunction("lpaGetSMDSAddress", true);

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaGetSMDSAddress()");
    res = lpaManagerApiGetSMDSAddress(ptrAddressData);
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaGetSMDSAddress() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true); 
	return res;
}


/**
 * Set new Nickname field in profile identified by ICCID.
 * @param ptrProfileID Profile ICCI size, raw format
 * @param profileIdSize Profile ICCID size, size_t
 * @param ptrNickname New Nickname to set, raw hex format
 * @param nickNameSize New Nickname size, size_t
 * @return true is nickname change operation successful
 */
EXPORT_DLL bool lpaSetNicknameByIccid(const unsigned char* ptrProfileId, size_t profileIdSize, const unsigned char* ptrNickname, size_t nickNameSize)
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaSetNicknameByIccid()");
    
	_lpaBeginApiFunction("lpaSetNicknameByIccid", true);
    bool res = lpaManagerApiSetNickname(ptrProfileId, profileIdSize, ptrNickname, nickNameSize);
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaSetNicknameByIccid() : return %s", (res ? "true" : "false"));
	_lpaEndApiFunction(true);

    return res;
}


////////////////////////////////////////////////////////////
//                  Extended API PART
////////////////////////////////////////////////////////////

EXPORT_DLL bool lpaExGetFullParametersList(LPA_PARAMETERS_LIST * ptrLpaParametersList)
{
	bool res = false;
	_lpaBeginApiFunction("lpaExGetFullParametersList", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaExGetFullParametersList()");

#if defined(LPA_SDK__USING_EX_API)
	res = lpaManagerApiGetFullParametersList(ptrLpaParametersList);
#else
	lpaSetErrorCode(LPA_ERROR_EXTENDED_API_UNAVAILABLE);
#endif // LPA_SDK__USING_EX_API
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaExGetFullParametersList() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaExClearProfileNotification(uint16_t sequenceNumber)
{
	bool res = false;
	_lpaBeginApiFunction("lpaExClearProfileNotification", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaExClearProfileNotification()");

#if defined(LPA_SDK__USING_EX_API)
	res = lpaManagerApiClearProfileNotification(sequenceNumber);
#else
	lpaSetErrorCode(LPA_ERROR_EXTENDED_API_UNAVAILABLE);
#endif // LPA_SDK__USING_EX_API

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaExClearProfileNotification() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaExGetProfileNotificationList(LPA_PROFILE_NOTIFICATION_LIST* ptrProfileNotificationList)
{
	bool res = false;
	_lpaBeginApiFunction("lpaExGetProfileNotificationList", true);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaExGetProfileNotificationList()");

#if defined(LPA_SDK__USING_EX_API)
	res = lpaManagerApiGetProfileNotificationList(ptrProfileNotificationList);
#else
	lpaSetErrorCode(LPA_ERROR_EXTENDED_API_UNAVAILABLE);
#endif // LPA_SDK__USING_EX_API

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaExGetProfileNotificationList() : return %s", (res ? "true" : "false"));

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool	lpaExCardReset()
{
	bool res = false;
	_lpaBeginApiFunction("lpaExCardReset", true);

#if defined(LPA_SDK__USING_EX_API)
	res = lpaManagerApiSEMediaCardReset();
#else
	lpaSetErrorCode(LPA_ERROR_EXTENDED_API_UNAVAILABLE);
#endif // LPA_SDK__USING_EX_API

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaExGetExtraVersion(char* ptrVersionBuffer, size_t versionBufferMaxSize)
{
	bool res = false;
	_lpaBeginApiFunction("lpaExGetExtraVersion", true);

#if defined(LPA_SDK__USING_EX_API)
	if (ptrVersionBuffer != NULL && versionBufferMaxSize > strlen(LPA_SDK_EXTRA_VERSION))
	{
		sprintf(ptrVersionBuffer, "%s", LPA_SDK_EXTRA_VERSION);
		res = true;
	}
#else
	lpaSetErrorCode(LPA_ERROR_EXTENDED_API_UNAVAILABLE);
#endif // LPA_SDK__USING_EX_API

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaExWriteMemoryStatusDumpToLog()
{
	bool res = false;
	_lpaBeginApiFunction("lpaExWriteMemoryStatusDumpToLog", true);

#if defined(LPA_SDK__USING_EX_API)
	#ifdef LPA_SDK__MEMORY
		if (_lpaInit)
		{
			lpaCoreMemoryDumpStatusIntoLog();
			res = true;
		}
		else
			lpaSetErrorCode(LPA_NOT_INITIALIZED);
	#else
		lpaSetErrorCode(LPA_ERROR_PARAMETER_NOT_AUTHORIZED);
	#endif // LPA_SDK__MEMORY
#else
	lpaSetErrorCode(LPA_ERROR_EXTENDED_API_UNAVAILABLE);
#endif // LPA_SDK__USING_EX_API


	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaExGetMemoryStatus(LPA_MEMORY_STATUS* prtMemoryStatus)
{
	bool res = false;
	_lpaBeginApiFunction("lpaExGetMemoryStatus", true);

#if defined(LPA_SDK__USING_EX_API)
	#ifdef LPA_SDK__MEMORY
		if (_lpaInit)
		{
			if (prtMemoryStatus != NULL)
				res = lpaCoreGetMemoryStatus(prtMemoryStatus);
			else
				lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
		}
		else
			lpaSetErrorCode(LPA_NOT_INITIALIZED);
	#else
		lpaSetErrorCode(LPA_ERROR_PARAMETER_NOT_AUTHORIZED);
	#endif // LPA_SDK__MEMORY
#else
	lpaSetErrorCode(LPA_ERROR_EXTENDED_API_UNAVAILABLE);
#endif // LPA_SDK__USING_EX_API

	_lpaEndApiFunction(true);
	return res;
}

EXPORT_DLL bool lpaExCheckMemoryAllocated()
{
	bool res = false;
	_lpaBeginApiFunction("lpaExCheckMemoryAllocated", true);

#if defined(LPA_SDK__USING_EX_API)
	#ifdef LPA_SDK__MEMORY
		if (_lpaInit)
		{
			lpaCoreMemoryCheckMemoryAllocated();
			res = true;
		}
		else
			lpaSetErrorCode(LPA_NOT_INITIALIZED);
	#else
		lpaSetErrorCode(LPA_ERROR_PARAMETER_NOT_AUTHORIZED);
	#endif // LPA_SDK__MEMORY
#else
	lpaSetErrorCode(LPA_ERROR_EXTENDED_API_UNAVAILABLE);
#endif // LPA_SDK__USING_EX_API

	_lpaEndApiFunction(true);
	return res;
}

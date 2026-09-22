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


#include "lpasdk/core/lpa_manager_api.h"		// For main API
#include "lpasdk/core/lpa_manager.h"		// For main API

#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/semedia_manager.h"

#include <assert.h>

// Monitor LPA MANAGER API usage
static size_t _apiUsage = 0;

#define LPA_MANAGER_API_BEGIN(apiName) { _apiUsage ++ ; assert( _apiUsage == 1); }
#define LPA_MANAGER_API_END()		{ _apiUsage --; assert( _apiUsage == 0); }


// Manage automatic connect/disconnect with reader + select/unselectISDR
bool _lpaManagerApiConnectReaderAndSelectISDR();
bool _lpaManagerApiUnselectISDRAndDisconnectReader();


////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

UT_EXPORT_DLL bool lpaManagerApiInitialize(const char* ptrLpaFolder)
{
	LPA_MANAGER_API_BEGIN("lpaManagerApiInitialize");
	bool res = lpaManagerInitialize(ptrLpaFolder);
	LPA_MANAGER_API_END();

	return res;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

UT_EXPORT_DLL bool lpaManagerApiSetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, const void* ptrParameterValue, bool internalCall)
{
	LPA_MANAGER_API_BEGIN("lpaManagerApiSetConfigParameter");
	bool res = lpaManagerSetConfigParameter(ptrParameterName, parameterType, ptrParameterValue, internalCall);
	LPA_MANAGER_API_END();

	return res;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

UT_EXPORT_DLL bool lpaManagerApiGetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, void* ptrParameterValue, size_t parameterValueMaxSize)
{
	LPA_MANAGER_API_BEGIN("lpaManagerApiGetConfigParameter");
	bool res = lpaManagerGetConfigParameter(ptrParameterName, parameterType, ptrParameterValue, parameterValueMaxSize);
	LPA_MANAGER_API_END();

	return res;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

/**
 * Check if parameter exists in parameter list, if yes return parameter type.
 * If not in Extended Mode and parameter use is restricted, function will return that parameter does not exist.
 * 
 * @param ptrParameterName Pointer on parameter name to check, string type
 * @param ptrParameterType Pointer on parameter type to return, LPA_PARAMETER_TYPE type
 * @param ptrIsExist Pointer on boolean flag returning if parameter exists, if yes return true
 * @return True if no error occurred during check
 */
UT_EXPORT_DLL bool lpaManagerApiIsConfigParameterExist(const char* ptrParameterName, LPA_PARAMETER_TYPE* ptrParameterType, bool* ptrIsExist)
{
	LPA_MANAGER_API_BEGIN("lpaManagerApiIsConfigParameterExist");
	
	bool accessRight = false;
	bool res = lpaManagerIsConfigParameterExist(ptrParameterName, ptrParameterType, ptrIsExist, &accessRight);

	// Manage AccessRight use case: Extended parameter not authorized in Normal Mode, so invalid parameter existence status
        // Note: Do not check ptrParameterType because already done in lpaManagerIsConfigParameterExist()
	if (res && (NULL != ptrIsExist) && (true == *ptrIsExist) && (!accessRight))
	{
		*ptrIsExist = false;
                *ptrParameterType = LPA_PARAMETER_TYPE_UNKNOWN;
	}

	LPA_MANAGER_API_END();

	return res;
}

////////////////////////////////////////////////////////////
// Redirection
////////////////////////////////////////////////////////////

UT_EXPORT_DLL bool lpaManagerApiGetFullParametersList(LPA_PARAMETERS_LIST * ptrLpaParametersList)
{
	LPA_MANAGER_API_BEGIN("lpaManagerApiGetFullParametersList");
	bool res = lpaManagerGetFullParametersList(ptrLpaParametersList);
	LPA_MANAGER_API_END();

	return res;
}


// Reconnect SEMedia
UT_EXPORT_DLL bool lpaManagerApiSEMediaCardReset()
{
	LPA_MANAGER_API_BEGIN("lpaManagerApiSEMediaCardReset");
	bool res = lpaManagerSEMediaCardReset();
	LPA_MANAGER_API_END();

	return res;
}


// Exchange with ISDR applet
UT_EXPORT_DLL bool lpaManagerApiGetReaderList(LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiGetReaderList");
	// In first, establish SEMedia
	if (seMediaManagerIsContextEstablished() || seMediaManagerEstablishContext())
	{
		res = lpaManagerGetReaderList(ptrReaderNameInfoList, readerNameInfoMax, ptrCountReader);
	}
	// At the end, do cleanup
	bool resCleanup = seMediaManagerReleaseContext();

	if (res) // manage error during cleanup
		res = resCleanup;
	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiGetProfilesInfo(LPA_GET_PROFILES_INFO* ptrGetProfilesInfo)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiGetProfilesInfo");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerGetProfilesInfo(ptrGetProfilesInfo);
	}

	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;

	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiGetProfilesInfo_ex(LPA_GET_PROFILES_INFO* ptrGetProfilesInfo)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiGetProfilesInfo");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerGetProfilesInfo_ex(ptrGetProfilesInfo);
	}

	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;

	LPA_MANAGER_API_END();

	return res;
}
UT_EXPORT_DLL bool lpaManagerApiGetProfilesNumber(size_t * ptrNumberOfProfiles)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiGetProfilesNumber");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerGetProfilesNumber(ptrNumberOfProfiles);
	}

	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;

	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiGetEID(LPA_GET_EID* ptrGetEID)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiGetEID");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerGetEID(ptrGetEID);
	}

	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;

	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiMemoryReset(const unsigned char* ptrMemoryResetOptionParameter, const size_t memoryResetOptionSize)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiMemoryReset");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerMemoryReset(ptrMemoryResetOptionParameter, memoryResetOptionSize);
	}

	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
	res = resCleanup;

	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiSendPendingNotification(LPA_EventCallback* ptrLpaEventCallback, LPA_SENDING_NOTIFICATION_RESULT* ptrSendingNotificationResult)
{
	bool res = false;
	LPA_MANAGER_API_BEGIN("lpaManagerApiSendPendingNotification");
        
        // Initialize values in case not already done in calling application, avoid return random values if cannot connect to reader / ISDR and main result not checked
        if(ptrSendingNotificationResult != NULL)
        {
            ptrSendingNotificationResult->countNotificationDetected = 0;
            ptrSendingNotificationResult->countNotificationSend = 0;
        }
        
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerSendPendingNotification(ptrLpaEventCallback, ptrSendingNotificationResult);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;

	LPA_MANAGER_API_END();

	return res;
}


// Manage Enable/Disable/Delete Profile
UT_EXPORT_DLL bool lpaManagerApiEnableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiEnableProfileByIccid");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerEnableProfileByIccid(ptrProfileId, profileIdSize);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;
	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiDisableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiDisableProfileByIccid");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerDisableProfileByIccid(ptrProfileId, profileIdSize);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;
	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiDeleteProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiDeleteProfileByIccid");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerDeleteProfileByIccid(ptrProfileId, profileIdSize);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;
	LPA_MANAGER_API_END();

	return res;
}

// Manage notification list
UT_EXPORT_DLL bool lpaManagerApiGetProfileNotificationList(LPA_PROFILE_NOTIFICATION_LIST* ptrProfileNotificationList)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiGetProfileNotificationList");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerGetProfileNotificationList(ptrProfileNotificationList);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();
	LPA_MANAGER_API_END();
	if (res) // manage error during cleanup
		res = resCleanup;

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiClearProfileNotification(uint16_t sequenceNumber)
{
	bool res = false;
	LPA_MANAGER_API_BEGIN("lpaManagerApiClearProfileNotification");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerClearProfileNotification(sequenceNumber);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();
	LPA_MANAGER_API_END();
	if (res) // manage error during cleanup
	res = resCleanup;


	return res;
}

UT_EXPORT_DLL bool lpaManagerApiSetDefaultSMDPAddress(const char* ptrSMDPAddr)
{
	bool res = false;
	LPA_MANAGER_API_BEGIN("lpaManagerApiSetDefaultSMDPAddress");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerSetDefaultSMDPAddress(ptrSMDPAddr);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();
	LPA_MANAGER_API_END();
	if (res) // manage error during cleanup
		res = resCleanup;

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiGetSMDPAddress(ADDRESS_DATA* ptrAddressData)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiGetSMDPAddress");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerGetSMDPAddress(ptrAddressData);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();
	LPA_MANAGER_API_END();
	if (res) // manage error during cleanup
		res = resCleanup;

	return res;
}
UT_EXPORT_DLL bool lpaManagerApiGetSMDSAddress(ADDRESS_DATA* ptrAddressData)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiGetSMDSAddress");
	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerGetSMDSAddress(ptrAddressData);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();
	LPA_MANAGER_API_END();
	if (res) // manage error during cleanup
		res = resCleanup;
	return res;
}

UT_EXPORT_DLL bool lpaManagerApiDownloadProfile(const char * ptrActivationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiDownloadProfile");

	// Reset DownloadProfileResult if defined
	if (ptrDownloadProfileResult != NULL)
	{
		ptrDownloadProfileResult->countProfileInstalled = 0;
		ptrDownloadProfileResult->countProfileTotal = 0;
	}

	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerDownloadProfile(ptrActivationCodeStr, ptrLpaEventCallback, ptrDownloadProfileResult);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;

	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiDownloadProfileWithConfirmationCode(const char * ptrActivationCodeStr, const char * ptrConfirmationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiDownloadProfileWithConfirmationCode");

	// Reset DownloadProfileResult if defined
	if (ptrDownloadProfileResult != NULL)
	{
		ptrDownloadProfileResult->countProfileInstalled = 0;
		ptrDownloadProfileResult->countProfileTotal = 0;
	}

	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerDownloadProfileWithConfirmationCode(ptrActivationCodeStr, ptrConfirmationCodeStr, ptrLpaEventCallback, ptrDownloadProfileResult);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;
	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiDownloadProfileWithDefaultSMDPAddress(const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiDownloadProfileWithDefaultSMDPAddress");

	// Reset DownloadProfileResult if defined
	if (ptrDownloadProfileResult != NULL)
	{
		ptrDownloadProfileResult->countProfileInstalled = 0;
		ptrDownloadProfileResult->countProfileTotal = 0;
	}

	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerDownloadProfileWithDefaultSMDPAddress(ptrLpaEventCallback, ptrDownloadProfileResult);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;
	LPA_MANAGER_API_END();

	return res;
}

UT_EXPORT_DLL bool lpaManagerApiDownloadProfileWithSDMSAddress(const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult)
{
	bool res = false;

	LPA_MANAGER_API_BEGIN("lpaManagerApiDownloadProfileWithSDMSAddress");

	// Reset DownloadProfileResult if defined
	if (ptrDownloadProfileResult != NULL)
	{
		ptrDownloadProfileResult->countProfileInstalled = 0;
		ptrDownloadProfileResult->countProfileTotal = 0;
	}

	// In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
	if (_lpaManagerApiConnectReaderAndSelectISDR())
	{
		res = lpaManagerDownloadProfileWithSMDSAddress(ptrLpaEventCallback, ptrDownloadProfileResult);
	}
	// At the end, unselect ISDR (if selected) and disconnect reader (if connected)
	bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();

	if (res) // manage error during cleanup
		res = resCleanup;
	LPA_MANAGER_API_END();

	return res;
}


////////////////////////////////////////////////////////////
// Redirection
////////////////////////////////////////////////////////////

/**
 * Set new Nickname field in profile identified by ICCID.
 * @param ptrProfileID Profile ICCI size, raw format
 * @param profileIdSize Profile ICCID size, size_t
 * @param ptrNickname New Nickname to set, raw hex format
 * @param nickNameSize New Nickname size, size_t
 * @return true is nickname change operation successful
 */
UT_EXPORT_DLL bool lpaManagerApiSetNickname(const unsigned char* ptrProfileId, size_t profileIdSize, const unsigned char* ptrNickname, size_t nickNameSize)
{
    bool res = false;
    
    LPA_MANAGER_API_BEGIN("lpaManagerApiSetNickname");
    // In first, connect reader (if not yet connected) and select ISDR (if not yet selected)
    if (_lpaManagerApiConnectReaderAndSelectISDR())
    {
        res = lpaManagerSetNickname(ptrProfileId, profileIdSize, ptrNickname, nickNameSize);
    }
    
    // At the end, unselect ISDR (if selected) and disconnect reader (if connected)
    bool resCleanup = _lpaManagerApiUnselectISDRAndDisconnectReader();
    
    // manage error during cleanup
    if (res)
        res = resCleanup;

    LPA_MANAGER_API_END();

    return res;
}


/////////////////////////////////////////////
// Redirection
/////////////////////////////////////////////

UT_EXPORT_DLL bool lpaManagerApiSEMediaManagerIsInitialized()
{
    LPA_MANAGER_API_BEGIN("lpaManagerApiSEMediaManagerIsInitialized");
    
    bool res = lpaManagerSEMediaManagerIsInitialized();
    
    LPA_MANAGER_API_END();

    return res;
}

/////////////////////////////////////////////
// Redirection
/////////////////////////////////////////////

UT_EXPORT_DLL bool lpaManagerApiSEMediaManagerUninitialize()
{
    LPA_MANAGER_API_BEGIN("lpaManagerApiSEMediaManagerUninitialize");
    
    bool res = lpaManagerSEMediaManagerUninitialize();
    
    LPA_MANAGER_API_END();

    return res;

}

/////////////////////////////////////////////
// Redirection
/////////////////////////////////////////////

UT_EXPORT_DLL bool lpaManagerApiHttpMediaManagerIsInitialized()
{
    LPA_MANAGER_API_BEGIN("lpaManagerApiHttpMediaManagerIsInitialized");
    
    bool res = lpaManagerHttpMediaManagerIsInitialized();
    
    LPA_MANAGER_API_END();

    return res;
}

/////////////////////////////////////////////
// Redirection
/////////////////////////////////////////////

UT_EXPORT_DLL bool lpaManagerApiHttpMediaManagerDelete()
{
    LPA_MANAGER_API_BEGIN("lpaManagerApiHttpMediaManagerDelete");
    
    bool res = lpaManagerHttpMediaManagerDelete();
    
    LPA_MANAGER_API_END();

    return res;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////


bool _lpaManagerApiConnectReaderAndSelectISDR()
{
	return lpaManagerConnectReaderAndSelectISDR();
}

bool _lpaManagerApiUnselectISDRAndDisconnectReader()
{
	return lpaManagerUnselectISDRAndDisconnectReader();
}

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

#include "lpasdk/core/lpa_manager_es10c.h"
#include "lpasdk/core/lpa_manager.h"
#include "lpasdk/core/lpa_manager_helper.h"

#include "lpasdk/core/rawdata_object.h"
#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/bertlv_object.h"
#include "lpasdk/lpasdk_internal_api.h"
#include "lpasdk/core/util.h"
#include "lpasdk/core/lpa_memory.h"

/////////////////////////////////////////////

#define LPA_MANAGER_ES10C_DATA_BUFFER_MAX_SIZE MAX_LPA_MANAGER_APDU_BUFFER_SIZE
static unsigned char _dataBuffer[LPA_MANAGER_ES10C_DATA_BUFFER_MAX_SIZE];

#define	PROFILE_ICCID_TAG			0x5A
#define PROFILE_NICKNAME_TAG                    0x90

#define GET_PROFILE_INFO_DGI_TAG		0xBF2D
#define GET_EID_DGI_TAG				0xBF3E
#define MEMORY_RESET_TAG			0xBF34

#define ENABLE_PROFILE_DGI_TAG			0xBF31
#define DISABLE_PROFILE_DGI_TAG			0xBF32
#define DELETE_PROFILE_DGI_TAG			0xBF33

#define SET_NICKNAME_TAG                        0xBF29

#define DER_ATTRIBUTE_TAG			0x80

#define MEMORY_RESET_OPTION_SIZE                0x02

#define TEMP_BUFFER_SIZE_FOR_PROFILES_COUNT     768    // Temporary buffer size for request "GetProfilesInfo ICCID only". Should never need to be changed, very large.

static const unsigned char PROFILE_INFO_LIST_ICCID_ONLY_REQUEST [] = {0x5C, 0x01, 0x5A};
static const unsigned char PROFILE_INFO_FOR_PPR_MGT_REQUEST [] = {0x5C, 0x05 , 0x5A, 0x9F, 0x70, 0x95, 0x99};

#ifdef _FFW_PCIOT_LPA_MODIFY_
//custom get profile request: iccid(5A),profile state(9f70),profileclass(95),profilename(92)
static const unsigned char PROFILE_INFO_FOR_CUSTOM_REQUEST[] = { 0x5C, 0x05 , 0x5A, 0x9F, 0x70, 0x95, 0x92 };
#endif

typedef enum
{
	enableProfile = ENABLE_PROFILE_DGI_TAG,
	disableProfile = DISABLE_PROFILE_DGI_TAG,
	deleteProfile = DELETE_PROFILE_DGI_TAG
}PROFILE_OPERATION;

unsigned char EIDOptionParameter[] = { 0x5C, 0x01, 0x5A };

static char _bufferFormatLogMessage[1024];	// 1Ko is enough for logging message
static bool _refreshFlagActivated = true;

/////////////////////////////////////////////

bool _extractDataFromGetProfileInfoRawData(LPA_GET_PROFILES_INFO* ptrLpaGetProfileInfoAll, unsigned char* rawData, size_t rawDataSize, bool fillFrofileFields, bool requestForPPRmanagement);
bool _updateProfileInfoFromBerTLV(unsigned char* ptrLpaProfileInfo, BeerTLV* ptrBerTLV, bool requestForPPRmanagement);
bool _doProfileOperationByIccid(PROFILE_OPERATION profileOperation, const unsigned char* profileId, size_t profileIdSize); 
LPA_MEMORY_RESET_STATUS _doExtractMemoryResetResponse(const unsigned char *ptrData, size_t dataSize);

bool _extractResponseForEnableProfileOperation(const unsigned char *ptrData, size_t dataSize);
bool _extractResponseForDisableProfileOperation(const unsigned char *ptrData, size_t dataSize);
bool _extractResponseForDeleteProfileOperation(const unsigned char *ptrData, size_t dataSize);
bool _doExtractEIDResponse(const unsigned char *ptrData, size_t dataSize, LPA_GET_EID* ptrGetEID);

bool _extractResponseForSetNicknameOperation(const unsigned char *ptrData, size_t dataSize);

/////////////////////////////////////////////
//
/////////////////////////////////////////////

void lpaManagerES10c_SetRefreshFlag(bool refreshFlagActivated)
{
	_refreshFlagActivated = refreshFlagActivated;
}

bool lpaManagerES10c_IsRefreshFlag()
{
	return _refreshFlagActivated;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Perform ES10C GetProfilesInfo request and fill LPA_GET_PROFILES_INFO structure with profiles informations returned by eUICC
 * @param ptrLpaGetProfilesInfo Pointer on LPA_GET_PROFILES_INFO structure to fill with profiles informations
 * @param continueRetry Pointer of flag informing if retry can be performed again, true = yes (Manage case when failed to retrieve number of profiles due to Chained GetResponse issue)
 * @param requestForPPRmanagement If set to "true" request will be performed for PPR management (Different profiles structure, find number of profiles, memory allocation)
 * @return true if operation is successful
 */
bool lpaManagerES10c_GetProfilesInfo(LPA_GET_PROFILES_INFO* ptrLpaGetProfilesInfo, bool * continueRetry, bool requestForPPRmanagement)
{
    bool res = false;
    bool resCount = false;
    size_t numberOfProfilesOnEUICC = 0;
    size_t fullProfileInfoBufferSize = 0;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerES10c_GetProfilesInfo(...)");
    
    // profileInfoList storage area must have been initialized, and maxNumberProfileInfo must define at least one profile
    if(ptrLpaGetProfilesInfo != NULL && continueRetry != NULL && 
                                    ((ptrLpaGetProfilesInfo->profileInfoList != NULL && ptrLpaGetProfilesInfo->maxNumberProfileInfo > 0) || requestForPPRmanagement))
    {
        // Avoid potential problems of initialization, if forget to be done in caller
        ptrLpaGetProfilesInfo->countProfileInfo = 0;
        ptrLpaGetProfilesInfo->numberProfileInfoFound = 0;
        
        // Management of profile-info list depending PPR management of not
        if(requestForPPRmanagement)
        {
            // De-allocate profile-info list if used for PPR management (Bad parameter or retry for chained GetResponse issues)
            if(ptrLpaGetProfilesInfo->profileInfoList != NULL)
                lpaCoreMemoryFree(ptrLpaGetProfilesInfo->profileInfoList);
        }
        else
        {
            // Reset profile info list
            memset(ptrLpaGetProfilesInfo->profileInfoList, 0, (ptrLpaGetProfilesInfo->maxNumberProfileInfo * sizeof(LPA_PROFILE_INFO)));
        }
        
        // Retrieve number of profiles stored in card, to set request response buffer size.
        // Note: Use entry point in lpa_manager instead of one in lpa_manager_es10c to get benefit of retry mechanism set for this command (Chained GetResponse issue)
        resCount = lpaManagerGetProfilesNumber(&numberOfProfilesOnEUICC);
        
        // Invalidate retry in case failed to retrieve number of profiles due to Chained GetResponse error permanent issue, in that case inform caller to give up performing request.
        if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE)
            *continueRetry = false;
        else
            *continueRetry = true;
        
        // If failed to retrieve number of profiles stored in card, exit on error
        // If failed on retry for lpaManagerES10c_GetProfilesNumber(), resCount will be false anyway.
        if(resCount)
        {
            // If no profiles found on card, it is useless to perform request
            if(numberOfProfilesOnEUICC > 0)
            {
                
                // If request for PPR management, set maximum of profiles and Allocate memory for profile info list
                if(requestForPPRmanagement)
                {
                    ptrLpaGetProfilesInfo->maxNumberProfileInfo = numberOfProfilesOnEUICC;
                    ptrLpaGetProfilesInfo->profileInfoList = lpaCoreMemoryAlloc(numberOfProfilesOnEUICC * sizeof(LPA_PROFILE_INFO_FOR_PPR));
                    
                    // Memory allocation success => Initialize memory area else exit with error
                    if(ptrLpaGetProfilesInfo->profileInfoList != NULL)
                    {
                        memset(ptrLpaGetProfilesInfo->profileInfoList, 0, (numberOfProfilesOnEUICC * sizeof(LPA_PROFILE_INFO_FOR_PPR)));
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot allocate memory area for profileInfoList data! (PPR management) Needed %d bytes", numberOfProfilesOnEUICC * sizeof(LPA_PROFILE_INFO_FOR_PPR));
                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                        return res;
                    }
                }

                // Full GetProfileInfo size = Header BF2D / A0 (9 bytes + 1 security) + (number of profiles found in card x Size of Retrievable profiles)
                if(requestForPPRmanagement)
                    fullProfileInfoBufferSize = 10 + (numberOfProfilesOnEUICC * LPA_PROFILE_INFO_BUFFER_MAX_SIZE_FOR_PPR);
                else
                    fullProfileInfoBufferSize = 10 + (numberOfProfilesOnEUICC * LPA_PROFILE_INFO_BUFFER_MAX_SIZE);

                unsigned char * fullProfileInfoBuffer = lpaCoreMemoryAlloc(fullProfileInfoBufferSize);

                // Cannot allocate buffer for GetProfileInfoResponse => exit with error
                if(fullProfileInfoBuffer == NULL)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot allocate response buffer for GetProfileInfo eUICC response! Needed %d bytes", CAST_SIZET_PLATFORM(fullProfileInfoBufferSize));
                    lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                    
                    // Avoid possible memory leak
                    if(requestForPPRmanagement && ptrLpaGetProfilesInfo->profileInfoList != NULL)
                        lpaCoreMemoryFree(ptrLpaGetProfilesInfo->profileInfoList);
                    
                    return res;
                }

                size_t dataBufferSize = 0;
                uint16_t sw = 0x0000;
                RawDataObject* rawDataObjectGetProfilesInfo = NULL;

                // Build request depending normal request or for PPR management
                if(requestForPPRmanagement)
                {
                    // For PPR request, specify defined list of tags defined in PROFILE_INFO_FOR_PPR_TAG_LIST
                    rawDataObjectGetProfilesInfo = berTLV_createAndBuildRawDataObject(GET_PROFILE_INFO_DGI_TAG, sizeof(PROFILE_INFO_FOR_PPR_MGT_REQUEST), PROFILE_INFO_FOR_PPR_MGT_REQUEST);
                }
                else
                {
                #ifdef _FFW_PCIOT_LPA_MODIFY_
                    //use custom tag to get profile list,
                    //because of adapter api ffw_pal_event_ats_device_proccess only has 2048 byte buffer size, so we only get iccid, profile name, profile class,profile state
                    rawDataObjectGetProfilesInfo = berTLV_createAndBuildRawDataObject(GET_PROFILE_INFO_DGI_TAG, sizeof(PROFILE_INFO_FOR_CUSTOM_REQUEST), PROFILE_INFO_FOR_CUSTOM_REQUEST);
                #else
                    // Empty request for normal profile info request
                    rawDataObjectGetProfilesInfo = berTLV_createAndBuildRawDataObject(GET_PROFILE_INFO_DGI_TAG, 0, NULL);
                #endif
                }

                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "do buildAndSendStoreDataCase4(GetProfilesInfo) ...");
                if (rawDataObjectGetProfilesInfo != NULL)
                {
                    if (buildAndSendStoreDataCase4(rawDataObjectGetProfilesInfo, &sw, fullProfileInfoBuffer, fullProfileInfoBufferSize, &dataBufferSize))
                    {
                        // Check if SW=90.00 or 91.xx
                        if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SW=90.00 or 91.xx => Extracting data");

                            if (dataBufferSize > 0)
                            {
                                // Extract data, recover and fill profiles fields required
                                if(_extractDataFromGetProfileInfoRawData(ptrLpaGetProfilesInfo, fullProfileInfoBuffer, dataBufferSize, true, requestForPPRmanagement))
                                    res = true;
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Profile data extraction cannot be performed!");
                                    lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNABLE_TO_EXTRACT_DATA);
                                }                                                        
                            }
                            else
                            {
                                // No data = error, card shall return at least a response
                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No Raw data available!");
                            }
                        }
                        else
                        {
                            // No or Invalid SW
                            lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                        }
                    }
                    else
                    {
                        lpaSetErrorCode(LPA_ERROR_INVALID_GET_PROFILES_INFO_EXCHANGE);
                    }
                }
                else
                {
                    lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                }

                // Do memory cleanup
                ERASE_RAWDATAOBJECT(rawDataObjectGetProfilesInfo);
                lpaCoreMemoryFree(fullProfileInfoBuffer);
                fullProfileInfoBuffer = NULL;
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No profiles found in card. GetProfileInfo \"Full Profile\" request is bypassed");
                res = true;
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to retrieve number of profiles stored in card! GetProfileInfo aborted.");
            lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNABLE_TO_EXTRACT_DATA);
        }
    }
    else
    {
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerES10c_GetProfilesInfo(...)");
    
    return res;
}


bool lpaManagerES10c_GetProfilesInfo_ex(LPA_GET_PROFILES_INFO* ptrLpaGetProfilesInfo, bool * continueRetry, bool requestForPPRmanagement)
{
    bool res = false;
    size_t numberOfProfilesOnEUICC = 0;
    size_t fullProfileInfoBufferSize = 0;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerES10c_GetProfilesInfo(...)");
    
    // profileInfoList storage area must have been initialized, and maxNumberProfileInfo must define at least one profile
    if(ptrLpaGetProfilesInfo != NULL && continueRetry != NULL && 
                                    ((ptrLpaGetProfilesInfo->profileInfoList != NULL && ptrLpaGetProfilesInfo->maxNumberProfileInfo > 0) || requestForPPRmanagement))
    {
        // Avoid potential problems of initialization, if forget to be done in caller
        ptrLpaGetProfilesInfo->countProfileInfo = 0;
        ptrLpaGetProfilesInfo->numberProfileInfoFound = 0;

        // Reset profile info list
        if(ptrLpaGetProfilesInfo->profileInfoList)
            memset(ptrLpaGetProfilesInfo->profileInfoList, 0, (ptrLpaGetProfilesInfo->maxNumberProfileInfo * sizeof(LPA_PROFILE_INFO_FOR_PPR)));

        // Retrieve number of profiles stored in card, to set request response buffer size.
        numberOfProfilesOnEUICC = ptrLpaGetProfilesInfo->maxNumberProfileInfo;

        // Invalidate retry in case failed to retrieve number of profiles due to Chained GetResponse error permanent issue, in that case inform caller to give up performing request.
        if(lpaGetErrorCodeNoClear() == SE_MEDIA_E_CHAINING_GET_RESPONSE)
            *continueRetry = false;
        else
            *continueRetry = true;
        
        // If no profiles found on card, it is useless to perform request
        if(numberOfProfilesOnEUICC > 0)
        {
            // Full GetProfileInfo size = Header BF2D / A0 (9 bytes + 1 security) + (number of profiles found in card x Size of Retrievable profiles)
            fullProfileInfoBufferSize = 10 + (numberOfProfilesOnEUICC * LPA_PROFILE_INFO_BUFFER_MAX_SIZE_FOR_PPR);

            unsigned char * fullProfileInfoBuffer = lpaCoreMemoryAlloc(fullProfileInfoBufferSize);

            // Cannot allocate buffer for GetProfileInfoResponse => exit with error
            if(fullProfileInfoBuffer == NULL)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot allocate response buffer for GetProfileInfo eUICC response! Needed %d bytes", CAST_SIZET_PLATFORM(fullProfileInfoBufferSize));
                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                
                return res;
            }

            size_t dataBufferSize = 0;
            uint16_t sw = 0x0000;
            RawDataObject* rawDataObjectGetProfilesInfo = NULL;

            // For PPR request, specify defined list of tags defined in PROFILE_INFO_FOR_PPR_TAG_LIST
            rawDataObjectGetProfilesInfo = berTLV_createAndBuildRawDataObject(GET_PROFILE_INFO_DGI_TAG, sizeof(PROFILE_INFO_FOR_PPR_MGT_REQUEST), PROFILE_INFO_FOR_PPR_MGT_REQUEST);

            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "do buildAndSendStoreDataCase4(GetProfilesInfo) ...");
            if (rawDataObjectGetProfilesInfo != NULL)
            {
                if (buildAndSendStoreDataCase4(rawDataObjectGetProfilesInfo, &sw, fullProfileInfoBuffer, fullProfileInfoBufferSize, &dataBufferSize))
                {
                    // Check if SW=90.00 or 91.xx
                    if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SW=90.00 or 91.xx => Extracting data");

                        if (dataBufferSize > 0)
                        {
                            // Extract data, recover and fill profiles fields required
                            if(_extractDataFromGetProfileInfoRawData(ptrLpaGetProfilesInfo, fullProfileInfoBuffer, dataBufferSize, true, requestForPPRmanagement))
                                res = true;
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Profile data extraction cannot be performed!");
                                lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNABLE_TO_EXTRACT_DATA);
                            }                                                        
                        }
                        else
                        {
                            // No data = error, card shall return at least a response
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No Raw data available!");
                        }
                    }
                    else
                    {
                        // No or Invalid SW
                        lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                    }
                }
                else
                {
                    lpaSetErrorCode(LPA_ERROR_INVALID_GET_PROFILES_INFO_EXCHANGE);
                }
            }
            else
            {
                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
            }

            // Do memory cleanup
            ERASE_RAWDATAOBJECT(rawDataObjectGetProfilesInfo);
            lpaCoreMemoryFree(fullProfileInfoBuffer);
            fullProfileInfoBuffer = NULL;
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No profiles found in card. GetProfileInfo \"Full Profile\" request is bypassed");
            res = true;
        }
    }
    else
    {
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerES10c_GetProfilesInfo(...)");
    
    return res;
}

/**
 * Uses ES10C GetProfilesInfo request to retrieve number of profiles available on eUICC
 * @param ptrLpaGetProfilesInfo Pointer on variable were number of profile will be returned, size_t type
 * @return true if operation is successful
 */
bool lpaManagerES10c_GetProfilesNumber(size_t * ptrNumberOfProfiles)
{
    bool res = false;
    LPA_GET_PROFILES_INFO tempLpaGetProfilesInfo;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerES10c_GetProfilesNumber(...)");
        
    if (ptrNumberOfProfiles != NULL)
    {
        // Reset profiles number value if not already done by caller
        *ptrNumberOfProfiles = 0;
        
        // Temporary buffer for GetProfileInfo request
        unsigned char * requestResponseBuffer = lpaCoreMemoryAlloc(TEMP_BUFFER_SIZE_FOR_PROFILES_COUNT);
        
        // Cannot allocate response buffer for GetProfileIno request => exit with error
        if(requestResponseBuffer == NULL)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot allocate response buffer for GetProfileNumber eUICC response! Needed %d bytes", TEMP_BUFFER_SIZE_FOR_PROFILES_COUNT);
            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
            return res;
        }

        // Temporary profile info structure initialization
        tempLpaGetProfilesInfo.countProfileInfo = 0;        // No real function here
        tempLpaGetProfilesInfo.maxNumberProfileInfo = 0;    // No real function here
        tempLpaGetProfilesInfo.numberProfileInfoFound = 0;
        // Here we do not want to fill profiles fields, just count profiles so profileInfoList data area pointer is set to NULL
        tempLpaGetProfilesInfo.profileInfoList = NULL;
        
        size_t dataBufferSize = 0;
        uint16_t sw = 0x0000;
        RawDataObject* rawDataObjectGetProfilesInfo = NULL;

        // Build request, only ICCID tags required
        rawDataObjectGetProfilesInfo = berTLV_createAndBuildRawDataObject(GET_PROFILE_INFO_DGI_TAG, sizeof(PROFILE_INFO_LIST_ICCID_ONLY_REQUEST), PROFILE_INFO_LIST_ICCID_ONLY_REQUEST);

        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "do buildAndSendStoreDataCase4(GetProfilesNumber) ...");
        if (rawDataObjectGetProfilesInfo != NULL)
        {
            if (buildAndSendStoreDataCase4(rawDataObjectGetProfilesInfo, &sw, requestResponseBuffer, TEMP_BUFFER_SIZE_FOR_PROFILES_COUNT, &dataBufferSize))
            {
                // Check if SW=90.00 or 91.xx
                if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SW=90.00 or 91.xx => Extracting data");

                    if (dataBufferSize > 0)
                    {
                        // Extract data, do not require to recover profiles fields, not a PPR request
                        if(_extractDataFromGetProfileInfoRawData(&tempLpaGetProfilesInfo, requestResponseBuffer, dataBufferSize, false, false))
                        {
                            // Return number of profiles effectively found
                            *ptrNumberOfProfiles = tempLpaGetProfilesInfo.numberProfileInfoFound;
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES10c_GetProfilesNumber: Found %d profile(s)", CAST_SIZET_PLATFORM(*ptrNumberOfProfiles));
                            
                            res = true;
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Profile data extraction cannot be performed!");
                            lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNABLE_TO_EXTRACT_DATA);
                        }                                                        
                    }
                    else
                    {
                        // No data = error, card shall return at least a response
                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No Raw data available!");
                    }
                }
                else
                {
                    // No or Invalid SW
                    lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                }
            }
            else
            {
                lpaSetErrorCode(LPA_ERROR_INVALID_GET_PROFILES_INFO_EXCHANGE);
            }
        }
        else
        {
            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
        }

        // Do memory cleanup
        ERASE_RAWDATAOBJECT(rawDataObjectGetProfilesInfo);
        lpaCoreMemoryFree(requestResponseBuffer);
        requestResponseBuffer = NULL;
    }
    else
    {
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerES10c_GetProfilesNumber(...): return %s", (res ? "true" : "false"));
    
    return res;
}


/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES10c_GetEID(LPA_GET_EID* ptrGetEID)
{
	bool res = false;

	if (ptrGetEID != NULL)
	{
		// Reset Raw Data size
		ptrGetEID->EID_DataSize = 0;

		size_t dataBufferSize = 0;
		uint16_t sw = 0x0000;
		RawDataObject* rawDataObjectGetEID = NULL;

		// Select done correctly
		rawDataObjectGetEID = berTLV_createAndBuildRawDataObject(GET_EID_DGI_TAG, sizeof(EIDOptionParameter), EIDOptionParameter);
		if (rawDataObjectGetEID != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "do buildAndSendStoreDataCase4(GetEID) ...");
			if (buildAndSendStoreDataCase4(rawDataObjectGetEID, &sw, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
			{
				// Check if SW=90.00 or 91.xx
				if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
				{
					res = true; // By default no error but if error detected => reset it to false
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SW=90.00 or 91.xx => Extracting data");

					if (dataBufferSize > 0)
					{
						res = _doExtractEIDResponse(_dataBuffer, dataBufferSize, ptrGetEID);
					}
					else
					{
						// No error, but no data :(
						lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No Raw data available !");
					}
				}
				else
				{
					// No or Invalid SW
					lpaSetErrorCode(LPA_ERROR_INVALID_SW);
				}

				// 
				if (lpaIsError())
					res = false;
			}
			else
			{
				lpaSetErrorCode(LPA_ERROR_INVALID_GET_EID_EXCHANGE);
			}
		}
		else
			lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);

		// Do memory cleanup
		ERASE_RAWDATAOBJECT(rawDataObjectGetEID);
	}
	else
	{
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
	}

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES10c_MemoryReset(const unsigned char* memoryResetOptionParameter, const size_t memoryResetOptionSize)
{
	bool res = false;
        
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerES10c_MemoryReset(...)");
        
        if((memoryResetOptionParameter != NULL) && (memoryResetOptionSize == MEMORY_RESET_OPTION_SIZE))
		{
            RawDataObject* rawDataMemoryResetRequest = NULL;
            RawDataObject* rawDataMemoryResetParameterTag = NULL;

            // create reset request
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Creating rawDataMemoryResetParameterTag object ...");

            rawDataMemoryResetParameterTag = berTLV_createAndBuildRawDataObject(0x82, memoryResetOptionSize, memoryResetOptionParameter);
            if (rawDataMemoryResetParameterTag != NULL)
            {
                    rawDataMemoryResetRequest = berTLV_createAndBuildRawDataObject(MEMORY_RESET_TAG, rawDataMemoryResetParameterTag->rawDataSize, rawDataMemoryResetParameterTag->rawData);
            }

            if (rawDataMemoryResetRequest != NULL)
            {
                    // build and Send APDU
                    uint16_t apduSW = 0x0000;
                    size_t dataBufferSize = 0;

                    if (buildAndSendStoreDataCase4(rawDataMemoryResetRequest, &apduSW, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
                    {
                        // Check if SW = 90.00 or 91.xx (STK refresh or other)
                        if((apduSW == 0x9000) || ((apduSW & 0xFF00) == 0x9100))
                        {
                            // APDU successfully sent
                            if (dataBufferSize == 0)
                            {
                                    res = true; // no data
                            }
                            else
                            {
                                // Check APDU response data
                                LPA_MEMORY_RESET_STATUS memoryResetStatus = _doExtractMemoryResetResponse(_dataBuffer, dataBufferSize);
                                switch (memoryResetStatus)
                                {
                                case LPA_MEMORY_RESET_OK:
                                        res = true;
                                        break;

                                case LPA_MEMORY_RESET_NOTHING_TO_DELETE:
                                        lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_NOTHING_TO_DELETE);
                                        break;

                                case LPA_MEMORY_RESET_CAT_BUSY:
                                        lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_CAT_BUSY);
                                        break;

                                case LPA_MEMORY_RESET_UNDEFINED_ERROR:
                                        lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNDEFINED_ERROR);
                                        break;

                                default:
                                        lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNKNOWN_ERROR);
                                        break;
                                }
                            }
                        }
                        else
                            lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
                    }
                    else
                    {
                            lpaSetErrorCode(LPA_ERROR_INVALID_MEMORY_RESET_EXCHANGE);
                    }
            }
            else
            {
                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
            }

            // Do memory cleanup
            ERASE_RAWDATAOBJECT(rawDataMemoryResetParameterTag);
            ERASE_RAWDATAOBJECT(rawDataMemoryResetRequest);

            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerES10c_MemoryReset() : return %s", (res ? "true" : "false"));
        }
        else
	{
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
	}

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES10c_EnableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerES10c_EnableProfileByIccid(...)");

	res = _doProfileOperationByIccid(enableProfile, ptrProfileId, profileIdSize);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerES10c_EnableProfileByIccid() : return %s", (res ? "true" : "false"));
	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES10c_DisableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerES10c_DisableProfileByIccid(...)");

	res = _doProfileOperationByIccid(disableProfile, ptrProfileId, profileIdSize);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerES10c_DisableProfileByIccid() : return %s", (res ? "true" : "false"));
	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES10c_DeleteProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerES10c_DeleteProfileByIccid(...)");

	res = _doProfileOperationByIccid(deleteProfile, ptrProfileId, profileIdSize);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerES10c_DeleteProfileByIccid() : return %s", (res ? "true" : "false"));
	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Store profiles information from GetProfilesInfo returned by eUICC in LPA_GET_PROFILES_INFO data structure
 * @param ptrLpaGetProfileInfoAll Pointer on LPA_GET_PROFILES_INFO structure to fill with profiles informations
 * @param rawData Pointer on GetProfilesInfo response returned by eUICC
 * @param rawDataSize Size of GetProfilesInfo response returned by eUICC
 * @param fillFrofileFields It true, will fill ptrLpaGetProfileInfoAll->profileInfoList data area with profile fields, else will only count profiles
 * @param requestForPPRmanagement If set to "true" request will be performed for PPR management (Different profiles structure)
 * @return true if operation is successful
 */
bool _extractDataFromGetProfileInfoRawData(LPA_GET_PROFILES_INFO* ptrLpaGetProfileInfoAll, unsigned char* rawData, size_t rawDataSize, bool fillFrofileFields, bool requestForPPRmanagement)
{
    bool res = false;

    bool isTagFound_BF2D = false;
    BeerTLV* berTLV_BF2D = NULL;
    unsigned long profileAccessOffset = 0;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractDataFromGetProfileInfoRawData()");

    if((ptrLpaGetProfileInfoAll != NULL) && (rawData != NULL) && (rawDataSize > 0))
    {
        berTLV_BF2D = berTLV_extractTagUInt16(0xBF2D, rawData, rawDataSize, &isTagFound_BF2D);
        if (berTLV_BF2D != NULL)
        {
            bool isTagFound_A0 = false;
            
            lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, "Tag <BF2D> found", "berTLV_BF2D", berTLV_BF2D->value, berTLV_BF2D->length);

            BeerTLV* berTLV_A0 = berTLV_extractTagUInt8(0xA0, berTLV_BF2D->value, berTLV_BF2D->length, &isTagFound_A0);

            if (berTLV_A0 != NULL)
            {
                // Finding A0 tag is the minimum to consider that there is a valid response
                res = true;

                uint8_t countTLVFoundInsideA0 = 0;
                
                lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, "Tag <A0> found", "berTLV_A0", berTLV_A0->value, berTLV_A0->length);

                BerTLVList* berTLVListInsideA0 = berTLV_extractList(berTLV_A0->value, berTLV_A0->length, &countTLVFoundInsideA0);
                if (berTLVListInsideA0 != NULL)
                {
                    if (countTLVFoundInsideA0 > 0)
                    {
                        BerTLVList* berTLVCurrentInsideA0 = berTLVListInsideA0;
                        while (berTLVCurrentInsideA0 != NULL)
                        {
                            BerTLVList* berTLVNextInsideA0 = berTLVCurrentInsideA0->ptrNext;
                            if (berTLVCurrentInsideA0->berTLV != NULL && berTLVCurrentInsideA0->berTLV->tag == 0xE3)
                            {
                                // E3 Tag found :)
                                BeerTLV* berTLV_E3 = berTLVCurrentInsideA0->berTLV;

                                lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, "Tag <E3> found", "berTLV_E3", berTLV_E3->value, berTLV_E3->length);

                                // Fill profiles field area only if required, else will only count profiles
                                if(fillFrofileFields)
                                {
                                    if (ptrLpaGetProfileInfoAll->countProfileInfo < ptrLpaGetProfileInfoAll->maxNumberProfileInfo)
                                    {
                                        // Calculate offset in profileInfoList from profileInfoList base, profile# and LPA_PROFILE_INFO size, depending normal or PPR request
                                        if(requestForPPRmanagement)
                                            profileAccessOffset = ptrLpaGetProfileInfoAll->countProfileInfo * sizeof(LPA_PROFILE_INFO_FOR_PPR);
                                        else
                                            profileAccessOffset = ptrLpaGetProfileInfoAll->countProfileInfo * sizeof(LPA_PROFILE_INFO);
                                        
                                        if(! _updateProfileInfoFromBerTLV((unsigned char *)(ptrLpaGetProfileInfoAll->profileInfoList + profileAccessOffset), berTLV_E3, requestForPPRmanagement))
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "WARNING: Invalid data detected for profile #%u !", CAST_SIZET_PLATFORM(ptrLpaGetProfileInfoAll->countProfileInfo));

                                        ptrLpaGetProfileInfoAll->countProfileInfo++;
                                    }
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "WARNING: Number of profiles found (%d) exceeds maximum allowed (%d)! Profile not stored.", ptrLpaGetProfileInfoAll->numberProfileInfoFound + 1, ptrLpaGetProfileInfoAll->maxNumberProfileInfo);
                                }    

                                ptrLpaGetProfileInfoAll->numberProfileInfoFound++;
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Current BerTLV NULL or Tag not equal 0xE3, jumping on next");

                            berTLVCurrentInsideA0 = berTLVNextInsideA0;
                        }
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "TLV count inside tag <A0> <= 0 !");

                    berTLV_freeBerTLVList(berTLVListInsideA0);
                    berTLVListInsideA0 = NULL;
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No BerTLV inside tag <A0> !");

                berTLV_freeBerTLV(berTLV_A0);
                berTLV_A0 = NULL;
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Tag <A0> not found !");

            berTLV_freeBerTLV(berTLV_BF2D);
            berTLV_BF2D = NULL;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Tag <BF2D> not found !");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Incorrect parameter(s)!");
    
    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Extract profile informations from profile data bloc (BERTLV E3 from eUICC) and store them in profile information list
 * @param ptrLpaProfileInfo Profile location in profile information list (pointer)
 * @param ptrBerTLV Data bloc containing BerTlv E3 profile information, BeerTLV type
 * @param requestForPPRmanagement If set to "true" request will be performed for PPR management (Different profiles structure)
 * @return true if profile extraction is successful (Can extract BerTLVList from ptrBerTLV)
 */
bool _updateProfileInfoFromBerTLV(unsigned char* ptrLpaProfileInfo, BeerTLV* ptrBerTLV, bool requestForPPRmanagement)
{
    bool res = false;
    
    if((ptrLpaProfileInfo != NULL) && (ptrBerTLV != NULL))
    {
        // Set structure type depending type of request used (Normal or for PPR management
        // Note: Both structures are declared on same pointer source. Avoid warnings / errors for structure elements matching check at compiling.
        LPA_PROFILE_INFO* ptrNormalProfileInfoAccess = (LPA_PROFILE_INFO *)ptrLpaProfileInfo;
        LPA_PROFILE_INFO_FOR_PPR* ptrPPRprofileInfoAccess = (LPA_PROFILE_INFO_FOR_PPR *)ptrLpaProfileInfo;
        
        // Reset profile structure depending type of request effectively asked (Use correct size)
        if(requestForPPRmanagement)
            memset(ptrPPRprofileInfoAccess, 0x00, sizeof(LPA_PROFILE_INFO_FOR_PPR));
        else
            memset(ptrNormalProfileInfoAccess, 0x00, sizeof(LPA_PROFILE_INFO));
        
	// Copy Raw Data (If not for PPR management). If exceeds maximum size limit or returned profile raw data = NULL, exit with error state
        if(! requestForPPRmanagement)
        {
            if (ptrBerTLV->length <= LPA_PROFILE_INFO_BUFFER_MAX_SIZE && ptrBerTLV->value != NULL)
            {
                memcpy(ptrNormalProfileInfoAccess->rawData, ptrBerTLV->value, ptrBerTLV->length);
                ptrNormalProfileInfoAccess->rawDataSize = ptrBerTLV->length;
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_updateProfileInfoFromBerTLV() - Profile raw data size too big (%u vs %u max) or profile raw data = NULL !", ptrBerTLV->length, LPA_PROFILE_INFO_BUFFER_MAX_SIZE);
                return res;
            }
        }

	// Read all TLV present inside E3 Tag
	uint8_t countTLVFound = 0;
	BerTLVList* berTLVList = berTLV_extractList(ptrBerTLV->value, ptrBerTLV->length, &countTLVFound);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "countTLVFound : %d", countTLVFound);
	if (berTLVList != NULL)
	{
            // At this step we consider that we have a possible valid structure.
            // No more control is done on elements considering they are all declared "Optional" in SGP.22
            res = true;

            BerTLVList* berTLVCurrent = berTLVList;
            while (berTLVCurrent != NULL)
            {
                BeerTLV* currentBerTLV = berTLVCurrent->berTLV;
                if (currentBerTLV != NULL)
                {
                    if (formatBytesToHexaString(currentBerTLV->value, currentBerTLV->length, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "IDX[%d] TLV tag<%04x> (%d bytes) => %s", berTLVCurrent->index, currentBerTLV->tag,
                                                                                                                            currentBerTLV->length, _bufferFormatLogMessage);
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "IDX[%d] TLV tag<%04x> (%d bytes) => ...", berTLVCurrent->index, currentBerTLV->tag, currentBerTLV->length);
                    }

                    switch (currentBerTLV->tag)
                    {
                    case 0x4F: // ISDP AID
                        // Do nothing, tag not used for the moment
                        // Avoid useless warning when retrieving profiles
                        break;
                        
                    case 0x5A: // ICCID
                        if (currentBerTLV->length > 0 && currentBerTLV->value != NULL)
                        {
                            if (currentBerTLV->length <= LPA_PROFILE_ICCID_BUFFER_MAX_SIZE)
                            {
                                if(requestForPPRmanagement)
                                {
                                    memcpy(ptrPPRprofileInfoAccess->iccid, currentBerTLV->value, currentBerTLV->length);
                                    ptrPPRprofileInfoAccess->iccidSize = currentBerTLV->length;
                                }
                                else
                                {
                                    memcpy(ptrNormalProfileInfoAccess->iccid, currentBerTLV->value, currentBerTLV->length);
                                    ptrNormalProfileInfoAccess->iccidSize = currentBerTLV->length;
                                }
                            }
                        }
                        break;

                    case 0x9F70: // profileState
                        if (currentBerTLV->length > 0 && currentBerTLV->value != NULL)
                        {
                            if (currentBerTLV->length <= LPA_PROFILE_STATE_MAX_SIZE)
                            {
                                if(requestForPPRmanagement)
                                {
                                    memcpy(ptrPPRprofileInfoAccess->profileState, currentBerTLV->value, currentBerTLV->length);
                                    ptrPPRprofileInfoAccess->profileStateSize = currentBerTLV->length;
                                }
                                else
                                {
                                    memcpy(ptrNormalProfileInfoAccess->profileState, currentBerTLV->value, currentBerTLV->length);
                                    ptrNormalProfileInfoAccess->profileStateSize = currentBerTLV->length;
                                }                                   
                            }
                        }
                        break;

                    case 0x90: // nickname 
                        if(requestForPPRmanagement)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "WARNING! Tag 0x90 \"nickname\" detected on profile info request for PPR. Shall not happen.");
                        }
                        else
                        {
                            if (currentBerTLV->length > 0 && currentBerTLV->value != NULL)
                            {
                                if (currentBerTLV->length <= LPA_PROFILE_NICKNAME_MAX_SIZE)
                                {
                                    memcpy(ptrNormalProfileInfoAccess->profileNickname, currentBerTLV->value, currentBerTLV->length);
                                    ptrNormalProfileInfoAccess->profileNicknameSize = currentBerTLV->length;
                                }
                            }
                        }
                        break;

                    case 0x91: // serviceProviderName 
                        if(requestForPPRmanagement)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "WARNING! Tag 0x91 \"serviceProviderName\" detected on profile info request for PPR. Shall not happen.");
                        }
                        else
                        {
                            if (currentBerTLV->length > 0 && currentBerTLV->value != NULL)
                            {
                                if (currentBerTLV->length <= LPA_PROFILE_SERVICE_PROVIDER_NAME_MAX_SIZE)
                                {
                                    memcpy(ptrNormalProfileInfoAccess->serviceProviderName, currentBerTLV->value, currentBerTLV->length);
                                    ptrNormalProfileInfoAccess->serviceProviderNameSize = currentBerTLV->length;
                                }
                            }
                        }
                        break;

                    case 0x92: // profileName  
                        if(requestForPPRmanagement)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "WARNING! Tag 0x92 \"profileName\" detected on profile info request for PPR. Shall not happen.");
                        }
                        else
                        {
                            if (currentBerTLV->length > 0 && currentBerTLV->value != NULL)
                            {
                                if (currentBerTLV->length <= LPA_PROFILE_NAME_MAX_SIZE)
                                {
                                    memcpy(ptrNormalProfileInfoAccess->profileName, currentBerTLV->value, currentBerTLV->length);
                                    ptrNormalProfileInfoAccess->profileNameSize = currentBerTLV->length;
                                }
                            }
                        }
                        break;

                    case 0x93: // iconType
                        if(requestForPPRmanagement)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "WARNING! Tag 0x93 \"iconType\" detected on profile info request for PPR. Shall not happen.");
                        }
                        else
                        {
                            if (currentBerTLV->length > 0 && currentBerTLV->value != NULL)
                            {
                                if (currentBerTLV->length <= LPA_PROFILE_ICON_TYPE_MAX_SIZE)
                                {
                                    memcpy(ptrNormalProfileInfoAccess->profileIconType, currentBerTLV->value, currentBerTLV->length);
                                    ptrNormalProfileInfoAccess->profileIconTypeSize = currentBerTLV->length;
                                }
                            }
                        }
                        break;

                    case 0x94: // icon
                        if(requestForPPRmanagement)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "WARNING! Tag 0x94 \"icon\" detected on profile info request for PPR. Shall not happen.");
                        }
                        else
                        {
                            if (currentBerTLV->length > 0 && currentBerTLV->value != NULL)
                            {
                                if (currentBerTLV->length <= LPA_PROFILE_ICON_MAX_SIZE)
                                {
                                    memcpy(ptrNormalProfileInfoAccess->profileIcon, currentBerTLV->value, currentBerTLV->length);
                                    ptrNormalProfileInfoAccess->profileIconSize = currentBerTLV->length;
                                }
                            }
                        }
                        break;

                    case 0x95: // profileClass
                        if (currentBerTLV->length > 0 && currentBerTLV->value != NULL)
                        {
                            if (currentBerTLV->length <= LPA_PROFILE_CLASS_MAX_SIZE)
                            {
                                if(requestForPPRmanagement)
                                {
                                    memcpy(ptrPPRprofileInfoAccess->profileClass, currentBerTLV->value, currentBerTLV->length);
                                    ptrPPRprofileInfoAccess->profileClassSize = currentBerTLV->length;
                                }
                                else
                                {
                                    memcpy(ptrNormalProfileInfoAccess->profileClass, currentBerTLV->value, currentBerTLV->length);
                                    ptrNormalProfileInfoAccess->profileClassSize = currentBerTLV->length;
                                }
                            }
                        }
                        break;

                    case 0x99: // profilePolicyRules
                        if(requestForPPRmanagement)
                        {
                            if (currentBerTLV->length > 0 && currentBerTLV->value != NULL)
                            {
                                if (currentBerTLV->length <= LPA_PROFILE_PPR_MAX_SIZE)
                                {
                                    memcpy(ptrPPRprofileInfoAccess->profilePolicyRules, currentBerTLV->value, currentBerTLV->length);
                                    ptrPPRprofileInfoAccess->profilePolicyRulesSize = currentBerTLV->length;
                                }
                            }
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "WARNING! Tag 0x99 \"profilePolicyRules\" detected on normal profile info request. Shall not happen.");
                        }
                        break;

                    default:
                        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "WARNING! Unattended Tag 0x%X detected in profile info. Ignoring it.", currentBerTLV->tag);
                        break;
                    }
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Current BerTLV NULL, jumping on next");

                berTLVCurrent = berTLVCurrent->ptrNext;
            }

            berTLV_freeBerTLVList(berTLVList);
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_updateProfileInfoFromBerTLV() - BerTLV list NULL !");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_updateProfileInfoFromBerTLV() - Incorrect parameter(s)!");
    
    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

LPA_MEMORY_RESET_STATUS _doExtractMemoryResetResponse(const unsigned char *ptrData, size_t dataSize)
{
	LPA_MEMORY_RESET_STATUS memoryResetStatus = LPA_MEMORY_RESET_UNKNOWN;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ _doExtractMemoryResetResponse(...)");

	if (ptrData != NULL && dataSize > 0)
	{
		bool tagFound = false;
		BeerTLV* ptrBerTLVMemoryResetTag = NULL;
		BeerTLV* ptrBerTLVAttributeValue = NULL;

		ptrBerTLVMemoryResetTag = berTLV_extractTagUInt16(MEMORY_RESET_TAG, ptrData, dataSize, &tagFound);
		if (ptrBerTLVMemoryResetTag != NULL)
		{
			ptrBerTLVAttributeValue = berTLV_extractTagUInt16(0x80, ptrBerTLVMemoryResetTag->value, ptrBerTLVMemoryResetTag->length, &tagFound);
		}

		if (ptrBerTLVAttributeValue != NULL && ptrBerTLVAttributeValue->length == 1)
		{
			switch (ptrBerTLVAttributeValue->value[0])
			{
			case 0:
				memoryResetStatus = LPA_MEMORY_RESET_OK;
				break;

			case 1:
				memoryResetStatus = LPA_MEMORY_RESET_NOTHING_TO_DELETE;
				break;

			case 5:
				memoryResetStatus = LPA_MEMORY_RESET_CAT_BUSY;
				break;

			case 127:
				memoryResetStatus = LPA_MEMORY_RESET_UNDEFINED_ERROR;
				break;

			default:
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, " -> Incorrect memoryResetStatus returned by the card : 0x%02x", ptrBerTLVAttributeValue->value[0]);
				break;
			}
		}
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "BerTLV attribute Value NULL ot length not equal 1 !");

		// Do cleanup
		ERASE_BERTLV(ptrBerTLVAttributeValue);
		ERASE_BERTLV(ptrBerTLVMemoryResetTag);
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Incorrect parameter(s)!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- _doExtractMemoryResetResponse(...) : return %d", memoryResetStatus);

	return memoryResetStatus;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _doProfileOperationByIccid(PROFILE_OPERATION profileOperation, const unsigned char* ptrProfileId, size_t profileIdSize)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_doProfileOperationByIccid(...)");

	// profileOperation not checked, just an enum passed by value, not critical and managed below
        if (ptrProfileId != NULL && profileIdSize > 0)
	{
		RawDataObject* rawDataICCID = NULL;
		RawDataObject* rawDataA0 = NULL;
		RawDataObject* rawDataDGI = NULL;
		RawDataObject* rawDataA0WithRefresh = NULL;

		// create ICCID Ber TVL
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Creating ICCID BerTLV ...");

		rawDataICCID = berTLV_createAndBuildRawDataObject(PROFILE_ICCID_TAG, profileIdSize, ptrProfileId);
		if (rawDataICCID != NULL)
		{
			if (formatBytesToHexaString(rawDataICCID->rawData, rawDataICCID->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ICCID BertLV RawData : %s", _bufferFormatLogMessage);
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ICCID BertLV RawData : ...");
		}
		else
			lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);

		if (profileOperation == enableProfile || profileOperation == disableProfile)
		{
			// Adding A0 Tag + Refresh flag
			if (rawDataICCID != NULL)
			{
				// Creating A0 Tag (contains ICCID tag)
				rawDataA0 = berTLV_createAndBuildRawDataObject(0xA0, rawDataICCID->rawDataSize, rawDataICCID->rawData);
                if (formatBytesToHexaString(rawDataA0->rawData, rawDataA0->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "rawDataA0 BertLV RawData : %s", _bufferFormatLogMessage);
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "rawDataA0 BertLV RawData : ...");
			}

			if (rawDataA0 != NULL)
			{
				// Append RefreshFlag for Enable/Disable
				unsigned char refreshFlagValue = (_refreshFlagActivated ? 0xFF : 0x00);

				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Append Refreshflag with value 0x%02X", refreshFlagValue);
				RawDataObject* rawDataObjectRefreshFlag = berTLV_createAndBuildRawDataObject(0x81, 0x01, &refreshFlagValue);
				if (rawDataObjectRefreshFlag != NULL)
				{
                    if (formatBytesToHexaString(rawDataObjectRefreshFlag->rawData, rawDataObjectRefreshFlag->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "rawDataObjectRefreshFlag BertLV RawData : %s", _bufferFormatLogMessage);
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "rawDataObjectRefreshFlag BertLV RawData : ...");

					rawDataA0WithRefresh = rawDataObject_concat(rawDataA0, rawDataObjectRefreshFlag);

                    if (formatBytesToHexaString(rawDataA0WithRefresh->rawData, rawDataA0WithRefresh->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "rawDataA0WithRefresh BertLV RawData : %s", _bufferFormatLogMessage);
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "rawDataA0WithRefresh BertLV RawData : ...");

					ERASE_RAWDATAOBJECT(rawDataObjectRefreshFlag);
					rawDataObjectRefreshFlag = NULL;
				}
			}
                        else
                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
		}

		if (((profileOperation == enableProfile || profileOperation == disableProfile) && rawDataA0WithRefresh != NULL) || (profileOperation == deleteProfile && rawDataICCID != NULL))
		{
			// Include it on DGI TAG
			RawDataObject* rawDataObjectInsideDGI = NULL;
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Creating DGI BerTLV ...");

			// 
			if (profileOperation == deleteProfile)
				rawDataObjectInsideDGI = rawDataICCID;
			else
				rawDataObjectInsideDGI = rawDataA0WithRefresh;

			switch (profileOperation)
			{
				case enableProfile:
					rawDataDGI = berTLV_createAndBuildRawDataObject(ENABLE_PROFILE_DGI_TAG, rawDataObjectInsideDGI->rawDataSize, rawDataObjectInsideDGI->rawData);
				break;

				case disableProfile:
					rawDataDGI = berTLV_createAndBuildRawDataObject(DISABLE_PROFILE_DGI_TAG, rawDataObjectInsideDGI->rawDataSize, rawDataObjectInsideDGI->rawData);
				break;

				case deleteProfile:
					rawDataDGI = berTLV_createAndBuildRawDataObject(DELETE_PROFILE_DGI_TAG, rawDataObjectInsideDGI->rawDataSize, rawDataObjectInsideDGI->rawData);
				break;

				default:
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Invalid profileOperation !");
				break;
			}

			rawDataObjectInsideDGI = NULL;
			if (rawDataDGI != NULL)
			{
				if (formatBytesToHexaString(rawDataDGI->rawData, rawDataDGI->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "DGI BerTLV RawData : %s", _bufferFormatLogMessage);
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "DGI BertLV RawData : ...");
			}
                        else
                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
		}
                else
                    lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNDEFINED_ERROR);

		if (rawDataDGI != NULL)
		{
			// build and Send APDU
			uint16_t apduSW = 0x0000;
			size_t dataBufferSize = 0;

			if (buildAndSendStoreDataCase4(rawDataDGI, &apduSW, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
			{
				// Check SW = 90.00 or 91.xx
                                if ((apduSW == 0x9000) || ((apduSW & 0xFF00) == 0x9100))
				{
					// Analyze response (depending of profileOperation)
					switch (profileOperation)
					{
						case enableProfile:
							res = _extractResponseForEnableProfileOperation(_dataBuffer, dataBufferSize);
						break;

						case disableProfile:
							res = _extractResponseForDisableProfileOperation(_dataBuffer, dataBufferSize);
						break;

						case deleteProfile:
							res = _extractResponseForDeleteProfileOperation(_dataBuffer, dataBufferSize);
						break;

						default:
							lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Invalid profileOperation !");
							lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNDEFINED_ERROR);
						break;
					}
				}
				else
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
			}
                        else
                            lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INVALID_DATA_EXCHANGE);
		}

		// Do memory cleanup
		ERASE_RAWDATAOBJECT(rawDataDGI);
		ERASE_RAWDATAOBJECT(rawDataA0);
		ERASE_RAWDATAOBJECT(rawDataA0WithRefresh);
		ERASE_RAWDATAOBJECT(rawDataICCID);
	}
	else
	{
		lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
	}

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _extractResponseForEnableProfileOperation(const unsigned char *ptrData, size_t dataSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractResponseForEnableProfileOperation(...)");

	if (ptrData != NULL && dataSize > 0)
	{
		bool tagFound = false;
		BeerTLV* ptrBerTLVEnableOperationTag = NULL;
		BeerTLV* ptrBerTLVAttributeValue = NULL;

		ptrBerTLVEnableOperationTag = berTLV_extractTagUInt16(ENABLE_PROFILE_DGI_TAG, ptrData, dataSize, &tagFound);
		if (ptrBerTLVEnableOperationTag != NULL)
		{
			ptrBerTLVAttributeValue = berTLV_extractTagUInt16(0x80, ptrBerTLVEnableOperationTag->value, ptrBerTLVEnableOperationTag->length, &tagFound);
			if (ptrBerTLVAttributeValue != NULL && ptrBerTLVAttributeValue->length == 1)
			{
				int enableResult = ptrBerTLVAttributeValue->value[0];
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "enableResult:%d", enableResult);

				switch (enableResult)
				{
				case 0: // OK
					res = true;
					break;

				case 1:	// iccidOrAidNotFound
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_ICCID_OR_AID_NOT_FOUND);
					break;

				case 2: // profileNotInDisabledState
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_NOT_IN_DISABLE_STATE);
					break;

				case 3: // disallowedByPolicy
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_DISALLOWED_BY_POLICY);
					break;

				case 4: // wrongProfileReenabling
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_WRONG_PROFILE_REENABLING);
					break;

				case 5: // catBusy
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_CAT_BUSY);
					break;

				case 127: // undefinedError
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNDEFINED_ERROR);
					break;

				default:
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
					break;
				}
			}
			else
				lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
		}
		else
			lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);

		// Do cleanup
		ERASE_BERTLV(ptrBerTLVAttributeValue);
		ERASE_BERTLV(ptrBerTLVEnableOperationTag);
	}
        else
        {
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Incorrect parameter(s)!");
        }

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _extractResponseForDisableProfileOperation(const unsigned char *ptrData, size_t dataSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractResponseForDisableProfileOperation(...)");
	if (ptrData != NULL && dataSize > 0)
	{
		bool tagFound = false;
		BeerTLV* ptrBerTLVDisableOperationTag = NULL;
		BeerTLV* ptrBerTLVAttributeValue = NULL;

		ptrBerTLVDisableOperationTag = berTLV_extractTagUInt16(DISABLE_PROFILE_DGI_TAG, ptrData, dataSize, &tagFound);
		if (ptrBerTLVDisableOperationTag != NULL)
		{
			ptrBerTLVAttributeValue = berTLV_extractTagUInt16(0x80, ptrBerTLVDisableOperationTag->value, ptrBerTLVDisableOperationTag->length, &tagFound);
			if (ptrBerTLVAttributeValue != NULL && ptrBerTLVAttributeValue->length == 1)
			{
				int disableResult = ptrBerTLVAttributeValue->value[0];
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "disableResult:%d", disableResult);

				switch (disableResult)
				{
				case 0:
					res = true;
				break;
				
				case 1:	// iccidOrAidNotFound
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_ICCID_OR_AID_NOT_FOUND);
				break;
				
				case 2: // profileNotInEnabledState
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_NOT_IN_ENABLE_STATE);
				break;

				case 3: // disallowedByPolicy
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_DISALLOWED_BY_POLICY);
				break;

				case 5: // catBusy
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_CAT_BUSY);
				break;

				case 127: // undefinedError
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNDEFINED_ERROR);
				break;

				default:
					lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
				break;
				}

			}
			else
				lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
		}
		else
			lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);

		// Do cleanup
		ERASE_BERTLV(ptrBerTLVAttributeValue);
		ERASE_BERTLV(ptrBerTLVDisableOperationTag);
	}
        else
        {
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Incorrect parameter(s)!");
        }

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _extractResponseForDeleteProfileOperation(const unsigned char *ptrData, size_t dataSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractResponseForDeleteProfileOperation(...)");
	
	if (ptrData != NULL && dataSize > 0)
	{
		bool tagFound = false;
		BeerTLV* ptrBerTLVDeleteOperationTag = NULL;
		BeerTLV* ptrBerTLVAttributeValue = NULL;

		ptrBerTLVDeleteOperationTag = berTLV_extractTagUInt16(DELETE_PROFILE_DGI_TAG, ptrData, dataSize, &tagFound);
		if (ptrBerTLVDeleteOperationTag != NULL)
		{
			ptrBerTLVAttributeValue = berTLV_extractTagUInt16(0x80, ptrBerTLVDeleteOperationTag->value, ptrBerTLVDeleteOperationTag->length, &tagFound);
			if (ptrBerTLVAttributeValue != NULL && ptrBerTLVAttributeValue->length == 1)
			{
				int deleteResult = ptrBerTLVAttributeValue->value[0];
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "deleteResult:%d", deleteResult);

				switch (deleteResult)
				{
					case 0:
						res = true;
					break;

					case 1:	// iccidOrAidNotFound
						lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_ICCID_OR_AID_NOT_FOUND);
					break;

					case 2: // profileNotInDisabledState
						lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_NOT_IN_DISABLE_STATE);
					break;

					case 3: // disallowedByPolicy
						lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_DISALLOWED_BY_POLICY);
					break;

					case 127: // undefinedError
						lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNDEFINED_ERROR);
					break;

					default:
						lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
					break;
				}
			}
			else
				lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
		}
		else
			lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);

		// Do cleanup
		ERASE_BERTLV(ptrBerTLVAttributeValue);
		ERASE_BERTLV(ptrBerTLVDeleteOperationTag);
	}
        else
        {
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Incorrect parameter(s)!");
        }

	return res;
}

bool _doExtractEIDResponse(const unsigned char *ptrData, size_t dataSize, LPA_GET_EID* ptrGetEID)
{
	bool res = false;

	if (ptrData != NULL && dataSize > 0 && ptrGetEID != NULL )
	{
		BeerTLV* ptrBerTLV_EIDTag = NULL;
		BeerTLV* ptrBerTLV_5ATag = NULL;

		ptrBerTLV_EIDTag = berTLV_extractTagUInt16(GET_EID_DGI_TAG, ptrData, dataSize, NULL);
		if (ptrBerTLV_EIDTag != NULL)
		{
			ptrBerTLV_5ATag = berTLV_extractTagUInt16(0x5A, ptrBerTLV_EIDTag->value, ptrBerTLV_EIDTag->length, NULL);
			if (ptrBerTLV_5ATag != NULL)
			{
				if (ptrBerTLV_5ATag->length <= LPA_GET_EID_BUFFER_SIZE)
				{
					// Copy Data
					memcpy(ptrGetEID->EID_Data, ptrBerTLV_5ATag->value, ptrBerTLV_5ATag->length);
					ptrGetEID->EID_DataSize = ptrBerTLV_5ATag->length;

					res = true;
				}
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Buffer too small for copying raw data !");
					lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
				}
			}
		}
                else
                    lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNABLE_TO_EXTRACT_DATA);

		// Do cleanup
		ERASE_BERTLV(ptrBerTLV_5ATag);
		ERASE_BERTLV(ptrBerTLV_EIDTag);
	}
        else
        {
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Incorrect parameter(s)!");
        }

	return res;
}

/**
 * Set new Nickname field in profile identified by ICCID.
 * @param ptrProfileID Profile ICCID size, raw format
 * @param profileIdSize Profile ICCID size, size_t
 * @param ptrNickname New Nickname to set, raw hex format
 * @param nickNameSize New Nickname size, size_t - If length = 0, Nickname will be cleared
 * @return true is nickname change operation successful
 */
bool lpaManagerES10c_SetNickname(const unsigned char* ptrProfileId, size_t profileIdSize, const unsigned char* ptrNickname, size_t nickNameSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ lpaManagerES10c_SetNickname(...)");

	// Zero value case for 'nickNameSize' is accepted, allow to clear nickname
        if(ptrProfileId != NULL && profileIdSize > 0 && ptrNickname != NULL && nickNameSize >= 0 && nickNameSize <= LPA_PROFILE_NICKNAME_MAX_SIZE)
        {
            RawDataObject* rawDataICCID = NULL;
            RawDataObject* rawDataNewNickname = NULL;
            RawDataObject* rawDataSetNickmameData = NULL;
            RawDataObject* rawDataSetNicknameRequest = NULL;

            // Create ICCID Ber TVL
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Creating ICCID and Nickname BerTLV ...");

            rawDataICCID = berTLV_createAndBuildRawDataObject(PROFILE_ICCID_TAG, profileIdSize, ptrProfileId);
            if (rawDataICCID != NULL)
            {
                if (formatBytesToHexaString(rawDataICCID->rawData, rawDataICCID->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ICCID BertLV RawData : %s", _bufferFormatLogMessage);
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ICCID BertLV RawData : ...");
            }
            else
                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
            
            // Create Nickname Ber TVL
            rawDataNewNickname = berTLV_createAndBuildRawDataObject(PROFILE_NICKNAME_TAG, nickNameSize, ptrNickname);
            if (rawDataNewNickname != NULL)
            {
                if (formatBytesToHexaString(rawDataNewNickname->rawData, rawDataNewNickname->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Nickname BertLV RawData : %s", _bufferFormatLogMessage);
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Nickname BertLV RawData : ...");
            }
            else
                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
            
            if(rawDataICCID != NULL && rawDataNewNickname != NULL)
            {
                // Concatenate command data's
                rawDataSetNickmameData = rawDataObject_concat(rawDataICCID, rawDataNewNickname);
                
                if(rawDataSetNickmameData != NULL)
                {
                    // Generate setNickname request
                    rawDataSetNicknameRequest = berTLV_createAndBuildRawDataObject(SET_NICKNAME_TAG, rawDataSetNickmameData->rawDataSize, rawDataSetNickmameData->rawData);
                    if(rawDataSetNicknameRequest != NULL)
                    {
                        if (formatBytesToHexaString(rawDataSetNicknameRequest->rawData, rawDataSetNicknameRequest->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Set Nickname request : %s", _bufferFormatLogMessage);
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Set Nickname request : ...");
                    }
                    else
                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                }
                else
                    lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
            }
            
            // If request build OK, send it
            if(rawDataSetNicknameRequest != NULL)
            {
                // build and Send APDU
                uint16_t apduSW = 0x0000;
                size_t dataBufferSize = 0;

                if (buildAndSendStoreDataCase4(rawDataSetNicknameRequest, &apduSW, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
                {
                    // Check SW = 90.00 or 91.xx
                    if ((apduSW == 0x9000) || ((apduSW & 0xFF00) == 0x9100))
                    {
                        res = _extractResponseForSetNicknameOperation(_dataBuffer, dataBufferSize);
                    }
                    else
                        lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
                }
                else
                    lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INVALID_DATA_EXCHANGE);
            }
            
            // Memory cleanup
            ERASE_RAWDATAOBJECT(rawDataICCID);
            ERASE_RAWDATAOBJECT(rawDataNewNickname);
            ERASE_RAWDATAOBJECT(rawDataSetNickmameData);
            ERASE_RAWDATAOBJECT(rawDataSetNicknameRequest);
        }
        else
        {
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "lpaManagerES10c_SetNickname: Incorrect parameters(s)");
        }
 
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- lpaManagerES10c_SetNickname() : return %s", (res ? "true" : "false"));
	return res;
}

/**
 * Analyzes response to ES10c SetNickname request and report execution status
 * @param ptrData Pointer on response returned buy eUICC, raw format
 * @param dataSize Size of response returned by eUICC  
 * @return true if response report operation successful
 */
bool _extractResponseForSetNicknameOperation(const unsigned char *ptrData, size_t dataSize)
{
    bool res = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractResponseForSetNicknameOperation(...)");

    if (ptrData != NULL && dataSize > 0)
    {
        bool tagFound = false;
        BeerTLV* ptrBerTLVSetNicknameOperationTag = NULL;
        BeerTLV* ptrBerTLVAttributeValue = NULL;

        ptrBerTLVSetNicknameOperationTag = berTLV_extractTagUInt16(SET_NICKNAME_TAG, ptrData, dataSize, &tagFound);
        if (ptrBerTLVSetNicknameOperationTag != NULL)
        {
            ptrBerTLVAttributeValue = berTLV_extractTagUInt16(0x80, ptrBerTLVSetNicknameOperationTag->value, ptrBerTLVSetNicknameOperationTag->length, &tagFound);
            if (ptrBerTLVAttributeValue != NULL && ptrBerTLVAttributeValue->length == 1)
            {
                int setNicknameResult = ptrBerTLVAttributeValue->value[0];
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "setNicknameResult:%d", setNicknameResult);

                switch (setNicknameResult)
                {
                case 0: // OK
                    res = true;
                    break;

                case 1:	// iccidOrAidNotFound
                    lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_ICCID_OR_AID_NOT_FOUND);
                    break;

                case 127: // undefinedError
                    lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_UNDEFINED_ERROR);
                    break;

                default:
                    lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
                    break;
                }
            }
            else
                lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
        }
        else
            lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);

        // Do cleanup
        ERASE_BERTLV(ptrBerTLVAttributeValue);
        ERASE_BERTLV(ptrBerTLVSetNicknameOperationTag);
    }
    else
    {
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_extractResponseForSetNicknameOperation: Incorrect parameter(s)!");
    }

    return res;
}
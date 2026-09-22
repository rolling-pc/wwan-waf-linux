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

#include "lpasdk/core/lpa_manager_es10b.h"
#include "lpasdk/core/lpa_manager.h"
#include "lpasdk/core/lpa_manager_helper.h"
#include "lpasdk/core/semedia_manager.h"

#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/rawdata_object.h"
#include "lpasdk/core/util.h"
#include "lpasdk/lpasdk_internal_api.h"
#include "lpasdk/core/lpa_memory.h"


/////////////////////////////////////////////

#define LPA_MANAGER_ES10B_DATA_BUFFER_MAX_SIZE MAX_LPA_MANAGER_APDU_BUFFER_SIZE
static unsigned char _dataBuffer[LPA_MANAGER_ES10B_DATA_BUFFER_MAX_SIZE];

static char _bufferFormatLogMessage[1024];	// 1Ko is enough (to increase it, use dynamic memory allocation)


#define GET_EUICC_CHALLENGE_DGI_TAG		0xBF2E
#define PREPARE_DOWNLOAD_DGI_TAG		0xBF21
#define AUTH_SERVER_DGI_TAG			0xBF38
#define CANCEL_SESSION_DGI_TAG                  0xBF41
#define GET_EUICC_INFO_DGI_TAG			0xBF20
#define GET_RAT_DGI_TAG                         0xBF43

bool _prepareDownloadTlv(ptr_serverData p_serverData, const char * ptrStringHashCC, RawDataObject ** ptrPrepareDownloadTlv);
bool _checkBoundProfilePackageMainObjects(BeerTLV* p_berTLV_BF36);
bool _sendInitalizeSecureChannel(BeerTLV* p_berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors);
bool _sendConfigureISDP(const BeerTLV* berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors);
bool _sendStoreMetaData(const BeerTLV* berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors);
bool _sendReplaceSessionKey(const BeerTLV* berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors);
bool _loadProfileElements(const BeerTLV* berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors);
bool _sendBoundProfileRawDataOject(RawDataObject* rawDataObj, PROFILE_INSTALLATION_RESULT *pir);
bool _extract_PIRdataFromEUICCresponse(PROFILE_INSTALLATION_RESULT *pir, unsigned char * dataFromEUICC, const size_t dataFromEUICC_size);
bool _prepareAuthenticateServerTlv(ptr_serverData p_serverData, RawDataObject * ptrCtxParam, RawDataObject ** ptrAuthServerTlv);
bool _isPIRContainsSuccessResult(PROFILE_INSTALLATION_RESULT *pir);

bool _isValidAuthenticateServerResponse(unsigned char * ptrRawData, size_t rawDataSize, size_t maxRawDataSize);
bool _isValidPrepareDownloadResponse(unsigned char * ptrRawData, size_t rawDataSize, size_t maxRawDataSize);
bool _checkAndExtractEuiccChallenge(unsigned char * ptrRawData, size_t rawDataSize, LPA_GET_EUICC* ptrGetEuicc);
bool _extractDataFromSmdpSigned2(unsigned char* ptrSmdpSigned2, size_t smdpSigned2Len, SMDP_SIGNED2_DATA *ptrSmd2Signed2Detail);

bool _prepareCancelSessionTlv(const char * transactionID, const unsigned int p_reasonCode, RawDataObject** ptrRawDataCancelSessionRequest);
bool _isValidCancelSessionResponse(unsigned char * ptrResponse, size_t rawResponseSize);

bool _storeHexBase64StructureRawDataPair(RawDataObject ** ptrHexElement, RawDataObject ** ptrBase64Element, const unsigned char * ptrHexData, const size_t hexDataSize, const size_t maxDataSize);

/////////////////////////////////////////////
// ES10b part
/////////////////////////////////////////////

bool lpaManagerES10b_PrepareDownload(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, const char * ptrStringHashCC, PREPARE_DOWNLOAD_RESPONSE* ptrPrepareDownloadResp)
{
	bool res = false, isError = false;
	RawDataObject * prepareDownloadTlv = NULL;
	
	SMDP_SIGNED2_DATA smdpSigned2Detail;
	
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES10b_PrepareDownload...");
        
        if((p_serverData != NULL) && (ptrStringHashCC != NULL) && (ptrPrepareDownloadResp != NULL))
        {
                memset(&smdpSigned2Detail, 0x0, sizeof(SMDP_SIGNED2_DATA));

                if (p_serverData->_smdpSigned2.val != NULL)
                {
                        // Check if Confirmation code required by SmdpSigned2
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Extracting data from SmdpSigned2 ...");

                        if (_extractDataFromSmdpSigned2(p_serverData->_smdpSigned2.val, p_serverData->_smdpSigned2.len, &smdpSigned2Detail))
                        {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Data extracted from SmdpSigned2");

                                if (smdpSigned2Detail.ptrRawDataObjectTLV_transactionId != NULL)
                                        lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, NULL, "TLV transactionId", smdpSigned2Detail.ptrRawDataObjectTLV_transactionId->rawData, smdpSigned2Detail.ptrRawDataObjectTLV_transactionId->rawDataSize);

                                if (smdpSigned2Detail.ptrRawDataObjectTLV_ccRequiredFlag != NULL)
                                        lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, NULL, "TLV ccRequiredFlag", smdpSigned2Detail.ptrRawDataObjectTLV_ccRequiredFlag->rawData, smdpSigned2Detail.ptrRawDataObjectTLV_ccRequiredFlag->rawDataSize);

                                if (smdpSigned2Detail.ptrRawDataObjectTLV_bppEuiccOtpk != NULL)
                                        lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, NULL, "TLV bppEuiccOtpk", smdpSigned2Detail.ptrRawDataObjectTLV_bppEuiccOtpk->rawData, smdpSigned2Detail.ptrRawDataObjectTLV_bppEuiccOtpk->rawDataSize);
                        }
                        else
                        {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "unable to extract data from SmdpSigned2 !");
                                isError = true;
                        }

                        if (!isError && _prepareDownloadTlv(p_serverData, ptrStringHashCC, &prepareDownloadTlv))
                        {
                                if ((prepareDownloadTlv->rawData != NULL) && (prepareDownloadTlv->rawDataSize >0))
                                {
                                        uint16_t sw = 0x0000;
                                        size_t dataBufferSize = 0;
                                        if (buildAndSendStoreDataCase4(prepareDownloadTlv, &sw, _dataBuffer, LPA_AUTHENTICATE_SERVER_MAX_SIZE, &dataBufferSize)) {
                                                // Check 90.00 or 91.xx
                                                if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                                                {
                                                    // Check if PrepareDownloadResponse contains downloadResponseOk or downloadResponseError
                                                    if (_isValidPrepareDownloadResponse(_dataBuffer, dataBufferSize, LPA_AUTHENTICATE_SERVER_MAX_SIZE))
                                                    {
                                                        if(_storeHexBase64StructureRawDataPair(&(ptrPrepareDownloadResp->ptrPrepareDownloadResponse), 
                                                                                               &(ptrPrepareDownloadResp->ptrPrepareDownloadResponse_Base64),
                                                                                               _dataBuffer, dataBufferSize, LPA_AUTHENTICATE_SERVER_MAX_SIZE))
                                                            res = true;
                                                    }
                                                    else
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "PrepareDownload response does not contain downloadResponseOk !");
                                                }
                                                else
                                                {
                                                    // No or Invalid SW
                                                    lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                                                }
                                        }
                                        else
                                                lpaSetErrorCode(LPA_ERROR_INVALID_PREPARE_DOWNLOAD_RESPONSE);
                                }
                                else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect prepare download TLV data!");
                        }
                        else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to prepare download TLV!");
                }
                else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid server data!");


                // Cleanup memory
                ERASE_RAWDATAOBJECT(smdpSigned2Detail.ptrRawDataObjectTLV_transactionId);
                ERASE_RAWDATAOBJECT(smdpSigned2Detail.ptrRawDataObjectTLV_ccRequiredFlag);
                ERASE_RAWDATAOBJECT(smdpSigned2Detail.ptrRawDataObjectTLV_bppEuiccOtpk);
                
                ERASE_RAWDATAOBJECT(prepareDownloadTlv);
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter!");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _extractDataFromSmdpSigned2(unsigned char* ptrSmdpSigned2, size_t smdpSigned2Len, SMDP_SIGNED2_DATA *ptrSmd2Signed2Detail)
{
	bool res = false;

	// SmdpSigned2 :: = SEQUENCE{
	//   transactionId[0] TransactionId, --The TransactionID generated by the SM - DP + 
	//   ccRequiredFlag BOOLEAN, --Indicates if the Confirmation Code is required 
	//   bppEuiccOtpk[APPLICATION 73] OCTET STRING OPTIONAL -- otPK.EUICC.ECKA already used for binding the BPP, tag '5F49' 
	// }

	if (ptrSmdpSigned2 != NULL && smdpSigned2Len > 0 && ptrSmd2Signed2Detail != NULL)
	{
		BeerTLV *ptrBerTLV30 = NULL;
		BerTLVList* ptrTlvList = NULL;

		ptrBerTLV30 = berTLV_extractTagUInt16(0x30, ptrSmdpSigned2, smdpSigned2Len, NULL);
		if (ptrBerTLV30 != NULL)
		{
			uint8_t countTlvFound = 0;
			ptrTlvList = berTLV_extractList(ptrBerTLV30->value, ptrBerTLV30->length, &countTlvFound);
			if (ptrTlvList != NULL )
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Smdp2 contains %d TLV => extracting data ...", countTlvFound);

				if (countTlvFound >= 2 )
				{
					BeerTLV *ptrBerTlv_transactionId = NULL, *ptrBerTlv_ccRequiredFlag = NULL, *ptrBerTlv_bppEuiccOtpk = NULL;
					BerTLVList* ptrCurrentTlvItem = ptrTlvList;
					bool isError = false;
                                        
					// Item [0] : transactionId
					if (ptrCurrentTlvItem != NULL)
					{
						if (ptrCurrentTlvItem->berTLV != NULL)
							ptrBerTlv_transactionId = ptrCurrentTlvItem->berTLV;

						if (ptrBerTlv_transactionId != NULL)
						{
							ptrSmd2Signed2Detail->ptrRawDataObjectTLV_transactionId = berTLV_buildRawDataObject(ptrBerTlv_transactionId);
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Smdp2 -> transactionId extracted ");
						}
						else
							isError = true;

						ptrCurrentTlvItem = ptrCurrentTlvItem->ptrNext;
					}

					// Item [1] : ccRequiredFlag
					if (ptrCurrentTlvItem != NULL)
					{
						if (ptrTlvList->berTLV != NULL)
							ptrBerTlv_ccRequiredFlag = ptrCurrentTlvItem->berTLV;

						if (ptrBerTlv_ccRequiredFlag != NULL)
						{
							ptrSmd2Signed2Detail->ptrRawDataObjectTLV_ccRequiredFlag = berTLV_buildRawDataObject(ptrBerTlv_ccRequiredFlag);
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Smdp2 -> ccRequiredFlag extracted ");
						}
						else
							isError = true;

						ptrCurrentTlvItem = ptrCurrentTlvItem->ptrNext;
					}

					// Item [2] : bppEuiccOtpk
					if (ptrCurrentTlvItem != NULL)
					{
						if (ptrTlvList->berTLV != NULL)
							ptrBerTlv_bppEuiccOtpk = ptrCurrentTlvItem->berTLV;

						if (ptrBerTlv_bppEuiccOtpk != NULL)
						{
							ptrSmd2Signed2Detail->ptrRawDataObjectTLV_bppEuiccOtpk = berTLV_buildRawDataObject(ptrBerTlv_bppEuiccOtpk);
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Smdp2 -> bppEuiccOtpk extracted");
						}
						else
							isError = true;

						ptrCurrentTlvItem = ptrCurrentTlvItem->ptrNext;
					}

					if (!isError)
						res = true;
				}
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "one or more TLV missing on SmdpSigned2 item !");
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to extract TLVs from SmdpSigned2 item !");
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to extract TLV from SmdpSigned2 item !");

		// Memory cleanup
		ERASE_BERTLV_LIST(ptrTlvList);
		ERASE_BERTLV(ptrBerTLV30);
	}
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter!");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool lpaManagerES10b_LoadBoundProfilePackage(ptr_serverData p_serverData, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES10b_LoadBoundProfilePackage...");
	
	bool isError = false, res = false;
	bool isTagFound_BF36 = false;
	BeerTLV* berTLV_BF36 = NULL;

        if((p_serverData != NULL) && (pir != NULL) && (cancelForBPPerrors != NULL))
        {
            *cancelForBPPerrors = false;
            
            if (!isError)
            {	
                    // Step 01 : Analyze BF36 Tag
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Step 01 : Analyze BF36 tag ...");
                    if (p_serverData != NULL && p_serverData->_boundProfilePackage.val != NULL && p_serverData->_boundProfilePackage.len > 0)
                    {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "p_serverData->_boundProfilePackage.len found: %lu", CAST_SIZET_PLATFORM(p_serverData->_boundProfilePackage.len));
                            
                            berTLV_BF36 = berTLV_extractTagUInt16(0xBF36, p_serverData->_boundProfilePackage.val, p_serverData->_boundProfilePackage.len, &isTagFound_BF36);

                            if (berTLV_BF36 == NULL || berTLV_BF36->length == 0)
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "berTLV_BF36 not found or empty");
                                    lpaSetErrorCode(LPA_ERROR_INVALID_BPP_DATA);
                                    *cancelForBPPerrors = true;
                                    isError = true;
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "berTLV_BF36 object length found: %lu", CAST_SIZET_PLATFORM(berTLV_BF36->length));
                    }
                    else
                    {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "p_serverData (or internal element) is NULL !");
                            lpaSetErrorCode(LPA_ERROR_FAILED_LOAD_BPP);
                            isError = true;
                    }
            }

            // Step 02 : Check Bound Profile package main objects (Unknown / empty tags detection)
            if (!isError)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Step 02 : Check Bound Profile Package main objects...");
                
                *cancelForBPPerrors = ! _checkBoundProfilePackageMainObjects(berTLV_BF36);
                if(*cancelForBPPerrors)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Bound Profile Package main objects checking failed");
                    lpaSetErrorCode(LPA_ERROR_INVALID_BPP_DATA);
                    isError = true;
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Bound Profile Package main objects checked successfully");
            }
            
            // Step 03 : Send Init Secure Channel
            if (!isError)
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Step 03 : Send Init Secure Channel ...");
                    if (_sendInitalizeSecureChannel(berTLV_BF36, pir, cancelForBPPerrors))
                    {
                            if (pir->hasResult)
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send InitializeSecureChannel (PIR has result)");
                                    lpaSetErrorCode(LPA_ERROR_FAILED_INITIAL_SECURITY_CHANNEL);
                                    isError = true;
                            }
                    }
                    else
                    {
                        if(*cancelForBPPerrors)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error detected in tag BF23 data object");
                            lpaSetErrorCode(LPA_ERROR_INVALID_BPP_DATA);
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send InitializeSecureChannel ");
                            lpaSetErrorCode(LPA_ERROR_FAILED_INITIAL_SECURITY_CHANNEL);
                        }
                        isError = true;
                    }
            }

            // Step 04 : Send Configure ISDP
            if (!isError )
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Step 04 : Send Configure ISDP ...");
                    if (_sendConfigureISDP(berTLV_BF36, pir, cancelForBPPerrors))
                    {
                            if (pir->hasResult)
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send Configure ISDP (PIR has result)");
                                    lpaSetErrorCode(LPA_ERROR_FAILED_CONFIGURE_ISDP);
                                    isError = true;
                            }
                    }
                    else
                    {
                        if(*cancelForBPPerrors)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error detected in tag A0 data object");
                            lpaSetErrorCode(LPA_ERROR_INVALID_BPP_DATA);
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send Configure ISDP ");
                            lpaSetErrorCode(LPA_ERROR_FAILED_CONFIGURE_ISDP);
                        }
                        isError = true;
                    }
            }

            // Step 05 : Send Store Metadata
            if (!isError)
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Step 05 : Send Store Metadata ...");
                    if (_sendStoreMetaData(berTLV_BF36, pir, cancelForBPPerrors))
                    {
                            if (pir->hasResult)
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send Store Metadata (PIR has result)");
                                    lpaSetErrorCode(LPA_ERROR_FAILED_STORE_META_DATA);
                                    isError = true;
                            }
                    }
                    else
                    {
                        if(*cancelForBPPerrors)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error detected in tag A1 data object");
                            lpaSetErrorCode(LPA_ERROR_INVALID_BPP_DATA);
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send Store Metadata");
                            lpaSetErrorCode(LPA_ERROR_FAILED_STORE_META_DATA);
                        }
                        isError = true;
                    }
            }

            // Step 06 : Send replace Session Key
            if (!isError)
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Step 06 : Send replace Session Key ...");
                    if (_sendReplaceSessionKey(berTLV_BF36, pir, cancelForBPPerrors))
                    {
                            if (pir->hasResult)
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send replace Session Key (PIR has result)");
                                    lpaSetErrorCode(LPA_ERROR_FAILED_REPLACE_SESSION_KEY);
                                    isError = true;
                            }
                    }
                    else
                    {
                        if(*cancelForBPPerrors)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error detected in tag A2 data object");
                            lpaSetErrorCode(LPA_ERROR_INVALID_BPP_DATA);
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send replace Session Key");
                            lpaSetErrorCode(LPA_ERROR_FAILED_REPLACE_SESSION_KEY);
                        }
                        isError = true;
                    }
            }

            // Step 07 : load Profile elements
            if (!isError)
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Step 07 : load Profile elements ...");
                    if (_loadProfileElements(berTLV_BF36, pir, cancelForBPPerrors))
                    {
                            if (!pir->hasResult || pir->ptrProfileInstallationResultTlv == NULL)
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " No Profile Installation Result! ");
                                    lpaSetErrorCode(LPA_ERROR_FAILED_LOAD_PROFILE_ELEMENTS);
                            }
                            else
                            {
                                    if (!_isPIRContainsSuccessResult(pir))    
                                    {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to load profile elements (PIR contains error)");
                                            lpaSetErrorCode(LPA_ERROR_FAILED_LOAD_PROFILE_ELEMENTS);
                                    }
                            }
                    }
                    else
                    {
                        if(*cancelForBPPerrors)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error detected in tag A3 data object");
                            lpaSetErrorCode(LPA_ERROR_INVALID_BPP_DATA);
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to load profile elements ");
                            lpaSetErrorCode(LPA_ERROR_FAILED_LOAD_PROFILE_ELEMENTS);
                        }
                        isError = true;
                    }
            }
            
            // Do memory cleanup
            ERASE_BERTLV(berTLV_BF36);
            
            if (!isError)
                res = true;
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter!");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }

	return res;
}

/**
 * Check PIR response
 * @param pir PIR data structure "PROFILE_INSTALLATION_RESULT" type
 * @return True if Successful parsing and "successful result" tag found
 */
bool _isPIRContainsSuccessResult(PROFILE_INSTALLATION_RESULT *pir)
{
	bool isOk = false;

	bool isTagFound_BF37 = false, isTagFound_BF27 = false;
	BeerTLV *ptrBerTLV_BF37 = NULL, *ptrBerTLV_BF27 = NULL;
	BerTLVList* ptrBerTlvList = NULL;
        
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_isPIRContainsSuccessResult()");
        
        if(pir != NULL)
        {
            ptrBerTLV_BF37 = berTLV_extractTagUInt16(0xBF37, pir->ptrProfileInstallationResultTlv->rawData, pir->ptrProfileInstallationResultTlv->rawDataSize, &isTagFound_BF37);
            if (ptrBerTLV_BF37 != NULL && ptrBerTLV_BF37->length > 0)
            {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "BER-TLV BF37 present and not empty => analyze it");

                    ptrBerTLV_BF27 = berTLV_extractTagUInt16(0xBF27, ptrBerTLV_BF37->value, ptrBerTLV_BF37->length, &isTagFound_BF27);
                    if (ptrBerTLV_BF27 != NULL && ptrBerTLV_BF27->length > 0)
                    {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "BER-TLV BF27 present and not empty =>analyze it");

                            uint8_t countTlvTag = 0;
                            ptrBerTlvList = berTLV_extractList(ptrBerTLV_BF27->value, ptrBerTLV_BF27->length, &countTlvTag);
                            if (ptrBerTlvList != NULL && countTlvTag > 0)
                            {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "<%d> BER-TLV present inside BER-TLV BF27", countTlvTag);

                                    BerTLVList* ptrBerTLVCurrentInsideBF27 = ptrBerTlvList;
                                    BeerTLV* ptrBerTLV_A2 = NULL;

                                    while (ptrBerTLVCurrentInsideBF27 != NULL)
                                    {
                                            if (ptrBerTLVCurrentInsideBF27->berTLV == NULL)
                                                    break;

                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "TLV tag <%04X> present (%d bytes)", ptrBerTLVCurrentInsideBF27->berTLV->tag, ptrBerTLVCurrentInsideBF27->berTLV->length);

                                            if (ptrBerTLVCurrentInsideBF27->berTLV->tag == 0xA2)
                                            {
                                                    // final result tag found :)
                                                    ptrBerTLV_A2 = ptrBerTLVCurrentInsideBF27->berTLV;
                                                    break;
                                            }

                                            ptrBerTLVCurrentInsideBF27 = ptrBerTLVCurrentInsideBF27->ptrNext;
                                    }

                                    if (ptrBerTLV_A2 != NULL)
                                    {
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Analyzing 'finalResult' Tag ...");

                                            // check if A0 (successResultTag) or A1 (errorResultTag) tag present
                                            BeerTLV* ptrBerTLV_A2_A0 = berTLV_extractTagUInt16(0xA0, ptrBerTLV_A2->value, ptrBerTLV_A2->length, NULL);
                                            BeerTLV* ptrBerTLV_A2_A1 = berTLV_extractTagUInt16(0xA1, ptrBerTLV_A2->value, ptrBerTLV_A2->length, NULL);

                                            if (ptrBerTLV_A2_A0 != NULL || ptrBerTLV_A2_A1 != NULL)
                                            {
                                                    if (ptrBerTLV_A2_A0 != NULL)
                                                    {
                                                            if (formatBytesToHexaString(ptrBerTLV_A2_A0->value, ptrBerTLV_A2_A0->length, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SuccessResult Tag A0 present (%d bytes) => %s", ptrBerTLV_A2_A0->length, _bufferFormatLogMessage);
                                                            else
                                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SuccessResult Tag A0 present (%d bytes) => ...", ptrBerTLV_A2_A0->length);

                                                            isOk = true;
                                                    }

                                                    if (ptrBerTLV_A2_A1 != NULL)
                                                    {
                                                            if (formatBytesToHexaString(ptrBerTLV_A2_A1->value, ptrBerTLV_A2_A1->length, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ErrorResult Tag A1 present (%d bytes) => %s", ptrBerTLV_A2_A1->length, _bufferFormatLogMessage);
                                                            else
                                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ErrorResult Tag A1 present (%d bytes) => ...", ptrBerTLV_A2_A1->length);
                                                    }
                                            }

                                            // Cleanup memory
                                            ERASE_BERTLV(ptrBerTLV_A2_A0);
                                            ERASE_BERTLV(ptrBerTLV_A2_A1);
                                    }
                                    else
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Tag A2 not found !");
                                    }
                            }
                    }
            }

            // cleanup memory
            ERASE_BERTLV(ptrBerTLV_BF27);
            ERASE_BERTLV(ptrBerTLV_BF37);
            ERASE_BERTLV_LIST(ptrBerTlvList);
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_isPIRContainsSuccessResult(): Invalid parameter!");
        
	return isOk;
}


/////////////////////////////////////////////
//
/////////////////////////////////////////////
bool lpaManagerES10b_GetEuiccChallenge(LPA_GET_EUICC* ptrGetEUICC)
{
	bool res = false;
	
	size_t dataBufferSize = 0;
	uint16_t sw = 0x0000;
	RawDataObject* rawDataObjectGetEuiccChallenge = NULL;

	if (ptrGetEUICC != NULL)
	{
		rawDataObjectGetEuiccChallenge = berTLV_createAndBuildRawDataObject(GET_EUICC_CHALLENGE_DGI_TAG, 0, NULL);
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "do buildAndSendStoreDataCase4(GetEuiccChallenge) ...");
		if (rawDataObjectGetEuiccChallenge != NULL)
		{
			if (buildAndSendStoreDataCase4(rawDataObjectGetEuiccChallenge, &sw, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
			{
				// Check if SW=90.00 or 91.xx
				if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SW=90.00 => Extracting data");

					if (dataBufferSize > 0)
					{
						if (dataBufferSize <= LPA_GET_EUICC_BUFFER_MAX_SIZE)
						{
							if (_checkAndExtractEuiccChallenge(_dataBuffer, dataBufferSize, ptrGetEUICC))
                                                        {
                                                            res = true;
                                                        }
                                                        else
							{
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid Raw Data for EuiccChallenge !");
                                                            lpaSetErrorCode(LPA_ERROR_INVALID_GET_UICC_CHALLENGE);
							}
						}
						else
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Buffer too small for copying raw data !");
							lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
						}
					}
					else
					{
						// No data = error too
						lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No Raw data available !");
                                                lpaSetErrorCode(LPA_ERROR_INVALID_GET_UICC_CHALLENGE);
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
				lpaSetErrorCode(LPA_ERROR_INVALID_GET_UICC_CHALLENGE);
			}

			// Do memory cleanup
			ERASE_RAWDATAOBJECT(rawDataObjectGetEuiccChallenge);
		}
		else
		{
			lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
		}
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

bool lpaManagerES10b_GetEuiccInfo(LPA_GET_EUICC* ptrGetEUICC)
{
    bool res = false;

    if (ptrGetEUICC != NULL)
    {
        size_t dataBufferSize = 0;
        uint16_t sw = 0x0000;
        RawDataObject* ptrRawDataObjectGetEuiccInfo = NULL;

        ptrRawDataObjectGetEuiccInfo = berTLV_createAndBuildRawDataObject(GET_EUICC_INFO_DGI_TAG, 0, NULL);
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "do buildAndSendStoreDataCase4(GetEuiccInfo) ...");
        if (ptrRawDataObjectGetEuiccInfo != NULL)
        {
            if (buildAndSendStoreDataCase4(ptrRawDataObjectGetEuiccInfo, &sw, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
            {
                // Check if SW=90.00 or 91.xx
                if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SW=90.00 => Extracting data");

                    if (dataBufferSize > 0)     // Also tested below, but allow here to produce a specific error if needed
                    {
                        if(_storeHexBase64StructureRawDataPair(&(ptrGetEUICC->ptrEUICC), &(ptrGetEUICC->prtEUICC_Base64), _dataBuffer, dataBufferSize, LPA_GET_EUICC_BUFFER_MAX_SIZE))
                            res = true;
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No Raw data available !");
                        lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_INFO);
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
                lpaSetErrorCode(LPA_ERROR_INVALID_GET_EUICC_INFO);
            }

            // Do memory cleanup
            ERASE_RAWDATAOBJECT(ptrRawDataObjectGetEuiccInfo);
        }
        else
        {
            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
        }
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

bool lpaManagerES10b_AuthenticateServer(ptr_serverData p_serverData, const LPA_EventCallback* ptrLpaEventCallback, RawDataObject * ptrCtxParam, AUTHENTICATE_SERVER_RESPONSE* ptrAuthServerResp)
{
	bool res = false;

	RawDataObject * authServerTlv = NULL;
	
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES10b_AuthenticateServer()...");
        
        if((p_serverData != NULL) && (ptrCtxParam != NULL) && (ptrAuthServerResp != NULL))
	{
            if (_prepareAuthenticateServerTlv(p_serverData, ptrCtxParam, &authServerTlv))
            {
                if (authServerTlv != NULL) 
                {
                    uint16_t sw = 0x0000;
                    size_t dataBufferSize = 0;

                    if (buildAndSendStoreDataCase4(authServerTlv, &sw, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
                    {
                        // Check if SW=90.00 or 91.xx
                        if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                        {
                            // Check if AuthenticateServerResponse contains authenticateResponseOk or authenticateResponseError
                            if (_isValidAuthenticateServerResponse(_dataBuffer, dataBufferSize, LPA_AUTHENTICATE_SERVER_MAX_SIZE))
                            {

                                if(_storeHexBase64StructureRawDataPair(&(ptrAuthServerResp->ptrAuthenticateServerResponse),
                                                                       &(ptrAuthServerResp->ptrAuthenticateServerResponse_Base64),
                                                                       _dataBuffer, dataBufferSize, LPA_AUTHENTICATE_SERVER_MAX_SIZE))
                                    res = true;
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Authenticate server response does not contain authenticateResponseOk !");
                        }
                        else 
                        {
                            // No or Invalid SW
                            lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                        }
                    }
                    else
                        lpaSetErrorCode(LPA_ERROR_INVALID_AUTHENTICATE_SERVER_RESPONSE);
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Returned authServerTlv object is void !");
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to prepareAuthenticateServerTlv !");

            ERASE_RAWDATAOBJECT(authServerTlv);
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter!");
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        }

	return res;
}

bool _isValidAuthenticateServerResponse(unsigned char * ptrRawData, size_t rawDataSize, size_t maxRawDataSize)
{
	bool isValid = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_isValidAuthenticateServerResponse()...");

	if ((ptrRawData != NULL) && (rawDataSize > 0) && (rawDataSize <= maxRawDataSize))
	{
		bool isTagFound_BF38 = false;
		BeerTLV* berTLV_BF38 = berTLV_extractTagUInt16(0xBF38, ptrRawData, rawDataSize, &isTagFound_BF38);
		if (berTLV_BF38 != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "<BF38> tag present ");

			uint8_t countTLVFound = 0;
			BerTLVList* BerTLVListInsideBF38 = berTLV_extractList(berTLV_BF38->value, berTLV_BF38->length, &countTLVFound);

			if (BerTLVListInsideBF38 != NULL)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "%d tag present inside BF38", countTLVFound );

				// Normally only one TLV present
				if (countTLVFound == 1)
				{
					switch (BerTLVListInsideBF38->berTLV->tag)
					{
						case 0xA1: // authenticateResponseError
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "authenticateResponseError detected");
						break;

						case 0xA0:  // authenticateResponseOk
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "authenticateResponseOk detected");
							isValid = true;
						break;
						
						default:
							lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid authenticateResponse Tag !");
						break;
					}
				}
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Only one tag must be present inside BF38 !");

				berTLV_freeBerTLVList(BerTLVListInsideBF38);
				BerTLVListInsideBF38 = NULL;
			}
			berTLV_freeBerTLV(berTLV_BF38);
			berTLV_BF38 = NULL;
		}
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Tag BF38 not found !");
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter!");

	return isValid;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _isValidPrepareDownloadResponse(unsigned char * ptrRawData, size_t rawDataSize, size_t maxRawDataSize)
{
	bool isValid = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_isValidPrepareDownloadResponse()...");

	if ((ptrRawData != NULL) && (rawDataSize > 0) && (rawDataSize <= maxRawDataSize))
	{
		bool isTagFound_BF21 = false;
		BeerTLV* berTLV_BF21 = berTLV_extractTagUInt16(0xBF21, ptrRawData, rawDataSize, &isTagFound_BF21);
		if (berTLV_BF21 != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "<BF21> tag present ");

			uint8_t countTLVFound = 0;
			BerTLVList* BerTLVListInsideBF21 = berTLV_extractList(berTLV_BF21->value, berTLV_BF21->length, &countTLVFound);

			if (BerTLVListInsideBF21 != NULL)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "%d tag present inside BF21", countTLVFound);

				// Normally only one TLV present
				if (countTLVFound == 1)
				{
					switch (BerTLVListInsideBF21->berTLV->tag)
					{
						case 0xA1: // downloadResponseError
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "downloadResponseError detected");
						break;

						case 0xA0:  // downloadResponseOk
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "downloadResponseOk detected");
							isValid = true;
						break;

						default:
							lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid downloadResponse Tag !");
						break;
					}
				}
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Only one tag must be present inside BF21 !");

				berTLV_freeBerTLVList(BerTLVListInsideBF21);
				BerTLVListInsideBF21 = NULL;
			}
			berTLV_freeBerTLV(berTLV_BF21);
			berTLV_BF21 = NULL;
		}
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Tag BF21 not found !");
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter or given data size out of bounds!");

	return isValid;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _checkAndExtractEuiccChallenge(unsigned char * ptrRawData, size_t rawDataSize, LPA_GET_EUICC* ptrGetEuicc)
{
	bool res = false;
        
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_checkAndExtractEuiccChallenge()...");

	if (ptrRawData != NULL && rawDataSize > 0 && ptrGetEuicc != NULL)
	{
		BeerTLV* berTLV_BF2E = NULL;
		bool isTagFound_BF2E = false;

		berTLV_BF2E = berTLV_extractTagUInt16(0xBF2E, ptrRawData, rawDataSize, &isTagFound_BF2E);
		if (berTLV_BF2E != NULL)
		{

			snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "TLV tag<%04x> (%lu bytes) :", berTLV_BF2E->tag, CAST_SIZET_PLATFORM(berTLV_BF2E->length));
			lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "TLV", berTLV_BF2E->value, berTLV_BF2E->length);

			bool isTagFound_80 = false;
			BeerTLV* berTLV_80 = berTLV_extractTagUInt8(0x80, berTLV_BF2E->value, berTLV_BF2E->length, &isTagFound_80);
			if (berTLV_80 != NULL)
			{
				snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "TLV tag<%04x> (%lu bytes) :", berTLV_80->tag, CAST_SIZET_PLATFORM(berTLV_80->length));
				lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "TLV", berTLV_80->value, berTLV_80->length);

                                if(_storeHexBase64StructureRawDataPair(&(ptrGetEuicc->ptrEUICC), &(ptrGetEuicc->prtEUICC_Base64), berTLV_80->value, 
                                                                       berTLV_80->length, LPA_GET_EUICC_BUFFER_MAX_SIZE))
                                    res = true;

				berTLV_freeBerTLV(berTLV_80);
				berTLV_80 = NULL;
			}

			berTLV_freeBerTLV(berTLV_BF2E);
			berTLV_BF2E = NULL;
		}
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Tag BF2E not found !");
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid parameter!");

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _prepareDownloadTlv(ptr_serverData p_serverData, const char * ptrStringHashCC, RawDataObject ** ptrPrepareDownloadTlv)
{
	bool res = false;
        int convLen = 32;
        unsigned char TLVConfirmationCodeHash[34];

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_prepareDownloadTlv() ...");


	// Verify mandatory parameters
	if (p_serverData != NULL && p_serverData->_smdpSigned2.len >0 && p_serverData->_smdpSignature2.len >0 && p_serverData->_smdpCertificate.len > 0 && 
            ptrStringHashCC != NULL && ptrPrepareDownloadTlv != NULL)
	{
		// Container for smdpSigned2, smdpSignature2, hashCc and smdpCertificate
		bool error = false;
                ERASE_RAWDATAOBJECT(*ptrPrepareDownloadTlv);

		RawDataObject* ptrRawData = rawDataObject_allocate();
		if (ptrRawData != NULL)
		{
			if (!rawDataObject_appendRawDataArray(ptrRawData, p_serverData->_smdpSigned2.val, p_serverData->_smdpSigned2.len))
				error = true;

			if (!error &&!rawDataObject_appendRawDataArray(ptrRawData, p_serverData->_smdpSignature2.val, p_serverData->_smdpSignature2.len) )
				error = true;

                        if (!error)
                        {
                            if(strlen(ptrStringHashCC) == 64)
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code hash with correct length provided, adding hashCc TLV.");

                                if(hexStr2ByteArray((unsigned char *)ptrStringHashCC, 64, TLVConfirmationCodeHash + 2, &convLen))
                                {
                                    // TLV = 04 20 hash32bytes
                                    TLVConfirmationCodeHash[0] = 0x04;
                                    TLVConfirmationCodeHash[1] = 0x20;

                                    if(!rawDataObject_appendRawDataArray(ptrRawData, TLVConfirmationCodeHash, 34))
                                        error = true;
                                }
                                else
                                    error = true;
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Confirmation Code hash empty or incorrect length, hashCc TLV not added.");
                        }
                        else
                            error = true;

                        if (!error && !rawDataObject_appendRawDataArray(ptrRawData, p_serverData->_smdpCertificate.val, p_serverData->_smdpCertificate.len))
                                error = true;

                        if (!error)
                        {
                            if(ptrRawData->rawDataSize < LPA_AUTHENTICATE_SERVER_MAX_SIZE)
                            {
                                *ptrPrepareDownloadTlv = berTLV_createAndBuildRawDataObject(PREPARE_DOWNLOAD_DGI_TAG, ptrRawData->rawDataSize, ptrRawData->rawData);
                                if ((*ptrPrepareDownloadTlv != NULL) && ((*ptrPrepareDownloadTlv)->rawDataSize > 0))
                                {
                                    snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "_prepareDownloadTlv() => request generated (%u bytes) : ...", (int) ((*ptrPrepareDownloadTlv)->rawDataSize));
                                    lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "PrepareDownloadRequest", (*ptrPrepareDownloadTlv)->rawData, (*ptrPrepareDownloadTlv)->rawDataSize);

                                    res = true;
                                }
                                else
                                {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to allocate ptrPrepareDownloadTlv or incorrect length !");
                                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                }
                            }
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "ptrPrepareDownloadTlv out of bounds: %d needed, %d max allowed", ptrRawData->rawDataSize, LPA_AUTHENTICATE_SERVER_MAX_SIZE - 1);
                                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                            }
                        }
                        else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error when updating ptrRawData !");
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to allocate ptrRawData !");
			lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
		}

                ERASE_RAWDATAOBJECT(ptrRawData);
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s) !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_prepareDownloadTlv() return %s", (res ? LPA_RES_TRUE_STRING : LPA_RES_FALSE_STRING));

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Check if any unknown tag is not defined between the first tag (Tags BF23, A0 to A3) for GCF testing, and if regular tags are not empty
 * @param p_berTLV_BF36 Pointer on BerTLV object containing Bound Profile Package
 * @return true if checking is successful
 */
bool _checkBoundProfilePackageMainObjects(BeerTLV* p_berTLV_BF36)
{
    bool res = false;
    
    BerTLVList * berTLVlistBPPdataContainerBase = NULL;
    uint8_t tlvListBPPdataContainerCount = 0;
    BerTLVList * berTLVlisBPPdataContainerParser = NULL;
    BeerTLV * berTLVlistBPPdataContainerCurrent = NULL;
    
    bool objectA3notReached = true;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_checkBoundProfilePackageMainObjects()");
    
    if(p_berTLV_BF36 != NULL)
    {
        // Generate main objects list
        berTLVlistBPPdataContainerBase = berTLV_extractList(p_berTLV_BF36->value, p_berTLV_BF36->length, &tlvListBPPdataContainerCount);
        // At least 4 mandatory objects must be found (BF23, A0, A1 & A3)
        if(berTLVlistBPPdataContainerBase != NULL && tlvListBPPdataContainerCount > 3)
        {
            // Will presume Bound Profile Package is OK. If any error is detected, will be turned to false.
            res = true;
            
            berTLVlisBPPdataContainerParser = berTLVlistBPPdataContainerBase;
            // Scan all objects, until end reached or A3 (sequenceOf86, payload) found or error detected
            while(berTLVlisBPPdataContainerParser != NULL && objectA3notReached && res)
            {
                berTLVlistBPPdataContainerCurrent = berTLVlisBPPdataContainerParser->berTLV;
                
                switch(berTLVlistBPPdataContainerCurrent->tag)
                {
                    case 0xBF23:
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Object BF23 (InitialiseSecureChannelRequest) found");
                        if(berTLVlistBPPdataContainerCurrent->length < 1)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Object BF23 empty, canceling download...");
                            res = false;
                        }
                        break;
                    
                    case 0xA0:
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Object A0 (firsSequenceOf87) found");
                        if(berTLVlistBPPdataContainerCurrent->length < 2)       // At least one segment header
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Object A0 size too small, canceling download...");
                            res = false;
                        }
                        break;
                    
                    case 0xA1:
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Object A1 (SequenceOf88) found");
                        if(berTLVlistBPPdataContainerCurrent->length < 2)       // At least one segment header
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Object A1 size too small, canceling download...");
                            res = false;
                        }
                        break;

                    case 0xA2:
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Object A2 (secondSequenceOf87) found");
                        if(berTLVlistBPPdataContainerCurrent->length < 2)       // At least one segment header
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Object A2 size too small, canceling download...");
                            res = false;
                        }
                        break;
                        
                    case 0xA3:
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Object A3 (sequenceOf86) found, do not check for more possible unknown tags in Bound Profile Package");
                        objectA3notReached = false;
                        
                        if(berTLVlistBPPdataContainerCurrent->length < 2)       // At least one segment header
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Object A3 size too small, canceling download...");
                            res = false;
                        }
                        break;
                        
                    default:
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Object with unknown tag \"%X\" found, canceling download...", berTLVlistBPPdataContainerCurrent->tag);
                        res = false;
                        break;
                }
                
                berTLVlisBPPdataContainerParser = berTLVlisBPPdataContainerParser->ptrNext;
            }
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot generate main BPP objects list or not enough element found (%d, at least 4 needed)", tlvListBPPdataContainerCount);
        
        // Memory cleanup
        ERASE_BERTLV_LIST(berTLVlistBPPdataContainerBase);
        berTLVlisBPPdataContainerParser = NULL;
        berTLVlistBPPdataContainerCurrent = NULL;
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s) !");
    
    return res;
}

bool _sendInitalizeSecureChannel(BeerTLV* p_berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "sendInitalizeSecureChannel()");

    RawDataObject* rawDataObjectBF36Header = NULL;
    RawDataObject* rawDataObjectBF23Data = NULL;
    RawDataObject* rawDataObjectInitializeSecureChannel = NULL;

    if((p_berTLV_BF36 != NULL) && (pir != NULL) && (cancelForBPPerrors != NULL))
    {
        bool isTagFound_BF23 = false;
        if (formatBytesToHexaString(p_berTLV_BF36->value, p_berTLV_BF36->length, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Tag <BF36> found ==> value (%d bytes) = %s", p_berTLV_BF36->length, _bufferFormatLogMessage);
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Tag <BF36> found ==> value (%d bytes) = ... ", p_berTLV_BF36->length);

        unsigned char BF36Header[20];
        unsigned char lenHexStr[10];
        size_t headsize = 0;
        // Note: encodeLength() result not tested here, normally failure reasons (lenHexStr NULL / too small) shall never happen
        encodeLength(p_berTLV_BF36->length, lenHexStr, sizeof(lenHexStr), &headsize);
        headsize += 2;//include the tag size

        // Note: Normally headsize shall never be < 3 but leave this as initially written
        if (headsize > 0)
        {
            BF36Header[0] = 0xBF;
            BF36Header[1] = 0x36;
            memcpy(BF36Header + 2, lenHexStr, headsize);
            rawDataObjectBF36Header = rawDataObject_create(BF36Header, headsize);
        }

        //BF23 TLV
        BeerTLV* berTLV_BF23 = NULL;
        berTLV_BF23 = berTLV_extractTagUInt16(0xBF23, p_berTLV_BF36->value, p_berTLV_BF36->length, &isTagFound_BF23);

        if (berTLV_BF23 != NULL) 
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "BF23.getTag() = %02X", berTLV_BF23->tag);
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "BF23.getValueLength() = %d", berTLV_BF23->length);

            rawDataObjectBF23Data = berTLV_createAndBuildRawDataObject(berTLV_BF23->tag, berTLV_BF23->length, berTLV_BF23->value);
            ERASE_BERTLV(berTLV_BF23);

            if (rawDataObjectBF23Data != NULL) 
            {
                rawDataObjectInitializeSecureChannel = rawDataObject_concat(rawDataObjectBF36Header, rawDataObjectBF23Data);
                ERASE_RAWDATAOBJECT(rawDataObjectBF36Header);
                ERASE_RAWDATAOBJECT(rawDataObjectBF23Data);

                if (rawDataObjectInitializeSecureChannel != NULL)
                {
                    if (_sendBoundProfileRawDataOject(rawDataObjectInitializeSecureChannel, pir))
                    {
                        res = true;
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send InitializeSecureChannel");

                    ERASE_RAWDATAOBJECT(rawDataObjectInitializeSecureChannel);
                }
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to generate rawDataObjectInitializeSecureChannel!");
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "BF23 TLV not found! ");
            // Incorrect BPP structure detected, initiate Cancel Session
            *cancelForBPPerrors = true;
        }
    }
    else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s) !");

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _sendBoundProfileRawDataOject(RawDataObject* rawDataObj, PROFILE_INSTALLATION_RESULT *pir)
{
	bool res = false;
        
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_sendBoundProfileRawDataOject()");
        
	if (rawDataObj != NULL && pir != NULL)
	{
		//build and send APDU
		uint16_t apduSW = 0x0000;
		size_t dataBufferSize = 0;

		if (buildAndSendStoreDataCase4(rawDataObj, &apduSW, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Send APDU raw data object success => check APDU response ...");

			// Check SW = 90.00 or 91.xx
			if((apduSW == 0x9000) || ((apduSW & 0xFF00) == 0x9100))
			{
                if (dataBufferSize == 0)
                {
                    res = true;
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Send APDU raw data object success !");
                }
                else
                {
                    if(_extract_PIRdataFromEUICCresponse(pir, _dataBuffer, dataBufferSize))
                    {
                        if (_isPIRContainsSuccessResult(pir))
                            res = true;
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " Failed to _sendBoundProfileRawDataOject");
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " Failed to extract / convert PIR data!");
                }
			}
            else
                lpaSetErrorCode(LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE);
		}
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s) !");

	return res;
}

/**
 * Extract PIR data from eUICC response and store it in PROFILE_INSTALLATION_RESULT structure (HEX and BASE64 format)
 * @param pir Structure that will receive PIR, PROFILE_INSTALLATION_RESULT type
 * @param dataFromEUICC Data recovered from eUICC
 * @param dataFromEUICC_size Data recovered from eUICC size, must be greater than 0 and smaller than LPA_PIR_BUFFER_SIZE
 * @return True if extraction and storage is successful
 */
bool _extract_PIRdataFromEUICCresponse(PROFILE_INSTALLATION_RESULT *pir, unsigned char * dataFromEUICC, const size_t dataFromEUICC_size)
{
    bool res = false;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extract_PIRdataFromEUICCresponse()");
    
    if((pir != NULL) && (dataFromEUICC != NULL) && (dataFromEUICC_size > 0))
    {
        pir->hasResult = false;

        // Other pir structure elements are initialized and filled below
        if(_storeHexBase64StructureRawDataPair(&(pir->ptrProfileInstallationResultTlv), &(pir->ptrProfileInstallationResultTlv_Base64), dataFromEUICC, 
                                               dataFromEUICC_size, LPA_PIR_BUFFER_MAX_SIZE))
        {
            pir->hasResult = true;
            res = true;
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s) !");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }
    
    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _sendConfigureISDP(const BeerTLV* berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors)
{
	bool res = false;
	bool isTagFound_A0 = false;
	BeerTLV* berTLV_A0 = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ConfigureISDP(...)");

	RawDataObject* rawDataObjectConfigureISDP = NULL;
        
        if((berTLV_BF36 != NULL) && (pir != NULL) && (cancelForBPPerrors != NULL))
        {
            //A0 configure ISDP
            berTLV_A0 = berTLV_extractTagUInt8(0xA0, berTLV_BF36->value, berTLV_BF36->length, &isTagFound_A0);

            if (berTLV_A0 != NULL)
            {
                // Check that object A0 contain a segment 87 like it shall be
                if(berTLV_A0->value[0] == 0x87)
                {
                    snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "Tag <A0> found ==> value (%lu bytes) :", CAST_SIZET_PLATFORM(berTLV_A0->length));
                    lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "<A0>", berTLV_A0->value, berTLV_A0->length);

                    rawDataObjectConfigureISDP = berTLV_createAndBuildRawDataObject(berTLV_A0->tag, berTLV_A0->length, berTLV_A0->value);
                    if (rawDataObjectConfigureISDP != NULL)
                    {
                            if (_sendBoundProfileRawDataOject(rawDataObjectConfigureISDP, pir))
                            {
                                    res = true;
                            }
                            else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send Configure ISDP");
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to generate rawDataObjectConfigureISDP!");
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No segment 0x87 found in A0 TLV object!");
                    // Incorrect BPP structure detected, initiate Cancel Session
                    *cancelForBPPerrors = true;
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "A0 TLV not found! ");
                // Incorrect BPP structure detected, initiate Cancel Session
                *cancelForBPPerrors = true;
            }

            ERASE_BERTLV(berTLV_A0);
            ERASE_RAWDATAOBJECT(rawDataObjectConfigureISDP);
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s) !");

	return res;
}
/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _sendStoreMetaData(const BeerTLV* berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "StoreMetaData(...)");

    bool isTagFound_A1 = false;
    BeerTLV *berTLV_A1 = NULL;
    BerTLVList* berTLVListInsideA1 = NULL;
    RawDataObject* rawDataObjectA1 = NULL, *rawDataObjectHeader = NULL, *rawDataObjectStoreMetaData = NULL;
    uint8_t countTLVFoundInsideA1 = 0;
    int countTLV88 = 0;

    if((berTLV_BF36 != NULL) && (pir != NULL) && (cancelForBPPerrors != NULL))
    {
        //A1 storeMetaData
        berTLV_A1 = berTLV_extractTagUInt8(0xA1, berTLV_BF36->value, berTLV_BF36->length, &isTagFound_A1);

        if (berTLV_A1 != NULL)
        {
            snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "Tag <A1> found ==> value (%lu bytes) :", CAST_SIZET_PLATFORM(berTLV_A1->length));
            lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "<A1>", berTLV_A1->value, berTLV_A1->length);

            rawDataObjectA1 = berTLV_createAndBuildRawDataObject(berTLV_A1->tag, berTLV_A1->length, berTLV_A1->value);
            unsigned char A1Header[20];
            unsigned char lenHexStr[10];
            size_t headsize = 0;
            // Note: encodeLength() result not tested here, normally failure reasons (lenHexStr NULL / too small) shall never happen
            encodeLength(berTLV_A1->length, lenHexStr, sizeof(lenHexStr), &headsize);
            headsize += 1; //include the A1 tag size

            // Note: Normally headsize shall never be < 2, but could be also greater, minimum "88" segment size not evaluated yet
            if (headsize > 1) 
            {
                    memcpy(A1Header, rawDataObjectA1->rawData, headsize);
                    rawDataObjectHeader = rawDataObject_create(A1Header, headsize);
            }
            // Note: If too small value for headsize rawDataObjectHeader will be = NULL so next step will be not performed

            if (rawDataObjectHeader != NULL) 
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Sending \"A1\" header containing sequence of \"88\" segments...");
                if (_sendBoundProfileRawDataOject(rawDataObjectHeader, pir)) 
                {
                    berTLVListInsideA1 = berTLV_extractList(berTLV_A1->value, berTLV_A1->length, &countTLVFoundInsideA1);

                    // Try to found one or more "88" BerTLV objects in "A1" BerTLV
                    if(berTLVListInsideA1 != NULL && countTLVFoundInsideA1 > 0)
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "countTLVFoundInsideA1 : %d", countTLVFoundInsideA1);

                        // Look inside list
                        BerTLVList* berTLVCurrentInsideA1 = berTLVListInsideA1;
                        while (berTLVCurrentInsideA1 != NULL)
                        {
                            // Check for tag 88. If yes send segment to eUICC
                            if (berTLVCurrentInsideA1->berTLV != NULL && berTLVCurrentInsideA1->berTLV->tag == 0x88)
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "===============================================");
                                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Sending Segment \"88\" >>>> #%d", ++countTLV88);

                                res = false;    // Reset flag in case one "88" segment already successfully sent

                                rawDataObjectStoreMetaData = berTLV_createAndBuildRawDataObject(berTLVCurrentInsideA1->berTLV->tag, berTLVCurrentInsideA1->berTLV->length, berTLVCurrentInsideA1->berTLV->value);
                                if (rawDataObjectStoreMetaData != NULL) 
                                {
                                    if (_sendBoundProfileRawDataOject(rawDataObjectStoreMetaData, pir)) 
                                    {
                                        res = true;
                                    }
                                    else
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send StoreMetaData");

                                        // Avoid memory leak in case of failure
                                        ERASE_RAWDATAOBJECT(rawDataObjectStoreMetaData);

                                        // Any failure to send a segment will stop A1 object parsing, avoid return successful result if sending
                                        // of possible next segment worked without returning any error
                                        break;
                                    }

                                    // Clear it immediately after use else will create memory leak if multiple segments used
                                    ERASE_RAWDATAOBJECT(rawDataObjectStoreMetaData);
                                }
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot create RawDataObject for segment \"88\" sending!");
                                    lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                    break;
                                }
                            }
                            else
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unexpected TLV found in TLV A1! (NULL or not \"88\" tag)");
                                // Incorrect BPP structure detected, stop segments parsing and initiate Cancel Session
                                *cancelForBPPerrors = true;
                                res = false;
                                break;
                            }

                            berTLVCurrentInsideA1 = berTLVCurrentInsideA1->ptrNext;
                        } // End of while loop for segments ==============

                        // In case parsing loop interrupted due to segment data error or sending problem, free pointer before BerTLV list berTLVListInsideA1 cleanup
                        berTLVCurrentInsideA1 = NULL;

                        if(countTLV88 < 1)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Not any TLV \"88\" found in \"A1\" object!");
                            // Incorrect BPP structure detected, initiate Cancel Session
                            *cancelForBPPerrors = true;
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Not any TLV found in \"A1\" object or failed to create list");
                        // Incorrect BPP structure detected, initiate Cancel Session
                        *cancelForBPPerrors = true;
                    }
                }
                else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed send A1 header to eUICC!");
            }
            else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed create A1 rawData Object!");
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "A1 TLV not found!");
            // Incorrect BPP structure detected, initiate Cancel Session
            *cancelForBPPerrors = true;
        }

        ERASE_BERTLV(berTLV_A1);
        ERASE_BERTLV_LIST(berTLVListInsideA1);

        ERASE_RAWDATAOBJECT(rawDataObjectStoreMetaData); // Normally useless, but added by security
        ERASE_RAWDATAOBJECT(rawDataObjectA1);
        ERASE_RAWDATAOBJECT(rawDataObjectHeader);
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "StoreMetaData(): Incorrect parameter(s) !");

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _sendReplaceSessionKey(const BeerTLV* berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors)
{
	bool res = false;
	bool isTagFound_A2 = false;
	BeerTLV* berTLV_A2 = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Replace Session Key(...)");

	RawDataObject* rawDataObjectReplaceSessionKey = NULL;

        if((berTLV_BF36 != NULL) && (pir != NULL) && (cancelForBPPerrors != NULL))
        {
            //A1 storeMetaData
            berTLV_A2 = berTLV_extractTagUInt8(0xA2, berTLV_BF36->value, berTLV_BF36->length, &isTagFound_A2);
            if (berTLV_A2 != NULL)
            {
                // Check that object A2 contain a segment 87 like it shall be
                if(berTLV_A2->value[0] == 0x87)
                {
                    snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "Tag <A2> found ==> value (%lu bytes) :", CAST_SIZET_PLATFORM(berTLV_A2->length));
                    lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "<A2>", berTLV_A2->value, berTLV_A2->length);

                    rawDataObjectReplaceSessionKey = berTLV_createAndBuildRawDataObject(berTLV_A2->tag, berTLV_A2->length, berTLV_A2->value);
                    if (rawDataObjectReplaceSessionKey != NULL)
                    {
                            if (_sendBoundProfileRawDataOject(rawDataObjectReplaceSessionKey, pir))
                            {
                                    res = true;
                            }
                            else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send Replace Session Key");
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to generate rawDataObjectReplaceSessionKey");
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No segment 0x87 found in A2 TLV object!");
                    // Incorrect BPP structure detected, initiate Cancel Session
                    *cancelForBPPerrors = true;
                }
            }
            else
            {
                    res = true;
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No Replace Session Key");
            }

            ERASE_BERTLV(berTLV_A2);
            ERASE_RAWDATAOBJECT(rawDataObjectReplaceSessionKey);
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect parameter(s) !");

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////
bool _loadProfileElements(const BeerTLV* berTLV_BF36, PROFILE_INSTALLATION_RESULT *pir, bool * cancelForBPPerrors)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Load Profile Elements(...)");

    if ((berTLV_BF36 != NULL) && (pir !=NULL) && (cancelForBPPerrors != NULL))
    {
        bool isTagFound_A3 = false;
        BeerTLV* berTLV_A3 = NULL;

        RawDataObject* rawDataObjectHeader = NULL;
        RawDataObject* rawDataObjectProfileElement = NULL;

        //A1 storeMetaData
        berTLV_A3 = berTLV_extractTagUInt8(0xA3, berTLV_BF36->value, berTLV_BF36->length, &isTagFound_A3);

        if (berTLV_A3 != NULL)
        {
            snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "Tag <A3> found ==> value (%lu bytes) :", CAST_SIZET_PLATFORM(berTLV_A3->length));
            lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "<A3>", berTLV_A3->value, berTLV_A3->length);

            unsigned char lenHexStr[10];    // Not used but needed for encodeLength()
            size_t headsize = 0;
            // Note: encodeLength() result not tested here, normally failure reasons (lenHexStr NULL / too small) shall never happen
            //       This function is here only used to know how many bytes will be used to encode the length in "A3Header"
            encodeLength(berTLV_A3->length, lenHexStr, sizeof(lenHexStr), &headsize);
            headsize += 1; //include the A3 tag size

            // Note: Normally headsize shall never be found < 2 but leaved this as initially written, add code robustness
            if (headsize > 1)
            {
                unsigned char A3Header[20];
                A3Header[0] = (unsigned char)(berTLV_A3->tag & 0x00FF); // Cast / mask avoid warnings with some compilers
                switch (headsize)
                {
                    case 2:
                        A3Header[1] = (unsigned char)berTLV_A3->length;
                    break;

                    case 3:
                        A3Header[1] = 0x81;
                        A3Header[2] = (unsigned char)berTLV_A3->length;
                    break;

                    case 4:
                        A3Header[1] = 0x82;
                        A3Header[2] = (berTLV_A3->length >> 8) & 0xFF;
                        A3Header[3] = berTLV_A3->length & 0x00FF;
                    break;

                    case 5:
                        A3Header[1] = 0x83;
                        A3Header[2] = (berTLV_A3->length >> 16) & 0xFF;
                        A3Header[3] = (berTLV_A3->length >> 8) & 0x00FF;
                        A3Header[4] = berTLV_A3->length & 0x0000FF;
                    break;
                }

                rawDataObjectHeader = rawDataObject_create(A3Header, headsize);
            }


            if (rawDataObjectHeader != NULL)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Sending \"A3\" header containing sequence of \"86\" segments...");
                if (_sendBoundProfileRawDataOject(rawDataObjectHeader, pir))
                {
                    // davy
                    ERASE_RAWDATAOBJECT(rawDataObjectHeader);

                    // Read all TLV present inside A3 Tag
                    uint8_t countTLVFoundInsideA3 = 0;
                    BerTLVList* ber86TLVList = berTLV_extractList(berTLV_A3->value, berTLV_A3->length, &countTLVFoundInsideA3);

                    ERASE_BERTLV(berTLV_A3);

                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "countTLVFoundInsideA3 : %d", countTLVFoundInsideA3);
                    int countTLV86 = 0;

                    if (ber86TLVList != NULL && countTLVFoundInsideA3 > 0)
                    {
                        BerTLVList* berTLVCurrentInside86 = ber86TLVList;
                        RawDataObject* ptrRawData86 = NULL;
                        BerTLVList* berTLVNextInside86 = NULL;
                        
                        // Turnaround to detect invalid segment before beginning to send segments 0x86 (Case of Cancel Session not possible after first segment send)
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,"Check segments before sending them to eUICC");
                        while (berTLVCurrentInside86 != NULL)
                        {
                            berTLVNextInside86 = berTLVCurrentInside86->ptrNext;
                            
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Checking segment #%d", ++countTLV86);
                            
                            if (berTLVCurrentInside86->berTLV == NULL || berTLVCurrentInside86->berTLV->tag != 0x86)
                            {
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unexpected TLV found in TLV A3! (NULL or not \"86\" tag)");
                                // Incorrect BPP structure detected, stop segment parsing loop and initiate Cancel Session
                                *cancelForBPPerrors = true;
                                break;
                            }
                            
                            berTLVCurrentInside86 = berTLVNextInside86;
                        }
                            
                        // Begin to send segment to eUICC after successful segments checking
                        if(! *cancelForBPPerrors)
                        {
                            berTLVCurrentInside86 = ber86TLVList;
                            berTLVNextInside86 = NULL;
                            countTLV86 = 0;

                            while (berTLVCurrentInside86 != NULL)
                            {
                                berTLVNextInside86 = berTLVCurrentInside86->ptrNext;

                                if (berTLVCurrentInside86->berTLV != NULL && berTLVCurrentInside86->berTLV->tag == 0x86)
                                {
                                    ptrRawData86 = berTLV_createAndBuildRawDataObject(berTLVCurrentInside86->berTLV->tag, berTLVCurrentInside86->berTLV->length, berTLVCurrentInside86->berTLV->value);
                                    if (ptrRawData86 != NULL) 
                                    {
                                        uint16_t sw = 0x0000;
                                        size_t dataBufferSize = 0;
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "===============================================");
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Sending Segment \"86\" >>>> #%d", ++countTLV86);
                                        if (buildAndSendStoreDataCase4(ptrRawData86, &sw, _dataBuffer, LPA_PIR_BUFFER_MAX_SIZE, &dataBufferSize))
                                        {
                                            // Check if SW = 90.00 or 91.xx
                                            if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                                            {
                                                if (dataBufferSize > 0)
                                                {
                                                    if(_extract_PIRdataFromEUICCresponse(pir, _dataBuffer, dataBufferSize))
                                                    {
                                                        if (_isPIRContainsSuccessResult(pir))
                                                            res = true;
                                                        else
                                                        {
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to load profile elements!");
                                                            lpaSetErrorCode(LPA_ERROR_FAILED_LOAD_PROFILE_ELEMENTS);
                                                        }
                                                    }
                                                    else
                                                    {
                                                        // Error code will be set by _extract_ConvertB64_PIRdataFromEUICCresponse()
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, " Failed to extract / convert PIR data!");
                                                    }

                                                    if(pir->hasResult)
                                                    {
                                                        if (formatBytesToHexaString(pir->ptrProfileInstallationResultTlv->rawData, pir->ptrProfileInstallationResultTlv->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile Installation Result ==> value (%d bytes) = %s", pir->ptrProfileInstallationResultTlv->rawDataSize, _bufferFormatLogMessage);
                                                        else
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Profile Installation Result  (%d bytes) = ... ", pir->ptrProfileInstallationResultTlv->rawDataSize);
                                                    }

                                                    if (countTLV86 != countTLVFoundInsideA3)
                                                    {
                                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Not reach the last segment for loading!");
                                                        break;
                                                    }
                                                }
                                                else
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No data response available for this segment...");
                                            }
                                            else
                                            {
                                                lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            lpaSetErrorCode(LPA_ERROR_FAILED_LOAD_BPP);
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to create Raw Data for Tag 86!");
                                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                        break;
                                    }

                                    ERASE_RAWDATAOBJECT(ptrRawData86);
                                }
                                else
                                {
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unexpected TLV found in TLV A3! (NULL or not \"86\" tag) - SHALL NOT HAPPEN AT THIS STEP");
                                    // Incorrect BPP structure detected, stop segment parsing loop and initiate Cancel Session
                                    *cancelForBPPerrors = true;
                                    break;
                                }

                                berTLVCurrentInside86 = berTLVNextInside86;
                            } // End of while loop for segments ==============
                        }

                        // Memory cleanup even if loop terminated in error
                        ERASE_RAWDATAOBJECT(ptrRawData86);
                        // Release intermediate pointers from the memory zone they point on
                        berTLVNextInside86 = NULL;
                        berTLVCurrentInside86 = NULL;

                        if(countTLV86 < 1)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Not any TLV \"86\" found in \"A3\" object!");
                            // Incorrect BPP structure detected, initiate Cancel Session
                            *cancelForBPPerrors = true;
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Not any TLV found in \"A3\" object or failed to build list of Tag 86 Segments");
                        // Incorrect BPP structure detected, initiate Cancel Session
                        *cancelForBPPerrors = true;
                    }

                    ERASE_BERTLV_LIST(ber86TLVList);
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to send TLV A3 header to eUICC!");
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed create A3 rawData Object");
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "A3 TLV not found!");
            // Incorrect BPP structure detected, initiate Cancel Session
            *cancelForBPPerrors = true;
        }

        ERASE_BERTLV(berTLV_A3);
        ERASE_RAWDATAOBJECT(rawDataObjectProfileElement);
        ERASE_RAWDATAOBJECT(rawDataObjectHeader);
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Load Profile Elements: Incorrect parameter(s) !");

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

bool _prepareAuthenticateServerTlv(ptr_serverData p_serverData, RawDataObject * ptrCtxParam, RawDataObject ** ptrAuthServerTlv)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_prepareAuthenticateServerTlv() ...");

	// Verify mandatory parameters. For ptrAuthServerTlv a pointer container address must be provided
	if (p_serverData != NULL && ptrCtxParam != NULL && ptrAuthServerTlv != NULL &&
		p_serverData->_serverSigned1.val != NULL && p_serverData->_serverSigned1.len > 0 && 
		p_serverData->_serverSignature1.val != NULL && p_serverData->_serverSignature1.len >0 && 
		p_serverData->_euiccCiPKIdToBeUsed.val != NULL && p_serverData->_euiccCiPKIdToBeUsed.len > 0 && 
		p_serverData->_serverCertificate.val != NULL && p_serverData->_serverCertificate.len > 0 && 
		ptrCtxParam->rawData != NULL && ptrCtxParam->rawDataSize > 0 )
	{
		bool error = false;
		RawDataObject* ptrRawDataAuthenticateServer = NULL;
		RawDataObject* ptrRawData = rawDataObject_allocate();
		
		if (ptrRawData != NULL)
		{
			if (!rawDataObject_appendRawDataArray(ptrRawData, p_serverData->_serverSigned1.val, p_serverData->_serverSigned1.len ))
				error = true;

			if (!error &&!rawDataObject_appendRawDataArray(ptrRawData, p_serverData->_serverSignature1.val, p_serverData->_serverSignature1.len))
				error = true;

			if (!error &&!rawDataObject_appendRawDataArray(ptrRawData, p_serverData->_euiccCiPKIdToBeUsed.val, p_serverData->_euiccCiPKIdToBeUsed.len))
				error = true;

			if (!error &&!rawDataObject_appendRawDataArray(ptrRawData, p_serverData->_serverCertificate.val, p_serverData->_serverCertificate.len))
				error = true;

			if (!error &&!rawDataObject_appendRawDataArray(ptrRawData, ptrCtxParam->rawData, ptrCtxParam->rawDataSize))
				error = true;

			if (!error)
			{
				ptrRawDataAuthenticateServer = berTLV_createAndBuildRawDataObject(AUTH_SERVER_DGI_TAG, ptrRawData->rawDataSize, ptrRawData->rawData);
				if (ptrRawDataAuthenticateServer != NULL)
				{
					// Update structure
					if (ptrRawDataAuthenticateServer->rawDataSize <= LPA_AUTHENTICATE_SERVER_MAX_SIZE)
					{
                                                ERASE_RAWDATAOBJECT(*ptrAuthServerTlv);
                                                *ptrAuthServerTlv = rawDataObject_create(ptrRawDataAuthenticateServer->rawData, ptrRawDataAuthenticateServer->rawDataSize);

						if(*ptrAuthServerTlv != NULL)
                                                {
                                                    snprintf(_bufferFormatLogMessage, sizeof(_bufferFormatLogMessage), "_prepareAuthenticateServerTlv() => request generated (%u bytes) : ...", (int) ((*ptrAuthServerTlv)->rawDataSize));
                                                    lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, _bufferFormatLogMessage, "AuthenticateServerRequest", (*ptrAuthServerTlv)->rawData, (*ptrAuthServerTlv)->rawDataSize);

                                                    res = true;
                                                }
					}
					else
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "ptrRawDataAuthenticateServer->rawData too small to be updated !");
						lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
					}
				}
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to allocate ptrRawDataAuthenticateServer !");
					lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
				}
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Error when updating ptrRawData !");
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Unable to allocate ptrRawData !");
			lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
		}

		// Cleanup memory
		if (ptrRawDataAuthenticateServer != NULL)
			rawDataObject_free(ptrRawDataAuthenticateServer);

		if (ptrRawData != NULL)
			rawDataObject_free(ptrRawData);
	}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "incorrect parameter(s) !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_prepareAuthenticateServerTlv() return %s", (res ? LPA_RES_TRUE_STRING : LPA_RES_FALSE_STRING));

	return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Submit Cancel Session request to eUICC and retrieve result
 * @param transactionID TransactionID of current server session, string format
 * @param p_reasonCode Cancel Reason Code as defined in SGP.22 - Values limited between 0 and 255
 * @param ptrCancelSessionResp Data structure that will receive card response
 * @return True if operations OK and cancelResponseOK returned by eUICC
 */
bool lpaManagerES10b_CancelSession(const char * transactionID, const unsigned int p_reasonCode, CANCEL_SESSION_RESPONSE * ptrCancelSessionResp)
{
    bool res = false;
    char printBuffer[LPA_CANCEL_SESSION_RESPONSE_BUFFER_SIZE * 2];
    
    if((transactionID != NULL) && (ptrCancelSessionResp != NULL) && isElementPresentInArrayUInt(LPA_ALLOWED_CANCEL_SESSION_CODE_LIST, LPA_ALLOWED_CANCEL_SESSION_CODE_LIST_SIZE, p_reasonCode))
    {
        RawDataObject* ptrRawDataCancelSessionRequest = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Cancel Session ES10B...");
        
        // Continue if preparation of command request is OK
        if(_prepareCancelSessionTlv(transactionID, p_reasonCode, &ptrRawDataCancelSessionRequest) && (ptrRawDataCancelSessionRequest != NULL))
        {
            uint16_t sw = 0x0000;
            size_t dataBufferSize = 0;

            // Send Request
            // _dataBuffer is a global variable of lpa_manager_es10b.c
            if (buildAndSendStoreDataCase4(ptrRawDataCancelSessionRequest, &sw, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
            {
                // Check 90.00 or 91.xx
                if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                {
                    if ((dataBufferSize > 0) && (dataBufferSize <= LPA_CANCEL_SESSION_RESPONSE_BUFFER_SIZE))
                    {
                        // Save eUICC response in CANCEL_SESSION_RESPONSE ptrCancelSessionResp structure
                        memcpy(ptrCancelSessionResp->cancelSessionResponse_RawData, _dataBuffer, dataBufferSize);
                        ptrCancelSessionResp->cancelSessionResponse_RawDataSize = dataBufferSize;
                        
                        ptrCancelSessionResp->resultOK = false;
                        
                        // Analyze if cancelSessionResponseOk returned by eUICC
                        if (_isValidCancelSessionResponse(_dataBuffer, dataBufferSize))
                        {
                            ptrCancelSessionResp->resultOK = true;
                            res = true;
                            formatBytesToHexaString(ptrCancelSessionResp->cancelSessionResponse_RawData, ptrCancelSessionResp->cancelSessionResponse_RawDataSize, printBuffer, LPA_CANCEL_SESSION_RESPONSE_BUFFER_SIZE * 2);
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Cancel Session ES10B: Returned cancelSessionResponse: %s", printBuffer);
                        }
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cancel Session ES10B: Did not returned cancelSessionResponseOk.");
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cancel Session ES10B: Response from eUICC out of bounds.");
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cancel Session ES10B: Incorrect status word reported by card : %X", sw);
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cancel Session ES10B: Failed to execute command request.");
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cancel Session ES10B: Failed to generate command request.");

        rawDataObject_free(ptrRawDataCancelSessionRequest);
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cancel Session ES10B: Incorrect parameter detected");
    
    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Builds request for Cancel Session command (TLV object coded in raw format)
 * @param transactionID TransactionID of current server session, string format
 * @param p_reasonCode Reason Code as defined in SGP.22 - Values limited between 0 and 255
 * @param ptrRawDataCancelSessionRequest Output, will contain request to send to eUICC, RawDataObject format
 * @return true if request building ran correctly
 */
bool _prepareCancelSessionTlv(const char * transactionID, const unsigned int p_reasonCode, RawDataObject** ptrRawDataCancelSessionRequest)
{
    bool res = false;
    unsigned char tabReasonCode[1] = { 0 };
    unsigned char tabTransactionID[16] = { 0 };
    int convLen = 16;
    
    // Check input parameters before building
    if ((transactionID != NULL) && (strlen(transactionID) > 0) && (strlen(transactionID) < LPA_TRANSACTION_ID_MAX_SIZE) && (p_reasonCode >= 0) && (p_reasonCode < 256) &&
        (ptrRawDataCancelSessionRequest != NULL))
    {
        if (hexStr2ByteArray((unsigned char *)transactionID, 32, tabTransactionID, &convLen) && (convLen == 16))
        {
            // Build "Cancel Session" request
            RawDataObject* ptrRawDataTransactionID = NULL;
            RawDataObject* ptrRawDataReasonCode = NULL;

            // Reminder: Request = BF41 L (80 L transactionID + 81 L reasonCode)

            ptrRawDataTransactionID = berTLV_createAndBuildRawDataObject(0x80, (size_t)16, tabTransactionID);
            tabReasonCode[0] = (unsigned char)p_reasonCode;
            ptrRawDataReasonCode = berTLV_createAndBuildRawDataObject(0x81, (size_t)1, tabReasonCode);

            if((ptrRawDataTransactionID != NULL) && (ptrRawDataReasonCode != NULL))
            {
                RawDataObject* ptrRawDataObjectsConcat = NULL;

                ptrRawDataObjectsConcat = rawDataObject_concat(ptrRawDataTransactionID, ptrRawDataReasonCode);

                if(ptrRawDataObjectsConcat != NULL)
                {
                    *ptrRawDataCancelSessionRequest = berTLV_createAndBuildRawDataObject(CANCEL_SESSION_DGI_TAG, ptrRawDataObjectsConcat->rawDataSize, ptrRawDataObjectsConcat->rawData);

                    if(*ptrRawDataCancelSessionRequest != NULL)
                        res = true;
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_prepareCancelSessionTlv: TLV objects creation step #3 problem");
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_prepareCancelSessionTlv: TLV objects creation step #2 problem");

                rawDataObject_free(ptrRawDataObjectsConcat);
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_prepareCancelSessionTlv: TLV objects creation step #1 problem");

            rawDataObject_free(ptrRawDataTransactionID);
            rawDataObject_free(ptrRawDataReasonCode);
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_prepareCancelSessionTlv: Cannot convert TransactionID in hex.");

    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_prepareCancelSessionTlv: Invalid TransactionID or reasonCode");

    return res;
}

/////////////////////////////////////////////
//
/////////////////////////////////////////////

/**
 * Analyzes eUICC data returned after Cancel Session request. Search for cancelSessionResponseOk Object. If found copy it in Cancel Session response storage structure.
 * @param ptrRawResponse Response data field returned by card (bytes)
 * @param rawResponseSize Response data field returned by card size (bytes number)
 * @param ptrCancelSessionResp Pointer on Cancel Session response storage structure (CANCEL_SESSION_RESPONSE type)
 * @return true if cancelSessionResponseOk Object detected.
 */
bool _isValidCancelSessionResponse(unsigned char * ptrRawResponse, size_t rawResponseSize)
{
    bool isValid = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_isValidCancelSessionResponse()...");

    if ((ptrRawResponse != NULL) && (rawResponseSize > 0))
    {
        BeerTLV* berTLV_BF41 = berTLV_extractTagUInt16(CANCEL_SESSION_DGI_TAG, ptrRawResponse, rawResponseSize, NULL);
        if (berTLV_BF41 != NULL)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_isValidCancelSessionResponse: <BF41> tag present");

            BeerTLV* berTLV_A0 = berTLV_extractTagUInt8((uint8_t)0xA0, berTLV_BF41->value, berTLV_BF41->length, NULL);

            if (berTLV_A0 != NULL)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_isValidCancelSessionResponse: <A0> tag present, cancelSessionResponseOk detected.");

                // Confirm cancelSessionResponseOk detected, so response is valid
                isValid = true;

                berTLV_freeBerTLV(berTLV_A0);
                berTLV_A0 = NULL;
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_isValidCancelSessionResponse: <A0> tag not present or cancelSessionResponseError encountered");

            berTLV_freeBerTLV(berTLV_BF41);
            berTLV_BF41 = NULL;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_isValidCancelSessionResponse: <BF41> tag not present");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_isValidCancelSessionResponse: Invalid parameters.");

    return isValid;
}

/**
 * Convert Hex data in Base 64 and store it in Rawdata objects pair, generally stored in data structure.
 * Note 1: Zero size objects is considered as an error.
 * Note 2: Eventual existing RawData objects will be cleared.
 * Note 3: In case of error output RawData will be cleared (So addresses will point on NULL).
 * @param ptrHexElement     Address of pointer that will receive the RawdataObject who will store HEX version of the data
 * @param ptrBase64Element  Address of pointer that will receive the RawdataObject who will store BASE64 version of the data
 * @param ptrHexData        Pointer on HEX data to be stored in RawdataObjec
 * @param hexDataSize       Size of HEX data to store. Must be > 0
 * @param maxDataSize       Maximum size allowed for HEX data given
 * @return true if conversion and RawData objects allocation / storage OK (Zero size objects goes on error state)
 */
bool _storeHexBase64StructureRawDataPair(RawDataObject ** ptrHexElement, RawDataObject ** ptrBase64Element, const unsigned char * ptrHexData, const size_t hexDataSize, const size_t maxDataSize)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_storeHexBase64StructureRawDataPair()...");
            
    if((ptrHexElement != NULL) && (ptrBase64Element != NULL) && (ptrHexData != NULL))
    {
        if(hexDataSize > 0)
        {
            if(hexDataSize <= maxDataSize)
            {
                // Reserve 150% greater for Base 64 conversion (Shall be at least 137%)
                size_t BASE_64_CONVERSION_BUFFER_SIZE = hexDataSize + (hexDataSize /2) + 1;	// Adding one byte for final 0x00 (End of String)

                unsigned char * tmpInfo = lpaCoreMemoryAlloc(BASE_64_CONVERSION_BUFFER_SIZE);
                if(tmpInfo != NULL)
                {
                    memset(tmpInfo, 0, BASE_64_CONVERSION_BUFFER_SIZE);

                    // Reset objects by security
                    ERASE_RAWDATAOBJECT(*ptrHexElement);
                    ERASE_RAWDATAOBJECT(*ptrBase64Element);

                    // Create Hex element object
                    *ptrHexElement = rawDataObject_create(ptrHexData, hexDataSize);

                    if(((*ptrHexElement) != NULL) && ((*ptrHexElement)->rawDataSize > 0))
                    {
                        size_t outlen = 0;
                        if (ffw_base64_encode((*ptrHexElement)->rawData, (*ptrHexElement)->rawDataSize, (char *)tmpInfo, &outlen, BASE_64_CONVERSION_BUFFER_SIZE))    
                        {
                            // Adding one byte at the end to manage End of String (0x00)
                            tmpInfo[outlen] = 0x00;

                            *ptrBase64Element = rawDataObject_create(tmpInfo, outlen +1 ); 

                            if(((*ptrBase64Element) != NULL) && ((*ptrBase64Element)->rawDataSize > 0))
                                res = true;
                            else
                            {
                                // If Base64 object creation NOK invalidate all objects in data structure
                                ERASE_RAWDATAOBJECT(*ptrHexElement);
                                ERASE_RAWDATAOBJECT(*ptrBase64Element);
                                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to create Raw Data Base64 element !");
                            }
                        }
                        else
                        {
                            // If Base64 conversion NOK invalidate raw object in AUTHENTICATE_SERVER_RESPONSE structure
                            ERASE_RAWDATAOBJECT(*ptrHexElement);
                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Cannot convert hex element in Base 64 !");
                        }
                    }
                    else
                    {
                        ERASE_RAWDATAOBJECT(*ptrHexElement);
                        lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to create Raw Data hex element !");
                    }

                    // Memory cleanup
                    lpaCoreMemoryFree(tmpInfo);
                    tmpInfo = NULL;
                }
                else
                {
                    lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Not enough memory for Base 64 conversion buffer !");
                }
            }
            else
            {
                lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Raw / hex data given size out of bounds: %d requested, %d maximum allowed!", hexDataSize, maxDataSize);
            } 
        }
        else
        {
            lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No raw / hex data available or size <= 0!");
        }
            
    }
    else
    {
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid NULL parameter !");
    }
    
    return res;
 }



/**
 * Get RAT (Rules Access Table) from eUICC
 * @param ptrGetRATA - Address of pointer that will receive the RawdataObject who will store RAT value
 * @return true if operation is correct
 */
bool lpaManagerES10b_GetRAT(RawDataObject ** ptrGetRAT)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES10b_GetRAT() ...");
    
    // Reset object by security
    ERASE_RAWDATAOBJECT(*ptrGetRAT);
    
    if (ptrGetRAT != NULL)
    {
        size_t dataBufferSize = 0;
        uint16_t sw = 0x0000;
        RawDataObject* ptrRawDataObjectGetRATrequest = NULL;
        
        ptrRawDataObjectGetRATrequest = berTLV_createAndBuildRawDataObject(GET_RAT_DGI_TAG, 0, NULL);
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Do buildAndSendStoreDataCase4(GetRAT) ...");
        if (ptrRawDataObjectGetRATrequest != NULL)
        {
            if (buildAndSendStoreDataCase4(ptrRawDataObjectGetRATrequest, &sw, _dataBuffer, sizeof(_dataBuffer), &dataBufferSize))
            {
                // Check if SW=90.00 or 91.xx
                if ((sw == 0x9000) || ((sw & 0xFF00) == 0x9100))
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SW=90.00 or 91.xx => Extracting data");

                    // At least 5 bytes (Tag BF43 + Length + Tag A0 + Length) shall be returned
                    if (dataBufferSize > 4 && dataBufferSize <= LPA_RAT_MAXIMUM_SIZE)
                    {
                        *ptrGetRAT = rawDataObject_create(_dataBuffer, dataBufferSize);
                        
                        if(*ptrGetRAT != NULL)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "PPR data found, %d bytes length", dataBufferSize);
                            res = true;
                        }
                        else
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to create RawDataObject for RAT storage!");
                            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "No response data available or response length out of bounds (5 to %d)! Returned size = %d", LPA_RAT_MAXIMUM_SIZE, dataBufferSize);
                        lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
                    }
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid response Status Word!");
                    lpaSetErrorCode(LPA_ERROR_INVALID_SW);
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "GetRAT request execution failed!");
                lpaSetErrorCode(LPA_ERROR_INVALID_GET_RAT);
            }

            // Do memory cleanup
            ERASE_RAWDATAOBJECT(ptrRawDataObjectGetRATrequest);
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to create RawDataObject to build GetRAT request!");
            lpaSetErrorCode(LPA_ERROR_INSUFFICIENT_BUFFER);
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Invalid NULL parameter!");
        lpaSetErrorCode(LPA_ERROR_INVALID_PARAMETER);
    }

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "lpaManagerES10b_GetRAT() return %s", (res ? LPA_RES_TRUE_STRING : LPA_RES_FALSE_STRING));
    
    return res;
}

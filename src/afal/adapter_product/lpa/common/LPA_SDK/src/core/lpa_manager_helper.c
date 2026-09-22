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

#include "lpasdk/core/lpa_manager_helper.h"
#include "lpasdk/core/lpa_manager.h"
#include "lpasdk/core/semedia_manager.h"

#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_memory.h"
#include "lpasdk/core/util.h"
#include "lpasdk/api/lpasdk_api.h"

static unsigned char _apduCommandStoreDataProfileCase3Header[] = { 0x80, 0xE2, 0x90, 0x00, 0x00 };
static unsigned char _apduCommandStoreDataProfileCase4Header[] = { 0x80, 0xE2, 0x91, 0x00, 0x00 };

static unsigned char _apduResponseBytes[MAX_LPA_MANAGER_APDU_BUFFER_SIZE];

static char _bufferFormatLogMessage[2018];

static bool _addLeToApuCase4 = true;

bool _buildAndSendApduCase4(const RawDataObject* ptrRawDataObject, uint16_t *ptrSW, unsigned char *ptrResponseApduData, size_t responseApduDataMaxSize, size_t *ptrResponseApduDataSize);

///////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////

void lpaManagerHelperSetLeToAddApduCase4(bool enable)
{
	_addLeToApuCase4 = enable;
}

///////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////

bool lpaManagerHelperIsLeAddedToApduCase4()
{
	return _addLeToApuCase4;
}

///////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////

bool buildAndSendStoreDataCase3WithoutResponseData(const RawDataObject* ptrRawDataObject, uint16_t *ptrSW)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendStoreDataCase3WithoutResponseData(...)");
	return buildAndSendStoreDataCase3(ptrRawDataObject, ptrSW, NULL, 0, NULL);
}

bool buildAndSendStoreDataCase3(const RawDataObject* ptrRawDataObject, uint16_t *ptrSW, unsigned char *ptrResponseApduData, size_t responseApduDataMaxSize, size_t *ptrResponseApduDataSize)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendStoreDataCase3(...)");
	if (ptrRawDataObject != NULL && ptrSW != NULL)
	{
		if (ptrRawDataObject->rawDataSize <= LPA_STORE_DATA_APDU_DATA_SIZE_MAX)
		{
			RawDataObject* apduStoreDataHeader = rawDataObject_create(_apduCommandStoreDataProfileCase3Header, sizeof(_apduCommandStoreDataProfileCase3Header));
			if (apduStoreDataHeader != NULL)
			{
				// Add data
				RawDataObject* apduStoreData = rawDataObject_concat(apduStoreDataHeader, ptrRawDataObject);
				if (apduStoreData != NULL)
				{
					size_t apduResponseMaxSize = MAX_LPA_MANAGER_APDU_BUFFER_SIZE;

					if (apduStoreData->rawDataSize > 4)
					{
						// Update LE
						apduStoreData->rawData[4] = (unsigned char) ptrRawDataObject->rawDataSize;

						if (formatBytesToHexaString(apduStoreData->rawData, apduStoreData->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "StoreData APDU (case 3) : %s", _bufferFormatLogMessage);
						else
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "StoreData APDU (case 3) : ...");

						// Send APDU
						if (seMediaManagerTransmitApdu(apduStoreData->rawData, apduStoreData->rawDataSize, _apduResponseBytes, &apduResponseMaxSize))
						{
							if (apduResponseMaxSize >= 2)
							{
								// APDU successfully sent
								*ptrSW = _apduResponseBytes[apduResponseMaxSize - 2] << 8 | _apduResponseBytes[apduResponseMaxSize - 1];
								lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "StoreData APDU SW:%04lx", *ptrSW);
								res = true;
							}
                                                        else
                                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "APDU response too short!");
						}
                                                else
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "APDU sending through seMedia Manager failed!");
					}
					else
						lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "apduStoreData object too small : APDU not send !");

					rawDataObject_free(apduStoreData);
					apduStoreData = NULL;
				}
                                else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "buildAndSendStoreDataCase3() - apduStoreData NULL!");

				// Do cleanup
				rawDataObject_free(apduStoreDataHeader);
				apduStoreDataHeader = NULL;
			}
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "buildAndSendStoreDataCase3() - apduStoreDataHeader NULL!");
		}
		else
		{
			// Need to split APDU on many APDU - To be developed if needed
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Sending more than one APDU for StoreDataCase3 not yet implemented");
		}
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "buildAndSendStoreDataCase3() - Invalid NULL parameter!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendStoreDataCase3(...) => return %s", (res ? "true" : "false"));

	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////

bool buildAndSendStoreDataCase4WithoutResponseData(const RawDataObject* ptrRawDataObject, uint16_t *ptrSW)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendStoreDataCase4WithoutResponseData(...)");
	res = buildAndSendStoreDataCase4(ptrRawDataObject, ptrSW, NULL, 0, NULL);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendStoreDataCase4WithoutResponseData(...) => return %s", (res ? "true" : "false"));

	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////

bool buildAndSendStoreDataCase4(const RawDataObject* ptrRawDataObject, uint16_t *ptrSW, unsigned char *ptrResponseApduData, size_t responseApduDataMaxSize, size_t *ptrResponseApduDataSize)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendStoreDataCase4(...)");

        //  ptrResponseApduData & ptrResponseApduDataSize NULL checking is managed in buildAndSendApduCase4()
	if (ptrRawDataObject != NULL && ptrSW != NULL)
	{
		if (ptrRawDataObject->rawDataSize > 0 && ptrRawDataObject->rawData != NULL)
		{
			RawDataObject* apduStoreDataHeader = rawDataObject_create(_apduCommandStoreDataProfileCase4Header, sizeof(_apduCommandStoreDataProfileCase4Header));
			if (apduStoreDataHeader != NULL)
			{
				if (ptrRawDataObject->rawDataSize <= LPA_STORE_DATA_APDU_DATA_SIZE_MAX)
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Sending StoreData in one APDU ...");
 
					// add data
					RawDataObject* ptrApduStoreData = rawDataObject_concat(apduStoreDataHeader, ptrRawDataObject);
					if (ptrApduStoreData != NULL)
					{
						if (ptrApduStoreData->rawDataSize > 4)
						{
							// Update Lc
							ptrApduStoreData->rawData[4] = (unsigned char) ptrRawDataObject->rawDataSize;
							res = buildAndSendApduCase4(ptrApduStoreData, ptrSW, ptrResponseApduData, responseApduDataMaxSize, ptrResponseApduDataSize);
						}
						else
							lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "buildAndSendStoreDataCase4() - apduStoreData object too small : APDU not send !");

						// Do memory cleanup
						rawDataObject_free(ptrApduStoreData);
						ptrApduStoreData = NULL;
					}
                                        else
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "buildAndSendStoreDataCase4() - apduStoreData object NULL : APDU not send !");
				}
				else
				{
					bool isLastBlock = false;
					uint8_t blockNumber = 0;
					size_t dataSizeAlreadySend = 0;
					size_t blockSize = 0;
					bool isErrorDetected = false;

					// Need to split Data on many APDU
					lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "Sending StoreData with more than one APDU ...");

					while (!isLastBlock)
					{

						size_t remainingSize = ptrRawDataObject->rawDataSize - dataSizeAlreadySend;
						if (remainingSize <= LPA_STORE_DATA_APDU_DATA_SIZE_MAX)
						{
							isLastBlock = true;
							blockSize = remainingSize;
						}
						else
							blockSize = LPA_STORE_DATA_APDU_DATA_SIZE_MAX;

						// add data
						RawDataObject* apduStoreData = rawDataObject_concatPartially(apduStoreDataHeader, ptrRawDataObject, dataSizeAlreadySend, blockSize);
						if (apduStoreData == NULL)
                                                {
                                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "buildAndSendStoreDataCase4() - Insufficient memory resources!");
                                                    break;	// Memory allocation error
                                                }
						
						if (apduStoreData->rawDataSize > 4)
						{
							// Update P1
							if (isLastBlock)
								apduStoreData->rawData[2] = 0x91;	// Last block
							else
								apduStoreData->rawData[2] = 0x11;   //Not the last block 

							// Update P2
							apduStoreData->rawData[3] = blockNumber;

							// Update Lc
							apduStoreData->rawData[4] = (unsigned char) blockSize;
							res = buildAndSendApduCase4(apduStoreData, ptrSW, ptrResponseApduData, responseApduDataMaxSize, ptrResponseApduDataSize);
							if (!res)
								isErrorDetected = true; // Error when sending APDU

							dataSizeAlreadySend += blockSize;
							blockNumber++;
						}
						else
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "buildAndSendStoreDataCase4() - apduStoreData object too small : APDU not send !");
							isErrorDetected = true;
						}

						// Do memory cleanup
						rawDataObject_free(apduStoreData);
						apduStoreData = NULL;

						if (isErrorDetected)
						{
							res = false;
							break;
						}
					}
				}

				// Do memory cleanup
				rawDataObject_free(apduStoreDataHeader);
				apduStoreDataHeader = NULL;
			}
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "buildAndSendStoreDataCase4() - Invalid NULL APDU header!");
		}
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "buildAndSendStoreDataCase4() - Invalid NULL or zero length parameter!");
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "buildAndSendStoreDataCase4() - Invalid NULL parameter!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendStoreDataCase4(...) => return %s", (res ? "true" : "false"));

	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////

bool buildAndSendApduCase4(const RawDataObject* ptrRawDataObject, uint16_t *ptrSW, unsigned char *ptrResponseApduData, size_t responseApduDataMaxSize, size_t *ptrResponseApduDataSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendApduCase4(...)");
	
	res = _buildAndSendApduCase4(ptrRawDataObject, ptrSW, ptrResponseApduData, responseApduDataMaxSize, ptrResponseApduDataSize);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendApduCase4(...) => return %s", (res ? "true" : "false"));

	return res;
}

bool buildAndSendApduCase4Ex(const unsigned char* ptrApduC, uint16_t apduCSize, uint16_t *ptrSW, unsigned char *ptrResponseApduData, size_t responseApduDataMaxSize, size_t *ptrResponseApduDataSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendApduCase4Ex(...)");

	// create temporary RawDataObject without dynamic allocation
	RawDataObject rawDataObjectApduC;
	rawDataObjectApduC.rawData = (unsigned char*) ptrApduC;
	rawDataObjectApduC.rawDataSize = apduCSize;

	res = _buildAndSendApduCase4(&rawDataObjectApduC, ptrSW, ptrResponseApduData, responseApduDataMaxSize, ptrResponseApduDataSize);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "buildAndSendApduCase4Ex(...) => return %s", (res ? "true" : "false"));

	return res;
}

bool _buildAndSendApduCase4(const RawDataObject* ptrRawDataObject, uint16_t *ptrSW, unsigned char *ptrResponseApduData, size_t responseApduDataMaxSize, size_t *ptrResponseApduDataSize)
{
	bool res = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_buildAndSendApduCase4(...)");

	if (ptrRawDataObject != NULL && ptrSW != NULL && ptrResponseApduDataSize != NULL)
	{
		bool apduSend = false;
		size_t apduResponseMaxSize = MAX_LPA_MANAGER_APDU_BUFFER_SIZE;

		if (!_addLeToApuCase4)
		{
			if (formatBytesToHexaString(ptrRawDataObject->rawData, ptrRawDataObject->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU (without adding Le) : %s", _bufferFormatLogMessage);
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU (without adding Le) : ...");

			apduSend = seMediaManagerTransmitApdu(ptrRawDataObject->rawData, ptrRawDataObject->rawDataSize, _apduResponseBytes, &apduResponseMaxSize);
		}
		else
		{
			RawDataObject* rawDataObjectWithZeroFinal = NULL;
			RawDataObject* apduCase4WithLe = NULL;

			// Add '00' at the end (for case 4)
			unsigned char byteZero = 0x00;

			rawDataObjectWithZeroFinal = rawDataObject_create(&byteZero, 1);
			if (rawDataObjectWithZeroFinal != NULL)
				apduCase4WithLe = rawDataObject_concat(ptrRawDataObject, rawDataObjectWithZeroFinal);

			if (apduCase4WithLe != NULL)
			{
				if (formatBytesToHexaString(apduCase4WithLe->rawData, apduCase4WithLe->rawDataSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU (case 4 with Le added) : %s", _bufferFormatLogMessage);
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU (case 4 with Le added) : ...");

				apduSend = seMediaManagerTransmitApdu(apduCase4WithLe->rawData, apduCase4WithLe->rawDataSize, _apduResponseBytes, &apduResponseMaxSize);
			}
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Failed to build apduCase4WithLe object!");

			// Do memory cleanup
			rawDataObject_free(rawDataObjectWithZeroFinal);
			rawDataObject_free(apduCase4WithLe);
		}

		if (apduSend)
		{
			if (apduResponseMaxSize >= 2)
			{
				// APDU successfully sent
				*ptrSW = _apduResponseBytes[apduResponseMaxSize - 2] << 8 | _apduResponseBytes[apduResponseMaxSize - 1];
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU SW:%04lx", *ptrSW);

				if ((ptrResponseApduData != NULL) && (responseApduDataMaxSize > 0) && (ptrResponseApduDataSize != NULL))
				{
					size_t dataSize = apduResponseMaxSize - 2;
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Managing data response ...");

					// Managing data response => copy data from APDU Response
					*ptrResponseApduDataSize = 0;

					if (dataSize > 0)
					{
						// Data available
						if (dataSize <= responseApduDataMaxSize)
						{
							memcpy(ptrResponseApduData, _apduResponseBytes, dataSize);
							*ptrResponseApduDataSize = dataSize;
							res = true;
						}
						else
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Data buffer size to small (Maximum allowed:%d bytes - needed:%d bytes)", responseApduDataMaxSize, dataSize);
						}
					}
					else
					{
						// No data
						lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No data available from R-APDU");
						res = true;
					}
				}
				else
				{
					// Not managing data response
					res = true;
				}
			}
			else
			{
				apduSend = false;	// SW is empty / incomplete or incorrect data (Too short to be a SW, can be data corruption or error detected at lower layer)
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Incorrect data or invalid SW or error! (Too short response < 2)");
			}
		}
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "APDU sending operation through seMedia failed!");
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_buildAndSendApduCase4() - Invalid NULL parameter!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_buildAndSendApduCase4(...) => return %s", (res ? "true" : "false"));

	return res;
}

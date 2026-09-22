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

#include "lpasdk/core/bertlv_object.h"
#include "lpasdk/core/lpa_memory.h"
#include "lpasdk/core/lpa_log.h"

#include <stdio.h>
#include <string.h>

//#include <memory.h>

void _berTLV_freeRawDataBuffer(unsigned char* ptrRawData);
unsigned char* _berTLV_createRawDataBuffer(const BeerTLV* ptrBerTlv, size_t* ptrRawDataSize);

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Build BERTLV object (BeerTLV type) from raw data field, but only if specified Tag is found
 * @param tag TLV tag to be found, uint8_t format
 * @param ptrRawData Raw data field to analyze, bytes format
 * @param rawDataSize Size of given raw data field
 * @param ptrIsTagFound Boolean flag address, will be set to True if tag found. If address = NULL will be ignored / not updated
 * @return Pointer on BERTLV object (BeerTLV type) extracted from raw data bytes field.
 */
//UT_EXPORT_DLL BeerTLV* berTLV_extractTagUInt8(uint8_t tag, const unsigned char* ptrRawData, uint32_t rawDataSize, bool* ptrIsTagFound)
UT_EXPORT_DLL BeerTLV* berTLV_extractTagUInt8(uint8_t tag, const unsigned char* ptrRawData, size_t rawDataSize, bool* ptrIsTagFound)
{
	BeerTLV* berTLV = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "berTLV_extractTagUInt8(0x%02X,...)", tag);

	if (ptrRawData != NULL && rawDataSize >= 2)
	{
		uint16_t tag16 = (uint16_t)tag & 0x00FF;
		berTLV = berTLV_extractTagUInt16(tag16, ptrRawData, rawDataSize, ptrIsTagFound);
	}

	return berTLV;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Build BERTLV object (BeerTLV type) from raw data field, but only if specified Tag is found
 * @param tag TLV tag to be found, uint16_t format
 * @param ptrRawData Raw data field to analyze, bytes format
 * @param rawDataSize Size of given raw data field
 * @param ptrIsTagFound Boolean flag address, will be set to True if tag found. If address = NULL will be ignored / not updated
 * @return Pointer on BERTLV object (BeerTLV type) extracted from raw data bytes field.
 */
//UT_EXPORT_DLL BeerTLV* berTLV_extractTagUInt16(uint16_t tag, const unsigned char* ptrRawData, uint32_t rawDataSize, bool* ptrIsTagFound)
UT_EXPORT_DLL BeerTLV* berTLV_extractTagUInt16(uint16_t tag, const unsigned char* ptrRawData, size_t rawDataSize, bool* ptrIsTagFound)
{
	BeerTLV* berTLV = NULL;
	bool isTagFound = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "berTLV_extractTagUInt16(0x%04X,...)", tag);

	// ptrIsTagFound is now optionnal
	if (ptrRawData != NULL  && rawDataSize >= 2)
	{
		size_t offsetRawData = 0;
		uint16_t currentTag = 0x00;
		uint32_t currentLength = 0x00;
		bool invalidRawData = false;

		while ((isTagFound == false) && (!invalidRawData) && (offsetRawData < rawDataSize))
		{
			currentTag = 0x00;
			currentLength = 0x00;

			// Read Tag
			////////////////////

			currentTag = ptrRawData[offsetRawData];
			if (currentTag == 0xFF || currentTag == 0x9F || currentTag == 0xdf || currentTag == 0x5f || currentTag == 0xbf)
			{
				// Tag size : 2 bytes
				if ((offsetRawData + 1) < rawDataSize)
				{
					currentTag <<= 8;
					currentTag |= ptrRawData[offsetRawData + 1];

					offsetRawData += 2;
				}
				else
				{
					invalidRawData = true;
					break; // Incorrect <Tag> size
				}
			}
			else
			{
				// Tag size : 1 byte 
				offsetRawData += 1;
			}

			if (invalidRawData)
				break;

			// Read <Length>
			////////////////////
			if (offsetRawData < rawDataSize)
			{
				if (ptrRawData[offsetRawData] <= 0x7F)
				{
					currentLength = ptrRawData[offsetRawData];
					offsetRawData++;
				}
				else
				{
					switch (ptrRawData[offsetRawData])
					{
						case 0x81:
						{
							if (offsetRawData + 1 < rawDataSize)
							{
								currentLength = ptrRawData[offsetRawData + 1];
								offsetRawData += 2;
							}
							else
								invalidRawData = true;
						}
						break;
						
						case 0x82:
						{
							if (offsetRawData + 2 < rawDataSize)
							{
								currentLength = (ptrRawData[offsetRawData+1] << 8) | ptrRawData[offsetRawData + 2];
								offsetRawData += 3;
							}
							else
								invalidRawData = true;
						}
						break;

						case 0x83:
						{
							if (offsetRawData + 3 < rawDataSize)
							{
								currentLength = (ptrRawData[offsetRawData + 1] << 16) | (ptrRawData[offsetRawData + 2] << 8) | ptrRawData[offsetRawData + 3] ;
								offsetRawData += 4;
							}
							else
								invalidRawData = true;
						}
						break;

						default:
							invalidRawData = true;
						break;
					}
				}
			}
			else
			{
				invalidRawData = true;
				break; // Incorrect <Length> size 
			}

			if (invalidRawData)
				break;

			// check if Tag expected
			if (currentTag == tag)
			{
				// Tag is found
				isTagFound = true;

				if (offsetRawData + currentLength <= rawDataSize)
					berTLV = berTLV_create(tag, currentLength, &ptrRawData[offsetRawData]);
				else
					invalidRawData = true;
			}
			else
			{
				// No correct tag => go to next tag
				offsetRawData += currentLength;
			}
		}
	}

	// Update ptrIsTagFound if defined
	if (ptrIsTagFound != NULL)
		(*ptrIsTagFound) = isTagFound;

	return berTLV;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Extract BERTLV objects list (At same encapsulation level) from raw bytes field.
 * This list is a chained list, next list element is another BerTLVList object that can be reached by "ptrNext" attribute
 * Warning: Keep pointer of first element to be able to free generated list from memory
 * @param ptrRawData Raw data field to analyze, bytes format
 * @param rawDataSize Size of given raw data field
 * @param ptrCountTLVFound Return number of TLV objects found at this level
 * @return First element of BERTLV objects list, BerTLVList format
 */
//UT_EXPORT_DLL BerTLVList* berTLV_extractList(const unsigned char* ptrRawData, uint32_t rawDataSize, uint8_t* ptrCountTLVFound)
UT_EXPORT_DLL BerTLVList* berTLV_extractList(const unsigned char* ptrRawData, size_t rawDataSize, uint8_t* ptrCountTLVFound)
{
	BerTLVList* berTLVFirstElement = NULL;
	BerTLVList* berTLVLastElement = NULL;
	uint8_t countTLVFound = 0;

	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "berTLV_extractList(...)");

	if (ptrRawData != NULL && rawDataSize >= 2)
	{
		size_t offsetRawData = 0;
		uint16_t currentTag = 0x00;
		uint32_t currentLength = 0x00;
		bool invalidRawData = false;

		while ( (!invalidRawData) && (offsetRawData < rawDataSize))
		{
			currentTag = 0x00;
			currentLength = 0x00;

			// Read Tag
			////////////////////

			currentTag = ptrRawData[offsetRawData];
			if (currentTag == 0xFF || currentTag == 0x9F || currentTag == 0xdf || currentTag == 0x5f || currentTag == 0xbf)
			{
				// Tag size : 2 bytes
				if ((offsetRawData + 1) < rawDataSize)
				{
					currentTag <<= 8;
					currentTag |= ptrRawData[offsetRawData + 1];

					offsetRawData += 2;
				}
				else
				{
					invalidRawData = true;
					break; // Incorrect <Tag> size
				}
			}
			else
			{
				// Tag size : 1 byte 
				offsetRawData += 1;
			}

			if (invalidRawData)
				break;

			// Read <Length>
			////////////////////
			if (offsetRawData < rawDataSize)
			{
				if (ptrRawData[offsetRawData] <= 0x7F)
				{
					currentLength = ptrRawData[offsetRawData];
					offsetRawData++;
				}
				else
				{
					switch (ptrRawData[offsetRawData])
					{
					case 0x81:
						if (offsetRawData + 1 < rawDataSize)
						{
							currentLength = ptrRawData[offsetRawData+1];
							offsetRawData += 2;
						}
						else
							invalidRawData = true;
						break;

					case 0x82:
						if (offsetRawData + 2 < rawDataSize)
						{
							currentLength = (ptrRawData[offsetRawData+1] << 8) | ptrRawData[offsetRawData+2];
							offsetRawData += 3;
						}
						else
							invalidRawData = true;
						break;

					case 0x83:
					{
						if (offsetRawData + 3 < rawDataSize)
						{
							currentLength = (ptrRawData[offsetRawData + 1] << 16) | (ptrRawData[offsetRawData + 2] << 8) | ptrRawData[offsetRawData + 3];
							offsetRawData += 4;
						}
						else
							invalidRawData = true;
					}
					break;

					default:
						invalidRawData = true;
						break;
					}
				}
			}
			else
			{
				invalidRawData = true;
				break; // Incorrect <Length> size 
			}

			if (invalidRawData)
				break;

			// create TLV item
			BerTLVList* ptrBerTLVList = lpaCoreMemoryAlloc(sizeof(BerTLVList));
			if (ptrBerTLVList != NULL)
			{
				memset(ptrBerTLVList, 0x00, sizeof(BerTLVList));

				ptrBerTLVList->index = countTLVFound;
				ptrBerTLVList->berTLV = berTLV_create(currentTag, currentLength, &ptrRawData[offsetRawData]);
				ptrBerTLVList->ptrNext = NULL;

				if (ptrBerTLVList->berTLV != NULL)
				{
					// Add it to the list
					if (berTLVFirstElement == NULL)
						berTLVFirstElement = berTLVLastElement = ptrBerTLVList;
					else
					{
						berTLVLastElement->ptrNext = ptrBerTLVList;
						berTLVLastElement = ptrBerTLVList;
					}

					countTLVFound++;
				}
				else
				{
					// Memory issue => release list created, release new item and return NULL
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "berTLV_extractList(...) => memory issue !");
					if (berTLVFirstElement != NULL)
					{
						berTLV_freeBerTLVList(berTLVFirstElement);
						berTLVFirstElement = NULL;
					}

					lpaCoreMemoryFree(ptrBerTLVList);
					ptrBerTLVList = NULL;

					break;
				}
			}
			else
			{
				// Memory issue => clear list created and return NULL
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "berTLV_extractList(...) => memory issue !");
				if (berTLVFirstElement != NULL)
				{
					berTLV_freeBerTLVList(berTLVFirstElement);
					berTLVFirstElement = NULL;
				}

				break;
			}

			// go to next tag
			offsetRawData += currentLength;
		}
	}

	if (ptrCountTLVFound != NULL)
		*ptrCountTLVFound = countTLVFound;

	return berTLVFirstElement;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Free BERTLV object (BeerTLV) from memory
 * @param ptrBerTLV Pointer on BeerTLV object to free
 * @return 
 */
UT_EXPORT_DLL bool berTLV_freeBerTLV(BeerTLV* ptrBerTLV)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "berTLV_freeBerTLV(...)" );

	if (ptrBerTLV != NULL)
	{
		if (ptrBerTLV->value != NULL)
		{
			lpaCoreMemoryFree(ptrBerTLV->value);
		}
		lpaCoreMemoryFree(ptrBerTLV);
		ptrBerTLV = NULL;
	}

	return true;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Free BERTLV list object (BerTLVList) from memory
 * @param ptrBerTLVList Pointer on first element of the BERTLV list to free
 * @return 
 */
UT_EXPORT_DLL bool berTLV_freeBerTLVList(BerTLVList* ptrBerTLVList)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "berTLV_freeBerTLVList(...)");

	if (ptrBerTLVList != NULL)
	{
		BerTLVList* ptrCurrentBerTLVList = ptrBerTLVList;
		while (ptrCurrentBerTLVList != NULL)
		{
			BerTLVList* ptrNextBertTLVList = ptrCurrentBerTLVList->ptrNext;
			if (ptrCurrentBerTLVList->berTLV != NULL)
			{
				berTLV_freeBerTLV(ptrCurrentBerTLVList->berTLV);
				ptrCurrentBerTLVList->berTLV = NULL;
			}
			lpaCoreMemoryFree(ptrCurrentBerTLVList);
			ptrCurrentBerTLVList = ptrNextBertTLVList;
		}

		ptrBerTLVList = NULL;
	}

	return true;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Create BERTLV object from separate elements (Tag, size and data)
 * @param tag Tag of BERTLV object, uint16_t format
 * @param length Length of BERTLV, uint32_t format
 * @param ptrValue Data to be stored in BERTLV object, char array
 * @return Pointer on created BERTLV object, else NULL if failed
 */
//UT_EXPORT_DLL BeerTLV* berTLV_create(uint16_t tag, uint32_t length, const unsigned char* ptrValue)
UT_EXPORT_DLL BeerTLV* berTLV_create(uint16_t tag, size_t length, const unsigned char* ptrValue)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "berTLV_create(tag:0x%04X)", tag);

	BeerTLV* ptrBerTLV = lpaCoreMemoryAlloc(sizeof(BeerTLV));
	if (ptrBerTLV != NULL)
	{
		memset(ptrBerTLV, 0x00, sizeof(BeerTLV));

		ptrBerTLV->tag = tag;
		ptrBerTLV->length = 0;
		ptrBerTLV->value = NULL;

		if (length > 0 && ptrValue != NULL )
		{
			ptrBerTLV->value = lpaCoreMemoryAlloc(length);
			if (ptrBerTLV->value != NULL)
			{
				memcpy(ptrBerTLV->value, ptrValue, length);
				ptrBerTLV->length = length;
			}
			else
			{
				// If no memory for data, ptrBerTLV must be NULL (and release allocated memory)
				lpaCoreMemoryFree(ptrBerTLV);
				ptrBerTLV = NULL;
			}
		}
	}

	return ptrBerTLV;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Will create a raw data array storing image (Tag, encoded length and data) of BERTLV object stored in a BeerTLV variable / object
 * @param ptrBerTlv Pointer on BERTLV variable / object to store, BeerTLV type
 * @param ptrRawDataSize Will store size of array storing BERTLV object image, size_t type
 * @return Pointer on raw data array storing BERTLV image, else NULL if failed
 */
unsigned char* _berTLV_createRawDataBuffer(const BeerTLV* ptrBerTlv, size_t* ptrRawDataSize)
{
	unsigned char* ptrRawData = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "_berTLV_createRawDataBuffer(...)");

	if (ptrRawDataSize != NULL && ptrBerTlv != NULL)
	{
		*ptrRawDataSize = 0; // default raw data size

		uint32_t tlvTagSize = 0;
		uint32_t tlvLengthSize = 0;
		uint32_t tlvDataSize = 0;
		uint32_t tlvTotalSize = 0;

		// detect tag size
		tlvTagSize = ((ptrBerTlv->tag & 0xFF00) != 0x00) ? 2 : 1;
		tlvLengthSize = ptrBerTlv->length <= 0x7F ? 1 : (ptrBerTlv->length <= 0xFF ? 2 : (ptrBerTlv->length <= 0xFFFF ? 3 : 4 ) );
		tlvDataSize = ptrBerTlv->length;

		tlvTotalSize = tlvTagSize + tlvLengthSize + tlvDataSize;

		// Allocate buffer
		if (tlvTotalSize > 0)
		{
			size_t offestRawData = 0;
			ptrRawData = lpaCoreMemoryAlloc(tlvTotalSize);
			if (ptrRawData != NULL)
			{
				switch (tlvTagSize)
				{
					case 1:
						ptrRawData[offestRawData] = ptrBerTlv->tag & 0xFF;
						offestRawData++;
					break;

					case 2:
						ptrRawData[offestRawData] = (ptrBerTlv->tag >> 8) & 0xFF;
						offestRawData++;

						ptrRawData[offestRawData] = ptrBerTlv->tag & 0x00FF;
						offestRawData++;
					break;
				}

				switch (tlvLengthSize)
				{
					case 1:
						ptrRawData[offestRawData] = (unsigned char) ptrBerTlv->length;
						offestRawData++;
					break;
					
					case 2:
						ptrRawData[offestRawData] = 0x81;
						offestRawData++;

						ptrRawData[offestRawData] = (unsigned char) ptrBerTlv->length;
						offestRawData++;
					break;
					
					case 3:
						ptrRawData[offestRawData] = 0x82;
						offestRawData++;

						ptrRawData[offestRawData] = (ptrBerTlv->length >> 8) & 0xFF;
						offestRawData++;
						
						ptrRawData[offestRawData] = ptrBerTlv->length & 0x00FF;
						offestRawData++;
					break;

					case 4:
						ptrRawData[offestRawData] = 0x83;
						offestRawData++;

						ptrRawData[offestRawData] = (ptrBerTlv->length >> 16) & 0xFF;
						offestRawData++;

						ptrRawData[offestRawData] = (ptrBerTlv->length >> 8) & 0xFF;
						offestRawData++;
						
						ptrRawData[offestRawData] = ptrBerTlv->length & 0x00FF;
						offestRawData++;
					break;
				}

				// And now, add TLV Value part
				if (tlvDataSize > 0)
					memcpy(&ptrRawData[offestRawData], ptrBerTlv->value, tlvDataSize);

				*ptrRawDataSize = tlvTotalSize;
			}
		}

	}

	return ptrRawData;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Free BERTLV raw data array image created by _berTLV_createRawDataBuffer()
 * @param ptrRawData
 */
void _berTLV_freeRawDataBuffer(unsigned char* ptrRawData)
{
	if (ptrRawData != NULL)
	{
		lpaCoreMemoryFree(ptrRawData);
		ptrRawData = NULL;
	}
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Build a TLV object in a raw data data chain (RawDataObject type)
 * @param tag Tag of TLV object
 * @param length Length of TLV object
 * @param ptrValue Value of TLV object
 * @return TLV object raw data in RawDataObject type
 */
//RawDataObject* berTLV_createAndBuildRawDataObject(uint16_t tag, uint32_t length, const unsigned char* ptrValue)
RawDataObject* berTLV_createAndBuildRawDataObject(uint16_t tag, size_t length, const unsigned char* ptrValue)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "berTLV_createAndBuildRawDataObject(...)");

	RawDataObject* ptrRawDataObject = NULL;
	BeerTLV* ptrBerTLV = berTLV_create(tag, length, ptrValue);
	if (ptrBerTLV != NULL)
	{
		ptrRawDataObject = berTLV_buildRawDataObject(ptrBerTLV);
		berTLV_freeBerTLV(ptrBerTLV);
	}

	return ptrRawDataObject;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Create raw data image of BERTLV object (BeerTLV type) and store it in a RawDataObject object 
 * @param ptrBerTlv Pointer on BERTLV variable / object to store, BeerTLV type
 * @return Pointer on RawDataObject storing raw data image of BeerTLV object, else NULL if failed
 */
UT_EXPORT_DLL RawDataObject* berTLV_buildRawDataObject(const BeerTLV* ptrBerTlv)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "berTLV_buildRawDataObject(...)");

	RawDataObject* ptrRawDataObject = NULL;
	if (ptrBerTlv != NULL)
	{
		ptrRawDataObject = rawDataObject_allocate();
		if (ptrRawDataObject != NULL)
		{
			size_t rawDataSize = 0;
			ptrRawDataObject->rawData = _berTLV_createRawDataBuffer(ptrBerTlv, &rawDataSize);
			if (ptrRawDataObject->rawData != NULL)
			{
				ptrRawDataObject->rawDataSize = rawDataSize;
			}
			else
			{
				// Memory allocation issue => release ptrRawDataObject and return NULL
				rawDataObject_free(ptrRawDataObject);
				ptrRawDataObject = NULL;
			}
		}
	}
	return ptrRawDataObject;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////


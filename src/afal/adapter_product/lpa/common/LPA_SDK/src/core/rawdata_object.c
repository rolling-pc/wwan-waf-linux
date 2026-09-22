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

#include "lpasdk/core/rawdata_object.h"
#include "lpasdk/core/lpa_memory.h"
#include "lpasdk/core/lpa_log.h"

#include <memory.h>

//RawDataObject* rawDataObject_allocate();

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Create RawDataObject object structure in memory, and initializes its internal structure (Data points to NULL, size = 0)
 * @return Pointer on newly created RawDataObject object, else NULL if failed
 */
UT_EXPORT_DLL RawDataObject* rawDataObject_allocate()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_allocate()");

	RawDataObject* ptrRawDataObject = lpaCoreMemoryAlloc(sizeof(RawDataObject));
	if (ptrRawDataObject != NULL)
	{
		ptrRawDataObject->rawData = NULL;
		ptrRawDataObject->rawDataSize = 0;
	}

	return ptrRawDataObject;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Create RawDataObject object from bytes field and size
 * @param ptrRawData Pointer on data bytes field to store
 * @param rawDataSize Size of data bytes field, size_t format
 * @return Pointer on newly created RawDataObject object, else NULL if failed
 */
UT_EXPORT_DLL RawDataObject* rawDataObject_create(const unsigned char* ptrRawData, size_t rawDataSize)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_create()");

	RawDataObject* ptrRawDataObject = rawDataObject_allocate();
	if (ptrRawDataObject != NULL)
	{
		if (ptrRawData != NULL && rawDataSize > 0)
		{
			ptrRawDataObject->rawData = lpaCoreMemoryAlloc(rawDataSize);
			if (ptrRawDataObject->rawData != NULL)
			{
				memcpy(ptrRawDataObject->rawData, ptrRawData, rawDataSize);
				ptrRawDataObject->rawDataSize = rawDataSize;
			}
			else
			{
				// Memory allocation error : realase allocated memory
				lpaCoreMemoryFree(ptrRawDataObject);
				ptrRawDataObject = NULL;
			}
		}
	}

	return ptrRawDataObject;
}


/**
 * Create RawDataObject object from bytes field and size, but store data with header including size, as LV coded object
 * @param ptrRawData Pointer on data bytes field to store
 * @param rawDataSize Size of data bytes field, size_t format
 * @return Pointer on newly created RawDataObject object, else NULL if failed
 */
UT_EXPORT_DLL RawDataObject* rawDataObject_createAsLV(const unsigned char* ptrRawData, size_t rawDataSize)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_createAsLV()");

	RawDataObject* ptrRawDataObject = NULL;
	if (ptrRawData != NULL && rawDataSize > 0)
	{
		ptrRawDataObject = rawDataObject_allocate();
		if (ptrRawDataObject != NULL)
		{
			size_t nbBytesLength = rawDataSize <= 0x7F ? 1 : (rawDataSize <= 0xFF ? 2 : (rawDataSize <= 0xFFFF ? 3 : 4));
			size_t totalRawDataObjectSize = nbBytesLength + rawDataSize;

			ptrRawDataObject->rawData = lpaCoreMemoryAlloc(totalRawDataObjectSize);
			if (ptrRawDataObject->rawData != NULL)
			{
				switch (nbBytesLength)
				{
					case 1:
						ptrRawDataObject->rawData[0] = (unsigned char)rawDataSize;
					break;

					case 2:
						ptrRawDataObject->rawData[0] = 0x81;
						ptrRawDataObject->rawData[1] = (unsigned char)rawDataSize;
					break;

					case 3:
						ptrRawDataObject->rawData[0] = 0x82;
						ptrRawDataObject->rawData[1] = (rawDataSize >> 8) & 0xFF;
						ptrRawDataObject->rawData[2] = rawDataSize & 0x00FF;
					break;

					case 4:
						ptrRawDataObject->rawData[0] = 0x83;
						ptrRawDataObject->rawData[1] = (rawDataSize >> 16) & 0xFF;
						ptrRawDataObject->rawData[2] = (rawDataSize >> 8) & 0xFF;
						ptrRawDataObject->rawData[3] = rawDataSize & 0x00FF;
					break;
				}

				memcpy(&ptrRawDataObject->rawData[nbBytesLength], ptrRawData, rawDataSize);
				ptrRawDataObject->rawDataSize = totalRawDataObjectSize;
			}
			else
			{
				// Memory allocation error : release allocated memory
				lpaCoreMemoryFree(ptrRawDataObject);
				ptrRawDataObject = NULL;
			}
		}
	}

	return ptrRawDataObject;
}
//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Concatenates 2 RawDataObject objects in an unique RawDataObject object, first Object 1 then Object 2
 * @param ptrRawDataObject1 Pointer on Object 1 to concatenate, RawDataObject type
 * @param ptrRawDataObject2 Pointer on Object 2 to concatenate, RawDataObject type
 * @return Pointer on newly created RawDataObject object storing both concatenated input objects, else NULL if failed
 */
UT_EXPORT_DLL RawDataObject* rawDataObject_concat(const RawDataObject* ptrRawDataObject1, const RawDataObject*ptrRawDataObject2)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_concat()");

	RawDataObject* ptrRawDataObject = rawDataObject_allocate();
	if (ptrRawDataObject != NULL)
	{
		size_t rawDataSize1 = ((ptrRawDataObject1 != NULL && ptrRawDataObject1->rawData != NULL) ? ptrRawDataObject1->rawDataSize : 0);
		size_t rawDataSize2 = ((ptrRawDataObject2 != NULL && ptrRawDataObject2->rawData != NULL) ? ptrRawDataObject2->rawDataSize : 0);
		size_t rawDataSize = rawDataSize1 + rawDataSize2;

		if ((rawDataSize1 + rawDataSize2) > 0)
		{
			ptrRawDataObject->rawData = lpaCoreMemoryAlloc(rawDataSize);
			if (ptrRawDataObject->rawData != NULL)
			{
				if (rawDataSize1 > 0)
					memcpy(ptrRawDataObject->rawData, ptrRawDataObject1->rawData, rawDataSize1);

				if (rawDataSize2 > 0)
					memcpy(&ptrRawDataObject->rawData[rawDataSize1], ptrRawDataObject2->rawData, rawDataSize2);

				ptrRawDataObject->rawDataSize = rawDataSize;
			}
			else
			{
				// Memory allocation error : realase allocated memory
				lpaCoreMemoryFree(ptrRawDataObject);
				ptrRawDataObject = NULL;
			}
		}
	}

	return ptrRawDataObject;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Concatenates a RawDataObject object and a raw data array in an unique RawDataObject object, first RawDataObject object then raw data array.
 * Note 1: If one object is NULL (Including data of RawDataObject object) or empty, output RawDataObject object will only store other not NULL / empty object.
 * Note 2: If both objects are NULL / empty, output RawDataObject object will be empty.
 * @param ptrRawDataObjectInput1 Pointer on RawDataObject object to concatenate
 * @param ptrRawDataInput2 Pointer on raw data array to concatenate
 * @param rawDataSizeInput2 Size of raw data array to concatenate
 * @return Pointer on newly created RawDataObject object storing both concatenated input objects, else NULL if failed
 */
UT_EXPORT_DLL RawDataObject* rawDataObject_concatRawDataArray(const RawDataObject* ptrRawDataObjectInput1, const unsigned char* ptrRawDataInput2, size_t rawDataSizeInput2)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_concatRawDataArray()");

	RawDataObject* ptrRawDataObjectOutput = rawDataObject_allocate();
	if (ptrRawDataObjectOutput != NULL)
	{
		size_t rawDataSize1 = ((ptrRawDataObjectInput1 != NULL && ptrRawDataObjectInput1->rawData != NULL) ? ptrRawDataObjectInput1->rawDataSize : 0);
		size_t rawDataSize2 = (ptrRawDataInput2 != NULL ? rawDataSizeInput2 : 0);
		size_t rawDataSize = rawDataSize1 + rawDataSize2;

		if ((rawDataSize1 + rawDataSize2) > 0)
		{
			ptrRawDataObjectOutput->rawData = lpaCoreMemoryAlloc(rawDataSize);
			if (ptrRawDataObjectOutput->rawData != NULL)
			{
				if (rawDataSize1 > 0)
					memcpy(ptrRawDataObjectOutput->rawData, ptrRawDataObjectInput1->rawData, rawDataSize1);

				if (rawDataSize2 > 0)
					memcpy(&ptrRawDataObjectOutput->rawData[rawDataSize1], ptrRawDataInput2, rawDataSize2);

				ptrRawDataObjectOutput->rawDataSize = rawDataSize;
			}
			else
			{
				// Memory allocation error : realase allocated memory
				lpaCoreMemoryFree(ptrRawDataObjectOutput);
				ptrRawDataObjectOutput = NULL;
			}
		}
	}

	return ptrRawDataObjectOutput;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Append raw data array to an existing RawDataObject object. If data array pointer is NULL AND its size = 0 operation will still be considered as successful
 * @param ptrRawDataObjectSource Pointer on RawDataObject object. If NULL operation will be canceled / failed.
 * @param ptrRawDataAppend Pointer on raw data array to append.
 * @param rawDataSizeAppend Size of raw data to append, size_t format.
 * @return true if operation is successful, else false
 */
UT_EXPORT_DLL bool rawDataObject_appendRawDataArray(RawDataObject* ptrRawDataObjectSource, const unsigned char* ptrRawDataAppend, size_t rawDataSizeAppend)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_appendRawDataArray()");
	if (ptrRawDataObjectSource != NULL && ptrRawDataAppend != NULL && rawDataSizeAppend > 0 )
	{
            size_t rawDataSize1 = (ptrRawDataObjectSource->rawData != NULL ? ptrRawDataObjectSource->rawDataSize : 0);
            size_t rawDataSize2 = (ptrRawDataAppend != NULL ? rawDataSizeAppend : 0);
            size_t rawDataSize = rawDataSize1 + rawDataSize2;

            if ((rawDataSize1 + rawDataSize2) > 0)
            {
                // 1) create new buffer with all data
                unsigned char* ptrNewRawData = lpaCoreMemoryAlloc(rawDataSize);
                if (ptrNewRawData != NULL)
                {
                    if (rawDataSize1 > 0)
                        memcpy(ptrNewRawData, ptrRawDataObjectSource->rawData, rawDataSize1);

                    if (rawDataSize2 > 0)
                        memcpy(&ptrNewRawData[rawDataSize1], ptrRawDataAppend, rawDataSizeAppend);

                    // Free old buffer
                    if (ptrRawDataObjectSource->rawData != NULL)
                        lpaCoreMemoryFree(ptrRawDataObjectSource->rawData);

                    // Update RawData object source
                    ptrRawDataObjectSource->rawData = ptrNewRawData;
                    ptrRawDataObjectSource->rawDataSize = rawDataSize;

                    res = true;
                }
            }
	}
        
        // If Raw Data to happen is completely void, we consider result successful
        if (ptrRawDataAppend == NULL && rawDataSizeAppend == 0)
                res = true; // No data to append

	return res;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Concatenate data of RawDataObject object 1 and a selected area of data in RawDataObject object 2
 * Note 1: If one object is NULL (Including data of RawDataObject object) or empty, output RawDataObject object will only store target data of other not NULL / empty object.
 * Note 2: If both objects are NULL / empty, output RawDataObject object will be empty.
 * @param ptrRawDataObject1 Pointer on Object 1 to concatenate, RawDataObject type.
 * @param ptrRawDataObject2 Pointer on Object 2 to concatenate partially, RawDataObject type.
 * @param offset Starting offset for data concatenation from object 2, size_t format. If greater that size of Object 2, output RawDataObject object will only contain object 1.
 * @param length Length of data to concatenate from object 2, size_t format. If offset + length exceeds object 2 data length, length will be adjusted to max available in object 2.
 * @return Pointer on newly created RawDataObject object storing both concatenated input objects, else NULL if failed
 */
UT_EXPORT_DLL RawDataObject* rawDataObject_concatPartially(const RawDataObject* ptrRawDataObject1, const RawDataObject*ptrRawDataObject2, size_t offset, size_t length)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_concatPartially()");

	RawDataObject* ptrRawDataObject = rawDataObject_allocate();
	if (ptrRawDataObject != NULL)
	{
		size_t rawDataSize1 = ((ptrRawDataObject1 != NULL && ptrRawDataObject1->rawData != NULL) ? ptrRawDataObject1->rawDataSize : 0);
		size_t rawDataSize2 = ((ptrRawDataObject2 != NULL && ptrRawDataObject2->rawData != NULL) ? ptrRawDataObject2->rawDataSize : 0);
		size_t rawPartialSize2 = 0;

		if (offset < rawDataSize2)
		{
			if (offset + length <= rawDataSize2)
				rawPartialSize2 = length;
			else
				rawPartialSize2 = rawDataSize2 - offset;
		}
		size_t rawDataSize = rawDataSize1 + rawPartialSize2;

		if (rawDataSize > 0)
		{
			ptrRawDataObject->rawData = lpaCoreMemoryAlloc(rawDataSize);
			if (ptrRawDataObject->rawData != NULL)
			{
				if (rawDataSize1 > 0)
					memcpy(ptrRawDataObject->rawData, ptrRawDataObject1->rawData, rawDataSize1);

				if (rawPartialSize2 > 0)
					memcpy(&ptrRawDataObject->rawData[rawDataSize1], &ptrRawDataObject2->rawData[offset], rawPartialSize2);

				ptrRawDataObject->rawDataSize = rawDataSize;
			}
			else
			{
				// Memory allocation error : realase allocated memory
				lpaCoreMemoryFree(ptrRawDataObject);
				ptrRawDataObject = NULL;
			}
		}
	}

	return ptrRawDataObject;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
* Update RawDataObject object with new data content
* @param ptrRawDataObject pointer on RawDataObject object to update. If NULL operation is failed.
* @param ptrRawData Pointer on new data to store in RawDataObject object. If NULL operation is failed, except if rawDataSize = 0
* @param rawDataSize New size of data to store, size_t type. If < 1 RawDataObject object will be empty or leaved as it if new size = previous size.
* @return true if RawDataObject object updated successfully, else false
*/
UT_EXPORT_DLL bool rawDataObject_update(RawDataObject* ptrRawDataObject, const unsigned char* ptrRawData, size_t rawDataSize)
{
	bool doUpdate = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_update()");

	if (ptrRawDataObject != NULL)
	{
		// If old and new data have same size, do not allocate/free memory
		if (ptrRawDataObject->rawDataSize == rawDataSize)
		{
			doUpdate = true;
                        if (rawDataSize > 0)
                        {
                            if(ptrRawData != NULL)
				memcpy(ptrRawDataObject->rawData, ptrRawData, rawDataSize);
                            else
                                doUpdate = false;
			}
		}
		else
		{
			//Old and new data have not same size => allocate new buffer before release old buffer
			if (rawDataSize > 0)
			{
				if (ptrRawData != NULL)
				{
					unsigned char* ptrNewRawData = lpaCoreMemoryAlloc(rawDataSize);
					if (ptrNewRawData != NULL)
					{
						memcpy(ptrNewRawData, ptrRawData, rawDataSize);

						// Now release old buffer
						if (ptrRawDataObject->rawData != NULL)
							lpaCoreMemoryFree(ptrRawDataObject->rawData);

						ptrRawDataObject->rawData = ptrNewRawData;
						ptrRawDataObject->rawDataSize = rawDataSize;

						doUpdate = true;
					}
				}
			}
			else
			{
				// No data => Clear memory if needed
				if (ptrRawDataObject->rawData != NULL)
				{
					lpaCoreMemoryFree(ptrRawDataObject->rawData);
					ptrRawDataObject->rawData = NULL;
				}

				ptrRawDataObject->rawDataSize = 0;
				doUpdate = true;
			}
		}
	}

	return doUpdate;
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
* Remove content of a RawDataObject object
* @param ptrRawDataObject Pointer on RawDataObject object to clear
*/

UT_EXPORT_DLL void rawDataObject_clear(RawDataObject* ptrRawDataObject)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_clear()");

	if (ptrRawDataObject != NULL)
	{
		if (ptrRawDataObject->rawData != NULL)
		{
			lpaCoreMemoryFree(ptrRawDataObject->rawData);
			ptrRawDataObject->rawData = NULL;
		}

		ptrRawDataObject->rawDataSize = 0;
	}
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/**
 * Free memory used by RawDataObject object and affect object pointer to NULL
 * @param ptrRawDataObject Pointer on RawDataObject object to free
 */
UT_EXPORT_DLL void rawDataObject_free(RawDataObject* ptrRawDataObject)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "rawDataObject_free()");

	if (ptrRawDataObject != NULL)
	{
		if (ptrRawDataObject->rawData != NULL )
			lpaCoreMemoryFree(ptrRawDataObject->rawData);
		lpaCoreMemoryFree(ptrRawDataObject);
		ptrRawDataObject = NULL;
	}
}

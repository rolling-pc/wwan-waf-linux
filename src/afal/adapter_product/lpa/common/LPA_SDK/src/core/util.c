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


#include "lpasdk/core/util.h"
#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/bertlv_object.h"

#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "base64/base64.h"


bool _extractOIDfromCertificateExtensionList(const unsigned char * ptrExtensionListData, const size_t ExtensionListDataSize, unsigned char * ptrOID, size_t * ptrOIDsize, const size_t oidSizeMax);


size_t formatBytesToHexaString(const unsigned char *ptrDataBytes, size_t dataSize, char* ptrBufferString, size_t bufferStringMaxSize)
{
	size_t formattedStringSize = 0;

	if (ptrDataBytes != NULL && ptrBufferString != NULL && (dataSize * 2 + 1) < bufferStringMaxSize)
	{
		size_t offsetDataBytes = 0;

		while (offsetDataBytes < dataSize )
		{
			if (formattedStringSize + 3 < bufferStringMaxSize)
			{
				sprintf(&ptrBufferString[formattedStringSize], "%02X", ptrDataBytes[offsetDataBytes]);
				offsetDataBytes++;
				formattedStringSize += 2;
			}
			else
				break;
		}
	}

	return formattedStringSize;
}

bool writeIntegerValueToByteArray(uint16_t integerValue, unsigned char *ptrByteArray, size_t byteArrayMaxSize, size_t* byteArraySize)
{
	bool res = false;

	if (ptrByteArray != NULL && byteArraySize != NULL && byteArrayMaxSize > 0)
	{
		*byteArraySize = 0;

		if (integerValue <= 0xFF)
		{
			if (byteArrayMaxSize >= 1)
			{
				// One byte for Integer value
				ptrByteArray[0] = (unsigned char) integerValue;
				*byteArraySize = 1;

				res = true;
			}
		}
		else
		{
			if (integerValue <= 0xFFFF)
			{
				// Two byte for Integer value
				if (byteArrayMaxSize >= 2)
				{
					ptrByteArray[0] = (integerValue & 0xFF00) >> 8;
					ptrByteArray[1] = (integerValue & 0xFF);

					*byteArraySize = 2;
					res = true;
				}
			}
			else
			{
				if (integerValue <= 0xFFFFFF)
				{
					// Two byte for Integer value
					if (byteArrayMaxSize >= 3)
					{
						ptrByteArray[0] = (integerValue & 0x00FF0000) >> 16;
						ptrByteArray[1] = (integerValue & 0x00FF) >> 8 ;
						ptrByteArray[2] = (integerValue & 0xFF);

						*byteArraySize = 3;
						res = true;
					}
				}
			}
		}
	}

	return res;
}

bool extractIntegerFromByteArray(const unsigned char *ptrByteArray, size_t byteArraySize, uint16_t* ptrIntegerValue)
{
	bool res = false;
	
	if (ptrByteArray != NULL && byteArraySize > 0 && ptrIntegerValue != NULL)
	{
		if (byteArraySize == 0x01)
		{
			*ptrIntegerValue = ptrByteArray[0];
			res = true;
		}

		if (byteArraySize == 0x02)
		{
			*ptrIntegerValue = (ptrByteArray[0] << 8) | ptrByteArray[1];
			res = true;
		}

		if (byteArraySize == 0x03)
		{
			*ptrIntegerValue = (ptrByteArray[0] << 16) | (ptrByteArray[1] << 8) | ptrByteArray[2];
			res = true;
		}

		if (byteArraySize == 0x04)
		{
			*ptrIntegerValue = (ptrByteArray[0] << 24) | (ptrByteArray[1] << 16) | (ptrByteArray[2] << 8) | ptrByteArray[3];
			res = true;
		}
	}

	return res;
}


/**
 * Encode BerTLV length "L" in an array, with attributes (0x81, 82 & 83)
 * @param length - Value of the length to encode
 * @param lengthTLV - Pointer on array that will receive the encoded length, binary hex format
 * @param lengthTLVsize - Size of array that will receive the encoded length
 * @param attributeLength - Pointer on variable that will receive length of "L" object (0 to 4)
 * @return True if not problems with parameters (NULL pointer, array size too small)
 */
//bool encodeLength(int length, unsigned char* lengthTLV, const size_t lengthTLVsize, size_t * attributeLength)
bool encodeLength(size_t length, unsigned char* lengthTLV, const size_t lengthTLVsize, size_t* attributeLength)
{
    bool res = false;
       
    if(lengthTLV != NULL && attributeLength != NULL)
    {
        unsigned char *p = lengthTLV;
        *attributeLength = 0;
        
        if (length > 0)
        {
            if (length > 0x0000FFFF) {
                if(lengthTLVsize >= 4)
                {
                    *p++ = 0x83;
                    *p++ = (length & 0x00FF0000) >> 16;
                    *p++ = (length & 0x0000FF00) >> 8;
                    *p++ = length & 0x000000FF;
                    *attributeLength = 4;
                    res = true;
                }
            } else if (length > 0x000000FF) {
                if(lengthTLVsize >= 3)
                {
                    *p++ = 0x82;
                    *p++ = (length & 0x0000FF00) >> 8;
                    *p++ = length & 0x000000FF;
                    *attributeLength = 3;
                    res = true;
                }
            } else if (length > 0x0000007F) {
                if(lengthTLVsize >= 2)
                {
                    *p++ = 0x81;
                    *p++ = length & 0x000000FF;
                    *attributeLength = 2;
                    res = true;
                }
            } else {
                if(lengthTLVsize >= 1)
                {
                    *p++ = length & 0x000000FF;
                    *attributeLength = 1;
                    res = true;
                }
            }
            *p = 0;
        }
        else
            res = true; // Case of empty length
    }

    return res;
}


/**
 * Encode BerTLV length "L" in an array, without attributes (0x81, 82 & 83)
 * @param length - Value of the length to encode
 * @param lengthHex - Pointer on array that will receive the encoded length, binary hex format
 * @param lengthHexsize - Size of array that will receive the encoded length
 * @param attributeLength - Pointer on variable that will receive length of "L" object (0 to 3)
 * @return True if not problems with parameters (NULL pointer, array size too small)
 */
bool generateLength(int length, unsigned char* lengthHex, const size_t lengthHexsize, size_t * attributeLength)
{
    bool res = false;
    
    if (lengthHex != NULL && attributeLength != NULL)
    {
        unsigned char *p = lengthHex;
        *attributeLength = 0;

        if (length > 0)
        {
            if (length > 0x0000FFFF)
            {
                if(lengthHexsize >= 3)
                {
                    *p++ = (length & 0x00FF0000) >> 16;
                    *p++ = (length & 0x0000FF00) >> 8;
                    *p++ = length & 0x000000FF;
                    *attributeLength = 3;
                    res = true;
                }
            }
            else if (length > 0x00000FF)
            {
                if(lengthHexsize >= 2)
                {
                    *p++ = (length & 0x0000FF00) >> 8;
                    *p++ = length & 0x000000FF;
                    *attributeLength = 2;
                    res = true;
                }
            }
            else
            {
                if(lengthHexsize >= 1)
                {
                    *p++ = length & 0x000000FF;
                    *attributeLength = 1;
                    res = true;
                }
            }
        }
        else
            res = true; // Case of empty length
        
        *p = 0;
    }
    
    return res;
}


int oneHexCharToHex(char h)
{	
	int x = 0;	
	if (isdigit((unsigned char)h)) 
	{		
		x = h - '0';	
	} else if (isupper((unsigned char)h)) 
	{		
		x = h - 'A' + 10;	
	} else 
	{		
		x = h - 'a' + 10;	
	}	
	return x;
	
}


bool hexStr2ByteArray(const unsigned char * ptrInHexString, size_t inLen, unsigned char * ptrOutHex, int* ptrOutLen) 
{
    bool success = false;

    do
    {
        if (NULL == ptrInHexString || NULL == ptrOutHex || NULL == ptrOutLen )
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "hexStr2ByteArray() => error: ptrInHexString or ptrOutHex or ptrOutLen is null !");
            break;
        }

        if (inLen <= 1)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "hexStr2ByteArray() => error: inLen <= 1 !");
            break;
        }

        if (inLen % 2 != 0)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "hexStr2ByteArray() => error: ptrInHexString not modulo 2 !");
            break;
        }

        int outputMaxSize = *ptrOutLen;
        int outputSize = 0;
        *ptrOutLen = 0;
        for (int i = 0; i < inLen; i += 2)
        {
            if (outputSize > outputMaxSize)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "hexStr2ByteArray() => output buffer too small ! ");
                goto exit;
            }

            int ch1 = ptrInHexString[i];
            int ch2 = ptrInHexString[i + 1];

            //ptrOutHex[i / 2 + 1] = 0;
            if (isxdigit((unsigned char)ch1) && isxdigit((unsigned char)ch2)) 
            {
                ch1 = oneHexCharToHex(ch1);
                ch2 = oneHexCharToHex(ch2);

                ptrOutHex[outputSize] = (ch1 << 4) | ch2;
                outputSize++;
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "hexStr2ByteArray() => error: %c or %c is not hex digit! ", ch1, ch2);
                goto exit;
            }
        }

        if (outputSize <= outputMaxSize)
        {
            *ptrOutLen = outputSize;
            success = true; //OK	
        }
    } 
    while(0);

    exit:

    return success;
}


bool findSubstr(char* source, char* target)
{
    int i, j;
    int s_len = 0;
    int t_len = 0;
    
    if((source != NULL) && (target != NULL))
    {
        s_len = (int)strlen(source);
        t_len = (int)strlen(target);

        if (t_len > s_len)
        {
            return -1;
        }

        for (i = 0; i <= s_len - t_len; i++)
        {
            j = 0;
            int flag = 1;
            if (source[i] == target[j])
            {
                int k, p = i;
                for (k = 0; k < t_len; k++)
                {
                    if (source[p] == target[j])
                    {
                        p++;
                        j++;
                        continue;
                    }
                    else
                    {
                        flag = 0;
                        break;
                    }
                }
            }
            else
            {
                continue;
            }
            if (flag == 1)
            {
                return true;
            }
        }
    }
    return false;
}


/**
 * Return number of occurrences of a character in a string
 * @param pString String to analyse
 * @param c Character to find
 * @return Number of characters, 0 if none found
 */
int countCharOccurencesInString(const char * pString, const char c)
{
    int i;
    int len = 0;
    int count = 0;
    
    if(pString != NULL)
    {
        len = (int)strlen(pString);
        for(i = 0; i < len; i++)
        {
            if(pString[i] == c) count++;
        }
    }
    return count; 
}

/**
 * Check if a value is present in an array (Both unsigned integer)
 * @param pReferenceArray Array containing reference values
 * @param pArraySize Number of elements of reference array
 * @param pValue Value to check in reference array
 * @return True is value exists in reference array
 */
bool isElementPresentInArrayUInt(const unsigned int *pReferenceArray, const size_t pArraySize, const unsigned int pValue)
{
    size_t i = 0;
    bool res = false;
    
    if((pReferenceArray != NULL) && (pArraySize > 0))
    {
        while((i < pArraySize) && (! res))
        {
            if(*(pReferenceArray + i) == pValue) res = true;
            i++;
        }
    }
    
    return res;
}


/**
 * Check if two strings are equals ignoring case
 * @param pString1
 * @param pString2
 * @return true if strings are identical and not any string is NULL
 */
bool compareEqualStringIgnoringCase(const char * pString1, const char * pString2)
{
    bool res = false;
    size_t len_str1;
    size_t len_str2;
    size_t i = 0;
    
    if((pString1 != NULL ) && (pString2 != NULL))
    {
        len_str1 = strlen(pString1);
        len_str2 = strlen(pString2);
        
        // No need to compare different length strings, go to false directly
        if(len_str1 == len_str2)
        {
            // Status true will be invalidated if any difference is detected
            res = true;

            // Compare characters while end of string and still equal
            while((i < len_str1) && res)
            {
                if(toupper(pString1[i]) != toupper(pString2[i]))
                    res = false;

                i++;
            }
        }
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "compareEqualStringIgnoringCase: Invalid Parameters.");
    
    return res;
}


/**
 * Converts a string to boolean flag depending syntax (true / false, yes / no)
 * @param ptrParameterValue
 * @param ptrBooleanValue
 * @return True if any comparison matched
 */
bool convertStringToBoolean(const char* ptrParameterValue, bool *ptrBooleanValue)
{
	bool res = false;

	if (ptrParameterValue != NULL && ptrBooleanValue != NULL)
	{
		if (strcmp("true", ptrParameterValue) == 0 || strcmp("TRUE", ptrParameterValue) == 0 || strcmp("YES", ptrParameterValue) == 0 || strcmp("yes", ptrParameterValue) == 0)
		{
			*ptrBooleanValue = true;
			res = true;
		}

		if (strcmp("false", ptrParameterValue) == 0 || strcmp("FALSE", ptrParameterValue) == 0 || strcmp("NO", ptrParameterValue) == 0 || strcmp("no", ptrParameterValue) == 0)
		{
			*ptrBooleanValue = false;
			res = true;
		}

	}

	return res;
}


/**
 * Converts a string to long value, with checking of length reached
 * @param ptrParameterValue
 * @param ptrLongValue
 * @return True if conversion operation and length checking OK.
 */
bool convertStringToLong(const char* ptrParameterValue, long *ptrLongValue)
{
	bool res = false;

	if (ptrParameterValue != NULL && ptrLongValue != NULL)
	{
		char* ptrEnd = NULL;
		long longValue = strtol(ptrParameterValue, &ptrEnd, 10);
		if (ptrEnd == ptrParameterValue + strlen(ptrParameterValue))
		{
			*ptrLongValue = longValue;
			res = true;
		}
	}

	return res;
}


/**
 * Convert string to lowercase
 * @param ptrSourceString Pointer on string to convert
 * @param ptrDestString Pointer on buffer that will receive converted string
 * @param ptrDestStringSize Size of destination buffer. Must be large enough to receive string with '\0' character at the end.
 * @return True if conversion is successful.
 */
bool convertStringToLower(const char * ptrSourceString, char * ptrDestString, size_t ptrDestStringSize)
{
    bool res = false;
    size_t sourceLength = 0;
    
    if(ptrSourceString != NULL && ptrDestString != NULL && ptrDestStringSize > 0)
    {
        sourceLength = strlen(ptrSourceString);
        
        if(sourceLength < ptrDestStringSize)    // Check destination buffer size
        {
            size_t i = 0;
            
            while(i < sourceLength)
            {
                ptrDestString[i] = tolower(ptrSourceString[i]);
                i++;
            }
            ptrDestString[i] = '\0'; // Terminate destination string
            
            res = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "convertStringToLower: Destination buffer too small.");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "convertStringToLower: Invalid Parameters.");
        
    return res;
}

/**
 * Extract subjectAltName (Id 2.5.29.17) OID raw Object Identifier from a certificate (X509). 
 * Note: Certificate data structure check is not fully accurate (longer study needed), this is not the purpose of this function.
 * @param ptrCertificate Pointer on data area containing certificate, hex format
 * @param certificateLength Length of data field containing certificate
 * @param ptrOID Pointer on data area that will receive OID raw data
 * @param ptrOIDsize Will return size of OID found in certificate, zero if not found
 * @param oidSizeMax Define the maximum size allowed for data area that will contain OID
 * @return True if successful, false if problem during parsing (Incorrect TLV structure detected) or not found or incorrect parameter. Not having extensions defined is also considered as an error.
 */
bool extractOIDfromCertificate(const unsigned char * ptrCertificate, const size_t certificateLength, unsigned char * ptrOID, size_t * ptrOIDsize, const size_t oidSizeMax)
{
    bool res = false;
    
    BeerTLV * berTLVmainContainer30 = NULL;
    BeerTLV * berTLVtbsCertificateContainer30 = NULL;
    BerTLVList * berTLVlist_tbsCert30base = NULL;
    uint8_t tlvList_tbsCert30baseCount = 0;
    BerTLVList * berTLVlist_tbsCert30baseParser = NULL;
    BeerTLV * berTLVlist_tbsCert30baseCurrent = NULL;
    BeerTLV * berTLVextensionSequence30 = NULL;
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "extractOIDfromCertificate()...");
    
    // Certificate minimum size is totally arbitrary, not studied here. Better write at least 15 than 0
    // OID minimum data area size fixed to 1, because only one byte can encode the 2 first digits of OID
    if(ptrCertificate != NULL && certificateLength > 15 && ptrOID != NULL && ptrOIDsize != NULL && oidSizeMax > 0)
    {
        // Initialize OID length
        *ptrOIDsize = 0;
        
        // Main certificate container 0x30
        berTLVmainContainer30 = berTLV_extractTagUInt8(0x30, ptrCertificate, certificateLength, NULL);
        // Certificate minimum size is totally arbitrary, not studied here. Better write at least 12 than 0
        if(berTLVmainContainer30 != NULL && berTLVmainContainer30->length > 12)
        {
            berTLVtbsCertificateContainer30 = berTLV_extractTagUInt8(0x30, berTLVmainContainer30->value, berTLVmainContainer30->length, NULL);
            // tbsCertificate container minimum size is totally arbitrary, not studied here. Better write at least 10 than 0
            if(berTLVtbsCertificateContainer30 != NULL && berTLVtbsCertificateContainer30->length > 10)
            {
                berTLVlist_tbsCert30base = berTLV_extractList(berTLVtbsCertificateContainer30->value, berTLVtbsCertificateContainer30->length, &tlvList_tbsCert30baseCount);
                // At least 6 objects must exist in tbsCertificate container (Exact mandatory list not fully studied, should be 7 including Certificate Extensions, TBC)
                if(berTLVlist_tbsCert30base != NULL && tlvList_tbsCert30baseCount > 5)
                {
                    berTLVlist_tbsCert30baseParser = berTLVlist_tbsCert30base;
                    // Parse main objects to find Certificate Extensions container 0xA3
                    while(berTLVlist_tbsCert30baseParser != NULL)
                    {
                        berTLVlist_tbsCert30baseCurrent = berTLVlist_tbsCert30baseParser->berTLV;
                        
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Main object tag found: %X", berTLVlist_tbsCert30baseCurrent->tag);
                        
                        if(berTLVlist_tbsCert30baseCurrent->tag == 0xA3)
                        {
                            // Extract extensions main sequence container (0x30)
                            berTLVextensionSequence30 = berTLV_extractTagUInt8(0x30, berTLVlist_tbsCert30baseCurrent->value, berTLVlist_tbsCert30baseCurrent->length, NULL);
                            // At least 2 bytes in container (So tag + zero length)
                            if(berTLVextensionSequence30 != NULL && berTLVextensionSequence30->length > 1)
                            {
                                // Parse sequence container to find OID (subjectAltName -> Registered ID)
                                res = _extractOIDfromCertificateExtensionList(berTLVextensionSequence30->value, berTLVextensionSequence30->length, ptrOID, ptrOIDsize, oidSizeMax);
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "extractOIDfromCertificate: Sequence container 0x30 not found in Certificate Extension container 0xA3 or length too short.");
                            
                            ERASE_BERTLV(berTLVextensionSequence30);
                           
                            break; // No need to parse another main object in certificate
                        }
                        
                        // Parse next main object
                        berTLVlist_tbsCert30baseParser = berTLVlist_tbsCert30baseParser->ptrNext;
                    }
                    
                    // If main objects parser reached NULL, it means than Certificate Extension container was not found
                    if(berTLVlist_tbsCert30baseParser == NULL)
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "extractOIDfromCertificate: Certificate Extension container (Tag 0xA3) not found.");
                        
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "extractOIDfromCertificate: Failed to generate tbsCertificate objects list or not enough objects found.");
                
                ERASE_BERTLV_LIST(berTLVlist_tbsCert30base);
                berTLVlist_tbsCert30baseParser = NULL;
                berTLVlist_tbsCert30baseCurrent = NULL;
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "extractOIDfromCertificate: tbsCertificate container 0x30 not found or size too small.");
            
            ERASE_BERTLV(berTLVtbsCertificateContainer30);
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "extractOIDfromCertificate: Main container 0x30 not found or size too small.");
        
        ERASE_BERTLV(berTLVmainContainer30);
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "extractOIDfromCertificate: Invalid Parameters.");
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "extractOIDfromCertificate(): return %s", (res ? "true" : "false"));
    
    return res;
}


/**
 * Extract subjectAltName (Id 2.5.29.17) OID raw object (Registered ID) from Certificate Extension list (Sequence) - Note: Only first Registered ID will be used
 * @param ptrExtensionListData Pointer on extension list data, hex format
 * @param ExtensionListDataSize Extension list data size
 * @param ptrOID Pointer on data area that will receive OID raw data
 * @param ptrOIDsize Will return size of OID found in certificate, zero if not found
 * @param oidSizeMax Define the maximum size allowed for data area that will contain OID
 * @return True if successful, false if problem during parsing (Incorrect TLV structure detected) or incorrect parameter or OID not found
 */
bool _extractOIDfromCertificateExtensionList(const unsigned char * ptrExtensionListData, const size_t ExtensionListDataSize, unsigned char * ptrOID, size_t * ptrOIDsize, const size_t oidSizeMax)
{
    bool res = false;
    
    const unsigned char subjectAltNameID[] = {0x55, 0x1D, 0x11};  // Coding of OID identifying subjectAltName (Id 2.5.29.17)
    
    BerTLVList * berTLVlistExtContainerBase = NULL;
    uint8_t tlvListExtContainerCount = 0;
    BerTLVList * berTLVlisExtContainerParser = NULL;
    BeerTLV * berTLVlistExtContainerCurrent = NULL;
    
    BeerTLV * berTLVextensionId = NULL;
    BeerTLV * berTLVsubjectAltNameMain = NULL;
    BeerTLV * berTLVsubjectAltNameSeq30 = NULL;
    BeerTLV * berTLVsubjectAltNameRegID = NULL;
    
    
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractOIDfromCertificateExtensionList()...");
    
    // OID minimum data area size fixed to 1, because only one byte can encode the 2 first digits of OID
    if(ptrExtensionListData != NULL && ExtensionListDataSize > 0 && ptrOID != NULL && ptrOIDsize != NULL && oidSizeMax > 0)
    {
        // Initialize OID length
        *ptrOIDsize = 0;
        
        berTLVlistExtContainerBase = berTLV_extractList(ptrExtensionListData, ExtensionListDataSize, &tlvListExtContainerCount);
        // At least one object shall be present in list
        if(berTLVlistExtContainerBase != NULL && tlvListExtContainerCount > 0)
        {
            berTLVlisExtContainerParser = berTLVlistExtContainerBase;

            while(berTLVlisExtContainerParser != NULL)
            {
                berTLVlistExtContainerCurrent = berTLVlisExtContainerParser->berTLV;
                
                // Only consider sequence Extension objects, ignore eventual other ones
                if(berTLVlistExtContainerCurrent->tag == 0x30)
                {
                    lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, "Certificate Extension data", ">>", berTLVlistExtContainerCurrent->value, berTLVlistExtContainerCurrent->length);
                    
                    // Search for Extension identifier. If not found generate an error because shall be present
                    berTLVextensionId = berTLV_extractTagUInt8(0x06, berTLVlistExtContainerCurrent->value, berTLVlistExtContainerCurrent->length, NULL);
                    
                    // Extension ID is always 3 bytes long
                    if(berTLVextensionId != NULL && berTLVextensionId->length == 3)
                    {
                        // Check if Extension is subjectAltName (Id 2.5.29.17 so 0x551D11)
                        if(0 == memcmp(berTLVextensionId->value, subjectAltNameID, 3))
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "subjectAltName extension found (Id 2.5.29.17 so 0x551D11)");
                            // Extract subjectAltName main container data, Tag 0x04
                            berTLVsubjectAltNameMain = berTLV_extractTagUInt8(0x04, berTLVlistExtContainerCurrent->value, berTLVlistExtContainerCurrent->length, NULL);
                            
                            // Shall be at least 4 bytes long to contain sequence object
                            if(berTLVsubjectAltNameMain != NULL && berTLVsubjectAltNameMain->length > 3)
                            {
                                // Extract sequence object 0x30 from subjectAltName main container 
                                berTLVsubjectAltNameSeq30 = berTLV_extractTagUInt8(0x30, berTLVsubjectAltNameMain->value, berTLVsubjectAltNameMain->length, NULL);
                                
                                // Shall be at least 2 bytes long to contain at least one object, even empty (RFC5280 defines that at least one object shall be defined)
                                if(berTLVsubjectAltNameSeq30 != NULL && berTLVsubjectAltNameSeq30->length > 1)
                                {
                                    // Extract first Registered ID 0x88 in subjectAltName sequence
                                    berTLVsubjectAltNameRegID = berTLV_extractTagUInt8(0x88, berTLVsubjectAltNameSeq30->value, berTLVsubjectAltNameSeq30->length, NULL);
                                    
                                    // Empty Registered ID does not exist, even 0.0 OID is written in one byte set at 0x00
                                    if(berTLVsubjectAltNameRegID != NULL && berTLVsubjectAltNameRegID->length > 0)
                                    {
                                        // If Registered ID found, copy it in OID, if size does not exceed buffer size
                                        if(berTLVsubjectAltNameRegID->length <= oidSizeMax)
                                        {
                                            memcpy(ptrOID, berTLVsubjectAltNameRegID->value, berTLVsubjectAltNameRegID->length);
                                            *ptrOIDsize = berTLVsubjectAltNameRegID->length;
                                            
                                            lpaCoreLogAppendByteArray(SDK_LOG_LEVEL_DEBUG, "OID found (Registered ID, Tag 0x88)", ">>>", ptrOID, *ptrOIDsize);
                                            
                                            res = true;
                                        }
                                        else
                                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractOIDfromCertificateExtensionList: Registered ID size (%u) exceeds bound (%d)",berTLVsubjectAltNameRegID->length, oidSizeMax);
                                    }
                                    else
                                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractOIDfromCertificateExtensionList: Registered ID tag 0x88 not found in subjectAltName sequence or too short.");
                                }
                                else
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractOIDfromCertificateExtensionList: subjectAltName sequence container 0x30 not found or too short.");
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractOIDfromCertificateExtensionList: subjectAltName data TLV 0x04 not found in extension or too short.");

                            
                            break;  // No need to parse anymore whatever is result
                        }
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractOIDfromCertificateExtensionList: Extension identifier TLV 0x06 not found in extension or incorrect length.");
                        break;  // Stop to parse extensions
                    }
                }
                
                // For cleanup if not subjectAltName encountered
                ERASE_BERTLV(berTLVextensionId);
                
                // Go to next extension
                berTLVlisExtContainerParser = berTLVlisExtContainerParser->ptrNext;
            }
            
            if(berTLVlisExtContainerParser == NULL)
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "subjectAltName extension not found");
                                    
            // Some erase reported again here due to stop parsing with break
            ERASE_BERTLV(berTLVextensionId);
            ERASE_BERTLV(berTLVsubjectAltNameMain);
            ERASE_BERTLV(berTLVsubjectAltNameSeq30);
            ERASE_BERTLV(berTLVsubjectAltNameRegID);
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractOIDfromCertificateExtensionList: Failed to generate extension list or not enough elements.");
        
        ERASE_BERTLV_LIST(berTLVlistExtContainerBase);
        berTLVlisExtContainerParser = NULL;
        berTLVlistExtContainerCurrent = NULL;
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_extractOIDfromCertificateExtensionList: Invalid Parameters.");
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_extractOIDfromCertificateExtensionList(): return %s", (res ? "true" : "false"));
    
    return res;
}


/**
 * Convert raw ASN1 OID to text readable string (1.2....).
 * Note: As reference, with use of long long unsigned integer on 64 bits it allow to support at least 63 bits, the maximum possible with VLQ value coded on 9 bytes.
 * @param ptrOID Pointer on OID raw data, hex format
 * @param OIDsize Length of OID raw data
 * @param ptrOIDtext Pointer on text area that will receive OID text conversion, string format
 * @param OIDtextMaxSize Maximum size of text area that will receive OID text, including end of string
 * @return true if conversion is OK
 */
bool convertASN1_OIDtoText(const unsigned char * ptrOID, const size_t OIDsize, char * ptrOIDtext, const size_t OIDtextMaxSize)
{
    bool res = false;
    unsigned char currentNode[20];      // 20 bytes are widely enough to handle values of 160 bits, really more than possible normally
    size_t currentNodeLength = 0;
    unsigned long long nodeValue = 0;
    size_t oidParser = 0;
    bool processFirstNodes = true;
    unsigned long long firstNode = 0;
    unsigned long long secondNode = 0;
    char textBuffer[50];                // Very big numbers + additional node on startup can need some long space
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "convertASN1_OIDtoText()...");
    
    // Note: With a single byte, OID text will have at least 3 characters "x.y"
    if(ptrOID != NULL && OIDsize > 0 && ptrOIDtext != NULL && OIDtextMaxSize > 2)
    {
        // For the moment, we consider that no error is encountered
        res = true;
        
        // Clear output text area
        memset(ptrOIDtext, 0x00, OIDtextMaxSize);
        
        // Parse all source
        while(oidParser < OIDsize)
        {
            currentNodeLength = 0;
            
            // Parse ASN1 OID value, taking account of possible nodes coded as VLQ values
            if(parseDataWithVLQnodes(&ptrOID[oidParser], OIDsize - oidParser, currentNode, &currentNodeLength, sizeof(currentNode)))
            {
                // If node is not VLQ encoded, take value as it
                if(currentNodeLength == 1)
                {
                    nodeValue = currentNode[0];
                    oidParser++;
                }
                else
                {
                    // If node is VLQ encoded, decode it
                    if(decodeVLQvalue(currentNode, currentNodeLength, &nodeValue))
                    {
                        // Update parsing index with the length of VLQ node
                        oidParser = oidParser + currentNodeLength;
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "convertASN1_OIDtoText: Issue encountered while decoding VLQ value. Conversion canceled.");
                        ptrOIDtext[0] = 0;
                        res = false;
                        break;      
                    }
                }
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "convertASN1_OIDtoText: Issue encountered while parsing OID nodes. Conversion canceled.");
                ptrOIDtext[0] = 0;
                res = false;
                break;               
            }
            
            // Reset text Buffer
            textBuffer[0] = '\0';
            
            // Startup is specific: Two first OID nodes are encoded on a single value
            if(processFirstNodes)
            {
                // Divide by 40 to find first node. Only 0, 1 or 2 can exist so if >= 2 will be 2
                firstNode = nodeValue / 40;
                    
                if(firstNode > 1)
                    firstNode = 2;
                
                // Second node is the remaining of first value minus first node (Encoded x 40)
                secondNode = nodeValue - (firstNode * 40);
                
                snprintf(textBuffer, sizeof(textBuffer), "%llu.%llu", firstNode, secondNode);
                
                processFirstNodes = false;
            }
            else
                snprintf(textBuffer, sizeof(textBuffer), ".%llu", nodeValue);
            
            // Append new node while it does not exceed maximum output text length
            if((strlen(ptrOIDtext) + strlen(textBuffer)) < OIDtextMaxSize)
                strncat(ptrOIDtext, textBuffer, (OIDtextMaxSize - strlen(ptrOIDtext) - 1));
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "convertASN1_OIDtoText: OID value length exceeds output buffer capabilities. Conversion canceled.");
                ptrOIDtext[0] = 0;
                res = false;
                break;
            }
        }
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "convertASN1_OIDtoText: Invalid Parameters.");
    
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "convertASN1_OIDtoText(): return %s", (res ? "true" : "false"));
    
    return res;
}


/**
 * Parse data that could contain Variable Length Quantity (VLQ) node and return first value encountered (single value minus than 128 or VLQ array)
 * @param ptrSource Pointer on source data, hex format
 * @param sourceLength Length of source data
 * @param ptrExtractData Pointer on array that will receive value (Index 0) or VLQ array
 * @param ptrExtractDatalength Length of extracted data (1 if simple non VLQ value)
 * @param extractDataMaxSize Maximum size of array that will receive value or VLQ array
 * @return true if extraction is OK
 */
bool parseDataWithVLQnodes(const unsigned char * ptrSource, const size_t sourceLength, unsigned char * ptrExtractData, size_t * ptrExtractDatalength, const size_t extractDataMaxSize)
{
    bool res = false;
    unsigned int index = 0;
    
    if(ptrSource != NULL && sourceLength > 0 && ptrExtractData != NULL && ptrExtractDatalength != NULL && extractDataMaxSize > 0)
    {
        // For the moment, we consider that no error is encountered
        res = true;
        
        // For the moment, nothing extracted
        *ptrExtractDatalength = 0;
        
        // Check if first byte if VLQ value (Bit 7 set to 1) of other value
        if((ptrSource[0] & 0x80) > 0)
        {
            // VLQ node encountered, copy values from data source to extracted data buffer
            while(((ptrSource[index] & 0x80) > 0) && (index < sourceLength) && (index < extractDataMaxSize))
            {
                ptrExtractData[index] = ptrSource[index];
                index++;
            }
            
            // if end of data is not reached, copy next byte with b7 not set to "1"
            if((index < sourceLength) && (index < extractDataMaxSize))
            {
                ptrExtractData[index] = ptrSource[index];
                *ptrExtractDatalength = index + 1;
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "parseDataWithVLQnodes: End of data reached before last VLQ byte (b7 = 0) found or extraction buffer overflow.");
                res = false;
                *ptrExtractDatalength = 0;
            }
        }
        else
        {
            // Normal value between 0 and 0x7F, just return it as it
            ptrExtractData[0] = ptrSource[0];
            *ptrExtractDatalength = 1;
        }
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "parseDataWithVLQnodes: Invalid Parameters.");
    
    return res;
}


/**
 * Decode Variable Length Quantity (VLQ) value (Also named Base128) to unsigned integer (long long)
 * Note: As reference, with use of long long unsigned integer on 64 bits it allow to support at least 63 bits, the maximum possible with VLQ value coded on 9 bytes
 * @param ptrVLQvalue Pointer on array that contain VLQ value, hex format
 * @param VLQvalueSize Length of array containing VLQ value
 * @param ptrOutputValue Pointer on variable that will receive converted value
 * @return true if decoding is OK
 */
bool decodeVLQvalue(const unsigned char * ptrVLQvalue, const size_t VLQvalueSize, unsigned long long * ptrOutputValue)
{
    bool res = false;
    unsigned long long outputORmask = 0x01;
    unsigned char inputANDmask = 0x01;
    int inputShifter = 0;
    int inputIndex = 0;
    int totalShift = 0;
    int maxShift = 0;
    
    // Note: Having a VLQ value of less than 2 bytes long is not applicable
    if(ptrVLQvalue != NULL && VLQvalueSize > 1 && ptrOutputValue != NULL)
    {
        // For the moment, we consider that no error is encountered
        res = true;
       
        *ptrOutputValue = 0;
        // Size of long long can vary depending system / compiler used
        maxShift = 8 * sizeof(*ptrOutputValue);
                
        // Parse VLQ from last element to first one
        inputIndex = VLQvalueSize - 1;
        
        while(inputIndex >= 0 && res)
        {
            // Decode each bit of VLQ value byte and report it to output, from bits 0 to 6 (Bit 7 ignored)
            for(inputShifter = 0; inputShifter < 7; inputShifter++)
            {
                // Manage number of shifting performed.
                // If overflow long long size, stop process on error
                totalShift++;
                if(totalShift > maxShift)
                {
                    res = false;
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "decodeVariableLengthQuantityValue: Output value (unsigned long long) capacity overflow, cancel decoding.");
                    *ptrOutputValue = 0;
                    break;
                }
                
                // Get bit value in input. If set to 1, set corresponding bit to 1 in output, else leave it at 0.
                if((ptrVLQvalue[inputIndex] & inputANDmask) > 0)
                    *ptrOutputValue = *ptrOutputValue | outputORmask;
                
                // Shift masks
                inputANDmask = inputANDmask << 1;
                outputORmask = outputORmask << 1;
            }
            
            // Current VLQ byte finished, got to next one (Rigth to left) and reset input mask
            inputIndex--;
            inputANDmask = 0x01;
        }
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "decodeVariableLengthQuantityValue: Invalid Parameters.");
    
    return res;
}


/**
 * Encodes an hex data field in Base64 format - Wrapper to base64.c library
 * @param indata Input hex data field buffer, byte format
 * @param inlen Input, Length of input data field buffer
 * @param outdata Output Base64 data field, characters without \0 end
 * @param outlen Output, Length of Base64 data stored in output buffer, optional if NULL specified
 * @param maxSize Input, Maximum size allowed for output buffer
 * @return true if conversion was successful
 */
bool ffw_base64_encode(const unsigned char *indata, size_t inlen, char *outdata, size_t *outlen, size_t maxSize)
{
    bool res = false;
    unsigned int encodedSize = 0;
    
    if (indata != NULL && inlen > 0 && outdata != NULL)
    {
        // Check output buffer size before launch encoding
        if(maxSize >= b64e_size((unsigned int)inlen))
        {
            encodedSize = b64_encode(indata, (unsigned int)inlen, (unsigned char *)outdata);
            
            if(encodedSize > 0)
            {
                res = true;
                
                if (outlen != NULL)
                    *outlen = (size_t)encodedSize;
            }
        }
    }
    
    return res;
}


/**
 * Decodes a Base64 data field in hex format - Wrapper to base64.c library
 * @param indata Input Base64 data field buffer, characters
 * @param inlen Input, Length of input Base64 data field buffer
 * @param outdata Output hex data field, bytes
 * @param outlen Output, Length of Hex data stored in output buffer, optional if NULL specified
 * @param destSize Input, Maximum size allowed for output buffer
 * @return true if conversion was successful
 */
bool ffw_base64_decode(const char *indata, size_t inlen, unsigned char *outdata, size_t *outlen, size_t destSize)
{
    bool res = false;
    unsigned int decodedSize = 0;
    
    if (indata != NULL && inlen > 0 && outdata != NULL && (inlen % 4 == 0))
    {
        // Check output buffer size before launch decoding
        if(destSize >= b64d_size((unsigned int)inlen))
        {
            decodedSize = b64_decode((unsigned char*) indata, (unsigned int)inlen, outdata);
            
            if(decodedSize > 0)
            {
                res = true;
                
                if (outlen != NULL)
                    *outlen = (size_t)decodedSize;
            }
        }
    }

    return res;
}

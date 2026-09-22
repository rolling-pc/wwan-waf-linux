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

#ifndef LPA_SDK__CORE_UTIL_H
#define LPA_SDK__CORE_UTIL_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>	

size_t formatBytesToHexaString(const unsigned char *ptrDataBytes, size_t dataSize, char* ptrBuffer, size_t bufferMaxSize);
bool writeIntegerValueToByteArray(uint16_t integerValue, unsigned char *ptrByteArray, size_t byteArrayMaxSize, size_t* byteArraySize );
bool extractIntegerFromByteArray(const unsigned char *ptrByteArray, size_t byteArraySize, uint16_t* ptrIntegerValue);
bool hexStr2ByteArray(const unsigned char * inHexString,size_t inLen, unsigned char * outHex, int* outLen);
//bool encodeLength(int length, unsigned char* lengthTLV, const size_t lengthTLVsize, size_t * attributeLength);
bool encodeLength(size_t length, unsigned char* lengthTLV, const size_t lengthTLVsize, size_t* attributeLength);
bool generateLength(int length, unsigned char* lengthHex, const size_t lengthHexsize, size_t * attributeLength);
int oneHexCharToHex(char h);
bool split(char *src,const char *separator,char **dest,int *num);
bool findSubstr(char* source, char* target);
int countCharOccurencesInString(const char * pString, const char c);
bool isElementPresentInArrayUInt(const unsigned int *pReferenceArray, const size_t pArraySize, const unsigned int pValue);
bool compareEqualStringIgnoringCase(const char * pString1, const char * pString2);
bool convertStringToBoolean(const char* ptrParameterValue, bool *ptrBooleanValue);
bool convertStringToLong(const char* ptrParameterValue, long *ptrLongValue);
bool convertStringToLower(const char * ptrSourceString, char * ptrDestString, size_t ptrDestStringSize);
bool extractOIDfromCertificate(const unsigned char * ptrCertificate, const size_t certificateLength, unsigned char * ptrOID, size_t * ptrOIDsize, const size_t oidSizeMax);
bool convertASN1_OIDtoText(const unsigned char * ptrOID, const size_t OIDsize, char * OIDtext, const size_t OIDtextMaxSize);
bool parseDataWithVLQnodes(const unsigned char * ptrSource, const size_t sourceLength, unsigned char * ptrExtractData, size_t * ptrExtractDatalength, const size_t extractDataMaxSize);
bool decodeVLQvalue(const unsigned char * ptrVLQvalue, const size_t VLQvalueSize, unsigned long long * ptrOutputValue);
bool ffw_base64_encode(const unsigned char *indata, size_t inlen, char *outdata, size_t *outlen, size_t maxSize);
bool ffw_base64_decode(const char *indata, size_t inlen, unsigned char *outdata, size_t *outlen, size_t destSize);

#endif // LPA_SDK__CORE_UTIL_H

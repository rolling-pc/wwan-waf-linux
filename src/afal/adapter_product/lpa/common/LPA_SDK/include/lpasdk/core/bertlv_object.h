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

#ifndef LPA_SDK__CORE_BERTLV_H
#define LPA_SDK__CORE_BERTLV_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include "lpasdk/core/lpa_core.h"
#include "lpasdk/core/rawdata_object.h"

typedef struct
{
	uint16_t tag;
	//uint32_t length;
	size_t length;
	unsigned char* value;
}BeerTLV, *PtrBeerTLV;

//typedef struct BerTLVList BerTLVList;
typedef struct BerTLVList
{
	uint16_t index;
	BeerTLV* berTLV;
	struct BerTLVList* ptrNext;
} BerTLVList;

//UT_EXPORT_DLL BeerTLV* berTLV_create(uint16_t tag, uint32_t length, const unsigned char* ptrValue);
UT_EXPORT_DLL BeerTLV* berTLV_create(uint16_t tag, size_t length, const unsigned char* ptrValue);
//UT_EXPORT_DLL BeerTLV* berTLV_extractTagUInt8(uint8_t tag, const unsigned char* ptrRawData, uint32_t rawDataSize, bool* ptrIsTagFound);
UT_EXPORT_DLL BeerTLV* berTLV_extractTagUInt8(uint8_t tag, const unsigned char* ptrRawData, size_t rawDataSize, bool* ptrIsTagFound);
//UT_EXPORT_DLL BeerTLV* berTLV_extractTagUInt8(uint8_t tag, const unsigned char* ptrRawData, size_t rawDataSize, bool* ptrIsTagFound);
//UT_EXPORT_DLL BeerTLV* berTLV_extractTagUInt16(uint16_t tag, const unsigned char* ptrRawData, uint32_t rawDataSize, bool* ptrIsTagFound);
UT_EXPORT_DLL BeerTLV* berTLV_extractTagUInt16(uint16_t tag, const unsigned char* ptrRawData, size_t rawDataSize, bool* ptrIsTagFound);
//UT_EXPORT_DLL BerTLVList* berTLV_extractList(const unsigned char* ptrRawData, uint32_t rawDataSize, uint8_t* ptrCountTLVFound);
UT_EXPORT_DLL BerTLVList* berTLV_extractList(const unsigned char* ptrRawData, size_t rawDataSize, uint8_t* ptrCountTLVFound);

UT_EXPORT_DLL RawDataObject* berTLV_buildRawDataObject(const BeerTLV* ptrBerTlv);
//UT_EXPORT_DLL RawDataObject* berTLV_createAndBuildRawDataObject(uint16_t tag, uint32_t length, const unsigned char* ptrValue);
UT_EXPORT_DLL RawDataObject* berTLV_createAndBuildRawDataObject(uint16_t tag, size_t length, const unsigned char* ptrValue);
UT_EXPORT_DLL bool berTLV_freeBerTLV(BeerTLV* ptrBerTLV);
UT_EXPORT_DLL bool berTLV_freeBerTLVList(BerTLVList* ptrBerTLVList);


// Some macro to help memory cleanup
#define ERASE_BERTLV(_ptr) if ((_ptr) != NULL) { berTLV_freeBerTLV( (_ptr) ); (_ptr) = NULL;}
#define ERASE_BERTLV_LIST(_ptr) if ((_ptr) != NULL) { berTLV_freeBerTLVList( (_ptr) ); (_ptr) = NULL;}

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // LPA_SDK__CORE_BERTLV_H
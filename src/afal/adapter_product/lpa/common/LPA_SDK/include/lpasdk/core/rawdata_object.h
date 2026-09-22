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

#ifndef LPA_SDK__CORE_RAW_DATA_OBJECT_H
#define LPA_SDK__CORE_RAW_DATA_OBJECT_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lpasdk/core/lpa_core.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct
{
	unsigned char* rawData;
	size_t rawDataSize;
}RawDataObject, *PtrRawDataObject;

UT_EXPORT_DLL RawDataObject* rawDataObject_allocate();
UT_EXPORT_DLL RawDataObject* rawDataObject_create(const unsigned char* ptrRawData, size_t rawDataSize);
UT_EXPORT_DLL RawDataObject* rawDataObject_createAsLV(const unsigned char* ptrRawData, size_t rawDataSize);
UT_EXPORT_DLL RawDataObject* rawDataObject_concat(const RawDataObject* ptrRawDataObject1, const RawDataObject*ptrRawDataObject2);
UT_EXPORT_DLL RawDataObject* rawDataObject_concatRawDataArray(const RawDataObject* ptrRawDataObject1, const unsigned char* ptrRawData2, size_t rawDataSize2);
UT_EXPORT_DLL RawDataObject* rawDataObject_concatPartially(const RawDataObject* ptrRawDataObject1, const RawDataObject*ptrRawDataObject2, size_t offset, size_t length);
UT_EXPORT_DLL bool rawDataObject_appendRawDataArray(RawDataObject* ptrRawDataObjectSource, const unsigned char* ptrRawDataAppend, size_t rawDataSizeAppend);

UT_EXPORT_DLL bool rawDataObject_update(RawDataObject* ptrRawDataObject, const unsigned char* ptrRawData, size_t rawDataSize);
UT_EXPORT_DLL void rawDataObject_clear(RawDataObject* ptrRawDataObject);
UT_EXPORT_DLL void rawDataObject_free(RawDataObject* ptrRawDataObject);

// Some macro to help memory cleanup
#define ERASE_RAWDATAOBJECT(_ptr) if ((_ptr) != NULL) { rawDataObject_free( (_ptr) ); (_ptr) = NULL;}

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // LPA_SDK__CORE_RAW_DATA_OBJECT_H

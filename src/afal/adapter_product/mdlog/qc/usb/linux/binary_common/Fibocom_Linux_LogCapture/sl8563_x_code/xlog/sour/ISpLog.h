/* *****************************************************
SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0

Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
Licensed under the Unisoc General Software License, version 1.0 (the License);
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
Software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OF ANY KIND, either express or implied.
See the Unisoc General Software License, version 1.0 for more details.
******************************************************* */

#ifndef ISPLOG_H_INCLUDED
#define ISPLOG_H_INCLUDED

#include "typedef.h"
// --------------------------------------------------------------------------------
//  Maximum number of bytes stored in a string, for characters as follows:
//  ansi    :  MAX_STRING_IN_BYTES
//  unicode :  MAX_STRING_IN_BYTES / 2
//
//  Remarks:
//  If the number of bytes store in string exceed MAX_STRING_IN_BYTES,
//  the exceed bytes in string would be ignored.
#define MAX_STRING_IN_BYTES             ( 8192 )


//  Maximum bytes in a line
#define MAX_LINE_HEX_BYTES              ( 16 )

enum
{
    LOG_READ  = 0,
    LOG_WRITE = 1,
    LOG_ASYNC_READ =2
};

// --------------------------------------------------------------------------------
//  Log level:
//
typedef enum
{
    SPLOGLV_NONE      = 0,
    SPLOGLV_ERROR     = 1,
    SPLOGLV_WARN      = 2,
    SPLOGLV_INFO      = 3,
	SPLOGLV_DATA      = 4, //exclusive, only output data
    SPLOGLV_VERBOSE   = 5,
    SPLOGLV_SPLIT     = 6

}SPLOG_LEVEL;


#endif // ISPLOG_H_INCLUDED

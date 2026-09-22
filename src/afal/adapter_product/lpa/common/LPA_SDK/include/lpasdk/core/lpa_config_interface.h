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

#ifndef LPA_SDK__CONFIG_INTERFACE_H
#define LPA_SDK__CONFIG_INTERFACE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lpasdk/core/lpa_log_interface.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


	typedef struct LPA_CONFIG_INTERFACE LPA_CONFIG_INTERFACE;
	struct LPA_CONFIG_INTERFACE
	{
		bool (*setLog)(const LPA_LOG_INTERFACE* ptrLogInterface);
		bool (*setConfigFileName)(const char*ptrConfigFileName);
		bool (*load) ();
};

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // LPA_SDK__CONFIG_INTERFACE_H

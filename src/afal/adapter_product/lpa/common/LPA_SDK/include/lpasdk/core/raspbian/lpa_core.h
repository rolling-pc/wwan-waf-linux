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

#ifndef LPA_SDK__CORE_RASPBIAN__LPA_CORE_H
#define LPA_SDK__CORE_RASPBIAN__LPA_CORE_H

#ifndef LPA_SDK__PLATFORM_RASPBIAN
#error "Incorrect usage of lpasdk/core/raspbian/lpa_core.h (PLATFORM_RASPBIAN not defined)"
#endif


#define LPA_MAX_PATH	260 //MAX_PATH
#define LPA_PATH_SEPARATOR	"/"

// Declare LPA MACRO & Constant (plaform)
#define LPA_SDK_INLINE_FUNCTION static inline
#define UT_EXPORT_DLL								// Nothing specific for this platform


#endif // LPA_SDK__CORE_RASPBIAN__LPA_CORE_H
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

#ifndef LPA_SDK__LPA_CORE_H
#define LPA_SDK__LPA_CORE_H

#ifdef LPA_SDK__PLATFORM_WIN
#include "lpasdk/core/win/lpa_core.h"
#endif //

#ifdef LPA_SDK__PLATFORM_CYGWIN
#include "lpasdk/core/cygwin/lpa_core.h"
#endif //LPA_SDK__PLATFORM_CYGWIN

#ifdef LPA_SDK__PLATFORM_RASPBIAN
#include "lpasdk/core/raspbian/lpa_core.h"
#endif //LPA_SDK__PLATFORM_RASPBIAN

// Define generic MACRO & Constant
///////////////////////////////////////

#define LPA_RES_TRUE_STRING			"true"
#define LPA_RES_FALSE_STRING		"false"

// Check that macro exits for specific platform
///////////////////////////////////////

#ifndef UT_EXPORT_DLL
#error "UT_EXPORT_DLL not defined for current platform"
#endif // UT_EXPORT_DLL

#ifndef LPA_SDK_INLINE_FUNCTION
#error "LPA_SDK_INLINE_FUNCTION not defined for current platform"
#endif // LPA_SDK_INLINE_FUNCTION



#endif // LPA_SDK__LPA_CORE_H



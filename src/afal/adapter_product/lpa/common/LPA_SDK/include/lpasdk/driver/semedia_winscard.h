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

#ifndef LPA_SDK__DRIVER_SEMEDIA_WINSCARD_H
#define LPA_SDK__DRIVER_SEMEDIA_WINSCARD_H

#ifdef LPA_SDK__SEMEDIA_DRIVER_WINSCARD

#if defined(LPA_SDK__PLATFORM_WIN) || defined(LPA_SDK__PLATFORM_CYGWIN) || defined(LPA_SDK__PLATFORM_RASPBIAN)

#include <stdio.h>

#ifdef LPA_SDK__PLATFORM_WIN
// Only available under Windows platform
#include <winscard.h>
#else
    #ifdef LPA_SDK__PLATFORM_CYGWIN
        // Only available under Cygwin platform
        #include <w32api/winscard.h>
    #else // LPA_SDK__PLATFORM_RASPBIAN
        // Raspbian

        #define TEXT(quote) __TEXT(quote)   // r_winnt 
        #define __TEXT(quote) quote         // r_winnt 
        // MessageId: ERROR_BROKEN_PIPE
        //
        // MessageText:
        //
        // The pipe has been ended.
        //
        #define ERROR_BROKEN_PIPE                109L 
        #define SCARD_AUTOALLOCATE (DWORD)(-1)
        #define SCARD_SCOPE_USER     0  // The context is a user context, and any
                                        // database operations are performed within the
                                        // domain of the user. 
        #define SCARD_SCOPE_TERMINAL 1  // The context is that of the current terminal,
                                        // and any database operations are performed
                                        // within the domain of that terminal.  (The
                                        // calling application must have appropriate
                                        // access permissions for any database actions.)
        #define SCARD_SCOPE_SYSTEM    2 // The context is the system context, and any
                                        // database operations are performed within the
                                        // domain of the system.  (The calling
                                        // application must have appropriate access
                                        // permissions for any database actions.) 
        #define SCARD_ALL_READERS       TEXT("SCard$AllReaders\000")
        #define SCARD_DEFAULT_READERS   TEXT("SCard$DefaultReaders\000")
        #define SCARD_LOCAL_READERS     TEXT("SCard$LocalReaders\000")
        #define SCARD_SYSTEM_READERS    TEXT("SCard$SystemReaders\000") 

        #include <PCSC/wintypes.h>
        #include <PCSC/winscard.h>
    #endif // LPA_SDK__PLATFORM_CYGWIN
#endif //LPA_SDK__PLATFORM_WIN

#include "lpasdk/core/semedia_manager.h"
#include "lpasdk/core/semedia_base.h"

typedef struct
{
	// Base part
	/////////////////////////
	TSEMedia* _ptrBase;            

	// Specific part
	/////////////////////////
	SCARDCONTEXT _scardContext;     
	SCARDHANDLE _scardHandle;       
	bool _contextEstablished;

	SE_MEDIA_DISCONNECT_CARD_PARAM _disconnectParam;
	DWORD	_connectSharedMode;

} TSEMediaWinSCard;

// Simulate object constructor
TSEMedia* New_SEMediaWinSCard();

// Simulate object destructor
void Delete_SEMediaWinSCard(TSEMedia* ptrTSEMedia);

#endif //def LPA_SDK__PLATFORM_WIN || LPA_SDK__PLATFORM_CYGWIN

#endif // LPA_SDK__SEMEDIA_DRIVER_WINSCARD

#endif // LPA_SDK__DRIVER_SEMEDIA_WINSCARD_H
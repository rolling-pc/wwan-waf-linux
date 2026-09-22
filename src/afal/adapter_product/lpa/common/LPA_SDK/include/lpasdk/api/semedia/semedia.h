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

#ifndef LPA_SDK__API_SE_MEDIA_H
#define LPA_SDK__API_SE_MEDIA_H

// Increase size of Reader Name buffer in Debug mode to allow more space for test command when Windows test driver DLL is used
#if defined(_DEBUG) && defined(LPA_SDK__PLATFORM_WIN) && defined(LPA_SDK__SEMEDIA_DRIVER_EXTERNAL)
#define LPA_CFG_READER_NAME_MAX_SIZE 255
#else
#define LPA_CFG_READER_NAME_MAX_SIZE 64
#endif // _DEBUG & LPA_SDK__PLATFORM_WIN & LPA_SDK__SEMEDIA_DRIVER_EXTERNAL

typedef struct
{
	char readerName[LPA_CFG_READER_NAME_MAX_SIZE];
} LPA_SE_MEDIA_READER_NAME_INFO;

typedef enum seMediaCardStatus
{
	SE_MEDIA_STATUS_SCARD_UNKNOWN = 0x0001,
	SE_MEDIA_STATUS_SCARD_ABSENT = 0x0002,			// There is no card in the reader.
	SE_MEDIA_STATUS_SCARD_PRESENT = 0x0004,			// There is a card in the reader, but it has not been moved into position for use.
	SE_MEDIA_STATUS_SCARD_SWALLOWED = 0x0008,		// There is a card in the reader in position for use.The card is not powered.
	SE_MEDIA_STATUS_SCARD_POWERED = 0x0010,			// Power is being provided to the card, but the reader driver is unaware of the mode of the card.
	SE_MEDIA_STATUS_SCARD_NEGOTIABLE = 0x0020,		// The card has been reset and is awaiting PTS negotiation.
	SE_MEDIA_STATUS_SCARD_SPECIFIC = 0x0040,		// The card has been reset and specific communication protocols have been established.


	SE_MEDIA_STATUS_REMOVED_CARD = 0x80001,			// The smart card has been removed(SCARD_W_REMOVED_CARD)
	SE_MEDIA_RESET_CARD,					// The smart card has been reset(SCARD_W_RESET_CARD)
}SE_MEDIA_CARD_STATUS;

typedef enum seMediaDisconnectCardParam
{
	SE_MEDIA_DISCONNECT_LEAVE_CARD = 0, // Don't do anything special on close
	SE_MEDIA_DISCONNECT_RESET_CARD,     // Reset the card on close
	SE_MEDIA_DISCONNECT_UNPOWER_CARD,   // Power down the card on close
}SE_MEDIA_DISCONNECT_CARD_PARAM;

#endif // LPA_SDK__API_SE_MEDIA_H
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


#include "lpasdk/driver/semedia_winscard.h"
#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_memory.h"
#include "lpasdk/core/util.h"

// This driver is compiled only if LPA_SDK__SEMEDIA_DRIVER_WINSCARD build option exist
#ifdef LPA_SDK__SEMEDIA_DRIVER_WINSCARD

#ifdef LPA_SDK__PLATFORM_RASPBIAN
#include <unistd.h> 
#endif // LPA_SDK__PLATFORM_RASPBIAN

// Functions needed for SE Media driver
bool _seMediaWinSCardSetCallbackEventExecutionError(const struct TSEMedia* ptrTSEMedia, LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback);

bool _seMediaWinSCardEstablishContext(const struct TSEMedia* ptrTSEMedia);
bool _seMediaWinSCardReleaseContext(const struct TSEMedia* ptrTSEMedia);
bool _seMediaWinSCardIsValidContext(const struct TSEMedia* ptrTSEMedia);
bool _seMediaWinSCardIsContextEstablished(const struct TSEMedia* ptrTSEMedia);

bool _seMediaWinSCardListReader(const struct TSEMedia* ptrTSEMedia, LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader);
bool _seMediaWinSCardConnect(const struct TSEMedia* ptrTSEMedia, const char *ptrReaderName);
bool _seMediaWinSCardTransmitApdu(const struct TSEMedia* ptrTSEMedia, const unsigned char* ptrApduCommandBytes, size_t apduCommandSize, unsigned char* ptrApduResponseBytes, size_t* ptrApduResponseMaxSize);
bool _seMediaWinSCardIsConnected(const struct TSEMedia* ptrTSEMedia);
bool _seMediaWinSCardDisconnect(const struct TSEMedia* ptrTSEMedia);
bool _seMediaWinSCardDisconnectWithReset(const struct TSEMedia* ptrTSEMedia);

bool _seMediaWinSCardGetStatus(const struct TSEMedia* ptrTSEMedia, SE_MEDIA_CARD_STATUS* ptrStatus);

// Internals functions
char* _seMediaWinSCardInternalGetSCErrorDescription(DWORD scErrorCode);
bool _seMediaWinSCardInternalDisconnect(const struct TSEMedia* ptrTSEMedia, SE_MEDIA_DISCONNECT_CARD_PARAM disconnectCardParam);
void _seMediaWinSCardInternalSendLpaEventExecutionError(long scErrorCode, const char* ptrErrorCodeDescription);

#define SCARD_INVALID_HANDLE	0L

static char _bufferFormatLogMessage[1024];

// Event execution error callback
static LPA_EVENT_EXECUTION_ERROR _lpaEventExecutionErrorCallback = NULL;

typedef struct
{
	DWORD _errorValue;
	char* _ptrErrorString;
} SCARD_ERROR_INFO;


SCARD_ERROR_INFO _scardErrorList[] =
{
	{ ERROR_BROKEN_PIPE, "ERROR_BROKEN_PIPE" },
	{ SCARD_E_CANCELLED, "SCARD_E_CANCELLED" },
	{ SCARD_E_CANT_DISPOSE, "SCARD_E_CANT_DISPOSE" },
	{ SCARD_E_CARD_UNSUPPORTED, "SCARD_E_CARD_UNSUPPORTED" },
	{ SCARD_E_DUPLICATE_READER, "SCARD_E_DUPLICATE_READER" },
	{ SCARD_E_FILE_NOT_FOUND, "SCARD_E_FILE_NOT_FOUND" },
	{ SCARD_E_INSUFFICIENT_BUFFER, "SCARD_E_INSUFFICIENT_BUFFER" },
	{ SCARD_E_INVALID_ATR, "SCARD_E_INVALID_ATR" },
	{ SCARD_E_INVALID_HANDLE, "SCARD_E_INVALID_HANDLE" },
	{ SCARD_E_INVALID_PARAMETER, "SCARD_E_INVALID_PARAMETER" },
	{ SCARD_E_INVALID_TARGET, "SCARD_E_INVALID_TARGET" },
	{ SCARD_E_INVALID_VALUE, "SCARD_E_INVALID_VALUE" },
	{ SCARD_E_NO_MEMORY, "SCARD_E_NO_MEMORY" },
	{ SCARD_E_NO_READERS_AVAILABLE, "SCARD_E_NO_READERS_AVAILABLE" },
	{ SCARD_E_NO_SMARTCARD, "SCARD_E_NO_SMARTCARD" },
	{ SCARD_E_NOT_READY, "SCARD_E_NOT_READY" },
	{ SCARD_E_PROTO_MISMATCH, "SCARD_E_PROTO_MISMATCH" },
	{ SCARD_E_READER_UNAVAILABLE, "SCARD_E_READER_UNAVAILABLE" },
	{ SCARD_E_READER_UNSUPPORTED, "SCARD_E_READER_UNSUPPORTED" },
	{ SCARD_E_SERVER_TOO_BUSY, "SCARD_E_SERVER_TOO_BUSY" },
	{ SCARD_E_SERVICE_STOPPED, "SCARD_E_SERVICE_STOPPED" },
	{ SCARD_E_SHARING_VIOLATION, "SCARD_E_SHARING_VIOLATION" },
	{ SCARD_E_SYSTEM_CANCELLED, "SCARD_E_SYSTEM_CANCELLED" },
	{ SCARD_E_TIMEOUT, "SCARD_E_TIMEOUT" },
	{ SCARD_E_UNEXPECTED, "SCARD_E_UNEXPECTED" },
	{ SCARD_E_UNKNOWN_CARD, "SCARD_E_UNKNOWN_CARD" },
	{ SCARD_E_UNKNOWN_READER, "SCARD_E_UNKNOWN_READER" },
	{ SCARD_W_REMOVED_CARD, "SCARD_W_REMOVED_CARD" },
	{ SCARD_W_RESET_CARD, "SCARD_W_RESET_CARD" },
	{ SCARD_W_UNSUPPORTED_CARD, "SCARD_W_UNSUPPORTED_CARD" },
	{ SCARD_W_UNRESPONSIVE_CARD, "SCARD_W_UNRESPONSIVE_CARD" },
	{ SCARD_E_COMM_DATA_LOST, "SCARD_E_COMM_DATA_LOST" },

	// Latest record
	{ 0, NULL }
};

/**
 * Initializes a structure type "TSEMediaWinSCard", affect TSEMediaWinSCard->ptrBase to a pointer type "TSEMedia" object and return it.
 * Elements stored points on functions defined below and elsewhere.
 * 
 * @return Pointer on object type TSEMedia
 */
TSEMedia* New_SEMediaWinSCard()
{
	TSEMedia* ptrTSEMedia = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> New_SEMediaWinSCard()");

	TSEMediaWinSCard* seMediaWinSCard = (TSEMediaWinSCard*) lpaCoreMemoryAlloc(sizeof(TSEMediaWinSCard));
	if (seMediaWinSCard != NULL)
	{
		seMediaWinSCard->_ptrBase = New_SEMediaBase();                                          // See "semedia_base.c". Allocate memory bloc size of TSEMedia for TESMedia object
		seMediaWinSCard->_ptrBase->_childStruct = seMediaWinSCard;                              // Points on seMediaWinSCard to allow find it through TSEMedia pointer

		seMediaWinSCard->_ptrBase->seMediaSetCallbackEventExecutionError = _seMediaWinSCardSetCallbackEventExecutionError; // Function defined below
		
		seMediaWinSCard->_ptrBase->seMediaEstablishContext = _seMediaWinSCardEstablishContext;           // Function defined below
		seMediaWinSCard->_ptrBase->seMediaReleaseContext = _seMediaWinSCardReleaseContext;               // Function defined below
		seMediaWinSCard->_ptrBase->seMediaIsValidContext = _seMediaWinSCardIsValidContext;               // Function defined below
		seMediaWinSCard->_ptrBase->seMediaIsContextEstablished = _seMediaWinSCardIsContextEstablished;   // Function defined below

		seMediaWinSCard->_ptrBase->seMediaListReader = _seMediaWinSCardListReader;                       // Function defined below
		seMediaWinSCard->_ptrBase->seMediaConnect = _seMediaWinSCardConnect;                             // Function defined below
		seMediaWinSCard->_ptrBase->seMediaIsConnected = _seMediaWinSCardIsConnected;                     // Function defined below
		seMediaWinSCard->_ptrBase->seMediaTransmitApdu = _seMediaWinSCardTransmitApdu;                   // Function defined below
		seMediaWinSCard->_ptrBase->seMediaDisconnect = _seMediaWinSCardDisconnect;                       // Function defined below
		seMediaWinSCard->_ptrBase->seMediaDisconnectWithReset = _seMediaWinSCardDisconnectWithReset;     // Function defined below

		seMediaWinSCard->_ptrBase->seMediaGetStatus = _seMediaWinSCardGetStatus;                          // Function defined below

		seMediaWinSCard->_contextEstablished = false;                                           //  | These elements will be
		seMediaWinSCard->_scardContext = SCARD_INVALID_HANDLE;                                  //  | reachable after through
		seMediaWinSCard->_scardHandle = SCARD_INVALID_HANDLE;                                   //  | _childStruct

		ptrTSEMedia = (TSEMedia*)seMediaWinSCard->_ptrBase;                                     // Will return base of TSEMedia object contained in seMediaWinSCard 

		// Manage default driver Configuration
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_disconnectParam = SE_MEDIA_DISCONNECT_LEAVE_CARD");
		seMediaWinSCard->_disconnectParam = SE_MEDIA_DISCONNECT_LEAVE_CARD;

		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_connectSharedMode = SCARD_SHARE_SHARED");
		seMediaWinSCard->_connectSharedMode = SCARD_SHARE_SHARED;

	}

	// By default, No callback available for LPA Event Execution error
	_lpaEventExecutionErrorCallback = NULL;

	return ptrTSEMedia;
}

/**
 * Delete and free memory for TSEMedia object, including "parent" TSEMediaWinSCard structure linked through _childStruct.
 * 
 * @param ptrTSEMedia - Pointer on TSEMedia object to delete
 */
void Delete_SEMediaWinSCard(TSEMedia* ptrTSEMedia)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> Delete_SEMediaWinSCard()");

	if (ptrTSEMedia != NULL)
	{
		TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaWinSCard != NULL)
			lpaCoreMemoryFree(ptrTSEMediaWinSCard);
		
		lpaCoreMemoryFree(ptrTSEMedia);
		ptrTSEMedia = NULL;
	}
}

/**
 * Set LPA_EVENT_EXECUTION_ERROR callback
 *
 * @param ptrTSEMedia - TSEMedia object pointer
 * @param LPA_EVENT_EXECUTION_ERROR - callback
 * @return true if set callback successfully, otherwise false
 */

bool _seMediaWinSCardSetCallbackEventExecutionError(const struct TSEMedia* ptrTSEMedia, LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardSetCallbackEventExecutionError()");

	bool res = false;

	if (lpaEventExecutionErrorCallback != NULL)
	{
		if (_lpaEventExecutionErrorCallback != lpaEventExecutionErrorCallback)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, " registering lpaEventExecutionErrorCallback ...");
			_lpaEventExecutionErrorCallback = lpaEventExecutionErrorCallback;
			res = true;
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, " LpaEventExecutionErrorCallback already registered !");
	}
	else
	{
		if (_lpaEventExecutionErrorCallback != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, " unregistering lpaEventExecutionErrorCallback ...");
			_lpaEventExecutionErrorCallback = NULL;
			res = true;
		}
	}

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardSetCallbackEventExecutionError() return res=%s", (res ? "true" : "false"));

	return res;
}

/**
 * Try to establish TSEMedia object "context" through SCardEstablishContext() referred "extern" in "winscard.h"
 * And update "context" status in TSEMedia object
 * 
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return Status of "context established" state, boolean, true if established successfully
 */
bool _seMediaWinSCardEstablishContext(const struct TSEMedia* ptrTSEMedia)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+ semedia_winscard :> _seMediaWinSCardEstablishContext()");
	bool res = false;

	if (ptrTSEMedia != NULL)
	{
		TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;

		if (!ptrTSEMediaWinSCard->_contextEstablished)
		{
			long scReturnCode = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &ptrTSEMediaWinSCard->_scardContext);
			if (scReturnCode == SCARD_S_SUCCESS)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardEstablishContext() return SCARD_S_SUCCESS");

				ptrTSEMediaWinSCard->_contextEstablished = true;
				res = true;
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardEstablishContext() return error 0x%08lx (%s)",
					scReturnCode, _seMediaWinSCardInternalGetSCErrorDescription(scReturnCode));
			}
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardEstablishContext() already established");
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMedia is NULL !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardEstablishContext() return res=%s", (res ? "true" : "false"));

	return res;
}



/**
 * Return status of "context established" state stored in TSEMedia object
 * 
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return Boolean, true or false
 */
bool _seMediaWinSCardIsContextEstablished(const struct TSEMedia* ptrTSEMedia)
{
	bool isContextEstablished = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardIsContextEstablished()");
	if (ptrTSEMedia != NULL)
	{
		TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaWinSCard != NULL)
			isContextEstablished = ptrTSEMediaWinSCard->_contextEstablished;
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMedia is NULL !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardIsContextEstablished() return res=%s", (isContextEstablished ? "true" : "false"));

	return isContextEstablished;
}

/**
 * Try to release TSEMedia object "context" through SCardReleaseContext() referred "extern" in "winscard.h"
 * And update "context" status in TSEMedia object
 * 
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return True if "context" released successfully, boolean
 */
bool _seMediaWinSCardReleaseContext(const struct TSEMedia* ptrTSEMedia)
{
	bool res = false;
	TSEMediaWinSCard* ptrTSEMediaWinSCard = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardReleaseContext()");

	if (ptrTSEMedia != NULL)
	{
		ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaWinSCard->_contextEstablished)
		{
			// Check if ISDR applet selected
			if (_seMediaWinSCardIsConnected(ptrTSEMedia))
				_seMediaWinSCardDisconnect(ptrTSEMedia);

			long scReturnCode = SCardReleaseContext(ptrTSEMediaWinSCard->_scardContext);
			if (scReturnCode == SCARD_S_SUCCESS)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardReleaseContext() return SCARD_S_SUCCESS");

				ptrTSEMediaWinSCard->_scardContext = SCARD_INVALID_HANDLE;
				ptrTSEMediaWinSCard->_contextEstablished = false;
				ptrTSEMediaWinSCard->_scardHandle = SCARD_INVALID_HANDLE;

				res = true;
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardReleaseContext() return error 0x%08lx (%s)",
					scReturnCode, _seMediaWinSCardInternalGetSCErrorDescription(scReturnCode));
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardReleaseContext() : Context not established");
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMedia is NULL !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardReleaseContext() return res=%s", (res ? "true" : "false"));

	return res;
}

/**
 * Try to get status of "Media Valid" through SCardIsValidContext() referred "extern" in "winscard.h"
 * 
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return True if "Media Valid" status is verified successfully, boolean
 */
bool _seMediaWinSCardIsValidContext(const struct TSEMedia* ptrTSEMedia)
{
	bool isValidContext = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardIsValidContext()");

	if (ptrTSEMedia == NULL)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMedia is NULL !");
		return false;
	}

	TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
	if (ptrTSEMediaWinSCard != NULL)
	{
		long scReturnCode = SCardIsValidContext(ptrTSEMediaWinSCard->_scardContext);
		if (scReturnCode == SCARD_S_SUCCESS)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardIsValidContext() return SCARD_S_SUCCESS");
			isValidContext = true;
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardIsValidContext() return error 0x%08lx (%s)", 
			scReturnCode, _seMediaWinSCardInternalGetSCErrorDescription(scReturnCode));
	}

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardIsValidContext() return res=%s", (isValidContext ? "true" : "false"));

	return isValidContext;
}

/**
 * Try to get reader list through "SCardListReaders" defined in "winscard.h", itself defined by "__MINGW_NAME_AW" defined in "_minggw_unicode.h"
 * 
 * @param ptrTSEMedia - TSEMedia object pointer
 * @param readerNameInfoList - Structure LPA_SE_MEDIA_READER_NAME_INFO defined in "lpasdk_api.h" containing "readerName" char array.
 * @param readerNameInfoMax - Maximum number of reader that can be reported in list, size_t
 * @param countReader - Return number of readers found, size_t
 * @return True if reader list retrieving finished successfully.
 */
bool _seMediaWinSCardListReader(const struct TSEMedia* ptrTSEMedia, LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader)
{
	bool readListReader = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardListReader()");

	if (ptrTSEMedia != NULL && ptrReaderNameInfoList != NULL && ptrCountReader != NULL)
	{
		*ptrCountReader = 0;

		TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaWinSCard != NULL)
		{
			LPTSTR mszReaders = NULL;
			DWORD dwReaders = SCARD_AUTOALLOCATE;

			long scReturnCode = SCardListReaders(ptrTSEMediaWinSCard->_scardContext, SCARD_ALL_READERS, (LPTSTR)&mszReaders, &dwReaders);
			if (scReturnCode == SCARD_S_SUCCESS)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardListReaders() return SCARD_S_SUCCESS");
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "dwReaders: %d", dwReaders);

				readListReader = true;
				if (mszReaders != NULL && dwReaders > 0)
				{
					LPTSTR pReader = mszReaders;
					while ('\0' != *pReader)
					{
						// Manage JIRA DMSIMCE-362
						if (((DWORD)(pReader - mszReaders)) >= dwReaders)
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "End of reader list detected");
							break;
						}

						if (*ptrCountReader < readerNameInfoMax)
						{
							// Display the value.
							lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Reader found : <%s>", pReader);
							if (strlen(pReader) < LPA_CFG_READER_NAME_MAX_SIZE)
							{
								sprintf(ptrReaderNameInfoList[(*ptrCountReader)].readerName, "%s", pReader);

								// Advance to the next value.
								(*ptrCountReader)++;
								pReader = pReader + strlen(pReader) + 1;
							}
							else
							{
								lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Reader name too long !(max size:%d, current size:%d)", LPA_CFG_READER_NAME_MAX_SIZE, strlen(pReader));
								readListReader = false;
								break;
							}
						}
						else
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Too reader detected for updating memory struct (max supported:%d, detected:%d)", readerNameInfoMax, dwReaders);
							readListReader = false;
							break;
						}
					}

					// Free the memory.
					SCardFreeMemory(ptrTSEMediaWinSCard->_scardContext, mszReaders);
				}
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardListReaders() return error 0x%08lx (%s)",
					scReturnCode, _seMediaWinSCardInternalGetSCErrorDescription(scReturnCode));
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "NULL parameter detected!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardListReader() return res=%s", (readListReader ? "true" : "false"));

	return readListReader;
}

/**
 * Try to perform "Connect" operation on a reader using reader name to identify it.
 * Done through "SCardConnect" defined in "winscard.h", itself defined by "__MINGW_NAME_AW" defined in "_minggw_unicode.h"
 * Update "connection" status in TSEMedia object
 * 
 * @param ptrTSEMedia - TSEMedia object pointer
 * @param ptrReaderName - Reader name, string, coming from reader list retrieved with seMediaListReader()
 * @return True if connection operation was successful
 */

bool _seMediaWinSCardConnect(const struct TSEMedia* ptrTSEMedia, const char *ptrReaderName)
{
	bool connected = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardConnect()");

	if (ptrTSEMedia != NULL && ptrReaderName != NULL )
	{
		TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaWinSCard != NULL)
		{
			DWORD dwActiveProtocol = 0;

			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ReaderName='%s'", ptrReaderName);

			long scReturnCode = SCardConnect(ptrTSEMediaWinSCard->_scardContext, ptrReaderName, ptrTSEMediaWinSCard->_connectSharedMode,
				SCARD_PROTOCOL_T0, &ptrTSEMediaWinSCard->_scardHandle, &dwActiveProtocol);

			if (scReturnCode == SCARD_S_SUCCESS)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardConnect() return SCARD_S_SUCCESS");
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "active protocol used : %s", (dwActiveProtocol == SCARD_PROTOCOL_T0 ? "T0" : (dwActiveProtocol == SCARD_PROTOCOL_T1 ? "T1" : "N/A")));
				connected = true;
			}
			else
			{
				const char* ptrWinscardErrorDescription = _seMediaWinSCardInternalGetSCErrorDescription(scReturnCode);
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardConnect() return error 0x%08lx (%s)",
					scReturnCode, ptrWinscardErrorDescription);

				_seMediaWinSCardInternalSendLpaEventExecutionError(scReturnCode, ptrWinscardErrorDescription);

			}
		}
	}
	else
	{
		if (ptrTSEMedia == NULL)
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMedia is NULL !");

		if (ptrReaderName == NULL)
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "ptrReaderName is NULL !");
	}

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardConnect() return res=%s", (connected ? "true" : "false"));

	return connected;
}

/**
 * Return connection status of reader linked to TSEMedia object.
 * 
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return True if reader connection status is verified successfully, boolean
 */
bool _seMediaWinSCardIsConnected(const struct TSEMedia* ptrTSEMedia)
{
	bool isConnected = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardIsConnected()");

	if (ptrTSEMedia != NULL)
	{
		TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaWinSCard != NULL)
		{
			if (ptrTSEMediaWinSCard->_scardHandle != SCARD_INVALID_HANDLE)
				isConnected = true;
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMedia is NULL !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardIsConnected() return res=%s", (isConnected ? "true" : "false"));

	return isConnected;
}

/**
 * Try perform sending of APDU on reader linked to TSEMedia object and retrieve response.
 * Uses SCardTransmit() referred "extern" in "winscard.h"
 * 
 * @param ptrTSEMedia - TSEMedia object pointer
 * @param apduCommandBytes - APDU command to send - bytes format
 * @param apduCommandSize - Size of ADPDU command to send, size_t
 * @param apduResponseBytes - Store command response returned by reader - bytes format
 * @param apduResponseMaxSize - Size of command response, size_t
 * @return True if reader reported that exchange was successful.
 */
bool _seMediaWinSCardTransmitApdu(const struct TSEMedia* ptrTSEMedia, const unsigned char* apduCommandBytes, size_t apduCommandSize, unsigned char* apduResponseBytes, size_t* apduResponseMaxSize)
{
	bool transmitApdu = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardTransmitApdu()");

	if (ptrTSEMedia == NULL)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMedia is NULL !");
		return false;
	}

	if (apduCommandBytes != NULL && apduResponseBytes != NULL && apduCommandSize >= 4 && apduResponseMaxSize != NULL && *apduResponseMaxSize >= 2)
	{
		TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaWinSCard != NULL)
		{
			// manage the APDU
			if (formatBytesToHexaString(apduCommandBytes, apduCommandSize, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU Command (%d) : %s", apduCommandSize, _bufferFormatLogMessage);
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU Command (%d) : ...", apduCommandSize);

			DWORD cbRecvLength = (DWORD) (*apduResponseMaxSize);
			long scReturnCode = SCardTransmit(ptrTSEMediaWinSCard->_scardHandle, SCARD_PCI_T0, apduCommandBytes, apduCommandSize,
				NULL, apduResponseBytes, &cbRecvLength);
			if (scReturnCode == SCARD_S_SUCCESS)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardTransmit() return SCARD_S_SUCCESS");

				if (formatBytesToHexaString(apduResponseBytes, cbRecvLength, _bufferFormatLogMessage, sizeof(_bufferFormatLogMessage)) > 0)
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU Response (%d) : %s", cbRecvLength, _bufferFormatLogMessage);
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "APDU Response (%d) : ...", cbRecvLength);

				transmitApdu = true;
				*apduResponseMaxSize = (size_t) cbRecvLength;
			}
			else
			{
				const char* ptrWinscardErrorDescription = _seMediaWinSCardInternalGetSCErrorDescription(scReturnCode);
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardTransmit() return error 0x%08lx (%s)", 
					scReturnCode, ptrWinscardErrorDescription);
				*apduResponseMaxSize = 0L;

				_seMediaWinSCardInternalSendLpaEventExecutionError(scReturnCode, ptrWinscardErrorDescription);
			}
		}
	}

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardTransmitApdu() return res=%s", (transmitApdu ? "true" : "false"));

	return transmitApdu;
}

/**
 * Try to perform "Disconnect" operation on reader linked to TSEMedia object.
 *
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return True if disconnect operation was successful.
 */

bool _seMediaWinSCardDisconnect(const struct TSEMedia* ptrTSEMedia)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardDisconnect()");

	bool res = false;

	if (ptrTSEMedia != NULL)
	{
		TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaWinSCard != NULL)
		{
			res = _seMediaWinSCardInternalDisconnect(ptrTSEMedia, ptrTSEMediaWinSCard->_disconnectParam);
		}
	}
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardDisconnect() return res=%s", (res ? "true" : "false"));

	return  res;
}

/**
 * Try to perform "Disconnect" with Reset operation on reader linked to TSEMedia object.
 *
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return True if disconnect operation was successful.
 */

bool _seMediaWinSCardDisconnectWithReset(const struct TSEMedia* ptrTSEMedia)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardDisconnectWithReset()");

	bool res = _seMediaWinSCardInternalDisconnect(ptrTSEMedia, SE_MEDIA_DISCONNECT_RESET_CARD);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardDisconnectWithReset() return res=%s", (res ? "true" : "false"));

	return res;
}

/**
 * Try to perform "Disconnect" operation on reader linked to TSEMedia object.
 * Done through "SCardDisconnect" referred "extern" in "winscard.h"
 *
 * @param ptrTSEMedia - TSEMedia object pointer
 * @param disconnectCardParam - disconnect parameter
 * @return True if disconnect operation was successful.
 */

bool _seMediaWinSCardInternalDisconnect(const struct TSEMedia* ptrTSEMedia, SE_MEDIA_DISCONNECT_CARD_PARAM disconnectCardParam)
{
	bool disconnected = false;
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardInternalDisconnect()");

	if (ptrTSEMedia == NULL)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMedia is NULL !");
		return false;
	}

	TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
	if (ptrTSEMediaWinSCard != NULL)
	{
		long scReturnCode = SCardDisconnect(ptrTSEMediaWinSCard->_scardHandle, disconnectCardParam);
		if (scReturnCode == SCARD_S_SUCCESS)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardDisconnect() return SCARD_S_SUCCESS");
			ptrTSEMediaWinSCard->_scardHandle = SCARD_INVALID_HANDLE;
			disconnected = true;
		}
		else
		{
			const char* ptrWinscardErrorDescription = _seMediaWinSCardInternalGetSCErrorDescription(scReturnCode);
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardDisconnect() return error 0x%08lx (%s)",
				scReturnCode, ptrWinscardErrorDescription);

			_seMediaWinSCardInternalSendLpaEventExecutionError(scReturnCode, ptrWinscardErrorDescription);
		}
	}
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardInternalDisconnect() return res=%s", (disconnected ? "true" : "false"));

	return disconnected;
}

/**
* Get current status of reader linked to TSEMedia object.
* Done through "SCardStatus" defined in "winscard.h", itself defined by "__MINGW_NAME_AW" defined in "_minggw_unicode.h"
* Status will be returned only if condition SCARD_S_SUCCESS, SCARD_W_REMOVED_CARD or SCARD_W_RESET_CARD is returned by SCardStatus()
*
* @param ptrTSEMedia - TSEMedia object pointer
* @param ptrStatus - Return reader status in pointer on enum object type SE_MEDIA_CARD_STATUS defined in "semedia_base.h".
* @return True if condition SCARD_S_SUCCESS, SCARD_W_REMOVED_CARD or SCARD_W_RESET_CARD is returned by SCardStatus()
*/
bool _seMediaWinSCardGetStatus(const struct TSEMedia* ptrTSEMedia, SE_MEDIA_CARD_STATUS* ptrStatus)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_winscard :> _seMediaWinSCardGetStatus()");

	// ptrStatus NULL is managed at the end
        if (ptrTSEMedia == NULL)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "seMedia is NULL !");
		return false;
	}

	TSEMediaWinSCard* ptrTSEMediaWinSCard = (TSEMediaWinSCard*)ptrTSEMedia->_childStruct;
	if (ptrTSEMediaWinSCard != NULL)
	{
		if (ptrTSEMediaWinSCard->_scardHandle != SCARD_INVALID_HANDLE)
		{
			DWORD dwState = 0, dwProtocol = 0, dwReaderLen = 0, dwAtrLen = 0;
			SE_MEDIA_CARD_STATUS cardStatus = SE_MEDIA_STATUS_SCARD_UNKNOWN;

			long scReturnCode = SCardStatus(ptrTSEMediaWinSCard->_scardHandle, NULL, &dwReaderLen, &dwState, &dwProtocol, NULL, &dwAtrLen);
			if (scReturnCode == SCARD_S_SUCCESS)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardStatus() return SCARD_S_SUCCESS");
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardStatus : dwState = 0x%04X - dwProtocol = 0x%04X", dwState, dwProtocol);

				cardStatus = dwState;
				res = true;
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "SCardStatus() return error 0x%08lx (%s)", 
					scReturnCode, _seMediaWinSCardInternalGetSCErrorDescription(scReturnCode));

				if (scReturnCode == SCARD_W_REMOVED_CARD)
				{
					cardStatus = SE_MEDIA_STATUS_REMOVED_CARD;
					res = true;
				}

				if (scReturnCode == SCARD_W_RESET_CARD)
				{
					cardStatus = SE_MEDIA_RESET_CARD;
					res = true;
				}
			}

			if (ptrStatus != NULL && res == true)
				*ptrStatus = cardStatus;
		}
	}
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_winscard :> _seMediaWinSCardGetStatus() return res=%s", (res ? "true" : "false"));

	return res;
}

/**
 * Translate error code in user readable message.
 * Error code and readable message matching are defined in _scardErrorList data defined in the beginning of "semedia_winscard.c" (So here).
 * 
 * @param scErrorCode - Error code to translate, DWORD
 * @return String containing user readable error message.
 */
char* _seMediaWinSCardInternalGetSCErrorDescription(DWORD scErrorCode)
{
	char *ptrErrorDescription = "Unknown SmartCard error code";
	SCARD_ERROR_INFO* ptrSCardErrorList = _scardErrorList;
	
	while( true)
	{
		if (ptrSCardErrorList == NULL || ptrSCardErrorList->_ptrErrorString == NULL)
			break;

		if (ptrSCardErrorList->_errorValue == scErrorCode)
		{
			ptrErrorDescription = ptrSCardErrorList->_ptrErrorString;
			break;
		}

		ptrSCardErrorList++;
	}
	
	return ptrErrorDescription;
}

/**
 * send LPA Event execution error.
 *
 * @param scErrorCode - Error code
 * @param ptrErrorCodeDescription - String containing user readable error message
 */

void _seMediaWinSCardInternalSendLpaEventExecutionError(long scErrorCode, const char* ptrErrorCodeDescription)
{
	// using callback to share seMedia error
	if (_lpaEventExecutionErrorCallback != NULL)
	{
		LPA_EVENT_EXECUTION_ERROR_INFO eventExecutionErrorInfo;
		char errorSubjectCode[16];
		snprintf(errorSubjectCode, sizeof(errorSubjectCode), "0x%lx", scErrorCode);

		eventExecutionErrorInfo.executionErrorType = LPA_EVENT_EXECUTION_SEMEDIA_DRIVER_ERROR_TYPE;
		eventExecutionErrorInfo.detailErrorMask = LPA_EVENT_EXECUTION_ERROR_SUBJECT_CODE_MASK;
		eventExecutionErrorInfo.ptrErrorReasonCode = NULL;
		eventExecutionErrorInfo.ptrErrorSubjectCode = errorSubjectCode;

		if (ptrErrorCodeDescription != NULL)
		{
			eventExecutionErrorInfo.detailErrorMask |= LPA_EVENT_EXECUTION_ERROR_EXTRA_INFO_MASK;
			eventExecutionErrorInfo.ptrErrorExtraInfo = ptrErrorCodeDescription;
		}

		_lpaEventExecutionErrorCallback(NULL, &eventExecutionErrorInfo);
	}
}

#endif //LPA_SDK__SEMEDIA_DRIVER_WINSCARD

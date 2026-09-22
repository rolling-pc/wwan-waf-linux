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


#include "lpasdk/driver/semedia_genericmodem.h"
#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_memory.h"
#include "lpasdk/core/util.h"


// This driver is compiled only if LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM build option exist
#ifdef LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM

// For specific flags used by open()
#include <fcntl.h>

#if defined(LPA_SDK__PLATFORM_CYGWIN) || defined(LPA_SDK__PLATFORM_RASPBIAN) || defined(LPA_SDK__PLATFORM_WIN)

//#include <unistd.h>
//#include <termios.h>
#include "lpasdk/platform_adaptor/platform_lpa_adaptor.h"		//by fanming 2021.10.21
// For timer features
#include <time.h>

#endif // LPA_SDK__PLATFORM_CYGWIN ||  LPA_SDK__PLATFORM_RASPBIAN

#if defined(LPA_SDK__PLATFORM_WIN)
#include <Windows.h>
#endif


// Functions needed for SE Media driver
bool _seMediaGenericModemSetCallbackEventExecutionError(const struct TSEMedia* ptrTSEMedia, LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback);

bool _seMediaGenericModemEstablishContext(const struct TSEMedia* ptrTSEMedia);
bool _seMediaGenericModemReleaseContext(const struct TSEMedia* ptrTSEMedia);
bool _seMediaGenericModemIsValidContext(const struct TSEMedia* ptrTSEMedia);
bool _seMediaGenericModemIsContextEstablished(const struct TSEMedia* ptrTSEMedia);

bool _seMediaGenericModemListReader(const struct TSEMedia* ptrTSEMedia, LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader);
bool _seMediaGenericModemConnect(const struct TSEMedia* ptrTSEMedia, const char *ptrReaderName);
bool _seMediaGenericModemTransmitApdu(const struct TSEMedia* ptrTSEMedia, const unsigned char* ptrApduCommandBytes, size_t apduCommandSize, unsigned char* ptrApduResponseBytes, size_t* ptrApduResponseMaxSize);
bool _seMediaGenericModemIsConnected(const struct TSEMedia* ptrTSEMedia);
bool _seMediaGenericModemDisconnect(const struct TSEMedia* ptrTSEMedia);
bool _seMediaGenericModemDisconnectWithReset(const struct TSEMedia* ptrTSEMedia);

bool _seMediaGenericModemGetStatus(const struct TSEMedia* ptrTSEMedia, SE_MEDIA_CARD_STATUS* ptrStatus);

// Internal functions to process APDU and AT commands
static bool _seMediaGenericModemApduToAtCommand(const char* ptrAPDU_cmd, char* ptrBuild_ATcommand, const size_t pMaxSize);
static bool _seMediaGenericModemAtCommandResponseToApdu(const char* ptrAt_command_response, char* ptrAPDU_response, size_t pMaxSize);
static bool _seMediaGenericModemCheckAPDU_OkResponse(const char* ptrAPDU_response);
static bool _seMediaGenericModemGetATcommandResponseData(const struct TSEMedia* ptrTSEMedia);
static bool _seMediaGenericModemCheckATcommandResponseOK(void);
static bool _seMediaGenericModemSendAPDUtoModemAndCheckOK(const char* ptrAPDU, const struct TSEMedia* ptrTSEMedia, bool checkStatusWord);
static bool _seMediaGenericModemOpenUICCChannel(const struct TSEMedia* ptrTSEMedia);
static bool _seMediaGenericModemCloseUICCChannel(const struct TSEMedia* ptrTSEMedia);
static bool _seMediaGenericModemProcessAPDUresponse(unsigned char* ptrAPDU_response, const size_t pMaxSize, size_t * pAPDU_response_size);


static LPA_EVENT_EXECUTION_ERROR _lpaEventExecutionErrorCallback = NULL;
void _seMediaGenericModemSendLpaEventExecutionError(long scErrorCode, const char* ptrErrorCodeDescription);

void _seMediaGenericModemWaitingTimer(uint32_t pTime);
bool _seMediaGenericModemCloseDescriptorOrHandle(TSEMediaGenericModem* ptrTSEMediaGenericModem);

// Specific functions for use under windows
#if defined(LPA_SDK__PLATFORM_WIN)
void _seMediaGenericModemWriteOnLogWindowsError(DWORD windowsError);
bool _seMediaDriverUpdateModemCommunicationPortConfiguration(HANDLE pModemHandle);
bool _seMediaDriverUpdateModemCommunicationTimeOutConfiguration(HANDLE pModemHandle);
#endif // LPA_SDK__PLATFORM_WIN

#ifdef _FFW_PCIOT_LPA_MODIFY_
#define GENERIC_MODEM_RESPONSE_BUFFER_SIZE (16 * 1024 + 1)                  // Common size. Can be increased for modem chaining responses + Very big profiles info retrieve need
#else
#define GENERIC_MODEM_RESPONSE_BUFFER_SIZE 8192                  // Common size. Can be increased for modem chaining responses + Very big profiles info retrieve need
#endif
static char responseBuffer[GENERIC_MODEM_RESPONSE_BUFFER_SIZE];

#define GM_AT_COMMAND_BUFFER_SIZE 600     // AT command (Can vary) + (APDU header (5x2) + APDU data (255x2) + 2 quotes => 522) + '\0' => Commonly 537 characters, keep comfortable security

// MODEM PARAMETERS

// Port name. If already defined as a compilation parameter, will be not changed
// Note 1: Modem name is used as default port name if not specified and also used in _seMediaGenericModemListReader() for dummy list generation.
// Note 2: COM port number will vary depending host system used, it's even possible with 2 identical systems
#ifndef GM_SERIAL_PORT_NAME
    #if defined LPA_SDK__PLATFORM_CYGWIN
        #define GM_SERIAL_PORT_NAME "/dev/ttyS42"   // Cygwin AK
    #elif defined LPA_SDK__PLATFORM_RASPBIAN
        //#define GM_SERIAL_PORT_NAME "/dev/ttyACM0"  // Raspberry
		#define GM_SERIAL_PORT_NAME "/dev/ttyCMIPC11"
    #elif defined LPA_SDK__PLATFORM_WIN
        #define GM_SERIAL_PORT_NAME "\\\\.\\COM44"  // Windows AK
    #endif // Platforms for default driver name in reader list
#endif // ndef GM_SERIAL_PORT_NAME

#if defined(LPA_SDK__PLATFORM_WIN)
    // Mandatory modem parameters
    #define GM_SERIAL_PORT_BAUD_RATE	115200
    #define GM_SERIAL_PORT_PARITY	NOPARITY
    #define GM_SERIAL_PORT_BYTE_SIZE	8
    #define GM_SERIAL_STOP_BITS		ONESTOPBIT

    // Time out configuration when reading Modem file
    // Using 0 to deactivate timeout
    #define GM_SERIAL_READ_INTERVAL_TIMEOUT			10		// ReadIntervalTimeout
    #define GM_SERIAL_READ_TOTAL_TIMEOUT_CONSTANT		500		// ReadTotalTimeoutConstant

    // Optional modem parameters
    // =>Uncomment if needed

    //#define GM_SERIAL_FBINARY			1	// fBinary
    //#define GM_SERIAL_ABORT_ON_ERROR		1	// fAbortOnError
    //#define GM_SERIAL_F_RTS_CONTROL		1	// fRtsControl
    //#define GM_SERIAL_F_DTR_CONTROL		1	// fDtrControl
    //#define GM_SERIAL_F_OUT_X_CTS_FLOW	0	// fOutxCtsFlow
    //#define GM_SERIAL_F_OUT_X_DSR_FLOW	0	// fOutxDsrFlow
#else
    // For parameters definition see termios.h
    // cflag parameters:
    // B115200 = Serial port speed, here 115200 bps
    // CS8 = 8 bits data
    // CLOCAL = Ignore modem status lines (RTS / CTS ? DTR / DSR / DCD ?)
    // CREAD = Enable receiver
    // Default 1 stop bit (Set CSTOPB for 2)
    // Default parity none (PARENB not set)

    #define GM_SERIAL_PORT_PARAM_CFLAG  115200 | CS8 | CLOCAL | CREAD
#endif //  LPA_SDK__PLATFORM_WIN


// Maximum number of loop waiting modem response
// COM port time out differs from platforms, explaining why so much differences
#if defined(LPA_SDK__PLATFORM_WIN)
#define GM_RETRIEVE_RESPONSE_TIMEOUT 200
#else
#define GM_RETRIEVE_RESPONSE_TIMEOUT 200
#endif //  LPA_SDK__PLATFORM_WIN


// Set this flag only if Check Card Status feature is available on modem used here
//#define GM_CHECK_CARD_STATUS_ENABLED

// AT command syntax definitions
#define GM_AT_COMMAND_PREFIX "AT+CSIM="
#define GM_AT_COMMAND_RESPONSE_PREFIX "+CSIM:"
#define GM_AT_CMD_CHECK_CARD_STATUS "AT^SCKS?\r"
#define GM_AT_COMMAND_RESPONSE_OK "OK"
#define GM_AT_COMMAND_RESPONSE_ERROR "ERROR"
#define GM_AT_COMMAND_RESET_CARD "AT+CFUN=1,1\r"     // CAUTION: This command can disconnect USB interface on some modem devices. Can also be rejected by modem, reason unknown

// Note: Comment this define if modem does not support unsolicited event signaling modem startup
#define GM_AT_MODEM_START_EVENT	"^SYSSTART"     // This value depends on the modem brand / model used

// APDU commands and constants
#define GM_APDU_OPEN_SIM_CHANNEL "0070000001"
#define GM_APDU_CLOSE_SIM_CHANNEL_PREFIX "007080"
#define GM_APDU_OK 0x9000
#define GM_APDU_STK_OK 0x9100
#define GM_APDU_CHAIN_OK 0x6100     // Manages Big size chained responses / Eventual normal T=0 GetResponse not managed by modem itself
#define GM_APDU_UPPER_SW_MASK 0xFF00// Filter only data size returned
#define GM_APDU_OK_LEN 4


/**
* Initializes a structure type "TSEMediaGenericModem", affect TSEMediaGenericModem->ptrBase to a pointer type "TSEMedia" object and return it.
* Elements stored points on functions defined below and elsewhere.
*
* @return Pointer on object type TSEMedia
*/
TSEMedia* New_SEMediaGenericModem()
{
    TSEMedia* ptrTSEMedia = NULL;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> New_SEMediaGenericModem()");

    TSEMediaGenericModem* seMediaGenericModem = (TSEMediaGenericModem*)lpaCoreMemoryAlloc(sizeof(TSEMediaGenericModem));
    if (seMediaGenericModem != NULL)
    {
        seMediaGenericModem->_ptrBase = New_SEMediaBase();                                                       // See "semedia_base.c". Allocate memory bloc size of TSEMedia for TESMedia object
        seMediaGenericModem->_ptrBase->_childStruct = seMediaGenericModem;                                       // Points on seMediaGenericModem to allow find it through TSEMedia pointer

        seMediaGenericModem->_ptrBase->seMediaSetCallbackEventExecutionError = _seMediaGenericModemSetCallbackEventExecutionError; // Function defined below

        seMediaGenericModem->_ptrBase->seMediaEstablishContext = _seMediaGenericModemEstablishContext;           // Function defined below
        seMediaGenericModem->_ptrBase->seMediaReleaseContext = _seMediaGenericModemReleaseContext;               // Function defined below
        seMediaGenericModem->_ptrBase->seMediaIsValidContext = _seMediaGenericModemIsValidContext;               // Function defined below
        seMediaGenericModem->_ptrBase->seMediaIsContextEstablished = _seMediaGenericModemIsContextEstablished;   // Function defined below

        seMediaGenericModem->_ptrBase->seMediaListReader = _seMediaGenericModemListReader;                       // Function defined below
        seMediaGenericModem->_ptrBase->seMediaConnect = _seMediaGenericModemConnect;                             // Function defined below
        seMediaGenericModem->_ptrBase->seMediaIsConnected = _seMediaGenericModemIsConnected;                     // Function defined below
        seMediaGenericModem->_ptrBase->seMediaTransmitApdu = _seMediaGenericModemTransmitApdu;                   // Function defined below
        seMediaGenericModem->_ptrBase->seMediaDisconnect = _seMediaGenericModemDisconnect;                       // Function defined below
		seMediaGenericModem->_ptrBase->seMediaDisconnectWithReset = _seMediaGenericModemDisconnectWithReset;		 // Function defined below
        seMediaGenericModem->_ptrBase->seMediaGetStatus = _seMediaGenericModemGetStatus;                         // Function defined below

        seMediaGenericModem->_contextEstablished = false;                                                        // Context flag. More useful for other interfaces
        seMediaGenericModem->_apduChannel = 0;                                                                   // Specific APDU channel
        seMediaGenericModem->_apduChannelString[0] = '\0';

#if defined(LPA_SDK__PLATFORM_WIN)
        seMediaGenericModem->_modemHandle = 0;												// Handle used to communicate with Modem
#else
        seMediaGenericModem->_modemFD = 0;                                                                       // File Descriptor used to communicate with Modem
#endif // LPA_SDK__PLATFORM_WIN

        ptrTSEMedia = (TSEMedia*)seMediaGenericModem->_ptrBase;                                                  // Will return base of TSEMedia object contained in seMediaGenericModem
	}

    memset(responseBuffer, 0, GENERIC_MODEM_RESPONSE_BUFFER_SIZE);  // Flush response buffer, will also appear as empty string

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_genericmodem :> New_SEMediaGenericModem() return ptrTSEMedia=0x%lx", ptrTSEMedia);

    return ptrTSEMedia;
}

/**
* Delete and free memory for TSEMedia object, including "parent" TSEMediaGenericModem structure linked through _childStruct.
*
* @param ptrTSEMedia - Pointer on TSEMedia object to delete
*/
void Delete_SEMediaGenericModem(TSEMedia* ptrTSEMedia)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> Delete_SEMediaGenericModem()");

	if (ptrTSEMedia != NULL)
	{
		TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaGenericModem != NULL)
			lpaCoreMemoryFree(ptrTSEMediaGenericModem);

		lpaCoreMemoryFree(ptrTSEMedia);
		ptrTSEMedia = NULL;
	}
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "Delete_SEMediaGenericModem(): seMedia is NULL !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_external :> Delete_SEMediaExternal()");
}

/**
 * Set LPA_EVENT_EXECUTION_ERROR callback
 *
 * @param ptrTSEMedia - TSEMedia object pointer
 * @param LPA_EVENT_EXECUTION_ERROR - callback
 * @return true if set callback successfully, otherwise false
 */
bool _seMediaGenericModemSetCallbackEventExecutionError(const struct TSEMedia* ptrTSEMedia, LPA_EVENT_EXECUTION_ERROR lpaEventExecutionErrorCallback)
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemSetCallbackEventExecutionError()");

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
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, " LpaEventExecutionErrorCallback already registered !");
            res = true;
        }
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

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_genericmodem :> _seMediaGenericModemSetCallbackEventExecutionError() return res=%s", (res ? "true" : "false"));

    return res;
}
/**
 * Wait a time defined in milliseconds (Use various functions depending platform)
 * @param pTime Time to wait in milliseconds
 */
void _seMediaGenericModemWaitingTimer(uint32_t pTime)
{
    #if defined LPA_SDK__PLATFORM_CYGWIN
        // ! _POSIX_C_SOURCE >= 199309L
        struct timespec tim;
        tim.tv_sec = 0;
        tim.tv_nsec = pTime * 1000000L;  // Converts milli seconds into nano seconds
        nanosleep(&tim, NULL);

    #elif defined LPA_SDK__PLATFORM_RASPBIAN
        // Forced to redeclare it under Raspbian else "timespec" structure is not recognized
/**************************************************
        typedef struct
        {
            __time_t tv_sec;		// Seconds.
            __syscall_slong_t tv_nsec;	// Nanoseconds.
        }gm_timespec;

        gm_timespec tim;
***************************************************/
		struct timespec tim;                  //by fan ming 2021/9/28

        tim.tv_sec = 0;
        tim.tv_nsec = pTime * 1000000L;  // Converts milli seconds into nano seconds
        nanosleep(&tim, NULL);    // Like usleep() compilation warning due to Raspbian + C99 (warning: implicit declaration of function 'nanosleep'...)

    #elif defined LPA_SDK__PLATFORM_WIN
        // Here duration in milliseconds
        Sleep(pTime);

    #else
        // Special case for usleep()
        // Depend from unistd.h. Can drive to "implicit declaration" error depending platform
        // NOTE: Found that usleep() is considered as deprecated since POSIX-2008
        usleep(pTime * 1000);   // Convert millisecond in microseconds

    #endif
}


/**
* Try to establish TSEMedia object "context"
* And update "context" status in TSEMedia object
*
* @param ptrTSEMedia - TSEMedia object pointer
* @return Status of "context established" state, boolean, true if established successfully
*/
bool _seMediaGenericModemEstablishContext(const struct TSEMedia* ptrTSEMedia)
{
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "+ semedia_genericmodem :> _seMediaGenericModemEstablishContext()");
	bool res = false;

	if (ptrTSEMedia != NULL)
	{
		TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;

		if (!ptrTSEMediaGenericModem->_contextEstablished)
		{
			// Nothing to check on this interface
			ptrTSEMediaGenericModem->_contextEstablished = true;
			res = true;
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Context already established");
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_external :> _seMediaGenericModemEstablishContext() return res=%s", (res ? "true" : "false"));

    return res;
}

/**
* Try to release TSEMedia object "context"
* And update "context" status in TSEMedia object
*
* @param ptrTSEMedia - TSEMedia object pointer
* @return True if "context" released successfully, boolean
*/
bool _seMediaGenericModemReleaseContext(const struct TSEMedia* ptrTSEMedia)
{
	bool res = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemReleaseContext()");

	if (ptrTSEMedia != NULL)
	{
		// Disconnect before releasing context, if needed
		if (_seMediaGenericModemIsConnected(ptrTSEMedia))
		{
			_seMediaGenericModemDisconnect(ptrTSEMedia);
		}


		TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaGenericModem->_contextEstablished)
		{
			ptrTSEMediaGenericModem->_contextEstablished = false;
			res = true;
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Context not established");
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_external :> _seMediaGenericModemReleaseContext() return res=%s", (res ? "true" : "false"));

    return res;
}

/**
* Try to get status of "Media Valid"
*
* @param ptrTSEMedia - TSEMedia object pointer
* @return True if "Media Valid" status is verified successfully, boolean
*/
bool _seMediaGenericModemIsValidContext(const struct TSEMedia* ptrTSEMedia)
{
    bool isValidContext = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemIsValidContext()");

	if (ptrTSEMedia != NULL)
	{
            TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
            if (ptrTSEMediaGenericModem != NULL)
            {
#if defined(LPA_SDK__PLATFORM_WIN)
                // Check FileDescriptor valid or not. Channel opening status not applicable for this function.
                if (ptrTSEMediaGenericModem->_modemHandle > 0)
                    isValidContext = true;
#else
                // Check FileDescriptor valid or not. Channel opening status not applicable for this function.
                if (ptrTSEMediaGenericModem->_modemFD > 0)
                    isValidContext = true;
#endif // LPA_SDK__PLATFORM_WIN

                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Media status: %s", (isValidContext) ? "Valid" : "Not Valid");
            }
	}
	else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_external :> _seMediaGenericModemIsValidContext() return res=%s", (isValidContext ? "true" : "false"));

	return isValidContext;
}

/**
* Return status of "context established" state stored in TSEMedia object
*
* @param ptrTSEMedia - TSEMedia object pointer
* @return Boolean, true or false
*/
bool _seMediaGenericModemIsContextEstablished(const struct TSEMedia* ptrTSEMedia)
{
    bool isContextEstablished = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemIsContextEstablished()");

	if (ptrTSEMedia != NULL)
	{
		TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;

		if (ptrTSEMediaGenericModem != NULL)
			isContextEstablished = ptrTSEMediaGenericModem->_contextEstablished;
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_external :> _seMediaGenericModemIsContextEstablished() return res=%s", (isContextEstablished ? "true" : "false"));

	return isContextEstablished;
}

/**
* Try to get reader list
*
* @param ptrTSEMedia - TSEMedia object pointer
* @param readerNameInfoList - Structure LPA_SE_MEDIA_READER_NAME_INFO defined in "semedia.h" containing "readerName" char array.
* @param readerNameInfoMax - Maximum number of reader that can be reported in list, size_t
* @param countReader - Return number of readers found, size_t
* @return True if reader list retrieving finished successfully.
*/
bool _seMediaGenericModemListReader(const struct TSEMedia* ptrTSEMedia, LPA_SE_MEDIA_READER_NAME_INFO * ptrReaderNameInfoList, size_t readerNameInfoMax, size_t* ptrCountReader)
{
    bool readListReader = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemListReader()");

	if (ptrTSEMedia != NULL && ptrReaderNameInfoList != NULL && ptrCountReader != NULL)
	{
		*ptrCountReader = 0;

		TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaGenericModem != NULL)
		{
			memset(ptrReaderNameInfoList[0].readerName, 0x00, LPA_CFG_READER_NAME_MAX_SIZE);
			strncpy(ptrReaderNameInfoList[0].readerName, GM_SERIAL_PORT_NAME, LPA_CFG_READER_NAME_MAX_SIZE - 1);

			*ptrCountReader = 1;
			readListReader = true;

			// TODO: detect readers ...
			// Unix based : Could be done by detecting "tty" devices under /dev
			//              It does not warranty all tty are a modem but it's a beginning
			// Windows ? How to find different 'COMx' devices ?
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "NULL parameter detected!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_external :> _seMediaGenericModemListReader() return res=%s", (readListReader ? "true" : "false"));

    return readListReader;
}

#if defined(LPA_SDK__PLATFORM_WIN)

// Windows specific implementation
//////////////////////////////////////

/**
* Check and update if needed COM port main parameters
*
* @param pModemHandle - COM port handle
* @return True if configuration operation is successfull.
*/
bool _seMediaDriverUpdateModemCommunicationPortConfiguration(HANDLE pModemHandle)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_seMediaDriverUpdateModemCommunicationPortConfiguration() ...");

    DCB dcb;
    memset(&dcb, 0x00, sizeof(DCB));
    dcb.DCBlength = sizeof(DCB);

    // Read current configuration
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Read current configuration ...");
    if (GetCommState(pModemHandle, &dcb))
    {
        bool updateNeeded = false;

        ////////////////////////////////////////////////
        // MANDATORY PARAMETERS

        if (dcb.BaudRate != GM_SERIAL_PORT_BAUD_RATE)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Will update BaudRate '%d' to '%d' ...", dcb.BaudRate, GM_SERIAL_PORT_BAUD_RATE);
            dcb.BaudRate = GM_SERIAL_PORT_BAUD_RATE;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "BaudRate:%d", dcb.BaudRate);

        if (dcb.ByteSize != GM_SERIAL_PORT_BYTE_SIZE)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Will update ByteSize '%d' to '%d' ...", dcb.ByteSize, GM_SERIAL_PORT_BYTE_SIZE);
            dcb.ByteSize = GM_SERIAL_PORT_BYTE_SIZE;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ByteSize:%d", dcb.ByteSize);

        if (dcb.Parity != GM_SERIAL_PORT_PARITY)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Will update Parity '%d' to '%d' ...", dcb.Parity, GM_SERIAL_PORT_PARITY);
            dcb.Parity = GM_SERIAL_PORT_PARITY;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parity:%d", dcb.Parity);

        if (dcb.StopBits != GM_SERIAL_STOP_BITS)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Will update StopBits '%d' to '%d' ...", dcb.StopBits, GM_SERIAL_STOP_BITS);
            dcb.StopBits = GM_SERIAL_STOP_BITS;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "StopBits:%d", dcb.StopBits);

        ////////////////////////////////////////////////
        // OPTIONAL PARAMETERS

#if defined(GM_SERIAL_FBINARY)
        if (dcb.fBinary != GM_SERIAL_FBINARY)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Will update fBinary '%d' to '%d' ...", dcb.fBinary, GM_SERIAL_FBINARY);
            dcb.fBinary = GM_SERIAL_FBINARY;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fBinary:%d", dcb.fBinary);
#else
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fBinary:%d", dcb.fBinary);
#endif //

#if defined(GM_SERIAL_ABORT_ON_ERROR)
        // Abort all reads and writes on Error
        if (dcb.fAbortOnError != GM_SERIAL_ABORT_ON_ERROR)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Will update fAbortOnError '%d' to '%d' ...", dcb.fAbortOnError, GM_SERIAL_ABORT_ON_ERROR);
            dcb.fAbortOnError = GM_SERIAL_ABORT_ON_ERROR;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fAbortOnError:%d", dcb.fAbortOnError);
#else
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fAbortOnError:%d", dcb.fAbortOnError);
#endif

#if defined(GM_SERIAL_F_OUT_X_CTS_FLOW)
        // CTS handshaking on output
        if (dcb.fOutxCtsFlow != GM_SERIAL_F_OUT_X_CTS_FLOW)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Will update fOutxCtsFlow '%d' to '%d' ...", dcb.fOutxCtsFlow, GM_SERIAL_F_OUT_X_CTS_FLOW);
            dcb.fOutxCtsFlow = GM_SERIAL_F_OUT_X_CTS_FLOW;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fOutxCtsFlow:%d", dcb.fOutxCtsFlow);
#else
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fOutxCtsFlow:%d", dcb.fOutxCtsFlow);
#endif // GM_SERIAL_F_OUT_X_CTS_FLOW

#if defined(GM_SERIAL_F_DTR_CONTROL)
        // DTR Flow control
        if (dcb.fDtrControl != GM_SERIAL_F_DTR_CONTROL)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "will update fDtrControl '%d' to '%d' ...", dcb.fDtrControl, GM_SERIAL_F_DTR_CONTROL);
            dcb.fDtrControl = GM_SERIAL_F_DTR_CONTROL;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fDtrControl:%d", dcb.fDtrControl);
#else
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fDtrControl:%d", dcb.fDtrControl);
#endif // GM_SERIAL_F_DTR_CONTROL

        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fOutxDsrFlow:%d", dcb.fOutxDsrFlow);		// DSR handshaking on output
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "fRtsControl:%d", dcb.fRtsControl);		// Rts Flow control

        if (updateNeeded)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Updating configuration ...");
            if (SetCommState(pModemHandle, &dcb))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Configuration updated successfully");
                res = true;
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Unable to update modem configuration");
                _seMediaGenericModemWriteOnLogWindowsError(GetLastError());
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Modem already configured");
            res = true;
        }
    }
    else
    {
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Unable to configure communication with modem");
    _seMediaGenericModemWriteOnLogWindowsError(GetLastError());
    }

    return res;
}


/**
* Check and update if needed COM port timeout parameters
*
* @param pModemHandle - COM port handle
* @return True if configuration operation is successfull.
*/
bool _seMediaDriverUpdateModemCommunicationTimeOutConfiguration(HANDLE pModemHandle)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_seMediaDriverUpdateModemCommunicationTimeOutConfiguration() ...");

    COMMTIMEOUTS commTimeout;
    if (GetCommTimeouts(pModemHandle, &commTimeout))
    {
        bool updateNeeded = false;

#if defined(GM_SERIAL_READ_INTERVAL_TIMEOUT)
        if (commTimeout.ReadIntervalTimeout != GM_SERIAL_READ_INTERVAL_TIMEOUT)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Will update ReadIntervalTimeout '%ld' to '%ld'", commTimeout.ReadIntervalTimeout, GM_SERIAL_READ_INTERVAL_TIMEOUT);
            commTimeout.ReadIntervalTimeout = GM_SERIAL_READ_INTERVAL_TIMEOUT;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ReadIntervalTimeout : %ld", commTimeout.ReadIntervalTimeout);
#else
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ReadIntervalTimeout : %ld", commTimeout.ReadIntervalTimeout);
#endif // GM_SERIAL_READ_INTERVAL_TIMEOUT

#if defined(GM_SERIAL_READ_TOTAL_TIMEOUT_CONSTANT)
        if (commTimeout.ReadTotalTimeoutConstant != GM_SERIAL_READ_TOTAL_TIMEOUT_CONSTANT)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Will update ReadTotalTimeoutConstant '%ld' to '%ld'", commTimeout.ReadTotalTimeoutConstant, GM_SERIAL_READ_TOTAL_TIMEOUT_CONSTANT);
            commTimeout.ReadTotalTimeoutConstant = GM_SERIAL_READ_TOTAL_TIMEOUT_CONSTANT;
            updateNeeded = true;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ReadTotalTimeoutConstant : %ld", commTimeout.ReadTotalTimeoutConstant);
#else
        logAppend("ReadTotalTimeoutConstant : %ld", commTimeout.ReadTotalTimeoutConstant);
#endif // GM_SERIAL_READ_TOTAL_TIMEOUT_CONSTANT

        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ReadTotalTimeoutMultiplier : %ld", commTimeout.ReadTotalTimeoutMultiplier);

        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "WriteTotalTimeoutMultiplier : %ld", commTimeout.WriteTotalTimeoutMultiplier);
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "WriteTotalTimeoutConstant : %ld", commTimeout.WriteTotalTimeoutConstant);

        if (updateNeeded)
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Updating Communication Timeout ...");

            if (SetCommTimeouts(pModemHandle, &commTimeout))
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Communication Timeout updated successfully");
                res = true;
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Unable to update Communication Timeout");
                _seMediaGenericModemWriteOnLogWindowsError(GetLastError());
            }
        }
        else
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "No communication timeout configuration information");
            res = true;
        }
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Unable to read Communication Timeout configuration");
        _seMediaGenericModemWriteOnLogWindowsError(GetLastError());
    }

    return res;
}


bool _seMediaGenericModemConnect(const struct TSEMedia* ptrTSEMedia, const char *ptrReaderName)
{
    bool connected = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemConnect()");

	if (ptrTSEMedia != NULL && ptrReaderName != NULL)
	{
		TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
		if (ptrTSEMediaGenericModem != NULL)
		{
            ptrTSEMediaGenericModem->_apduChannel = 0;

            // If no reader name specified, connect using default defined in driver
            if (ptrReaderName != NULL && strlen(ptrReaderName) > 0)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ReaderName='%s'", ptrReaderName);
                //ptrTSEMediaGenericModem->_modemHandle = CreateFileA(ptrReaderName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
                ptrTSEMediaGenericModem->_modemHandle = pciot_lpa_open(ptrReaderName, 0);
            }
            else
            {
                // james 2022/11/26
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Connecting to default reader '%s' ...", GM_SERIAL_PORT_NAME);
                //ptrTSEMediaGenericModem->_modemHandle = CreateFileA(GM_SERIAL_PORT_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
                ptrTSEMediaGenericModem->_modemHandle = pciot_lpa_open(GM_SERIAL_PORT_NAME, 0);
            }

            // james 2022/11/26
			if (ptrTSEMediaGenericModem->_modemHandle == 0)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemConnect(): CreateFile() failed");

				_seMediaGenericModemWriteOnLogWindowsError(GetLastError());

				// Basic LPA Event error usage
				_seMediaGenericModemSendLpaEventExecutionError(1, "Unable to open modem Handle");
			}
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_seMediaGenericModemConnect(): CreateFile() is OK, Handle = %d", ptrTSEMediaGenericModem->_modemHandle);
                // COM port parameters update, can be not correctly initialized at opening
                //if (_seMediaDriverUpdateModemCommunicationPortConfiguration(ptrTSEMediaGenericModem->_modemHandle))
                //{
                //    // Time Out parameters update, can be not correctly initialized at opening
                //    if (_seMediaDriverUpdateModemCommunicationTimeOutConfiguration(ptrTSEMediaGenericModem->_modemHandle))
                //        connected = true;
                //}
                connected = true;// james 2022/11/26
            }


			// Port opened and ready, now try to open logical channel
			if (connected)
			{
                //#ifdef _FFW_PCIOT_LPA_MODIFY_
                // TX550 need to send AT+CSIM=20,"80AA000005A903830107"
                //if (TRUE == IsTX550Platform())
                //{
                    //BOOL ret = SyncSIMChipTerminalCapabilityInTx550Platform();
                    //lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG,
                        //"[IOTServiceActiveFunc] SyncSIMChipTerminalCapabilityInTx550Platform ret(%d)\n", ret);
                //}
                //#endif
                connected = _seMediaGenericModemOpenUICCChannel(ptrTSEMedia);

				// If logical channel opening fail, close port
				// Note: In case of failure, _openUICCChannel() manages clearing of logical channel attributes in TSEMediaGenericModem structure
				if (!connected)
				{
                    // james 2022/11/26
					//CloseHandle(ptrTSEMediaGenericModem->_modemHandle);
                    pciot_lpa_close(ptrTSEMediaGenericModem->_modemHandle); // by fanming 2021.10.21
					ptrTSEMediaGenericModem->_modemHandle = 0;
				}
			}
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "NULL parameter detected!");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "-- semedia_external :> _seMediaGenericModemConnect() return res=%s", (connected ? "true" : "false"));

    return connected;
}

#else // LPA_SDK__PLATFORM_WIN

// Cygwin & Raspbian implementation
//////////////////////////////////////
bool _seMediaGenericModemConnect(const struct TSEMedia* ptrTSEMedia, const char *ptrReaderName)
{
	bool connected = false;

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemConnect()");

	if (ptrTSEMedia == NULL)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");
		return false;
	}

	TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
	if (ptrTSEMediaGenericModem != NULL)
	{
		ptrTSEMediaGenericModem->_apduChannel = 0;
		ptrTSEMediaGenericModem->_modemFD = 0;

                // If no reader name specified, connect using default defined in driver
                if (ptrReaderName != NULL && strlen(ptrReaderName) > 0)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "ReaderName='%s'", ptrReaderName);
                    //ptrTSEMediaGenericModem->_modemFD = open(ptrReaderName, O_RDWR | O_NOCTTY | O_NDELAY);
			ptrTSEMediaGenericModem->_modemFD = pciot_lpa_open(ptrReaderName, 0); //by fanming 2021.10.21
                }
                else
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Connecting to default reader '%s' ...", GM_SERIAL_PORT_NAME);
                    //ptrTSEMediaGenericModem->_modemFD = open(GM_SERIAL_PORT_NAME, O_RDWR | O_NOCTTY | O_NDELAY);
			ptrTSEMediaGenericModem->_modemFD = pciot_lpa_open(GM_SERIAL_PORT_NAME, 0); //by fanming 2021.10.21
                }

		if (ptrTSEMediaGenericModem->_modemFD < 0)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemConnect(): open() failed, result = %d", ptrTSEMediaGenericModem->_modemFD);

			// Basic LPA Event erro usage
			_seMediaGenericModemSendLpaEventExecutionError(1, "Unable to open modem stream");
		}
		else
		{
			//struct termios options;
			//memset(&options, 0, sizeof(options));

			//options.c_cflag = GM_SERIAL_PORT_PARAM_CFLAG;

			// Flush communication port. TCIFLUSH = flush data received but not read
			//tcflush(ptrTSEMediaGenericModem->_modemFD, TCIFLUSH);
			//pciot_lpa_tcflush(ptrTSEMediaGenericModem->_modemFD, TCIFLUSH); //by fanming 2021.10.21

			// Set communication port parameters. TCSANOW = changes will occur immediately
			//tcsetattr(ptrTSEMediaGenericModem->_modemFD, TCSANOW, &options);
			//pciot_lpa_tcsetattr(ptrTSEMediaGenericModem->_modemFD, TCSANOW, &options); //by fanming 2021.10.21
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_seMediaGenericModemConnect(): open() is OK, file descriptor = %d", ptrTSEMediaGenericModem->_modemFD);

			connected = true;
		}

		// Port opened and ready, now try to open logical channel
		if (connected)
		{
			connected = _seMediaGenericModemOpenUICCChannel(ptrTSEMedia);

			// If logical channel opening fail, close port
			// Note: In case of failure, _openUICCChannel() manages clearing of logical channel attributes in TSEMediaGenericModem structure
			if (!connected)
			{
				//close(ptrTSEMediaGenericModem->_modemFD);
				pciot_lpa_close(ptrTSEMediaGenericModem->_modemFD); // by fanming 2021.10.21
				ptrTSEMediaGenericModem->_modemFD = 0;
			}
		}
	}

	return connected;
}
#endif // LPA_SDK__PLATFORM_WIN

/**
* Try perform sending of APDU on reader linked to TSEMedia object and retrieve response.
*
* @param ptrTSEMedia - TSEMedia object pointer
* @param apduCommandBytes - APDU command to send - bytes format
* @param apduCommandSize - Size of ADPDU command to send, size_t
* @param apduResponseBytes - Store command response returned by reader - bytes format
* @param apduResponseMaxSize - Size of command response, size_t
* @return True if reader reported that exchange was successful.
*/
bool _seMediaGenericModemTransmitApdu(const struct TSEMedia* ptrTSEMedia, const unsigned char* ptrApduCommandBytes, size_t apduCommandSize, unsigned char* ptrApduResponseBytes, size_t* ptrApduResponseMaxSize)
{
    bool transmitApdu = false;
    char * apduCommandString = NULL;
    size_t apduCommandStringSize = 0;

    unsigned char apduClass = 0;
    unsigned char apduLogicalChannel = 0;
    char apduClassString[4];

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemTransmitApdu()");

    if (ptrTSEMedia == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");
        return false;
    }

    if (ptrApduCommandBytes != NULL && ptrApduResponseBytes != NULL && apduCommandSize >= 4 && apduCommandSize < 262 && ptrApduResponseMaxSize != NULL && *ptrApduResponseMaxSize >= 2)
    {
        TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
        if (ptrTSEMediaGenericModem != NULL)
        {
            // manage the APDU
            apduCommandStringSize = (apduCommandSize * 2) + 2;
            apduCommandString = lpaCoreMemoryAlloc(apduCommandStringSize);

            if(apduCommandString != NULL)
            {
                if (formatBytesToHexaString(ptrApduCommandBytes, apduCommandSize, apduCommandString, apduCommandStringSize) > 0)
                {
                    // Class management. Set Logical Channel to be used to communicate with eUICC
                    // Cannot do it with original raw hex buffer because we cannot modify it. And did not want useless copy of whole raw hex buffer.
                    apduClass = (oneHexCharToHex(apduCommandString[0]) << 4) + oneHexCharToHex(apduCommandString[1]);
                    // Set coding of Logical Channel to apply to APDU class byte depending we work on normal or extended Logical Channel
                    if(ptrTSEMediaGenericModem->_apduChannel < 4)
                        apduLogicalChannel = ptrTSEMediaGenericModem->_apduChannel;
                    else
                        apduLogicalChannel = (ptrTSEMediaGenericModem->_apduChannel - 4) + 0x40;
                    // Note: Coding of bits relative to Secure Messaging (See GP chapters 11.1.4.1 / 11.1.4.2) are not implemented here because no Secure Messaging
                    //  is used for LPA operations. Could be added in the future.
                    apduClass = apduClass | apduLogicalChannel;
                    snprintf(apduClassString, 3, "%02X", apduClass);
                    apduCommandString[0] = apduClassString[0];
                    apduCommandString[1] = apduClassString[1];

                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_seMediaGenericModemTransmitApdu(): APDU Command (%d) : %s", strlen(apduCommandString), apduCommandString);

                    // Here do not check result status word, will be done by upper layers
                    if(_seMediaGenericModemSendAPDUtoModemAndCheckOK(apduCommandString, ptrTSEMedia, false))
                    {
                        size_t maxRecvLength = *ptrApduResponseMaxSize;

                        if(_seMediaGenericModemProcessAPDUresponse(ptrApduResponseBytes, maxRecvLength, ptrApduResponseMaxSize))
                            transmitApdu = true;
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemTransmitApdu(): Problem while extracting APDU response!");
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemTransmitApdu(): Failed to exchange successfully APDU with modem!");

                        // Basic LPA Event erro usage
                        _seMediaGenericModemSendLpaEventExecutionError(2, "transmit issue");
                    }
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemTransmitApdu(): Failed to convert APDU in string format!");

                lpaCoreMemoryFree(apduCommandString);
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemTransmitApdu(): Cannot allocate memory for APDU command string buffer!");
        }
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemTransmitApdu(): Invalid parameters!");

    return transmitApdu;
}

/**
* Return connection status of reader linked to TSEMedia object.
*
* @param ptrTSEMedia - TSEMedia object pointer
* @return True if reader connection status is verified successfully, boolean
*/
bool _seMediaGenericModemIsConnected(const struct TSEMedia* ptrTSEMedia)
{
    bool isConnected = false;
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemIsConnected()");

    if (ptrTSEMedia == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");
        return false;
    }

    TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
    if (ptrTSEMediaGenericModem != NULL)
    {
#if defined(LPA_SDK__PLATFORM_WIN)
        // Check Handle is opened and specific APDU channel created
        if ((ptrTSEMediaGenericModem->_modemHandle != 0) && (ptrTSEMediaGenericModem->_apduChannel > 0))
                isConnected = true;
#else
        // Check FileDescriptor is opened and specific APDU channel created
        if((ptrTSEMediaGenericModem->_modemFD > 0) && (ptrTSEMediaGenericModem->_apduChannel > 0))
            isConnected = true;
#endif
        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Connection Status: %s", (isConnected) ? "Connected" : "Not Connected");
    }

    return isConnected;
}

/**
* Try to perform "Disconnect" operation on reader linked to TSEMedia object.
*
* @param ptrTSEMedia - TSEMedia object pointer
* @return True if disconnect operation was successful.
*/
bool _seMediaGenericModemDisconnect(const struct TSEMedia* ptrTSEMedia)
{
    bool disconnected = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemDisconnect()");

    if (ptrTSEMedia == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");
        return false;
    }

    TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
    if (ptrTSEMediaGenericModem != NULL)
    {
        if (_seMediaGenericModemIsConnected(ptrTSEMedia))
        {
            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_seMediaGenericModemDisconnect(): Close logical channel.");

            // Close logical channel and file descriptor
            if(_seMediaGenericModemCloseUICCChannel(ptrTSEMedia))
            {
                // Close file descriptor / handle
                disconnected = _seMediaGenericModemCloseDescriptorOrHandle(ptrTSEMediaGenericModem);
            }
            else
            {
                disconnected = false;
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemInternalDisconnect(): Error for logical channel close!");
            }
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_seMediaGenericModemInternalDisconnect(): Modem is already disconnected!");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemInternalDisconnect(): ptrTSEMediaGenericModem is NULL!");

    return disconnected;
}

/**
* Try to perform "Disconnect" with Reset operation on reader linked to TSEMedia object.
*
* @param ptrTSEMedia - TSEMedia object pointer
* @return True if disconnect operation was successful.
*/
bool _seMediaGenericModemDisconnectWithReset(const struct TSEMedia* ptrTSEMedia)
{
    bool disconnected = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemDisconnectWithReset()");

    if (ptrTSEMedia == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");
        return false;
    }

    //######################################

    TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
    if (ptrTSEMediaGenericModem != NULL)
    {
        if (_seMediaGenericModemIsConnected(ptrTSEMedia))
        {
            bool writeResetCardCommand = false;

            #if defined(LPA_SDK__PLATFORM_WIN)
            //DWORD numberOfBytesWritten = 0;
            if (pciot_lpa_write(ptrTSEMediaGenericModem->_modemHandle, GM_AT_COMMAND_RESET_CARD, strlen(GM_AT_COMMAND_RESET_CARD)) == strlen(GM_AT_COMMAND_RESET_CARD))  //by fanming 2021.10.21
                writeResetCardCommand = true;
            else
                _seMediaGenericModemWriteOnLogWindowsError(GetLastError());
            #else
            //if( write(ptrTSEMediaGenericModem->_modemFD, GM_AT_COMMAND_RESET_CARD, strlen(GM_AT_COMMAND_RESET_CARD)) == strlen(GM_AT_COMMAND_RESET_CARD) )
			if( pciot_lpa_write(ptrTSEMediaGenericModem->_modemFD, GM_AT_COMMAND_RESET_CARD, strlen(GM_AT_COMMAND_RESET_CARD)) == strlen(GM_AT_COMMAND_RESET_CARD) )  //by fanming 2021.10.21
                writeResetCardCommand = true;
            #endif // LPA_SDK__PLATFORM_WIN

            if (!writeResetCardCommand)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemDisconnectWithReset(): Reset eUICC: Problem while transmitting AT command to modem.");
            }
            else
            {
                if (_seMediaGenericModemGetATcommandResponseData(ptrTSEMedia))
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_seMediaGenericModemDisconnectWithReset(): Reset eUICC response: %s", responseBuffer);
                    if (_seMediaGenericModemCheckATcommandResponseOK())
                        disconnected = true;
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemDisconnectWithReset(): Reset eUICC: Command acknowledge not received.");
            }

            // If card reset is successful, logical channel is marked closed, else try perform logical channel close to avoid leaving unused logical channel
            if(disconnected)
            {
                ptrTSEMediaGenericModem->_apduChannel = 0;
                ptrTSEMediaGenericModem->_apduChannelString[0] = '\0';
            }
            else
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemDisconnectWithReset(): Error detected during eUICC reset. Try to close current logical channel...");
                if(_seMediaGenericModemCloseUICCChannel(ptrTSEMedia))
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_seMediaGenericModemDisconnectWithReset(): Successful closing of current logical channel after failed eUICC reset.");
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemDisconnectWithReset(): Failed to close current logical channel after failed eUICC reset!");
            }

            // Close file descriptor / handle
            if(_seMediaGenericModemCloseDescriptorOrHandle(ptrTSEMediaGenericModem))
                disconnected = (disconnected) ? true : false;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_seMediaGenericModemDisconnectWithReset(): Modem is already disconnected!");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemDisconnectWithReset(): ptrTSEMediaGenericModem is NULL!");

    return disconnected;
}

/**
 * Close File Descriptor (Linux,...) or Handle (Windows,...) opened on Modem communication port
 * @param ptrTSEMediaGenericModem - TSEMediaGenericModem object pointer
 * @return True if closing is successful
 */
bool _seMediaGenericModemCloseDescriptorOrHandle(TSEMediaGenericModem* ptrTSEMediaGenericModem)
{
    bool closedStatus = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemCloseDescriptorOrHandle()");

    if(ptrTSEMediaGenericModem != NULL)
    {
        #if defined(LPA_SDK__PLATFORM_WIN)
        if (pciot_lpa_close(ptrTSEMediaGenericModem->_modemHandle) == 0)	// by james 2022.11.26
        {
            ptrTSEMediaGenericModem->_modemHandle = 0;

            // Return true only if previous operation (Reset or disconnect card) was successful
            closedStatus = true;
        }
        else
        {
            closedStatus = false;
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemCloseDescriptorOrHandle(): Error when closing Handle !");

            _seMediaGenericModemWriteOnLogWindowsError(GetLastError());
        }
        #else
        //if (close(ptrTSEMediaGenericModem->_modemFD) == 0)
		if (pciot_lpa_close(ptrTSEMediaGenericModem->_modemFD) == 0)	// by fanming 2021.10.21
        {
            ptrTSEMediaGenericModem->_modemFD = 0;
            // Return true only if previous operation (Reset or disconnect card) was successful
            closedStatus = true;
        }
        else
        {
            closedStatus = false;
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemCloseDescriptorOrHandle(): Error for file descriptor close!");
        }
        #endif // Platform

    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemCloseDescriptorOrHandle(): ptrTSEMediaGenericModem is NULL");

    return closedStatus;
}

/**
* Get current status of reader linked to TSEMedia object.
*
* @param ptrTSEMedia - TSEMedia object pointer
* @param ptrStatus - Return reader status in pointer on enum object type SE_MEDIA_CARD_STATUS defined in "semedia_base.h".
* @return True if condition SE_MEDIA_STATUS_SCARD_PRESENT or SE_MEDIA_STATUS_REMOVED_CARD is reported
*/
bool _seMediaGenericModemGetStatus(const struct TSEMedia* ptrTSEMedia, SE_MEDIA_CARD_STATUS* ptrStatus)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "++ semedia_genericmodem :> _seMediaGenericModemGetStatus()");

    // ptrStatus NULL is managed at the end
    if (ptrTSEMedia == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "seMedia is NULL !");
        return false;
    }

    TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
    if (ptrTSEMediaGenericModem != NULL)
    {
        if(_seMediaGenericModemIsConnected(ptrTSEMedia))
        {
            SE_MEDIA_CARD_STATUS cardStatus = SE_MEDIA_STATUS_SCARD_UNKNOWN;

#if defined GM_CHECK_CARD_STATUS_ENABLED
            // Send characters on serial port, and check that all has been really send
            //if(write(ptrTSEMediaGenericModem->_modemFD, GM_AT_CMD_CHECK_CARD_STATUS, strlen(GM_AT_CMD_CHECK_CARD_STATUS)) < strlen(GM_AT_CMD_CHECK_CARD_STATUS))
			if(pciot_lpa_write(ptrTSEMediaGenericModem->_modemFD, GM_AT_CMD_CHECK_CARD_STATUS, strlen(GM_AT_CMD_CHECK_CARD_STATUS)) < strlen(GM_AT_CMD_CHECK_CARD_STATUS))	 // by fanming 2021.10.21
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemGetStatus() return error for Card Check Status command %s", GM_AT_CMD_CHECK_CARD_STATUS);
            }
            else
            {
                if(_seMediaGenericModemGetATcommandResponseData(ptrTSEMedia))
                {
                    if(_seMediaGenericModemCheckATcommandResponseOK())
                    {
                        // TODO Retrieve of response status cannot be kept as this, ABSOLUTELY NO WARANTY that status byte will be always at 20th position !
                        // No more development done because function not implemented in sample modem
                        if(strlen(responseBuffer) > 20)
                        {
                            lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Response buffer = %s - responseBuffer[20] = %02x", responseBuffer, responseBuffer[20]);

                            switch(responseBuffer[20])
                            {
                                case '1':
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardStatus() : Card status reported USIM inserted");
                                    cardStatus = SE_MEDIA_STATUS_SCARD_PRESENT;
                                    res = true;
                                    break;

                                case '0':
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardStatus() : Card status reported USIM not inserted");
                                    cardStatus = SE_MEDIA_STATUS_REMOVED_CARD;
                                    res = true;
                                    break;

                                default:
                                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "SCardStatus() : Card status reported unexpected code: %c", responseBuffer[20]);
                                    break;
                            }
                        }
                        else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemGetStatus() Card Check Status request response too short!");
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemGetStatus() Card Check Status request returned an error!");
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemGetStatus() Failed to response for Card Check Status request!");

            }
#else
            // Used in case of Card Status Checking command not available with modem used
            lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_seMediaGenericModemGetStatus() NOTE: Card Check Status bypassed, feature not available with this modem!");
            cardStatus = SE_MEDIA_STATUS_SCARD_PRESENT;
            res = true;
#endif  // GM_CHECK_CARD_STATUS_ENABLED

            // Report status even if result is false, except if given pointer is NULL (Invalidates correct function status)
            if (ptrStatus != NULL)
                *ptrStatus = cardStatus;
            else
                res = false;
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemGetStatus() Modem connection status reported as disconnected!");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_seMediaGenericModemGetStatus() ptrTSEMediaGenericModem is NULL!");

    return res;
}


/**
 * Translates raw APDU command to AT modem command direct to SIM.
 * Wraps it with command prefix, apdu length and quotes.
 *
 * @param ptrAPDU_cmd  APDU to encode, string format
 * @param ptrBuild_ATcommand AT command to generate, string format
 * @param pMaxSize Maximum size allowed for AT command to build. Shall be able to store AT command + APDU + formatting characters
 * @return True if the conversion has been successful
 */
static bool _seMediaGenericModemApduToAtCommand(const char* ptrAPDU_cmd, char* ptrBuild_ATcommand, const size_t pMaxSize)
{
    char apdu_length[4];   // Size will be send as decimal value, so up to 3 digits + \0

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_apduToAtCommand()...");

    if ((ptrBuild_ATcommand == NULL) || (pMaxSize < (strlen(GM_AT_COMMAND_PREFIX) + 13))) // + 13 bytes: length(3) + comma + quotes x2 + APDU (At least 5) + CR + '\0'
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_apduToAtCommand(): Incorrect parameters!");
        return false;
    }

    // Check if AT command prefix + APDU command + size(3) + formating characters(5) + \0 will not overflow build command buffer
    if((strlen(GM_AT_COMMAND_PREFIX) + strlen(ptrAPDU_cmd) + 9) > pMaxSize)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_apduToAtCommand(): AT command to build too big for AT command buffer!");
        return false;
    }

    memset(apdu_length, 0, 4);
    snprintf(apdu_length, 4, "%d", (int)strlen(ptrAPDU_cmd));	// int to str conversion

	memset(ptrBuild_ATcommand, 0x00, pMaxSize);                               // Start forming new command at the beginning of the buffer

    // NOTE: Despite command prefix has been set configurable, it may no work if the syntax differs:
    //   AT command prefix + APDU length in decimal + comma + APDU between quotes + Carriage Return
    // In that case code below will need to be modified

    if((strlen(GM_AT_COMMAND_PREFIX) + strlen(apdu_length) + 2 + strlen(ptrAPDU_cmd) + 2) < pMaxSize)
    {
        // Note: lengths limits - 1 to reserve chain terminator '\0'
        // Add AT command prefix
        strncat(ptrBuild_ATcommand, GM_AT_COMMAND_PREFIX, (pMaxSize - 1));
        // Add apdu command length
        strncat(ptrBuild_ATcommand, apdu_length, (pMaxSize - strlen(ptrBuild_ATcommand) - 1));
        // Add comma and opening quote
        strncat(ptrBuild_ATcommand, ",\"", (pMaxSize - strlen(ptrBuild_ATcommand) - 1));
        // Add apdu code
        strncat(ptrBuild_ATcommand, ptrAPDU_cmd, (pMaxSize - strlen(ptrBuild_ATcommand) - 1));
        // Add closing quote and Carriage Return
        strncat(ptrBuild_ATcommand, "\"\r", (pMaxSize - strlen(ptrBuild_ATcommand) - 1));

        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_apduToAtCommand(): Generated AT command: %s", ptrBuild_ATcommand);

        return true;
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_apduToAtCommand(): Output AT command buffer size is too small to receive full AT command!");

    return false;
}


#ifdef _FFW_PCIOT_LPA_MODIFY_
static bool _seMediaGenericModemNoQuoteAtCommandResponseToApdu(const char* ptrAt_command_response, char* ptrAPDU_response, size_t pMaxSize)
{
    size_t trimmed_response_length = 0;

    // Get the response part starting by prefix defined in AT_COMMAND_RESPONSE_PREFIX
    char* response_part = strstr(ptrAt_command_response, GM_AT_COMMAND_RESPONSE_PREFIX);
    if (response_part == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_atCommandResponseToApdu(): Response prefix not found!");
        return false;
    }

    // skip GM_AT_COMMAND_RESPONSE_PREFIX
    response_part += strlen(GM_AT_COMMAND_RESPONSE_PREFIX);

    // skip space
    while (*response_part == ' ')
    {
        response_part++;
    }
    trimmed_response_length = atoi(response_part);

    // Cut from first to the last quote
    if ((trimmed_response_length < 0) || (trimmed_response_length > (pMaxSize - 1)))
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_atCommandResponseToApdu(): Invalid Response APDU length: %d found, max allowed %u !", trimmed_response_length, pMaxSize);
        return false;
    }

    char* first_data = strstr(response_part, ",");
    if (first_data == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_atCommandResponseToApdu(): Response Separator(,) not found!");
        return false;
    }
    first_data++;

    // skip space
    while (*first_data == ' ')
    {
        first_data++;
    }
    // Copy from first APDU character (first_quote)
    memcpy(ptrAPDU_response, first_data, trimmed_response_length);
    // Terminate string
    ptrAPDU_response[trimmed_response_length] = '\0';

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_atCommandResponseToApdu(): Extracted APDU response: %s", ptrAPDU_response);

    return true;
}
#endif


/**
 * Extract APDU response from AT command response
 *
 * @param ptrAt_command_response AT command response, string format
 * @param pAPDU_response Extracted APDU from At command response, string format
 * @param ptrMaxSize Maximum size allowed for APDU response buffer. At least 2.
 * @return True if the conversion has been successful
 */
static bool _seMediaGenericModemAtCommandResponseToApdu(const char* ptrAt_command_response, char* ptrAPDU_response, size_t pMaxSize)
{
    //int trimmed_response_length = 0;
    size_t trimmed_response_length = 0;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_atCommandResponseToApdu()...");

    if ((ptrAt_command_response == NULL) || (ptrAPDU_response == NULL) || (pMaxSize < 2))
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_atCommandResponseToApdu(): Incorrect parameters!");
        return false;
    }

    // Get the response part starting by prefix defined in AT_COMMAND_RESPONSE_PREFIX
    char* response_part = strstr(ptrAt_command_response, GM_AT_COMMAND_RESPONSE_PREFIX);
    if (response_part == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_atCommandResponseToApdu(): Response prefix not found!");
        return false;
    }

    // Get substring starting from the first quote, from the beginning of response prefix
    // NOTE: May have to be modified if modem does not use the same syntax (APDU response between quotes)
    char* first_quote = strstr(response_part, "\"");
    if (first_quote == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_atCommandResponseToApdu(): First quote not found!");
        #ifdef _FFW_PCIOT_LPA_MODIFY_
        return _seMediaGenericModemNoQuoteAtCommandResponseToApdu(ptrAt_command_response, ptrAPDU_response, pMaxSize);
        #else
        return false;
        #endif
    }

    // Cut the first quote
    first_quote++;

    // Find the second quote
    char* second_quote = strstr(first_quote, "\"");
    if (second_quote == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_atCommandResponseToApdu(): Second quote not found!");
        return false;
    }

    // Cut from first to the last quote
    trimmed_response_length = strlen(first_quote) - strlen(second_quote);
    if((trimmed_response_length < 0) || (trimmed_response_length > (pMaxSize - 1)))
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_atCommandResponseToApdu(): Invalid Response APDU length: %d found, max allowed %u !", trimmed_response_length, pMaxSize);
        return false;
    }

    // Copy from first APDU character (first_quote)
    memcpy(ptrAPDU_response, first_quote, trimmed_response_length);
    // Terminate string
    ptrAPDU_response[trimmed_response_length] = '\0';

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_atCommandResponseToApdu(): Extracted APDU response: %s", ptrAPDU_response);

    return true;
}


/**
 * Checks if the APDU response is ended with successful Status Word (Success or chained data reception)
 *
 * @param ptrAPDU_response Response to be checked, string format
 * @return True if the successful Status Word is found at end of response
 */
static bool _seMediaGenericModemCheckAPDU_OkResponse(const char* ptrAPDU_response)
{
    bool res = false;
    char responseSWstr[GM_APDU_OK_LEN + 2];
    long responseSWbyte = 0;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_checkAPDU_OkResponse()...");

    if (ptrAPDU_response == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_checkAPDU_OkResponse(): Incorrect NULL parameter!");
        return false;
    }


    if (strlen(ptrAPDU_response) < GM_APDU_OK_LEN)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_checkAPDU_OkResponse(): Response to check too short!");
        return false;
    }
    else
    {
        // Extract SW that shall be at end of  ptrAPDU_response
        strncpy(responseSWstr, (ptrAPDU_response + strlen(ptrAPDU_response) - GM_APDU_OK_LEN), GM_APDU_OK_LEN);
        responseSWstr[GM_APDU_OK_LEN] = '\0';
        responseSWbyte = strtol(responseSWstr, NULL, 16);

        if((responseSWbyte >= 0) && (responseSWbyte < 65536))
        {
            // Check successful SW: 9000 or 91xx or 61xx (Normal T=0 GetResponse not managed by modem)
            if((responseSWbyte == GM_APDU_OK) || ((responseSWbyte & GM_APDU_UPPER_SW_MASK) == GM_APDU_STK_OK) || ((responseSWbyte & GM_APDU_UPPER_SW_MASK) == GM_APDU_CHAIN_OK))
                res = true;
        }
    }
    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_checkAPDU_OkResponse(): Successful Status Word %s: %X", (res)?"found":"NOT found", responseSWbyte);

    return res;
}

/**
 * Gets the data from the reception stream and store it in reception buffer as a string.
 *
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return true if some data were successfully retrieved
 */
static bool _seMediaGenericModemGetATcommandResponseData(const struct TSEMedia* ptrTSEMedia)
{
    bool res = false;
    int nbReceivedChars = 0;
    int readResult = 0;
    int timeout = 0;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_getResponseData()...");

    if (ptrTSEMedia == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_getResponseData(): ptrTSEMedia is NULL!");
        return false;
    }

    TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
    if (ptrTSEMediaGenericModem != NULL)
    {
        responseBuffer[0]='\0';

        // Loop exit conditions are present below in the loop
        while (1)
        {
            _seMediaGenericModemWaitingTimer(50);

#if defined(LPA_SDK__PLATFORM_WIN)
            //DWORD countBytesRead = 0;
            readResult = pciot_lpa_read(ptrTSEMediaGenericModem->_modemHandle, (responseBuffer + nbReceivedChars), (sizeof(responseBuffer) - nbReceivedChars)); // by fanming 2021.10.21
#else
            //readResult = read(ptrTSEMediaGenericModem->_modemFD, (responseBuffer + nbReceivedChars), (sizeof(responseBuffer) - nbReceivedChars));
			readResult = pciot_lpa_read(ptrTSEMediaGenericModem->_modemFD, (responseBuffer + nbReceivedChars), (sizeof(responseBuffer) - nbReceivedChars)); // by fanming 2021.10.21
#endif //
            // Note: If retrieved data exceeds response buffer size, will not reach OK / ERROR status so will fall in timeout
            // Eventual truncated response will be blocked after in response syntax analysis

            if (readResult > 0)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Loop #%d: Number of bytes read : %d", (timeout + 1), readResult);

                nbReceivedChars += readResult;

                // Set receive buffer as string.
                // In case of problems during reading attempt, also cancel eventual writing in receive buffer
                responseBuffer[(nbReceivedChars < GENERIC_MODEM_RESPONSE_BUFFER_SIZE ? nbReceivedChars : (GENERIC_MODEM_RESPONSE_BUFFER_SIZE - 1))] = '\0';

                // Modem returned OK, stop reception loop successful
                if (strstr(responseBuffer, GM_AT_COMMAND_RESPONSE_OK) != NULL)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "AT COMMAND RESPONSE OK detected");

                    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_getResponseData(): Data received at waiting loop #%d: %s", (timeout + 1), responseBuffer);
                    res = true;
                    break;
                }

                // Log data received at this loop. Will be not displayed if OK response detected before
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Data read : '%s'", &responseBuffer[nbReceivedChars - readResult]);

                // Modem returns Error, stop reception loop with error
                if (strstr(responseBuffer, GM_AT_COMMAND_RESPONSE_ERROR) != NULL)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "AT COMMAND RESPONSE ERROR detected at waiting loop #%d", (timeout + 1));
                    break;
                }

#if defined(GM_AT_MODEM_START_EVENT)
                // Modem Startup event detected, stop reception loop with error
                if (strstr(responseBuffer, GM_AT_MODEM_START_EVENT) != NULL)
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "MODEM STARTUP EVENT <%s> detected at waiting loop #%d", GM_AT_MODEM_START_EVENT, (timeout + 1));
                    break;
                }
#endif // GM_AT_MODEM_START_EVENT

            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Loop #%d: No bytes read", (timeout + 1));

            timeout++;

            // Time Out elapsed, stop reception loop with error
            if (timeout >= GM_RETRIEVE_RESPONSE_TIMEOUT)
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_getResponseData(): Time Out elapsed, no valid data available");
                break;
            }
        }
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_getResponseData(): ptrTSEMediaGenericModem is NULL!");

    return res;
}


/**
 * Check if response buffer show AT command response as successful
 *
 * @return true if "success response" pattern is detected
 */
static bool _seMediaGenericModemCheckATcommandResponseOK(void)
{
    bool res = false;

    if((strlen(responseBuffer) > 1) && (strstr(responseBuffer, GM_AT_COMMAND_RESPONSE_OK) != NULL))
        res = true;

    return res;
}


/**
 * Handle the whole process of sending APDU to the modem, retrieve response data and check response OK
 *
 * @param ptrAPDU APDU command to be sent to the modem
 * @param ptrTSEMedia - TSEMedia object pointer
 * @param checkStatusWord If true function will check response Status Word OK or not
 * @return True if the response APDU is correct and ended with success Status Word
 */
static bool _seMediaGenericModemSendAPDUtoModemAndCheckOK(const char* ptrAPDU, const struct TSEMedia* ptrTSEMedia, bool checkStatusWord)
{
    bool res = false;
    char ATcommand[GM_AT_COMMAND_BUFFER_SIZE];

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_sendAPDUtoModemAndCheckOK()...");

    if((ptrTSEMedia == NULL) || (ptrAPDU == NULL))
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_sendAPDUtoModemAndCheckOK(): Incorrect NULL parameter!");
        return false;
    }

    TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
    if (ptrTSEMediaGenericModem != NULL)
    {
    	if(_seMediaGenericModemApduToAtCommand(ptrAPDU, ATcommand, GM_AT_COMMAND_BUFFER_SIZE))
        {
            bool writeSuccessfully = false;

#if defined(LPA_SDK__PLATFORM_WIN)
            //DWORD bytesWritten = 0;

            //if (WriteFile(ptrTSEMediaGenericModem->_modemHandle, ATcommand, (DWORD)strlen(ATcommand), &bytesWritten, NULL) == TRUE && (bytesWritten == strlen(ATcommand)))
            //        writeSuccessfully = true;
            if (pciot_lpa_write(ptrTSEMediaGenericModem->_modemHandle, ATcommand, strlen(ATcommand)) == strlen(ATcommand))  //by fanming 2021.10.21
            {
                writeSuccessfully = true;
            }
#else
            //if (write(ptrTSEMediaGenericModem->_modemFD, ATcommand, strlen(ATcommand)) == strlen(ATcommand))
			if (pciot_lpa_write(ptrTSEMediaGenericModem->_modemFD, ATcommand, strlen(ATcommand)) == strlen(ATcommand))  //by fanming 2021.10.21
                    writeSuccessfully = true;
#endif // LPA_SDK__PLATFORM_WIN

            if( !writeSuccessfully )
            {
                lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_sendAPDUtoModemAndCheckOK(): Problem while transmitting AT command to modem.");
            }
            else
            {
                if(_seMediaGenericModemGetATcommandResponseData(ptrTSEMedia))
                {
                    if(_seMediaGenericModemCheckATcommandResponseOK())
                    {
                        char * extracted_ADPU_response = lpaCoreMemoryAlloc(strlen(responseBuffer));
                        if(extracted_ADPU_response != NULL)
                        {
                            if(_seMediaGenericModemAtCommandResponseToApdu(responseBuffer, extracted_ADPU_response, strlen(responseBuffer)))
                            {
                                // Status word have to be checked for driver internal operations, not for APDU's from upper layers
                                if(checkStatusWord)
                                    res = _seMediaGenericModemCheckAPDU_OkResponse(extracted_ADPU_response);
                                else
                                    res = true;
                            }
                            else
                                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_sendAPDUtoModemAndCheckOK(): Cannot extract APDU response from response buffer!");

                            lpaCoreMemoryFree(extracted_ADPU_response);
                        }
                        else
                            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_sendAPDUtoModemAndCheckOK(): Cannot allocate memory for temporary APDU buffer!");
                    }
                    else
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_sendAPDUtoModemAndCheckOK(): AT command response report an error!");
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_sendAPDUtoModemAndCheckOK(): Cannot retrieve AT command response!");
            }
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_sendAPDUtoModemAndCheckOK(): Problem while converting APDU to AT command!");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_sendAPDUtoModemAndCheckOK(): ptrTSEMediaGenericModem is NULL!");

    return res;
}


/**
 * Sends "open logical channel" APDU command to the module and retrieve logical channel ID in seMedia structure
 *
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return true if the command was successfully achieved.
 */
static bool _seMediaGenericModemOpenUICCChannel(const struct TSEMedia* ptrTSEMedia)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_openUICCChannel()...");

    if (ptrTSEMedia == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_openUICCChannel(): ptrTSEMedia is NULL!");
        return false;
    }

    TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
    if (ptrTSEMediaGenericModem != NULL)
    {
        // This is a driver internal operation, so Status Word will be checked
        if(_seMediaGenericModemSendAPDUtoModemAndCheckOK(GM_APDU_OPEN_SIM_CHANNEL, ptrTSEMedia, true))
        {
            char * extracted_ADPU_response = lpaCoreMemoryAlloc(strlen(responseBuffer));
            if(extracted_ADPU_response != NULL)
            {
                if(_seMediaGenericModemAtCommandResponseToApdu(responseBuffer, extracted_ADPU_response, strlen(responseBuffer)))
                {
                    ptrTSEMediaGenericModem->_apduChannelString[0] = extracted_ADPU_response[0];
                    ptrTSEMediaGenericModem->_apduChannelString[1] = extracted_ADPU_response[1];
                    ptrTSEMediaGenericModem->_apduChannelString[2] = '\0';

                    ptrTSEMediaGenericModem->_apduChannel = atoi(ptrTSEMediaGenericModem->_apduChannelString);

                    // Extended logical channels also managed (So 1 to 19 range)
                    if((ptrTSEMediaGenericModem->_apduChannel > 0) && (ptrTSEMediaGenericModem->_apduChannel < 20))
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_openUICCChannel(): Logical channel %2d opened.", ptrTSEMediaGenericModem->_apduChannel);
                        res = true;
                    }
                    else
                    {
                        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_openUICCChannel(): Logical channel out of bounds or incorrect: %d", ptrTSEMediaGenericModem->_apduChannel);
                        ptrTSEMediaGenericModem->_apduChannelString[0] = '\0';
                        ptrTSEMediaGenericModem->_apduChannel = 0;
                    }
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_openUICCChannel(): Cannot extract APDU response from response buffer!");

                lpaCoreMemoryFree(extracted_ADPU_response);
            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_openUICCChannel(): Cannot allocate memory for temporary APDU buffer!");
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_openUICCChannel(): Problem while sending logical channel opening command!");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_openUICCChannel(): ptrTSEMediaGenericModem is NULL!");

    return res;
}


/**
 * Sends "close logical channel" APDU command to the module. Channel to close will be current one described in TSEmedia structures
 *
 * @param ptrTSEMedia - TSEMedia object pointer
 * @return true if the command was successfully achieved.
 */
static bool _seMediaGenericModemCloseUICCChannel(const struct TSEMedia* ptrTSEMedia)
{
    bool res = false;
    char apduCommand[20]; // APDU stored in string format

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_closeUICCChannel()...");

    if (ptrTSEMedia == NULL)
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_closeUICCChannel(): ptrTSEMedia is NULL!");
        return false;
    }

    TSEMediaGenericModem* ptrTSEMediaGenericModem = (TSEMediaGenericModem*)ptrTSEMedia->_childStruct;
    if (ptrTSEMediaGenericModem != NULL)
    {
        // Check logical channel defined
        if(ptrTSEMediaGenericModem->_apduChannel != 0)
        {
            // TODO Should be better to avoid sending Close Logical Channel command after eUICC reset has been performed:
            // A third party application could have re-opened this Logical Channel for another use in the meanwhile.
            // Suggestion of problem solving:
            // - Close Logical Channel may be blocked if profile Enable / Disable with eUICC Reset requested has just been invoked (Implies modification of whole library)
            // - Could detect a specific Unsolicited Event that may indicate eUICC reset in the meanwhile

            memset(apduCommand, 0 , sizeof(apduCommand));   // Empty string

            // Build APDU Command
            // Note: strncat lengths limits + 1 to avoid warnings with some compilers: "...'strncat' specified bound X equals source length..."
            strncat(apduCommand, GM_APDU_CLOSE_SIM_CHANNEL_PREFIX, (strlen(GM_APDU_CLOSE_SIM_CHANNEL_PREFIX) + 1));   // Prefix
            strncat(apduCommand, ptrTSEMediaGenericModem->_apduChannelString, 3);                                     // Channel to close
            strncat(apduCommand, "00", 3);                                                                            // Length

            // This is a driver internal operation, so Status Word will be checked
            if(_seMediaGenericModemSendAPDUtoModemAndCheckOK(apduCommand, ptrTSEMedia, true))
            {
                ptrTSEMediaGenericModem->_apduChannel = 0;
                ptrTSEMediaGenericModem->_apduChannelString[0] = '\0';
                res = true;
            }
            else
            {
                // Try to detect if failure to close Logical Channel is not caused by UICC Reset initiated by STK Refresh Reset for example
                // Here try to find that GM_AT_COMMAND_RESPONSE_PREFIX is missing in response and that GM_AT_COMMAND_RESPONSE_ERROR can be found.
                // -> In that case we will consider there was probably an UICC Reset performed by modem
                //
                // This method may not apply to other models / brand of modems and may be need to be changed
                // This is just a turnaround for problem explained before Close Logical Channel command sending

                if((strstr(responseBuffer, GM_AT_COMMAND_RESPONSE_PREFIX) == NULL) && (strstr(responseBuffer, GM_AT_COMMAND_RESPONSE_ERROR) != NULL))
                {
                    lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_closeUICCChannel(): ! Detected possible eUICC Reset performed by modem, consider that Logical Channel is closed.");

                    ptrTSEMediaGenericModem->_apduChannel = 0;
                    ptrTSEMediaGenericModem->_apduChannelString[0] = '\0';
                    res = true;
                }
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_closeUICCChannel(): Problem while sending logical channel closing command!");
            }

        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_closeUICCChannel(): No logical channel currently opened / described in TSEmedia structure!");
    }
    else
        lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_closeUICCChannel(): ptrTSEMediaGenericModem is NULL!");

    return res;
}


/**
 * Extract APDU response from reception buffer and return it in raw hex byte array.
 *
 * @param ptrAPDU_response Array that will receive raw hex transcription of APDU response
 * @param pMaxSize Maximum size allowed for Hex response data array
 * @param pAPDU_response_size Will receive useful length of raw hex data stored in byte array
 * @return True if the extraction / conversion was successful.
 */
static bool _seMediaGenericModemProcessAPDUresponse(unsigned char* ptrAPDU_response, const size_t pMaxSize, size_t * pAPDU_response_size)
{
    bool res = false;

    lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "_processAPDUresponse()...");

    if(_seMediaGenericModemCheckATcommandResponseOK())
    {
        char * extracted_ADPU_response = lpaCoreMemoryAlloc(strlen(responseBuffer));
        if(extracted_ADPU_response != NULL)
        {
            if(_seMediaGenericModemAtCommandResponseToApdu(responseBuffer, extracted_ADPU_response, strlen(responseBuffer)))
            {
                // Set maximum size for hexStr2ByteArray() output
                *pAPDU_response_size = pMaxSize;
                if(hexStr2ByteArray((const unsigned char *)extracted_ADPU_response, (int)strlen(extracted_ADPU_response), ptrAPDU_response, (int *)pAPDU_response_size))
                    res = true;
                else
                    lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_processAPDUresponse(): Cannot convert APDU response to raw Hex or APDU response buffer too small!");

            }
            else
                lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_processAPDUresponse(): Cannot extract APDU response from response buffer!");

            lpaCoreMemoryFree(extracted_ADPU_response);
        }
        else
            lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "_processAPDUresponse(): Cannot allocate memory for temporary APDU buffer!");
    }
    else
    {
        lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "_processAPDUresponse(): Success status not detected in AT command response buffer!");
    }

    return res;
}

/**
 * send LPA Event execution error.
 *
 * @param scErrorCode - Error code
 * @param ptrErrorCodeDescription - String containing user readable error message
 */
void _seMediaGenericModemSendLpaEventExecutionError(long scErrorCode, const char* ptrErrorCodeDescription)
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

#if defined(LPA_SDK__PLATFORM_WIN)
void _seMediaGenericModemWriteOnLogWindowsError(DWORD windowsError)
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Windows error : 0x%lx", windowsError);
}
#endif // LPA_SDK__PLATFORM_WIN

#endif // LPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM

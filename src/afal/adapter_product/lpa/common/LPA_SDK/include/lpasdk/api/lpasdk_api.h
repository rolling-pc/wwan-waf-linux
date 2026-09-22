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


#ifndef LPA_SDK__CORE_API_H
#define LPA_SDK__CORE_API_H


#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>	

#include "lpasdk/api/semedia/semedia.h"

/////////////////////////////////
// API versionning
/////////////////////////////////

#define LPA_API_MAJOR_VERSION		1
#define LPA_API_MINOR_VERSION		7

typedef struct
{
	int major;	// Update it when API compatibility is not "supported"
	int minor;	// Update it when function added without impact on API compatibility
}LPA_API_VERSION;

/////////////////////////////////
// LPA Type
/////////////////////////////////

typedef int16_t			LPA_Integer;
typedef char*			LPA_StringPtr;

/////////////////////////////////
// API definition
/////////////////////////////////



#define LPA_CFG_DEVICE_INFO_TLV_MAX_SIZE 385	// 384 + CR
#define LPA_CFG_DEVICE_INFO_TLV_BYTE_ARRAY_MAX_SIZE ((LPA_CFG_DEVICE_INFO_TLV_MAX_SIZE >> 1) + 1)

#define LPA_CFG_CERT_PATH_MAX_SIZE LPA_MAX_PATH

#define LPA_CFG_LINE_ENTRY_MAX_SIZE				1024

/*
Maximum profile info size retrieved from eUICC (Max length used):
                                            Tag/size bytes + max data lengths
Header tag E3                               5 bytes
iccid tag 5A                                2 + 10 = 12 bytes
isdpAid tag 4F                              2 + 10 = 12 bytes
profileState tag 9F70                       3 + 1 = 4 bytes
profilenickName tag 90                      2 + 60 = 68 bytes
serviceProviderName tag 91                  2 + 32 = 34 bytes
profileName tag 92                          2 + 64 = 68 bytes
iconType tag 93                             2 + 1 = 3 bytes
icon tag 94                                 4 + 1024 = 1028 bytes
profileClass tag 95                         2 + 1 = 3 bytes

TOTAL -------------------------------------- 1237 bytes - Set at 1300 for rounding / security
Note: Other tags (B6, B7, B8 & 99) not requested at default GetProfileInfo request performed here, so not taken in account
*/
#define LPA_PROFILE_INFO_BUFFER_MAX_SIZE 1300	// Size of profile raw data
#define LPA_MAX_EVENT_RECORD 16

// All profile fields limits match maximum defined in SGP.22
// LPA_PROFILE_STATE_MAX_SIZE, LPA_PROFILE_CLASS_MAX_SIZE, LPA_PROFILE_ICON_TYPE_MAX_SIZE defined as ASN1 'INTEGER'
#define LPA_ASN1_INTEGER_MAX_SIZE 2
#define LPA_PROFILE_ICCID_BUFFER_MAX_SIZE 10
#define LPA_PROFILE_STATE_MAX_SIZE LPA_ASN1_INTEGER_MAX_SIZE
#define LPA_PROFILE_SERVICE_PROVIDER_NAME_MAX_SIZE 32
#define LPA_PROFILE_NAME_MAX_SIZE 64
#define LPA_PROFILE_CLASS_MAX_SIZE LPA_ASN1_INTEGER_MAX_SIZE
#define LPA_PROFILE_ICON_MAX_SIZE 1024
#define LPA_PROFILE_ICON_TYPE_MAX_SIZE LPA_ASN1_INTEGER_MAX_SIZE
#define LPA_PROFILE_NICKNAME_MAX_SIZE 64

#define LPA_GET_EID_BUFFER_SIZE 64
#define LPA_GET_EUICC_BUFFER_MAX_SIZE  384	//255 previously

#define LPA_CANCEL_SESSION_RESPONSE_BUFFER_SIZE 160

// For memory: Full Activation code is 1$lpa_SM-Dx_address$matchingId$oid_object$ccRequiredFlag
#define LPA_ACTIVATION_CODE_MAX_SIZE (2 + LPA_SMDP_ADDRESS_SIZE + 1 + LPA_MATCHING_ID_SIZE + 1 + LPA_OID_SIZE + 1 + LPA_CC_REQUIRED_FLAG_SIZE)
#define LPA_ACTIVATION_CODE_MAX_STRING_BUFFER_SIZE  ( LPA_ACTIVATION_CODE_MAX_SIZE + 1 )		// Adding 1 bytes to End of String

#define LPA_SMDP_ADDRESS_SIZE  128
#define LPA_SMDS_ADDRESS_SIZE  LPA_SMDP_ADDRESS_SIZE
#define LPA_ADDRESS_MAX_SIZE 255 
#define LPA_MATCHING_ID_SIZE  255
#define LPA_OID_SIZE 128
#define LPA_CC_REQUIRED_FLAG_SIZE 2

#define LPA_MAX_PROFILE_NOTIFICATION_LIST_METADATA_ELEMENT		16
#define LPA_MAX_NOTIFICATION_ADDRESS_RAW_DATA_SIZE				128

// Managing Confirmation Code during downloadProfile
#define LPA_CONFIRMATION_CODE_MAX_SIZE					50

// Maximum number of parameters for parameter list generation
// ! CAUTION: Have to take care that list is long enough to store all parameters and element size long enough to fit longest parameters names 
#define LPA_MAX_PARAMETERS_LIST 25
#define LPA_MAX_PARAMETERS_LIST_ELEMENT_SIZE 40

// For conversion of size_t when used in snprintf(), unsigned in Raspbian, long unsigned in other platforms.
// Not the better way but values used here shall not exceed normal unsigned range
// May be replaced by something better after
#ifdef LPA_SDK__PLATFORM_RASPBIAN
#define CAST_SIZET_PLATFORM(value) value
#else
#define CAST_SIZET_PLATFORM(value) (unsigned)value
#endif

typedef struct
{
	int major;
	int minor;
	int patch;
	int build;
}LPA_VERSION;


// Support of Notification Error during a download profile
// Application owner should manage LPA_EVENT_REQUEST_CONFIRMATION_CODE callback 
typedef enum 
{
	LPA_EVENT_EXECUTION_SERVER_ERROR_TYPE = 1,			// Error send by server
	LPA_EVENT_EXECUTION_HTTP_ERROR_TYPE = 2,			// HTTP error during communication with server
	LPA_EVENT_EXECUTION_CURL_ERROR_TYPE = 3,			// Curl error
	LPA_EVENT_EXECUTION_SEMEDIA_DRIVER_ERROR_TYPE = 4	// SEMEDIA_DRIVER error
}LPA_EVENT_EXECUTION_ERROR_TYPE;

typedef enum
{
	LPA_EVENT_EXECUTION_ERROR_NO_DETAIL_MASK = 0x0000,		// 0b00000000

	LPA_EVENT_EXECUTION_ERROR_SUBJECT_CODE_MASK = 0x0001,	// 0b00000001
	LPA_EVENT_EXECUTION_ERROR_REASON_CODE_MASK = 0x0002,	// 0b00000010
	LPA_EVENT_EXECUTION_ERROR_EXTRA_INFO_MASK = 0x0004,		// 0b00000100
}LPA_EVENT_EXECUTION_ERROR_DETAIL_MASK;

typedef struct
{
	LPA_EVENT_EXECUTION_ERROR_TYPE				executionErrorType;
	LPA_EVENT_EXECUTION_ERROR_DETAIL_MASK		detailErrorMask;
	const char*									ptrErrorSubjectCode;
	const char*									ptrErrorReasonCode;
	const char*									ptrErrorExtraInfo;
} LPA_EVENT_EXECUTION_ERROR_INFO;


// Support of Confirmation Code requested by LPA SDK
// Application owner should manage LPA_EVENT_EXECUTION_ERROR callback 

typedef struct
{
	// Confirmation code value updated by application
	char confirmationCode[LPA_CONFIRMATION_CODE_MAX_SIZE];

	// Size of CC buffer (with final '\0')
	size_t confirmationCodeMaxBufferSize; 

	// Updated by LPA Application
	unsigned int reasonCodeNoCC; // used if no Confirmation Code
} LPA_REQUEST_CONFIRMATION_CODE;

// @Since LPASDK API 1.6
typedef struct
{
    unsigned char userCallBackType;                     // Set information(s) to display. Values masks defined in LPA_USER_CONSENT_TYPES enum. Each bit set an info to display.
    char profileName[LPA_PROFILE_NAME_MAX_SIZE + 2];    // Profile name that may be displayed. String coded. +1 byte for EOS, +1 byte for security
    bool downloadAllowed;                               // Confirm user accepted download (True) or refused (False)
    unsigned int cancelSessionReason;                   // Values defined in LPA_CANCEL_SESSION_REASON enum. Value has no importance if downloadAllowed returned "true"
}LPA_REQUEST_USER_CONSENT_FOR_LOADING_PROFILE;


// LPA Event mechanism
// eventType is dependant of LPA SDK API used
// zero, one or both LPA Event function can be registered

// function <lpaEventProgessValue> is used to manage a Progress Value 
typedef void (*LPA_EVENT_PROGRESS_VALUE) (const void* ptrAppParameter, size_t eventType, size_t valueMin, size_t currentValue, size_t valueMax);

// function <lpaEventProgressText> is used to communicate a text message to the application
typedef void(*LPA_EVENT_PROGRESS_TEXT) (const void* ptrAppParameter, size_t eventType, const char* ptrText);

// function <lpaEventProgressText> is used to communicate a text message to the application
// return true if CC entered correctly, otherwise return false and update reasonCodeNoCC field
typedef bool(*LPA_EVENT_REQUEST_CONFIRMATION_CODE) (const void* ptrAppParameter, LPA_REQUEST_CONFIRMATION_CODE* ptrRequestConfirmationCode);

// function <lpaEventExecutionError> is used to communicate some execution error information (subjectCode, reasonCode ...) to the application
typedef void(*LPA_EVENT_EXECUTION_ERROR) (const void* ptrAppParameter, const LPA_EVENT_EXECUTION_ERROR_INFO* ptrEventExecutionErrorInfo);

// function <lpaEventRequestUserConsentForLoadingProfile> is used to request a User Consent before loading a profile (depending of profile metadata)
// @since LPASDK API 1.6
typedef bool(*LPA_EVENT_REQUEST_USER_CONSENT_FOR_LOADING_PROFILE) (const void* ptrAppParameter, LPA_REQUEST_USER_CONSENT_FOR_LOADING_PROFILE* ptrRequestUserConsentForLoadingProfile);

typedef struct
{
	// LPA SDK event parameter
	void* _appParameter;	// NULL if not used, otherwise specific application parameter (using it at first parameter value of event function
	// In C++ application, could be instance of a class

	// LPA SDK Event callback functions - Must be initialized to NULL if not used.
	LPA_EVENT_PROGRESS_VALUE _lpaEventProgressValue;
	LPA_EVENT_PROGRESS_TEXT _lpaEventProgressText;
	LPA_EVENT_EXECUTION_ERROR _lpaEventExecutionError;

	// When LPA SDK request a confirmation code during a DownloadProfile 
	LPA_EVENT_REQUEST_CONFIRMATION_CODE _lpaEventRequestConfirmationCode;

    // Event for managing User Consent before loading profile
    LPA_EVENT_REQUEST_USER_CONSENT_FOR_LOADING_PROFILE _lpaEventRequestUserConsentForLoadingProfile;
}LPA_EventCallback;

typedef struct
{
	// Profile Info Record
	unsigned char rawData[LPA_PROFILE_INFO_BUFFER_MAX_SIZE];
	size_t rawDataSize;

	unsigned char iccid[LPA_PROFILE_ICCID_BUFFER_MAX_SIZE];
	size_t iccidSize;

	unsigned char profileState[LPA_PROFILE_STATE_MAX_SIZE];
	size_t profileStateSize;

	unsigned char serviceProviderName[LPA_PROFILE_SERVICE_PROVIDER_NAME_MAX_SIZE];
	size_t serviceProviderNameSize;

	unsigned char profileName[LPA_PROFILE_NAME_MAX_SIZE];
	size_t profileNameSize;

	unsigned char profileClass[LPA_PROFILE_CLASS_MAX_SIZE];
	size_t profileClassSize;
    
    unsigned char profileNickname[LPA_PROFILE_NICKNAME_MAX_SIZE];
    size_t profileNicknameSize;
    
    unsigned char profileIcon[LPA_PROFILE_ICON_MAX_SIZE];
    size_t profileIconSize;
    
    unsigned char profileIconType[LPA_PROFILE_ICON_TYPE_MAX_SIZE];
    size_t profileIconTypeSize;
}LPA_PROFILE_INFO; 

typedef struct
{
	// Get Profile Info All
	size_t countProfileInfo;            // Number of profiles effectively retrieved and available in profileInfoList
    size_t numberProfileInfoFound;      // Number of profiles found in eUICC. Can be higher than value returned in "countProfileInfo"
    size_t maxNumberProfileInfo;        // Maximum number of profiles that can be stored in profileInfoList
    unsigned char * profileInfoList;    // Data area storing profiles information list. Must be allocated with (maxNumberProfileInfo * sizeof(LPA_PROFILE_INFO)) size.
}LPA_GET_PROFILES_INFO;

typedef struct
{
	// EID
	unsigned char EID_Data[LPA_GET_EID_BUFFER_SIZE];
	size_t EID_DataSize;
}LPA_GET_EID;

typedef struct
{
	size_t	countProfileInstalled;
	size_t countProfileTotal;
} LPA_DOWNLOAD_PROFILE_RESULT;

typedef struct
{
	size_t	countNotificationDetected; // Before sending
	size_t	countNotificationSend;		// really send
} LPA_SENDING_NOTIFICATION_RESULT;

typedef enum
{
	LPA_NOTIFICATION_INSTALL = 0,
	LPA_NOTIFICATION_ENABLE = 1,
	LPA_NOTIFICATION_DISABLE = 2,
	LPA_NOTIFICATION_DELETE = 3,
	LPA_NOTIFICATION_UNKNOWN = 0xFF
} LPA_NOTIFICATION_EVENT;

    


typedef struct {
    bool resultOK;
    unsigned char cancelSessionResponse_RawData[LPA_CANCEL_SESSION_RESPONSE_BUFFER_SIZE];
    size_t cancelSessionResponse_RawDataSize;
}CANCEL_SESSION_RESPONSE;
 
typedef struct
{
	// SMDP Address
	char smdxAddr[LPA_SMDP_ADDRESS_SIZE];
    // activation code
	char matchingId[LPA_MATCHING_ID_SIZE];
    // OID
    char oid[LPA_OID_SIZE];
	// Confirmation Code Required flag
    char ccRequiredFlag[LPA_CC_REQUIRED_FLAG_SIZE];
} ACTIVATION_CODE;

typedef struct {
    char eventId[LPA_MATCHING_ID_SIZE];
    char rspServerAddress[LPA_SMDP_ADDRESS_SIZE];
    unsigned char EID_RawData[LPA_GET_EID_BUFFER_SIZE];
    bool forwardingIndicator;
}LPA_EVENT_RECORD;

typedef struct {
    size_t countEvent;
    LPA_EVENT_RECORD eventRecordList[LPA_MAX_EVENT_RECORD];
}EVENT_RECORD_LIST;

typedef struct {
    char eUICCConfiguredAddr_RawData[LPA_GET_EUICC_BUFFER_MAX_SIZE];
    size_t eUICCConfiguredAddr_RawDataSize;
} EUICC_CONFIGURE_ADDR;

typedef struct {
    char address_Data[LPA_SMDP_ADDRESS_SIZE];
    size_t address_DataSize;
}ADDRESS_DATA;

typedef enum
{
	LPA_DELETE_NOTIFICATION_OK = 0,
	LPA_DELETE_NOTIFICATION_NOTHING_TO_DELETE = 1,
	LPA_DELETE_NOTIFICATION_UNDEFINED_ERROR = 127,
	LPA_DELETE_NOTIFICATION_UNKNOWN = 0xFF
}LPA_DELETE_NOTIFICATION_STATUS;

typedef struct
{
	uint16_t seqNumber;
	LPA_NOTIFICATION_EVENT profileManagementOperation;
	unsigned char notificationAddressRawData[LPA_MAX_NOTIFICATION_ADDRESS_RAW_DATA_SIZE];
	size_t notificationAddressRawDataSize;
}LPA_PROFILE_NOTIFICATION_METADATA;


typedef struct
{
	size_t countNotification;
	LPA_PROFILE_NOTIFICATION_METADATA notificationMetadataList[LPA_MAX_PROFILE_NOTIFICATION_LIST_METADATA_ELEMENT];
} LPA_PROFILE_NOTIFICATION_LIST;

typedef enum
{
	LPA_MEMORY_RESET_OK = 0,
	LPA_MEMORY_RESET_NOTHING_TO_DELETE = 1,
	LPA_MEMORY_RESET_CAT_BUSY = 5,
	LPA_MEMORY_RESET_UNDEFINED_ERROR = 127,
	LPA_MEMORY_RESET_UNKNOWN = 0xFF
}LPA_MEMORY_RESET_STATUS;

// Cancel Session codes
typedef enum
{
    LPA_CANCEL_SESSION_END_USER_REJECTION = 0,
    LPA_CANCEL_SESSION_POSTPONED = 1,
    LPA_CANCEL_SESSION_TIME_OUT = 2,
    LPA_CANCEL_SESSION_PPR_NOT_ALLOWED = 3,
    LPA_CANCEL_SESSION_METADATA_MISMATCH = 4,
    LPA_CANCEL_SESSION_LOAD_BPP_EXECUTION_ERROR = 5,
    LPA_CANCEL_SESSION_UNDEFINED_REASON = 127
}LPA_CANCEL_SESSION_REASON;

static const unsigned int LPA_ALLOWED_CANCEL_SESSION_CODE_LIST[] = {0, 1, 2, 3, 4, 5, 127};
#define LPA_ALLOWED_CANCEL_SESSION_CODE_LIST_SIZE     7

// User consent type for Callback. These are binary mask, they can be mixed together or checked individually.
typedef enum
{
    LPA_USR_CONSENT_DISABLED = 0x00,
    LPA_USR_CONSENT_PPR1 = 0x01,
    LPA_USR_CONSENT_PPR2 = 0x02,
    LPA_USR_CONSENT_PROFILE_WITH_PPR1_ENABLED_PRESENT = 0x04
}LPA_USER_CONSENT_TYPES;

// Management of retry for chained GetResponse issue with modems (0x6D00 issue, other SW may be discovered) through LPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE
// If not defined or minus than 1 retry mechanism will be disabled
#ifndef LPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE
    #define LPA_RETRY_CHAINED_GET_RESPONSE_MGT false        // Do not modify this value !
    #define LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE 1      // Do not modify this value ! Must be AT LEAST value "1" else commands will be NOT played.
#elif LPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE < 1
    #define LPA_RETRY_CHAINED_GET_RESPONSE_MGT false        // Do not modify this value !
    #define LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE 1      // Do not modify this value ! Must be AT LEAST value "1" else commands will be NOT played.
#else
    #define LPA_RETRY_CHAINED_GET_RESPONSE_MGT true         // Do not modify this value !
    #define LPA_LOOP_NUMBER_FOR_CHAINED_GET_RESPONSE LPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE + 1 // Do not modify this expression ! Must be +1 to include initial command run to retries.
#endif // defined LPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE


// Management of maximum size of data send through APDU. Need to limit size due to some modems not supporting 261 bytes (So 522 characters) in case 4 APDU
// If not defined default value of 0xFF will be used.
// If out of bounds will issue an error and stop compilation. If #error ignored by compiler used, value will be set to 0xFF to show that something is wrong.
// Boundaries for usable limits shall not be changed.
// Minimum size is fixed to 0x52 (82) due to eUICC limitations that does not support chained Store Data for some commands. 1 byte has been reserved for security.
//  - Set Nickname : 0x51 (81) bytes minimum needed
//  - Enable / Disable / Delete profile : 0x14 (20) bytes minimum needed
//  - Any download command, Set default DP, Send Notification : Tested OK with 10 bytes size limitation
#ifndef LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU
    #define LPA_STORE_DATA_APDU_DATA_SIZE_MAX 0xFF
#elif LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU < 0x52  // Lower limit given by Set Nickname
    #define LPA_STORE_DATA_APDU_DATA_SIZE_MAX 0xFF
    #error LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU is out of authorized range ( decimal: [82,255 ] , hexa: [0x52,0xFF] ).
#elif LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU > 0xFF  // Upper limit, max possible in an APDU
    #define LPA_STORE_DATA_APDU_DATA_SIZE_MAX 0xFF
    #error LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU is out of authorized range ( decimal: [82,255 ] , hexa: [0x52,0xFF] ).
#else
    #define LPA_STORE_DATA_APDU_DATA_SIZE_MAX LPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU
#endif


/////////////////////////////////
// API ERROR
/////////////////////////////////

typedef enum {
	LPA_NO_ERROR = 0,

	// 0x1001 - 0x1999 => Configuration error
	LPA_NOT_INITIALIZED = 0x1001,

	// 0x2001 - 0x2FFF => Common error
	LPA_ERROR_INVALID_PARAMETER = 0x2001,
	LPA_ERROR_INSUFFICIENT_BUFFER,
	LPA_ERROR_INVALID_SW,
	LPA_ERROR_ISDR_NOT_SELECTED,
	LPA_ERROR_ISDR_ALREADY_SELECTED,
	LPA_ERROR_SE_MEDIA_NOT_INITIALIZED,
	LPA_ERROR_SE_MEDIA_CONTEXT_NOT_ESTABLISHED,
	LPA_ERROR_SE_MEDIA_READER_CONNECTION,
	LPA_ERROR_SE_MEDIA_CONTEXT_NOT_RELEASED,
	LPA_ERROR_SE_MEDIA_READER_NOT_DISCONNECTED,
	LPA_ERROR_SE_MEDIA_UNABLE_TO_SELECT_ISDR,
	LPA_ERROR_INVALID_GET_PROFILES_INFO_EXCHANGE,
	LPA_ERROR_INVALID_GET_EID_EXCHANGE,
	LPA_ERROR_INVALID_GET_EUICC_ADDRESS,
	LPA_ERROR_INVALID_MEMORY_RESET_EXCHANGE,
	LPA_ERROR_INVALID_SET_DEFAULT_SMDP_ADDRESS,
	LPA_ERROR_INVALID_GET_SMDP_ADDRESS,
	LPA_ERROR_INVALID_GET_SMDS_ADDRESS,        
	LPA_ERROR_PARAMETER_NOT_AUTHORIZED,
	LPA_ERROR_UNKNOWN_PARAMETER,
	LPA_ERROR_INVALID_PARAMETER_TYPE,
	LPA_ERROR_INCORRECT_PARAMETER_TYPE,
	LPA_ERROR_PARAMETER_INTERNAL_ERROR,
	LPA_ERROR_UNABLE_TO_INIT_LOG,
	LPA_ERROR_MISSING_CONFIG_FILE,						// 23/12/2019 No longer used
	LPA_ERROR_UNABLE_TO_LOAD_CONFIG_FILE,				// 23/12/2019 No longer used
	LPA_ERROR_CONFIG_FILE_MISSING_MANDATORY_KEY,        // 06/06/2019 No longer used
	LPA_ERROR_CONFIG_FILE_INCORRECT_KEY_VALUE,
	LPA_ERROR_UNABLE_TO_INITIALIZE_LPA_MANAGER,
	LPA_ERROR_UNABLE_TO_INITIALIZE_HTTP_MEDIA,
	LPA_ERROR_EXTENDED_API_UNAVAILABLE,
    LPA_ERROR_PROCESSING_ERROR,
                
	// Local Profile Management
	LPA_ERROR_LOCAL_PROFILE_NOT_FOUND = 0x2101,
	LPA_ERROR_LOCAL_PROFILE_INCORRECT_STATE,
	LPA_ERROR_LOCAL_PROFILE_CAT_BUSY,
	LPA_ERROR_LOCAL_PROFILE_INCORRECT_CARD_RESPONSE,
	LPA_ERROR_LOCAL_PROFILE_UNKNOWN_ERROR,
	LPA_ERROR_LOCAL_PROFILE_NOTHING_TO_DELETE,
	LPA_ERROR_LOCAL_PROFILE_UNDEFINED_ERROR,
	LPA_ERROR_LOCAL_PROFILE_UNABLE_TO_EXTRACT_DATA,
    
	LPA_ERROR_LOCAL_PROFILE_ICCID_OR_AID_NOT_FOUND,		// enableResult => iccidOrAidNotFound
	LPA_ERROR_LOCAL_PROFILE_NOT_IN_DISABLE_STATE,		// enableResult => profileNotInDisabledState
	LPA_ERROR_LOCAL_PROFILE_DISALLOWED_BY_POLICY,		// enableResult => disallowedByPolicy
	LPA_ERROR_LOCAL_PROFILE_WRONG_PROFILE_REENABLING,	// enableResult => wrongProfileReenabling
	LPA_ERROR_LOCAL_PROFILE_NOT_IN_ENABLE_STATE,		// disableResult => profileNotInEnabledState
    
    LPA_ERROR_LOCAL_PROFILE_INVALID_DATA_EXCHANGE,

	// Notification management
	LPA_ERROR_NOTIFICATION_INCORRECT_CARD_RESPONSE = 0x2201,
	LPA_ERROR_NOTIFICATION_NOTHING_TO_DELETE,
	LPA_ERROR_NOTIFICATION_UNDEFINED_ERROR,
	LPA_ERROR_NOTIFICATION_UNKNOWN_ERROR,
	LPA_ERROR_NOTIFICATION_INVALID_CARD_DATA,

	// Download Profile management
	LPA_ERROR_DOWNLOAD_PROFILE_PARAMETER_ERROR = 0x2301,
	LPA_ERROR_INVALID_GET_EUICC_INFO,
	LPA_ERROR_INVALID_GET_UICC_CHALLENGE,
	LPA_ERROR_INVALID_SERVER_ADDRESS,
	LPA_ERROR_FAILED_INITIAL_AUTHENTICATION,
	LPA_ERROR_FAILED_AUTHENTICATE_SERVER,
	LPA_ERROR_FAILED_AUTHENTICATE_CLIENT,
	LPA_ERROR_AUTHENTICATE_CLIENT_EXCHANGE,
	LPA_ERROR_FAILED_PREPARE_DOWNLOAD,
	LPA_ERROR_INVALID_ACTIVATION_CODE,
	LPA_ERROR_INVALID_CTX_PARAM,
	LPA_ERROR_INVALID_TRANSACTIONID,
	LPA_ERROR_INVALID_AUTHENTICATE_SERVER_RESPONSE,
	LPA_ERROR_INVALID_PREPARE_DOWNLOAD_RESPONSE,
    LPA_ERROR_INVALID_MATCHINGID_OR_DEVICE_INFO_TLV,    // 03/09/2020 Confirmed not yet used
    LPA_ERROR_INVALID_DEVICE_INFO_TLV,
    LPA_ERROR_FAILED_GET_BOUND_PROFILE_PACKAGE,
	LPA_ERROR_FAILED_LOAD_BPP,
    LPA_ERROR_FAILED_INITIAL_SECURITY_CHANNEL,
    LPA_ERROR_FAILED_CONFIGURE_ISDP,
    LPA_ERROR_FAILED_STORE_META_DATA,
    LPA_ERROR_FAILED_REPLACE_SESSION_KEY,
    LPA_ERROR_FAILED_LOAD_PROFILE_ELEMENTS,
    LPA_ERROR_FAILED_GET_DATA_FROM_ACTIVATION_CODE,
	LPA_ERROR_INVALID_PIR_RESPONSE,
    LPA_ERROR_GET_INTERNAL_SERVER_ERROR,
	LPA_ERROR_CJSON_PARSE_FAILURE,
    LPA_ERROR_FAILED_SEND_NOTIFICATION_OR_NOT_GET_STATUS_CODE,
    LPA_ERROR_SERVER_COMMUNICATION_ISSUE,
    LPA_ERROR_INVALID_EVENT_ID,
    LPA_ERROR_INVALID_RSP_SERVER_ADDRESS,
    LPA_ERROR_INVALID_EVENT_ENTRIES,
    LPA_ERROR_NO_EVENT_RECORD_FOUND,                // This error code is now obsolete since V1.6
    LPA_ERROR_CONFIRMATION_CODE_MISSING_OR_EMPTY,
	LPA_ERROR_SERVER_RETURN_404_STATUS_CODE,
	LPA_ERROR_SERVER_RETURN_500_STATUS_CODE,
	LPA_ERROR_SE_MEDIA_UNABLE_TO_UNSELECT_ISDR,
    LPA_ERROR_INVALID_SERVERSIGNED1,
    LPA_ERROR_INVALID_SERVERSIGNATURE1,
    LPA_ERROR_INVALID_EUICCCIPKIDTOBEUSED,
    LPA_ERROR_INVALID_SERVERCERTIFICATE,
    LPA_ERROR_INVALID_SERVER_RESPONSE,
    LPA_ERROR_FAILED_CANCEL_SESSION,
    LPA_ERROR_INVALID_PROFILE_METADATA,
    LPA_ERROR_INVALID_GET_RAT,
    LPA_ERROR_PPR_NOT_ALLOWED,
    LPA_ERROR_DOWNLOAD_SESSION_CANCELED_BY_USER,
    LPA_ERROR_OID_MISMATCH,
    LPA_ERROR_INVALID_BPP_DATA,

	// 0x8000 - 0x9999 => SE Media error
	SE_MEDIA_ERROR_BROKEN_PIPE = 0x8000,
	SE_MEDIA_E_CANCELLED,
	SE_MEDIA_E_CANT_DISPOSE,
	SE_MEDIA_E_CARD_UNSUPPORTED,
	SE_MEDIA_E_DUPLICATE_READER,
	SE_MEDIA_E_FILE_NOT_FOUND,
	SE_MEDIA_E_INSUFFICIENT_BUFFER,
	SE_MEDIA_E_INVALID_ATR,
	SE_MEDIA_E_INVALID_HANDLE,
	SE_MEDIA_E_INVALID_PARAMETER,
	SE_MEDIA_E_INVALID_TARGET,
	SE_MEDIA_E_INVALID_VALUE,
	SE_MEDIA_E_NO_MEMORY,
	SE_MEDIA_E_NO_READERS_AVAILABLE,
	SE_MEDIA_E_NO_SMARTCARD,
	SE_MEDIA_E_NOT_READY,
	SE_MEDIA_E_PROTO_MISMATCH,
	SE_MEDIA_E_READER_UNAVAILABLE,
	SE_MEDIA_E_READER_UNSUPPORTED,
	SE_MEDIA_E_SERVER_TOO_BUSY,
	SE_MEDIA_E_SERVICE_STOPPED,
	SE_MEDIA_E_SHARING_VIOLATION,
	SE_MEDIA_E_SYSTEM_CANCELLED,
	SE_MEDIA_E_TIMEOUT,
	SE_MEDIA_E_UNEXPECTED,
	SE_MEDIA_E_UNKNOWN_CARD,
	SE_MEDIA_E_UNKNOWN_READER,
	SE_MEDIA_W_REMOVED_CARD,
	SE_MEDIA_W_RESET_CARD,
	SE_MEDIA_W_UNSUPPORTED_CARD,
    SE_MEDIA_E_CHAINING_GET_RESPONSE,   // Can be issued only if LPA_SDK__DEACTIVATE_RETRY_FOR_CHAINED_GET_RESPONSE is not defined

} LPA_API_ERROR;

typedef enum 
{
	LPA_PARAMETER_TYPE_BOOL,		// bool
	LPA_PARAMETER_TYPE_LONG,		// long
	LPA_PARAMETER_TYPE_STRING,		// char*

	LPA_PARAMETER_TYPE_UNKNOWN = 1664,	// Unknown type; Must be the latest on the enum
} LPA_PARAMETER_TYPE;

// List of settable parameters. Memory contain initialization is done by lpaManagerGetFullParametersList()
// Size constants are fixed in lpasdk_api.h, shall fit all available parameters.
typedef struct
{
    size_t parametersCount;
    char parametersList[LPA_MAX_PARAMETERS_LIST][LPA_MAX_PARAMETERS_LIST_ELEMENT_SIZE];
    LPA_PARAMETER_TYPE parametersTypeList[LPA_MAX_PARAMETERS_LIST];
}LPA_PARAMETERS_LIST;

typedef struct
{
	LPA_API_ERROR apiErrorCode;
	char* ptrApiErrorDescription;
} LPA_API_ERROR_DESCRIPTION;

#if defined (LPA_SDK__PLATFORM_WIN) || defined (LPA_SDK__PLATFORM_CYGWIN)
#define EXPORT_DLL __declspec(dllexport)
#else
// to add default visibility on LINUX
#define EXPORT_DLL __attribute__ ((visibility ("default") ))
#endif

// LPA SDK API versionning
EXPORT_DLL const LPA_API_VERSION* lpaGetApiVersion();

// LPA SDK Initialization
EXPORT_DLL bool lpaInitialize(const char* ptrLpaFolder);
// @Since : API 1.6
EXPORT_DLL bool lpaInitializeWithInputOutputFolder(const char* ptrLpaInputFolder, const char* ptrLpaOutputFolder);

// Manage LPA SDK versionning
EXPORT_DLL bool lpaGetVersion(LPA_VERSION* ptrLpaVersion);

// LPA SDK Uninitialization
EXPORT_DLL bool lpaUninitialize();

// LPA SDK IS initialized
EXPORT_DLL bool lpaIsInitialized();

// LPA SDK => GetErrorCode
EXPORT_DLL LPA_API_ERROR lpaGetErrorCode();
EXPORT_DLL const char* lpaGetErrorCodeDescription(LPA_API_ERROR apiError);

// LPA SDK Configuration
EXPORT_DLL bool lpaSetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, const void* ptrParameterValue);
EXPORT_DLL bool lpaGetConfigParameter(const char* ptrParameterName, LPA_PARAMETER_TYPE parameterType, void* ptrParameterValue, size_t parameterValueMaxSize);
EXPORT_DLL bool lpaIsConfigParameterExist(const char* ptrParameterName, LPA_PARAMETER_TYPE* ptrParameterType, bool* ptrIsExist);

// LPA SDK Reader
EXPORT_DLL bool	lpaGetReaderList(LPA_SE_MEDIA_READER_NAME_INFO * readerNameInfoList, size_t readerNameInfoMax, size_t* countReader);


EXPORT_DLL bool lpaGetProfilesInfo(LPA_GET_PROFILES_INFO*);
EXPORT_DLL bool lpaGetProfilesInfo_ex(LPA_GET_PROFILES_INFO*);
EXPORT_DLL bool lpaGetProfilesNumber(size_t*);
EXPORT_DLL bool lpaGetEID(LPA_GET_EID*);
EXPORT_DLL bool lpaMemoryReset(const unsigned char* memoryResetOptionParameter, const size_t memoryResetOptionSize);
EXPORT_DLL bool lpaSendPendingNotification(LPA_EventCallback* ptrLpaEventCallback, LPA_SENDING_NOTIFICATION_RESULT* ptrSendingNotificationResult);

EXPORT_DLL bool lpaEnableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);
EXPORT_DLL bool lpaDisableProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);
EXPORT_DLL bool lpaDeleteProfileByIccid(const unsigned char* ptrProfileId, size_t profileIdSize);

EXPORT_DLL bool lpaSetDefaultSMDPAddress(const char* ptrSMDPAddr);
EXPORT_DLL bool lpaGetSMDPAddress(ADDRESS_DATA* ptrAddressData);
EXPORT_DLL bool lpaGetSMDSAddress(ADDRESS_DATA* ptrAddressData);

EXPORT_DLL bool lpaDownloadProfile(const char * ptrActivationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);
EXPORT_DLL bool lpaDownloadProfileWithConfirmationCode(const char * ptrActivationCodeStr, const char * ptrConfirmationCodeStr, const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);
EXPORT_DLL bool lpaDownloadProfileWithDefaultSMDPAddress(const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);
EXPORT_DLL bool lpaDownloadProfileWithSMDSAddress( const LPA_EventCallback* ptrLpaEventCallback, LPA_DOWNLOAD_PROFILE_RESULT* ptrDownloadProfileResult);

EXPORT_DLL bool lpaSetNicknameByIccid(const unsigned char* ptrProfileId, size_t profileIdSize, const unsigned char* ptrNickname, size_t nickNameSize);

#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif // LPA_SDK__CORE_API_H

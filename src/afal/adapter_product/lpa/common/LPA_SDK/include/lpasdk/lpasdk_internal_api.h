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


#ifndef LPA_SDK__CORE_INTERNAL_API_H
#define LPA_SDK__CORE_INTERNAL_API_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lpasdk/api/lpasdk_api.h"
#include "lpasdk/core/rawdata_object.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

///////////////////////////////////
// MACRO PART
///////////////////////////////////

#define FREEIF(ptr) do{\
    if(ptr) free(ptr);\
    ptr=NULL;\
}while(0)

#define FREEIF_LPAMEMORYBLOCK(ptr) do{\
    if(ptr) lpaCoreMemoryFree(ptr);\
    ptr=NULL;\
}while(0)

///////////////////////////////////
// DEFINE PART
///////////////////////////////////
	
#define LPA_AUTHENTICATE_SERVER_MAX_SIZE  4500
#define LPA_PIR_BUFFER_MAX_SIZE 400

//max size for the server data
#define LPA_TRANSACTION_ID_MAX_SIZE 40    
#define LPA_INITIAL_AUTHENTICATE_SERVER_SIGNED1_MAX_SIZE 350
#define LPA_INITIAL_AUTHENTICATE_SERVER_SIGNATURE1_MAX_SIZE 100
#define LPA_INITIAL_AUTHENTICATE_EUICC_CIPKID_TO_BE_USED_MAX_SIZE 40    
#define LPA_INITIAL_AUTHENTICATE_SERVER_CERTIFICATE_MAX_SIZE 1536 
#define LPA_AUTHENTICATE_CLIENT_SMDP_CERTIFICATE_MAX_SIZE 1536
#define LPA_AUTHENTICATE_CLIENT_SMDP_SIGNATURE2_MAX_SIZE 100
#define LPA_AUTHENTICATE_CLIENT_SMDP_SIGNED2_MAX_SIZE 350
#define LPA_AUTHENTICATE_CLIENT_PROFILE_METADATA_MAX_SIZE 2048
#define LPA_GET_BOUND_PROFILE_MAX_SIZE	135168	// 132 Kb

///////////////////////////////////
// STRUCTURE PART
///////////////////////////////////
typedef struct {
	unsigned char *val;
	size_t len;
} data_s;

typedef struct LPA_SERVER_DATA {
	data_s _transactionId;
	data_s _serverSigned1;
	data_s _serverSignature1;
	data_s _euiccCiPKIdToBeUsed;
	data_s _serverCertificate;

	data_s _smdpCertificate;
	data_s _smdpSignature2;
	data_s _smdpSigned2;
	data_s _profileMetadata;

	data_s _boundProfilePackage;
} LPA_SERVER_DATA, *ptr_serverData;

typedef struct {
	// MatchingId + deviceInfoTLV
	RawDataObject * ptrAuthenticateServerResponse;
	RawDataObject * ptrAuthenticateServerResponse_Base64;
} AUTHENTICATE_SERVER_RESPONSE;

typedef struct {
	// UICC challenge or UICC info
	RawDataObject * ptrEUICC;
	RawDataObject * prtEUICC_Base64;
} LPA_GET_EUICC;

typedef struct {
	RawDataObject * ptrPrepareDownloadResponse;
	RawDataObject * ptrPrepareDownloadResponse_Base64;
} PREPARE_DOWNLOAD_RESPONSE;

typedef struct {
	bool hasResult;
	RawDataObject * ptrProfileInstallationResultTlv;
	RawDataObject * ptrProfileInstallationResultTlv_Base64;
} PROFILE_INSTALLATION_RESULT;

// PPR ASN1 bitstring values (PprIds object)
// Note: PPRUC means PprUpdateControl bit defined in PprIds ASN1 object and related to ES6 features
typedef enum
{
    LPA_PPRDEF_PPR2 = 0x0520,
    LPA_PPRDEF_PPR1_PPR2 = 0x0560,
    LPA_PPRDEF_PPRUC_PPR2 = 0x05A0,
    LPA_PPRDEF_PPRUC_PPR1_PPR2 = 0x05E0,
    LPA_PPRDEF_PPR1 = 0x0640,
    LPA_PPRDEF_PPRUC_PPR1 = 0x06C0,
    LPA_PPRDEF_PPRUC = 0x0780
}LPA_PPR_ASN1_BIT_STRING_VALUES;

// Fields extracted from profile Metadata for PPR check
#define LPA_GID_MAX_SIZE 10     // TBC if bigger size can be encountered. Not fixed in 31.102. Saw at 4 bytes in SGP.23 examples.

typedef struct
{
    unsigned int profilePPR;                            // Will store ASN1 coding of profile PPR. If value = 0 No PPR in profile
    bool hasPPR1;                                       // True if PPR1 is enabled
    bool hasPPR2;                                       // True if PPR2 is enabled
    unsigned char mccMnc[3];
    unsigned char gid1[LPA_GID_MAX_SIZE];
    size_t gid1Size;
    bool gid1Defined;                                   // Allow to know if field exists, even if length = 0 (Used in PPR conditions)
    unsigned char gid2[LPA_GID_MAX_SIZE];
    size_t gid2Size;
    bool gid2Defined;                                   // Allow to know if field exists, even if length = 0 (Used in PPR conditions)
    char profileName[LPA_PROFILE_NAME_MAX_SIZE + 2];    // Profile name that may be displayed to user. String coded. +1 byte for EOS, +1 byte for security
    unsigned char userCallBackType;                     // Set information(s) to display to user. Values masks defined in LPA_USER_CONSENT_TYPES enum. Each bit set an info.
    bool performCancelSession;                          // If true download is not allowed (PPR1 vs profile, RAT...) and Cancel Session will be performed
    unsigned int cancelSessionReason;                   // Values defined in LPA_CANCEL_SESSION_REASON enum. Value has no importance while performCancelSession is not set
}LPA_DOWNLOADED_PROFILE_DATA_FOR_PPR;

// Profile info structure for PPR management
#define LPA_PROFILE_PPR_MAX_SIZE 2

typedef struct
{
	unsigned char iccid[LPA_PROFILE_ICCID_BUFFER_MAX_SIZE];
	size_t iccidSize;

	unsigned char profileState[LPA_PROFILE_STATE_MAX_SIZE];
	size_t profileStateSize;

	unsigned char profileClass[LPA_PROFILE_CLASS_MAX_SIZE];
	size_t profileClassSize;

	unsigned char profilePolicyRules[LPA_PROFILE_PPR_MAX_SIZE];
	size_t profilePolicyRulesSize;
}LPA_PROFILE_INFO_FOR_PPR;

/*
Maximum profile info size retrieved from eUICC for PPR (Max length used):
                                            Tag/size bytes + max data lengths
Header tag E3                               5 bytes
iccid tag 5A                                2 + 10 = 12 bytes
profileState tag 9F70                       3 + 1 = 4 bytes
profileClass tag 95                         2 + 1 = 3 bytes
profilePolicyRules tag 99                   2 + 2 = 4 bytes

TOTAL -------------------------------------- 28 bytes - Set at 30 for rounding / security
*/
#define LPA_PROFILE_INFO_BUFFER_MAX_SIZE_FOR_PPR 30	// Size of profile raw data for PPR request

#define LPA_RAT_MAXIMUM_SIZE 1024   // Arbitrary fixed to 1024 bytes

// Structure used for PPR analysis, one for each PPR
typedef struct
{
    bool pprValidated;              // If true, means this PPR has been validated one time
    int matchLevelMCC_MNC;          // Match level for MCC / MNC digits: From 0 (Most generic) to 6 (Most accurate). Most accurate = Highest priority
    bool matchedGID1;               // If true an exact matching has been found for GID1 (Same or not defined). Has most priority than wildcard (Defined in rule with L = 0).
    bool matchedGID2;               // If true an exact matching has been found for GID2 (Same or not defined). Has most priority than wildcard (Defined in rule with L = 0).
    int matchLevelGID;              // Match level for GID: From 0 (No match, both accepted in rule with L = 0) to 2 (Both same value or not defined).
    bool userConsentRequired;       // If true user consent is required. Has most priority than no consent required
}LPA_PPR_RAT_ANALYSIS_FLAGS;

///////////////////////////////////
// FUNCTION PART
///////////////////////////////////
void lpaResetErrorCode();
LPA_API_ERROR lpaGetErrorCodeNoClear();
bool lpaIsError();
void lpaSetErrorCode(LPA_API_ERROR errorCode);
void lpaWriteErrorMessageOnLog(LPA_API_ERROR errorCode);

#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif // LPA_SDK__CORE_API_H
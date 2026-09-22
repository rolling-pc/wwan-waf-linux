/*
* Copyright 2018-2020 THALES group. All Rights Reserved.
*
* Project name: LPASDK.
* Platform : Window, Linux.
* Language : C/C++
*
* Except if otherwise stated in a NOTICE file provided by Thales together with the software, below conditions are applicable by default.
*
* This computer program includes confidential and proprietary information of THALES and is a trade secret of
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


//#define SAMPLE__GENERIC_MODEM

#define INPUT_SUBFOLDER "/Config"
#define OUTPUT_SUBFOLDER "/Logs"

#define PROFILE_STATE_DISABLED	0
#define PROFILE_STATE_ENABLED	1
#define PROFILE_CLASS_TEST			0
#define PROFILE_CLASS_PROVISIONING	1
#define PROFILE_CLASS_OPERATIONAL	2

#include <signal.h>
//#include <dirent.h>

#include "log.hpp"
#include "mbim.hpp"
#include "common.hpp"
#include "Sample_exe.h"

#ifdef SAMPLE__GENERIC_MODEM
#include <fcntl.h>
//#include <termios.h>
#include <errno.h>
#endif
#include <QThread>

using namespace afal;
using namespace afal::mbim;
using namespace afal::log;
using namespace afal::error;

static char lpa_deviceInfo[] = "A112800435417440A10A81030B000085030D0000"; //need to improve

/*
#ifdef SAMPLE__GENERIC_MODEM
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

static int fd;

bool Modem_OpenAndConfigurePort()
{
	struct termios options;

	// Open port
	fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY | O_NDELAY);
	//printf("Opening serial port - fd=%d \n",fd);
	if (fd < 0)
	{
		//printf("Error opening serial port\n");
		return(false);
	}

	// Configure port
	//printf("Configuring options...");
	bzero(&options, sizeof(options));
	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD ;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &options);
	//printf("done\n\n");
	return(true);
}

void Modem_WaitForModemIsDetected()
{
	bool ModemIsDetected;
	const long sleep_duration = 500000;	// 500ms
	int timeout = 40;			 	// => timeout = 20s

	ModemIsDetected = Modem_OpenAndConfigurePort();
	while( ModemIsDetected == false && timeout>0)
	{
		usleep(sleep_duration);
		printf(".\n");
		ModemIsDetected = Modem_OpenAndConfigurePort();
		timeout--;
	}

	if(ModemIsDetected == false)
	{
		printf("\nModem detection has failed.\n");
		exit (1);
	}

	// Close port
	close(fd);
}

void Modem_GetProductIdentificationInformation(unsigned char* recv)
{
	const char* ATcommand="ATI\r";
	const long sleep_duration = 50000;	// 50ms
	int timeout = 200;			 	// => timeout = 10s

	//Open and configure port - will have to be closed
	if( Modem_OpenAndConfigurePort() == false)
		return;

	printf("AT command: %s\n",ATcommand);

	// Send AT command
	if (write(fd, ATcommand, strlen(ATcommand)) < strlen(ATcommand))
	{
		printf("Write error - %s \n", strerror(errno));
		close(fd);
		return;
	}

	// Retrieve AT command response
	recv[0]='\0';
	int nbReceivedChars=0;
	int nbLoops = timeout;
	while (strstr(recv,"\nOK") == NULL && strstr(recv,"ERROR") == NULL && timeout>0)
	{
		usleep(sleep_duration);
		nbReceivedChars += read(fd,recv+nbReceivedChars,sizeof(recv));
		recv[nbReceivedChars]='\0';
		timeout--;
	}
	nbLoops -= timeout;
	printf("Received from modem after %d waiting loop(s):\n----------\n%s\n----------\n",nbLoops,recv);

	// Erase response if OK is not received
	if(strstr(recv,"\nOK") == NULL) recv[0]='\0';

	// Close port
	close(fd);
}

void Modem_WaitForModemReady()
{
	unsigned char ATcommandExecution[50];
	//const char* ATresultReady="+CPAS: 0";
	const char* ATresultReady="OK";
	const long sleep_duration = 500000;	// 500ms
	int timeout = 20;			 	// => timeout = 10s

	printf("\nWait for modem to be ready...\n");

	Modem_GetProductIdentificationInformation(ATcommandExecution);

	while (strstr(ATcommandExecution,ATresultReady) == NULL && timeout>0)
	{
		usleep(sleep_duration);
		Modem_GetProductIdentificationInformation(ATcommandExecution);
		timeout--;
	}

	printf("Wait 3s more...\n");
	usleep(3000000);
	printf("3s!\n");

}

void Modem_SendAPDU(unsigned char* apdu_cmd, unsigned char* apdu_resp)
{
	unsigned char ATcommand[1000];
	unsigned char recv[3500];
	char* apdu_length[4];
	const long sleep_duration = 50000;	// 50ms
	int timeout = 200;			 	// => timeout = 10s

	// Change APDU into AT command (AT+CSIM)
	// AT+CSIM=apdu_length,"apdu_cmd"\r
	sprintf(apdu_length, "%d", strlen(apdu_cmd));	// int to str conversion
	strcpy(ATcommand, "AT+CSIM=");					// add AT+CSIM prefix
	strcat(ATcommand, apdu_length);					// add apdu command length
	strcat(ATcommand, ",\""); 						// add comma and opening quote
	strcat(ATcommand, apdu_cmd); 					// add apdu command
	strcat(ATcommand, "\"\r"); 						// add closing quote and CR

	printf("AT command:\n  %s \n",ATcommand);

	// Send AT command
	if (write(fd, ATcommand, strlen(ATcommand)) < strlen(ATcommand))
	{
		printf("Write error - %s \n", strerror(errno));
		apdu_resp[0]='\0';
		return;
	}

	// Retrieve AT command response
	recv[0]='\0';
	int nbReceivedChars=0;
	int nbLoops = timeout;
	while (strstr(recv,"\nOK") == NULL && strstr(recv,"ERROR") == NULL && timeout>0)
	{
		usleep(sleep_duration);
		nbReceivedChars += read(fd,recv+nbReceivedChars,sizeof(recv));
		recv[nbReceivedChars]='\0';
		timeout--;
	}
	nbLoops -= timeout;
	printf("Received from modem after %d waiting loop(s):\n----------\n%s\n----------\n",nbLoops,recv);

	// Extract APDU response
	apdu_resp[0]='\0';
	if(strstr(recv,"\nOK"))
	{
		unsigned char* response_part = strstr(recv, "\n+CSIM:");
		unsigned char* first_quote = strstr(response_part, "\"");
		unsigned char* first_char = first_quote+1;
		unsigned char* second_quote = strstr(first_char, "\"");
		if(second_quote != NULL)
		{
			second_quote[0] = '\0';
			strcpy(apdu_resp,first_char);
		}
	}
}
#endif //SAMPLE__GENERIC_MODEM
*/

int SampleLPA_Initialize(char *logpath, unsigned int type)
{
    char ExePath[256]            = {0};
    bool res                     = false;

    LOG_DEBUG("enter %s!\n", __func__);

    if (logpath == nullptr || strlen(logpath) < 1) {
        LOG_ERROR("Invalid pointer!\n");
        return ERR;
    }

    // *ExePath = getcwd(ExePath,256);
    strncat(ExePath, logpath, 255);
    LOG_DEBUG("ExePath = %s!\n",ExePath);

    res = lpaInitialize(ExePath);
    LOG_DEBUG("=> Result for lpaInitialize is %s\n", (res ? "True" : "False"));

    if (res && lpaIsInitialized())
    {
        LOG_DEBUG("=> LPA lib is initialized\n");

        // set log level
        lpaCoreSetLogLevel(SDK_LOG_LEVEL_VERBOSE);

        // set device info
        lpaSetConfigParameter("deviceInfoTlv", LPA_PARAMETER_TYPE_STRING, (const void*)lpa_deviceInfo);

        return OK;
    }
    else
    {
        LOG_ERROR("=> LPA lib is not initialized\n");
        return ERR;
    }
    // add logic to use new AT preset.
}

int SampleLPA_DeInitialize(void)
{
    bool res                     = false;

    LOG_DEBUG("enter %s!\n", __func__);

    res = lpaUninitialize();
    LOG_DEBUG("=> Result for lpaInitialize is %s\n", (res ? "True" : "False"));

    if (res && !lpaIsInitialized())
    {
        LOG_DEBUG("=> LPA lib is deinitialized!\n");
        return OK;
    }
    else
    {
        LOG_ERROR("=> LPA lib is still initialized!\n");
        return ERR;
    }
}

int SampleLPA_GetReader()
{
    size_t                        countReader    = 0;
    LPA_SE_MEDIA_READER_NAME_INFO readerInfo[16] = {0};
    bool                          res            = false;

    LOG_DEBUG("enter %s!\n", __func__);

    res = lpaGetReaderList( readerInfo, 16, &countReader);
    LOG_DEBUG("lpaGetReaderList API returned %s\n", (res ? "true" : "false"));
    if (res) {
        if (countReader > 0) {
            LOG_DEBUG("    <%d> reader(s) detected : \n", countReader);
            for (size_t index = 0; index < countReader; index ++) {
                LOG_DEBUG("    - <%s>\n", readerInfo[index].readerName);
                lpaSetConfigParameter("readerName",LPA_PARAMETER_TYPE_STRING,readerInfo[index].readerName);
            }
            // The default reader is set to the last enumerated one, we use readerInfo[0].readerName for storing it.
            if( lpaGetConfigParameter("readerName",LPA_PARAMETER_TYPE_STRING,readerInfo[0].readerName,LPA_CFG_READER_NAME_MAX_SIZE) )
            {
                LOG_DEBUG("\nDefault reader is set to: %s\n\n",readerInfo[0].readerName);
                return OK;
            }
            else {
                LOG_ERROR("\nError while retrieving parameter \"readerName\"\n\n");
                return ERR;
            }
        }
        else {
            LOG_ERROR("No reader detected\n\n");
            return ERR;
        }
    }
    else {
        LOG_ERROR("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
        return ERR;
    }
}

int SampleLPA_GetEID(char *EID)
{
    LPA_GET_EID getEID;
    bool res = false;

    LOG_DEBUG("enter %s!\n", __func__);

    if (!EID) {
        LOG_ERROR("Invalid pointer!\n");
        return ERR;
    }
    res = lpaGetEID(&getEID);
    LOG_DEBUG("lpaGetEID API returned %s\n", (res ? "true" : "false"));
    if(res)
    {
        for (size_t index = 0; index < getEID.EID_DataSize; index ++)
        {
            char temp[3] = {0};
            snprintf(temp, 3, "%02X", getEID.EID_Data[index]);
            strncat(EID, temp, strlen(EID) - 1);
        }
        LOG_DEBUG("EID = %s\n", EID);
        return OK;
    }
    else
    {
        LOG_ERROR("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
        strncat(EID, "ERROR", 32);
        return ERR;
    }
}

int SampleLPA_GetProfileInfoNum(int *num)
{
    bool res = false;

    LOG_DEBUG("enter %s!\n", __func__);

    res = lpaGetProfilesNumber((size_t *)num);
    if (res) {
        return OK;
    }
    else {
        *num = -1;
        return ERR;
    }
}

/*
void SampleLPA_GetSMDPAddress()
{
    ADDRESS_DATA AddressData;
    bool res = lpaGetSMDPAddress(&AddressData);
    printf("lpaGetSMDPAddress API returned %s\n", (res ? "true" : "false"));
    if(res)
    {
        printf("   SMDP Address = ");
        for (size_t index = 0; index < AddressData.address_DataSize; index ++)
        {
            printf("%02X",AddressData.address_Data[index]);
        }

        printf("\n");
        AddressData.address_Data[AddressData.address_DataSize]='\0';
        printf("   SMDP Address = %s\n",AddressData.address_Data);
    }
    else
    {
        printf("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    }
}


void SampleLPA_GetSMDSAddress()
{
    ADDRESS_DATA AddressData;
    bool res = lpaGetSMDSAddress(&AddressData);
    printf("lpaGetSMDSAddress API returned %s\n", (res ? "true" : "false"));
    if(res)
    {
        printf("   SMDS Address = ");
        for (size_t index = 0; index < AddressData.address_DataSize; index ++)
        {
            printf("%02X",AddressData.address_Data[index]);
        }
        printf("\n");
        AddressData.address_Data[AddressData.address_DataSize]='\0';
        printf("   SMDS Address = %s\n",AddressData.address_Data);
    }
    else
    {
        printf("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    }
}


void SampleLPA_SetCurlParameters()
{
    bool res;
    bool ParamValue=false;
    printf("Setting CURL_SSL_VERIFYPEER to false (this operation is supposed to fail when SDK is not built with LPA_SDK__USING_EX_API option)...\n");
    res = lpaSetConfigParameter("CURL_SSL_VERIFYPEER",LPA_PARAMETER_TYPE_BOOL,&ParamValue);
    printf("   Operation %s \n", (res ? "is successful:" : "has failed."));
    res = lpaGetConfigParameter("CURL_SSL_VERIFYPEER",LPA_PARAMETER_TYPE_BOOL,&ParamValue,1);
    if(res) printf("   CURL_SSL_VERIFYPEER set to %s\n", (ParamValue ? "true" : "false"));
}
*/
/*
SampleLPA_SetParameter_CertPath()
{
    // Instead of setting the CA certificate path in the LPA, it is actually adviced to install the CA certificates in the certificates store
    // On Linux we are completing /etc/ssl/certs/ca-certificates.crt with the content of both GSMA_CE_TEST_CI.crt and GSMA_CE_LIVE_CI.crt
    printf("\nPlease ensure the CA certificates (GSMA_CE_LIVE_CI, GSMA_CE_TEST_CI,...) are installed.\n");
    printf("On Debian/Ubuntu, this could consist in :\n");
    printf("\t1.Copying the .crt files in /usr/local/share/ca-certificates/\n");
    printf("\t2.Executing update-ca-certificates -f\n\n");


    bool res;
    char ParamValue[100];
    // strcpy(ParamValue,"CI-cert_NIST.crt");
    // printf("Setting CertPath to %s...\n",ParamValue);
    // res = lpaSetConfigParameter("CertPath",LPA_PARAMETER_TYPE_STRING,ParamValue);
    // printf("   Operation %s \n", (res ? "is successful:" : "has failed."));
    res= lpaGetConfigParameter("CertPath",LPA_PARAMETER_TYPE_STRING,ParamValue,100);
    if(res) printf("   CertPath set to %s\n\n", ParamValue);
}

SampleLPA_SetParameter_deviceInfoTlv()
{
    bool res;
    char ParamValue[131];
    strcpy(ParamValue,"A13F800411223344A12D80030200008103000000820300000083030000008403000000850300000086030000008703000000880302000082083537458936276597");
    printf("Setting deviceInfoTlv to %s...\n",ParamValue);
    res = lpaSetConfigParameter("deviceInfoTlv",LPA_PARAMETER_TYPE_STRING,ParamValue);
    printf("   Operation %s \n", (res ? "is successful:" : "has failed."));
    res= lpaGetConfigParameter("deviceInfoTlv",LPA_PARAMETER_TYPE_STRING,ParamValue,131);
    if(res) printf("   deviceInfoTlv set to %s\n", ParamValue);
}
*/
void SampleLPA_SetRefreshParameter(bool inputParamValue)
{
    bool res = false;
    bool ParamValue = false;

    res = lpaGetConfigParameter("profileRefreshFlag",LPA_PARAMETER_TYPE_BOOL,&ParamValue,1);
    if (res) {
        LOG_DEBUG("profileRefreshFlag is currently set to %s\n", (ParamValue ? "true" : "false"));
    }
    LOG_DEBUG("Setting profileRefreshFlag to %s...\n",(inputParamValue ? "true" : "false"));

    res = lpaSetConfigParameter("profileRefreshFlag",LPA_PARAMETER_TYPE_BOOL,&inputParamValue);
    LOG_DEBUG("   Operation %s \n", (res ? "is successful:" : "has failed."));

    res = lpaGetConfigParameter("profileRefreshFlag",LPA_PARAMETER_TYPE_BOOL,&ParamValue,1);
    if (res) {
        LOG_DEBUG("   profileRefreshFlag set to %s\n", (ParamValue ? "true" : "false"));
    }
    return;
}

void lpaEventTextNotificationFunction(const void * ptrAppParameter, size_t eventType, const char* ptrText)
{
    if(ptrText != NULL) {
        LOG_DEBUG("Evt progress text = %s\n", ptrText);
    }
    return;
}

void lpaEventValueNotificationFunction(const void * ptrAppParameter, size_t eventType, size_t valueMin, size_t currentValue, size_t valueMax)
{
    LOG_DEBUG("Evt progress value = %zu %zu %zu %zu\n",eventType,valueMin,currentValue,valueMax);
}

void lpaEventExecutionErrorNotificationFunction(const void* ptrAppParameter, const LPA_EVENT_EXECUTION_ERROR_INFO* ptrEventExecutionErrorInfo)
{
    LOG_DEBUG("Execution error:\n");
    //printf("\t executionErrorType: %d\n",ptrEventExecutionErrorInfo->executionErrorType);
    LOG_DEBUG("\t Error type: ");
    switch(ptrEventExecutionErrorInfo->executionErrorType)
    {
        case LPA_EVENT_EXECUTION_SERVER_ERROR_TYPE:
            LOG_DEBUG("Error sent by server\n");
            break;
        case LPA_EVENT_EXECUTION_HTTP_ERROR_TYPE:
            LOG_DEBUG("HTTP error during communication with server\n");
            break;
        case LPA_EVENT_EXECUTION_CURL_ERROR_TYPE:
            LOG_DEBUG("Curl error\n");
            break;
        case LPA_EVENT_EXECUTION_SEMEDIA_DRIVER_ERROR_TYPE:
            LOG_DEBUG("SEMEDIA_DRIVER error\n");
            break;
    }
    //printf("\t detailErrorMask: 0x%04X\n",ptrEventExecutionErrorInfo->detailErrorMask);
    if(ptrEventExecutionErrorInfo->detailErrorMask & LPA_EVENT_EXECUTION_ERROR_SUBJECT_CODE_MASK)
        LOG_DEBUG("\t Error Subject Code: %s\n",ptrEventExecutionErrorInfo->ptrErrorSubjectCode);
    if(ptrEventExecutionErrorInfo->detailErrorMask & LPA_EVENT_EXECUTION_ERROR_REASON_CODE_MASK)
        LOG_DEBUG("\t Error Reason Code: %s\n",ptrEventExecutionErrorInfo->ptrErrorReasonCode);
    if(ptrEventExecutionErrorInfo->detailErrorMask & LPA_EVENT_EXECUTION_ERROR_EXTRA_INFO_MASK)
        LOG_DEBUG("\t Error Extra Info: %s\n",ptrEventExecutionErrorInfo->ptrErrorExtraInfo);
}
/*
bool lpaEventRequestConfirmationCodeFunction(const void * ptrAppParameter, LPA_REQUEST_CONFIRMATION_CODE* ptrRequestConfirmationCode)
{
    LOG_DEBUG("\n\tThis is the lpaEventCallback._lpaEventRequestConfirmationCode function!\n");
    LOG_DEBUG("\tPlease enter confirmation code : ");
    scanf("%s",ptrRequestConfirmationCode->confirmationCode);
    //printf("\n\tRecorded confirmation code is : ");
    //for (size_t index = 0; index < LPA_CONFIRMATION_CODE_MAX_SIZE ; index++)
    //	printf("0x%02X ",ptrRequestConfirmationCode->confirmationCode[index]);
    printf("\n\tRecorded confirmation code is : %s",ptrRequestConfirmationCode->confirmationCode);
    printf("\n\n");
    if(ptrRequestConfirmationCode->confirmationCode[0] == 0x00) ptrRequestConfirmationCode->reasonCodeNoCC = LPA_CANCEL_SESSION_POSTPONED;
    return(true);
}

bool lpaEventRequestUserConsentFunction(const void * ptrAppParameter, LPA_REQUEST_USER_CONSENT_FOR_LOADING_PROFILE* ptrRequestUserConsentForLoadingProfile)
{
    char choice[10];

    printf("\n\tThis is the lpaEventCallback._lpaEventRequestUserConsentForLoadingProfile function!\n");
    if(ptrRequestUserConsentForLoadingProfile->userCallBackType == LPA_USR_CONSENT_DISABLED)
    {
        printf("\tUser consent is disabled\n");
    }
    else
    {
        if((ptrRequestUserConsentForLoadingProfile->userCallBackType & LPA_USR_CONSENT_PPR1) == LPA_USR_CONSENT_PPR1)
            printf("\tUser consent: LPA_USR_CONSENT_PPR1\n");
        if((ptrRequestUserConsentForLoadingProfile->userCallBackType & LPA_USR_CONSENT_PPR2) == LPA_USR_CONSENT_PPR2)
            printf("\tUser consent: LPA_USR_CONSENT_PPR2\n");
        if((ptrRequestUserConsentForLoadingProfile->userCallBackType & LPA_USR_CONSENT_PROFILE_WITH_PPR1_ENABLED_PRESENT) == LPA_USR_CONSENT_PROFILE_WITH_PPR1_ENABLED_PRESENT)
            printf("\tUser consent: LPA_USR_CONSENT_PROFILE_WITH_PPR1_ENABLED_PRESENT\n");
    }
    printf("\tDo you confirm that you want to laod new profile \"%s\" ?\n",ptrRequestUserConsentForLoadingProfile->profileName);
    choice[0]='\0';
    for(int timeout = 10; choice[0]!='y' && choice[0]!='Y' && choice[0]!='n' && choice[0]!='N' && choice[0]!='l' && choice[0]!='L' && timeout>0 ; timeout--)
    {
        printf("\tPlease reply with 'y' (yes) or 'n' (no) or 'l' (later): ");
        scanf("%s",choice);
        choice[1]='\0';
    }
    switch(choice[0])
    {
        case 'y':
        case 'Y':
            ptrRequestUserConsentForLoadingProfile->downloadAllowed = true;
            break;

        case 'n':
        case 'N':
            ptrRequestUserConsentForLoadingProfile->downloadAllowed = false;
            ptrRequestUserConsentForLoadingProfile->cancelSessionReason = LPA_CANCEL_SESSION_END_USER_REJECTION;
            break;

        case 'l':
        case 'L':
            ptrRequestUserConsentForLoadingProfile->downloadAllowed = false;
            ptrRequestUserConsentForLoadingProfile->cancelSessionReason = LPA_CANCEL_SESSION_POSTPONED;
            break;

        default:
            ptrRequestUserConsentForLoadingProfile->downloadAllowed = false;
            ptrRequestUserConsentForLoadingProfile->cancelSessionReason = LPA_CANCEL_SESSION_TIME_OUT;
            break;
    }
    if(ptrRequestUserConsentForLoadingProfile->downloadAllowed == true)
    {
        printf("\t -> Download is allowed\n\n");
    }
    else
    {
        printf("\t -> Download is not allowed\n");
        printf("\t -> Cancel Session Reason = ");
        switch(ptrRequestUserConsentForLoadingProfile->cancelSessionReason)
        {
            case LPA_CANCEL_SESSION_END_USER_REJECTION:
                printf("End User Rejection\n\n");
                break;

            case LPA_CANCEL_SESSION_POSTPONED:
                printf("Postponed\n\n");
                break;

            case LPA_CANCEL_SESSION_TIME_OUT:
                printf("Timeout\n\n");
                break;

            default:
                printf("This should never be displayed !\n\n");
                break;
        }
    }
    return(true);
}

bool SampleLPA_DownloadProfile(const char* ActivationCodeStr)
{
    LPA_DOWNLOAD_PROFILE_RESULT downloadProfileResult;
    LPA_EventCallback lpaEventCallback;
    char ActivationCode[255];

    strcpy(ActivationCode,ActivationCodeStr);

    lpaEventCallback._appParameter = NULL;
    lpaEventCallback._lpaEventProgressText = lpaEventTextNotificationFunction;
    lpaEventCallback._lpaEventProgressValue = lpaEventValueNotificationFunction;
    lpaEventCallback._lpaEventExecutionError = lpaEventExecutionErrorNotificationFunction;
    lpaEventCallback._lpaEventRequestConfirmationCode = lpaEventRequestConfirmationCodeFunction;
    lpaEventCallback._lpaEventRequestUserConsentForLoadingProfile= lpaEventRequestUserConsentFunction;


    printf("\nStarting Profile Download\n");
    printf("ActivationCode = %s\n",ActivationCode);
    bool res = lpaDownloadProfile(ActivationCode, &lpaEventCallback, &downloadProfileResult);
    printf("lpaDownloadProfile API returned %s\n", (res ? "true" : "false"));

    if(!res)
    {
        printf("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    }

    printf("Nb of installed profile(s) during operation : %zu\n",downloadProfileResult.countProfileInstalled);

    return(res);
}

bool SampleLPA_DownloadProfile_SMDS()
{
    LPA_DOWNLOAD_PROFILE_RESULT downloadProfileResult;
    LPA_EventCallback lpaEventCallback;

    lpaEventCallback._appParameter = NULL;
    lpaEventCallback._lpaEventProgressText = lpaEventTextNotificationFunction;
    lpaEventCallback._lpaEventProgressValue = lpaEventValueNotificationFunction;
    lpaEventCallback._lpaEventExecutionError = lpaEventExecutionErrorNotificationFunction;
    lpaEventCallback._lpaEventRequestConfirmationCode = lpaEventRequestConfirmationCodeFunction;

    printf("\nStarting Profile Download using the discovery mechanism (SM-DS)\n");
    bool res = lpaDownloadProfileWithSMDSAddress(&lpaEventCallback, &downloadProfileResult);
    printf("lpaDownloadProfileWithSMDSAddress API returned %s\n", (res ? "true" : "false"));

    if(!res)
    {
        printf("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    }

    printf("Nb of installed profile(s) during operation : %zu out of %zu from list retrieved on SM-DS\n",downloadProfileResult.countProfileInstalled,downloadProfileResult.countProfileTotal);

    return(res);
}

bool SampleLPA_DownloadProfile_defaultSMDP()
{
    LPA_DOWNLOAD_PROFILE_RESULT downloadProfileResult;
    LPA_EventCallback lpaEventCallback;

    lpaEventCallback._appParameter = NULL;
    lpaEventCallback._lpaEventProgressText = lpaEventTextNotificationFunction;
    lpaEventCallback._lpaEventProgressValue = lpaEventValueNotificationFunction;
    lpaEventCallback._lpaEventExecutionError = lpaEventExecutionErrorNotificationFunction;
    lpaEventCallback._lpaEventRequestConfirmationCode = lpaEventRequestConfirmationCodeFunction;

    printf("\nStarting Profile Download using the default SM-DP+\n");
    bool res = lpaDownloadProfileWithDefaultSMDPAddress(&lpaEventCallback, &downloadProfileResult);
    printf("lpaDownloadProfileWithDefaultSMDPAddress API returned %s\n", (res ? "true" : "false"));

    if(!res)
    {
        printf("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    }

    printf("Nb of installed profile(s) during operation : %zu\n",downloadProfileResult.countProfileInstalled);

    return(res);
}
*/
bool SampleLPA_GetProfilesInfo(LPA_GET_PROFILES_INFO* getProfilesInfo)
{
    size_t NbOfProfilesForMemAlloc;
    size_t ActualNbOfProfiles;
    bool res_1;
    bool res_2;

    res_1 = lpaGetProfilesNumber( &ActualNbOfProfiles );
    printf("\nlpaGetProfilesNumber API returned %s\n", (res_1 ? "true" : "false"));

    res_2 = res_1;

    if(res_1)
    {
        getProfilesInfo->countProfileInfo = 0;
        getProfilesInfo->numberProfileInfoFound = ActualNbOfProfiles;

        // Try to allocate memory for Nb of profiles = ActualNbOfProfiles (or less if not possible)
        NbOfProfilesForMemAlloc = ActualNbOfProfiles;
        if(getProfilesInfo->profileInfoList != NULL)
        {
            free(getProfilesInfo->profileInfoList);
            getProfilesInfo->profileInfoList = NULL;
        }
        while(getProfilesInfo->profileInfoList == NULL && NbOfProfilesForMemAlloc > 0)
        {
            getProfilesInfo->profileInfoList = (unsigned char*)malloc(sizeof(LPA_PROFILE_INFO)*NbOfProfilesForMemAlloc);
            getProfilesInfo->maxNumberProfileInfo = NbOfProfilesForMemAlloc;
            NbOfProfilesForMemAlloc--;
        }

        // Call lpaGetProfilesInfo only if memory has been allocated
        if(getProfilesInfo->profileInfoList != NULL)
        {
            res_2 = lpaGetProfilesInfo(getProfilesInfo);
            LOG_DEBUG("lpaGetrofilesInfo API returned %s\n", (res_2 ? "true" : "false"));
        }
    }

    if(res_2)
    {
        LOG_DEBUG("\tcountProfileInfo= %zu\n",getProfilesInfo->countProfileInfo);
        LOG_DEBUG("\tnumberProfileInfoFound= %zu\n",getProfilesInfo->numberProfileInfoFound);
    }
    else
    {
        getProfilesInfo->countProfileInfo = 0;
        LOG_ERROR("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    }

    LOG_DEBUG("\n");
    return(res_2);
}
/*
void SampleLPA_DisplayProfilesInfoFull(LPA_GET_PROFILES_INFO getProfilesInfo)
{
    LPA_PROFILE_INFO *profileData = getProfilesInfo.profileInfoList;

    for (size_t indexProfile = 0; indexProfile < getProfilesInfo.countProfileInfo; indexProfile ++)
    {
        printf("Profile %zu :\n",indexProfile);
        printf("\tProfile[%zu].rawDataSize = %zu\n",indexProfile, profileData[indexProfile].rawDataSize);
        printf("\tProfile[%zu].rawData = ",indexProfile);
        for (size_t indexRaw = 0; indexRaw < profileData[indexProfile].rawDataSize; indexRaw++)
            printf("%02X",profileData[indexProfile].rawData[indexRaw]);
        printf("\n");

        printf("\tProfile[%zu].iccidSize = %zu\n",indexProfile, profileData[indexProfile].iccidSize);
        printf("\tProfile[%zu].iccid = ",indexProfile);
        for (size_t indexIccid = 0; indexIccid < profileData[indexProfile].iccidSize; indexIccid++)
            printf("%02X",profileData[indexProfile].iccid[indexIccid]);
        printf("\n");

        printf("\tProfile[%zu].profileStateSize = %zu\n",indexProfile, profileData[indexProfile].profileStateSize);
        printf("\tProfile[%zu].profileState = ",indexProfile);
        for (size_t indexState = 0; indexState < profileData[indexProfile].profileStateSize; indexState++)
            printf("%02X",profileData[indexProfile].profileState[indexState]);
        printf("\n");

        printf("\tProfile[%zu].serviceProviderNameSize = %zu\n",indexProfile, profileData[indexProfile].serviceProviderNameSize);
        printf("\tProfile[%zu].serviceProviderName = ",indexProfile);
        for (size_t indexSPN = 0; indexSPN < profileData[indexProfile].serviceProviderNameSize; indexSPN++)
            printf("%02X",profileData[indexProfile].serviceProviderName[indexSPN]);
        printf("\n\t => SPN = ");
        for (size_t indexSPN = 0; indexSPN < profileData[indexProfile].serviceProviderNameSize; indexSPN++)
            printf("%c",profileData[indexProfile].serviceProviderName[indexSPN]);
        printf("\n");

        printf("\tProfile[%zu].profileNameSize = %zu\n",indexProfile, profileData[indexProfile].profileNameSize);
        printf("\tprofileInfoList[%zu].profileName = ",indexProfile);
        for (size_t indexName = 0; indexName < profileData[indexProfile].profileNameSize; indexName++)
            printf("%02X",profileData[indexProfile].profileName[indexName]);
        printf("\n\t => Profile Name = ");
        for (size_t indexName = 0; indexName < profileData[indexProfile].profileNameSize; indexName++)
            printf("%c",profileData[indexProfile].profileName[indexName]);
        printf("\n");

        printf("\tProfile[%zu].profileClassSize = %zu\n",indexProfile, profileData[indexProfile].profileClassSize);
        printf("\tProfile[%zu].profileClass = ",indexProfile);
        for (size_t indexClass = 0; indexClass < profileData[indexProfile].profileClassSize; indexClass++)
            printf("%02X",profileData[indexProfile].profileClass[indexClass]);
        printf("\n");

        printf("\tProfile[%zu].profileNicknameSize = %zu\n",indexProfile, profileData[indexProfile].profileNicknameSize);
        printf("\tProfile[%zu].profileNickname = ",indexProfile);
        for (size_t indexNickname = 0; indexNickname < profileData[indexProfile].profileNicknameSize; indexNickname++)
            printf("%02X",profileData[indexProfile].profileNickname[indexNickname]);
        printf("\n\t => Nickname = ");
        for (size_t indexNickname = 0; indexNickname < profileData[indexProfile].profileNicknameSize; indexNickname++)
            printf("%c",profileData[indexProfile].profileNickname[indexNickname]);
        printf("\n");

        printf("\tProfile[%zu].profileIconSize = %zu\n",indexProfile, profileData[indexProfile].profileIconSize);
        printf("\tProfile[%zu].profileIcon = ",indexProfile);
        for (size_t indexIcon = 0; indexIcon < profileData[indexProfile].profileIconSize; indexIcon++)
            printf("%02X",profileData[indexProfile].profileIcon[indexIcon]);
        printf("\n");

        printf("\tProfile[%zu].profileIconTypeSize = %zu\n",indexProfile, profileData[indexProfile].profileIconTypeSize);
        printf("\tProfile[%zu].profileIconType = ",indexProfile);
        for (size_t indexIconType = 0; indexIconType < profileData[indexProfile].profileIconTypeSize; indexIconType++)
            printf("%02X",profileData[indexProfile].profileIconType[indexIconType]);
        printf("\n");
    }
    printf("\n");
}

void SampleLPA_DisplayProfilesInfoSimplified(LPA_GET_PROFILES_INFO getProfilesInfo)
{
    LPA_PROFILE_INFO *profileData = getProfilesInfo.profileInfoList;

    for (size_t indexProfile = 0; indexProfile < getProfilesInfo.countProfileInfo; indexProfile ++)
    {
        printf("Profile %zu :\n",indexProfile);
        printf("\ticcid = ",indexProfile);
        for (size_t indexIccid = 0; indexIccid < profileData[indexProfile].iccidSize; indexIccid++)
            printf("%02X",profileData[indexProfile].iccid[indexIccid]);
        printf("\n");

        printf("\tprofileState = ",indexProfile);
        switch(profileData[indexProfile].profileState[0])
        {
            case PROFILE_STATE_DISABLED:
                printf("disabled\n");
                break;
            case PROFILE_STATE_ENABLED:
                printf("enabled\n");
                break;
            default:
                printf("UNEXPECTED VALUE\n");
                break;
        }

        printf("\tprofileName = ");
        for (size_t indexName = 0; indexName < profileData[indexProfile].profileNameSize; indexName++)
            printf("%c",profileData[indexProfile].profileName[indexName]);
        printf("\n");

        printf("\tprofileNickname = ");
        for (size_t indexNickname = 0; indexNickname < profileData[indexProfile].profileNicknameSize; indexNickname++)
            printf("%c",profileData[indexProfile].profileNickname[indexNickname]);
        printf("\n");

        printf("\tprofileClass = ",indexProfile);
        switch(profileData[indexProfile].profileClass[0])
        {
            case PROFILE_CLASS_TEST:
                printf("test\n");
                break;
            case PROFILE_CLASS_PROVISIONING:
                printf("provisioning\n");
                break;
            case PROFILE_CLASS_OPERATIONAL:
                printf("operational\n");
                break;
            default:
                printf("UNEXPECTED VALUE\n");
                break;
        }
    }
    printf("\n");
}
*/
int SampleLPA_GetOneProfileInfo(int *num) {
    LPA_PROFILE_INFO *ProfileData = NULL;
    LPA_GET_PROFILES_INFO getProfilesInfo;
    int ret = ERR;

    LOG_DEBUG("enter %s!\n", __func__);

//    if (SampleLPA_GetProfilesInfo(&getProfilesInfo)) {
//        ProfileData = getProfilesInfo.profileInfoList;
//        SampleLPA_DisplayProfilesInfoFull(getProfilesInfo);
//        SampleLPA_DisplayProfilesInfoSimplified(getProfilesInfo);
//    }
    return ret;
}

void SampleLPA_GetIccidStr(unsigned char* IccidStr, unsigned char* ProfileIdByteArray, size_t ProfileIdByteArraySize)
{
    unsigned char digit_left;
    unsigned char digit_right;
    size_t indexByte;

    for (indexByte = 0; indexByte < ProfileIdByteArraySize; indexByte ++)
    {
        digit_left = (ProfileIdByteArray[indexByte] & 0xF0)>>4;
        digit_right = (ProfileIdByteArray[indexByte] & 0x0F);
        IccidStr[2*indexByte] = digit_left == 0x0F ? 'F' : '0' + digit_left;
        IccidStr[2*indexByte+1] = digit_right == 0x0F ? 'F' : '0' + digit_right;
    }
    IccidStr[2*indexByte]='\0';

}

bool SampleLPA_EnableProfile(unsigned char* ProfileIdByteArray, size_t ProfileIdByteArraySize)
{
    // step2 enable profile
    bool ret = lpaEnableProfileByIccid(ProfileIdByteArray, ProfileIdByteArraySize);
    LPA_API_ERROR errorCode = lpaGetErrorCode();
    LOG_DEBUG("lpaEnableProfileByIccid return %d, errstr(%s)", ret,
              lpaGetErrorCodeDescription(errorCode));
    if (false == ret)
    {
        if (LPA_ERROR_LOCAL_PROFILE_NOT_IN_DISABLE_STATE == errorCode)
        {
            LOG_DEBUG("lpaEnableProfileByIccid return "
                      "LPA_ERROR_LOCAL_PROFILE_NOT_IN_DISABLE_STATE, ret = TRUE");
            ret = true;
        }
    }
    return ret;
}

bool SampleLPA_DisableProfile(unsigned char* ProfileIdByteArray, size_t ProfileIdByteArraySize)
{
    bool ret = lpaDisableProfileByIccid(ProfileIdByteArray, ProfileIdByteArraySize);
    LPA_API_ERROR errorCode = lpaGetErrorCode();
    LOG_DEBUG("lpaDisableProfileByIccid return %d, errstr(%s)", ret,
              lpaGetErrorCodeDescription(errorCode));
    if (false == ret)
    {
        if (LPA_ERROR_LOCAL_PROFILE_NOT_IN_ENABLE_STATE == errorCode)
        {
            LOG_DEBUG("lpaDisableProfileByIccid return "
                      "LPA_ERROR_LOCAL_PROFILE_NOT_IN_ENABLE_STATE, ret = TRUE");
            ret = true;
        }
    }
    return ret;
}


bool SampleLPA_DeleteProfile(unsigned char* ProfileIdByteArray, size_t ProfileIdByteArraySize)
{
    // step1 disable profile
    bool ret = lpaDisableProfileByIccid(ProfileIdByteArray, ProfileIdByteArraySize);
    LPA_API_ERROR errorCode = lpaGetErrorCode();
    LOG_DEBUG("lpaDisableProfileByIccid return %d, errstr(%s)", ret,
              lpaGetErrorCodeDescription(errorCode));
    if ((false == ret) &&
        (LPA_ERROR_LOCAL_PROFILE_NOT_IN_ENABLE_STATE == errorCode))
    {
        LOG_DEBUG("lpaDisableProfileByIccid return "
                  "LPA_ERROR_LOCAL_PROFILE_NOT_IN_ENABLE_STATE, ret = TRUE");
        ret = true;
    }
    if ((false == ret) &&
        (LPA_ERROR_LOCAL_PROFILE_INVALID_DATA_EXCHANGE == errorCode))
    {
        ret = true;
    }

    // step2 delete profile
    if (ret)
    {
        QThread::sleep(2); // G&D esim reset wait 250MS, then here use 2000MS
        ret = lpaDeleteProfileByIccid(ProfileIdByteArray, ProfileIdByteArraySize);
        LOG_DEBUG("lpaDeleteProfileByIccid return %d, errstr(%s)", ret,
                  lpaGetErrorCodeDescription(lpaGetErrorCode()));
        QThread::sleep(2); // G&D esim reset wait 250MS, then here use 2000MS
    }

    return ret;
}

bool SampleLPA_DownloadProfile(const char* ActivationCodeStr, size_t ActivationCodeStrLen, bool flag)
{
    LPA_DOWNLOAD_PROFILE_RESULT download_profile_result = {0};
    LPA_EventCallback lpa_event_callback = {0};
    char activation_code[256] = {0};

    memset(activation_code, 0, sizeof(activation_code));
    strncpy(activation_code, ActivationCodeStr, ActivationCodeStrLen);

    lpa_event_callback._appParameter = NULL;
    lpa_event_callback._lpaEventProgressText = NULL;
    lpa_event_callback._lpaEventProgressValue = NULL;
    lpa_event_callback._lpaEventExecutionError = NULL;
    lpa_event_callback._lpaEventRequestConfirmationCode = NULL;
    lpa_event_callback._lpaEventRequestUserConsentForLoadingProfile = NULL;

    LOG_DEBUG("\nStarting Profile Download\n");
    LOG_DEBUG("ActivationCode = %s\n", activation_code);
    bool res = lpaDownloadProfile(activation_code, &lpa_event_callback,
                                  &download_profile_result);
    LOG_DEBUG("lpaDownloadProfile API returned %s\n", (res ? "true" : "false"));

    if (!res)
    {
        LOG_DEBUG("=> Error code: %s\n",
                  lpaGetErrorCodeDescription(lpaGetErrorCode()));
        if (flag && download_profile_result.countProfileInstalled > 0)
        {
            res = true;
            LOG_DEBUG("already installed profile(%llu), think that download "
                      "profile success \n",
                      download_profile_result.countProfileInstalled);
        }
    }

    LOG_DEBUG("Nb of installed profile(s) during operation : %zu\n",
              download_profile_result.countProfileInstalled);
    return res;
}


void SampleLPA_SetNickname(unsigned char* ProfileIdByteArray,
                           size_t ProfileIdByteArraySize,
                           unsigned char* NickName,
                           size_t NickNameSize)
{
    bool res = lpaSetNicknameByIccid(ProfileIdByteArray, ProfileIdByteArraySize,
                                     NickName, NickNameSize);
    LOG_DEBUG("Call to lpaSetNicknameByIccid() %s.\n",
              (res ? "is successful" : "has failed"));
    if (!res) LOG_DEBUG("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
}


void SampleLPA_SendPendingNotifications()
{
    LPA_SENDING_NOTIFICATION_RESULT sendingNotificationResult;
    LPA_EventCallback lpaEventCallback;
    bool res = false;

    memset(&sendingNotificationResult, 0, sizeof(LPA_SENDING_NOTIFICATION_RESULT));
    memset(&lpaEventCallback, 0, sizeof(LPA_EventCallback));

    lpaEventCallback._appParameter = NULL;
    lpaEventCallback._lpaEventProgressText = lpaEventTextNotificationFunction;
    lpaEventCallback._lpaEventProgressValue = lpaEventValueNotificationFunction;
    lpaEventCallback._lpaEventExecutionError = lpaEventExecutionErrorNotificationFunction;

#ifdef SAMPLE__GENERIC_MODEM
    Modem_WaitForModemReady();
#endif //SAMPLE__GENERIC_MODEM

    LOG_DEBUG("\nStarting sending pending notifications\n");
    res = lpaSendPendingNotification(&lpaEventCallback, &sendingNotificationResult);
    LOG_DEBUG("lpaSendPendingNotification API returned %s\n", (res ? "true" : "false"));
    if (res)
    {
        LOG_DEBUG("Notification(s) detected = %zu\n",sendingNotificationResult.countNotificationDetected);
        LOG_DEBUG("Notification(s) sent = %zu\n",sendingNotificationResult.countNotificationSend);
    }
    else {
        LOG_ERROR("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    }
}

bool SampleLPA_MemoryReset()
{
    const unsigned char memoryResetOptionParameter[] = {0x05,0xE0};

    LOG_DEBUG("\nStarting Memory Reset\n");
    bool res = lpaMemoryReset(memoryResetOptionParameter,sizeof(memoryResetOptionParameter));
    LOG_DEBUG("lpaMemoryReset API returned %s\n", (res ? "true" : "false"));
    if (!res) LOG_DEBUG("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    return(res);
}

bool SampleLPA_SetDefaultSMDPAddress(const char* ptrSMDPAddr)
{
    bool res = lpaSetDefaultSMDPAddress(ptrSMDPAddr);
    LOG_DEBUG("SampleLPA_SetSMDPAddress API returned %s\n",
              (res ? "true" : "false"));
    if (!res) LOG_DEBUG("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    return res;
}

bool SampleLPA_SetParameter_CertPath(const char* ptrCertPath)
{
    bool res = lpaManagerES9Plus_setCertPath(ptrCertPath);
    LOG_DEBUG("SampleLPA_SetParameter_CertPath API returned %s\n",
              (res ? "true" : "false"));
    if (!res) LOG_DEBUG("=> Error code: %s\n", lpaGetErrorCodeDescription(lpaGetErrorCode()));
    return res;
}

// noted main function so that Sample_exe can be compiled as a lib as well.
// we will need all function and add a cover API list for higher service to use.

/************/
/**        **/
/**  Main  **/
/**        **/
/************/
/*
int main(int argc, char** argv)
{
    const LPA_API_VERSION* lpaApiVersion;

    const char* ActivationCodeStr_local_default="1$smdp-plus.test.gsma.com$default";
    const char* ActivationCodeStr_local_OP1="1$smdp-plus.test.gsma.com$GTO_PROFILE_OPERATIONAL1_8929901012345678905.ppp";
    const char* ActivationCodeStr_local_OP2="1$smdp-plus.test.gsma.com$GTO_PROFILE_OPERATIONAL2_8929901023456789019.ppp";
    const char* ActivationCodeStr_local_TEST1="1$smdp-plus.test.gsma.com$GTO_PROFILE_TEST1_89299010023456780024.ppp";
    const char* ActivationCodeStr_local_OP4_PPR1="1$smdp-plus.test.gsma.com$GTO_PROFILE_OPERATIONAL4_8929901045678901239.ppp";
    const char* ActivationCodeStr_local_OP51_PPR1_PPR2="1$smdp-plus.test.gsma.com$GTO_PROFILE_OPERATIONAL5.1_8929901056789012345.ppp";
    const char* ActivationCodeStr_local_OP11_PPR2="1$smdp-plus.test.gsma.com$GTO_PROFILE_OPERATIONAL1.1_8929901012345678905.ppp";

#define NB_OF_AVAILABLE_CODES 7
    char** ActivationCodeStr[NB_OF_AVAILABLE_CODES];
    ActivationCodeStr[0] = ActivationCodeStr_local_default;
    ActivationCodeStr[1] = ActivationCodeStr_local_OP1;
    ActivationCodeStr[2] = ActivationCodeStr_local_OP2;
    ActivationCodeStr[3] = ActivationCodeStr_local_TEST1;
    ActivationCodeStr[4] = ActivationCodeStr_local_OP4_PPR1;
    ActivationCodeStr[5] = ActivationCodeStr_local_OP51_PPR1_PPR2;
    ActivationCodeStr[6] = ActivationCodeStr_local_OP11_PPR2;

    size_t index_ActivationCode;
    size_t index_Profiles;

    LPA_GET_PROFILES_INFO getProfilesInfo;
    getProfilesInfo.profileInfoList = NULL;
    LPA_PROFILE_INFO *ProfileData = NULL;

    printf("\n --- Starting sample execution ---\n");
#ifdef SAMPLE__GENERIC_MODEM
    printf(" ---    compiled for MODEM     ---\n\n");
#else
    printf(" ---  not compiled for MODEM   ---\n\n");
#endif

    SampleLPA_Initialize();

    lpaApiVersion = lpaGetApiVersion();
    printf("LPA API version : %d.%d\n\n", lpaApiVersion->major, lpaApiVersion->minor);

    SampleLPA_GetReader();

    SampleLPA_GetEID();

    SampleLPA_GetSMDPAddress();

    SampleLPA_GetSMDSAddress();

    SampleLPA_SetCurlParameters();

    SampleLPA_SetParameter_CertPath();


    SampleLPA_SetParameter_deviceInfoTlv();

#ifdef SAMPLE__GENERIC_MODEM
    SampleLPA_SetRefreshParameter(true);
#else
    SampleLPA_SetRefreshParameter(false);
#endif //SAMPLE__GENERIC_MODEM

#ifdef SAMPLE__GENERIC_MODEM
    printf("\nWait for modem detection\n");
		Modem_WaitForModemIsDetected();
		printf("Modem has been detected\n");
#endif //SAMPLE__GENERIC_MODEM

    SampleLPA_SendPendingNotifications();

    if(SampleLPA_GetProfilesInfo(&getProfilesInfo))
    {
        ProfileData = getProfilesInfo.profileInfoList;
        SampleLPA_DisplayProfilesInfoFull(getProfilesInfo);
        SampleLPA_DisplayProfilesInfoSimplified(getProfilesInfo);
    }

    char choice[10];
    choice[0]= '0';
    while( choice[0] != 'x')
    {
        printf("\n------------------------------------------------------------\n");
        printf("L  - List activation codes\n");
        printf("Ax - download profile using Activation code #x\n");
        printf("Ex - Enable profile #x\n");
        printf("Dx - Disable profile #x\n");
        printf("Rx - delete (Remove) profile #x\n");
        printf("Sx - Set nickname for profile #x\n\n");
        printf("I  - get and display profiles Info (simplified)\n");
        printf("IF - get and display profiles Info (Full)\n");
        printf("N  - send Notifications\n");
        printf("MR - Memory Reset\n");
        printf("US - download profile Using sm-dS address\n");
        printf("UP - download profile Using default sm-dP+ address\n");
        printf("X  - eXit\n");
        printf("------------------------------------------------------------");
        printf("\nChoice : \n");

        choice[1]= '\0';
        choice[2]= '\0';
        scanf("%s",choice);

        index_ActivationCode = NB_OF_AVAILABLE_CODES; // set index to out of range value
        index_Profiles = getProfilesInfo.countProfileInfo; // set index to out of range value

        if((choice[1]  >= '0') && (choice[1] <='9'))
        {
            index_Profiles = choice[1]-'0';
            if((choice[2]  >= '0') && (choice[2] <='9'))
            {
                index_Profiles = index_Profiles*10;
                index_Profiles += choice[2]-'0';
            }
            index_ActivationCode = index_Profiles;
        }

        switch(choice[0])
        {
            case 'l':
            case 'L':
                for(index_ActivationCode=0; index_ActivationCode < NB_OF_AVAILABLE_CODES; index_ActivationCode++)
                {
                    printf("\tActivation Code #%d : %s\n",index_ActivationCode,ActivationCodeStr[index_ActivationCode]);
                }
                break;

            case 'a':
            case 'A':
                if(index_ActivationCode<NB_OF_AVAILABLE_CODES)
                {
                    if(SampleLPA_DownloadProfile(ActivationCodeStr[index_ActivationCode]))
                        SampleLPA_SendPendingNotifications();
                }
                else
                {
                    printf("\nThere is no Activation code #%c%c.\n",choice[1],choice[2]);
                }
                break;

            case 'e':
            case 'E':
                if(index_Profiles < getProfilesInfo.countProfileInfo)
                {
                    if(SampleLPA_EnableProfile(ProfileData[index_Profiles].iccid, ProfileData[index_Profiles].iccidSize))
                        SampleLPA_SendPendingNotifications();
                }
                else
                {
                    printf("\nThere is no profile #%c%c.\n",choice[1],choice[2]);
                }
                break;

            case 'd':
            case 'D':
                if(index_Profiles < getProfilesInfo.countProfileInfo)
                {
                    if(SampleLPA_DisableProfile(ProfileData[index_Profiles].iccid, ProfileData[index_Profiles].iccidSize))
                        SampleLPA_SendPendingNotifications();
                }
                else
                {
                    printf("\nThere is no profile #%c%c.\n",choice[1],choice[2]);
                }
                break;

            case 'r':
            case 'R':
                if(index_Profiles < getProfilesInfo.countProfileInfo)
                {
                    if(SampleLPA_DeleteProfile(ProfileData[index_Profiles].iccid, ProfileData[index_Profiles].iccidSize))
                        SampleLPA_SendPendingNotifications();
                }
                else
                {
                    printf("\nThere is no profile #%c%c.\n",choice[1],choice[2]);
                }
                break;

            case 's':
            case 'S':
                if(index_Profiles < getProfilesInfo.countProfileInfo)
                {
                    if(ProfileData[index_Profiles].profileClass[0] == PROFILE_CLASS_PROVISIONING)
                    {
                        printf("\nSet/Edit Nickname procedure is not applicable to Provisioning Profiles.\n");
                    }
                    else
                    {
                        SampleLPA_SetNickname(ProfileData[index_Profiles].iccid, ProfileData[index_Profiles].iccidSize);
                    }
                }
                else
                {
                    printf("\nThere is no profile #%c%c.\n",choice[1],choice[2]);
                }
                break;

            case 'i':
            case 'I':
                if(choice[1] == 'F' || choice[1] == 'f')
                {
                    if(SampleLPA_GetProfilesInfo(&getProfilesInfo))
                    {
                        ProfileData = getProfilesInfo.profileInfoList;
                        SampleLPA_DisplayProfilesInfoFull(getProfilesInfo);
                    }
                }
                else
                {
                    if(SampleLPA_GetProfilesInfo(&getProfilesInfo))
                    {
                        ProfileData = getProfilesInfo.profileInfoList;
                        SampleLPA_DisplayProfilesInfoSimplified(getProfilesInfo);
                    }
                }
                break;

            case 'n':
            case 'N':
                SampleLPA_SendPendingNotifications();
                break;

            case 'M':
            case 'm':
                if(choice[1] == 'R' || choice[1] == 'r')
                {
                    if(SampleLPA_MemoryReset())
                        SampleLPA_SendPendingNotifications();
                }
                else
                {
                    printf("\nCommand is not recognized.\n");
                }
                break;

            case 'U':
            case 'u':
                if(choice[1] == 'S' || choice[1] == 's')
                {
                    SampleLPA_DownloadProfile_SMDS();
                }
                else if(choice[1] == 'P' || choice[1] == 'p')
                {
                    SampleLPA_DownloadProfile_defaultSMDP();
                }
                else
                {
                    printf("\nCommand is not recognized.\n");
                }
                break;

            case 'x':
            case 'X':
            case 'q':
            case 'Q':
                return 0;

            default:
                printf("\nCommand is not recognized.\n");
                break;

        }
    }
}
*/

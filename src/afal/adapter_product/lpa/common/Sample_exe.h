//
// Created by chenhaotian on 12/7/24.
//

#ifndef WAF_SAMPLE_EXE_H
#define WAF_SAMPLE_EXE_H

extern "C" {
#include "lpasdk/api/lpasdk_api.h"
#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_manager_es9plus.h"
}

#define LPA_PROFILE_ICCID_BUFFER_MAX_SIZE 10
#define LPA_PROFILE_NAME_MAX_SIZE 64
#define LPA_PROFILE_NICKNAME_MAX_SIZE 64

typedef struct {
    int ProfileState;
    char ProfileName[512];
    char iccid[21];  // iccid must be 20 bytes length.
    int ProfileClass;
    char ProfileNickName[512];
} LpaProfileInfoType;


int  SampleLPA_Initialize(char *logpath, unsigned int type);
int  SampleLPA_DeInitialize(void);
int  SampleLPA_GetReader(void);
int  SampleLPA_GetEID(char *eid);
void SampleLPA_SetRefreshParameter(bool inputParamValue);
void SampleLPA_SendPendingNotifications(void);
bool SampleLPA_GetProfilesInfo(LPA_GET_PROFILES_INFO* getProfilesInfo);
int SampleLPA_GetProfileInfoNum(int *num);
int SampleLPA_GetOneProfileInfo(int *num);
bool SampleLPA_DownloadProfile(const char* ActivationCodeStr, size_t ActivationCodeStrLen, bool flag);
bool SampleLPA_EnableProfile(unsigned char* ProfileIdByteArray, size_t ProfileIdByteArraySize);
bool SampleLPA_DisableProfile(unsigned char* ProfileIdByteArray, size_t ProfileIdByteArraySize);
bool SampleLPA_DeleteProfile(unsigned char* ProfileIdByteArray, size_t ProfileIdByteArraySize);
void SampleLPA_SetNickname(unsigned char* ProfileIdByteArray, size_t ProfileIdByteArraySize,
                           unsigned char* NickName, size_t NickNameSize);
bool SampleLPA_MemoryReset();
bool SampleLPA_SetDefaultSMDPAddress(const char* ptrSMDPAddr);
bool SampleLPA_SetParameter_CertPath(const char* ptrCertPath);
#endif //WAF_SAMPLE_EXE_H

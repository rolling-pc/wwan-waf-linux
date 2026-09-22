#include <iostream>
#include "lpa.hpp"
#include "log.hpp"
#include "mbim.hpp"
#include "common.hpp"
#include "version.h"
#include <string>
#include <mutex>
#include "Sample_exe.h"
#include "lpasdk_api.h"

using namespace afal::log;
using namespace afal::mbim;
using namespace afal::lpa;
using namespace afal::error;

int g_channel = ERR;

int SendAtOverMbimMessage(char *inputStr, char *OutputStr, int OutputStrSize) {
    MbimDevice& mbimDevice = MbimDevice::getInstance();;
    int         ret        = ERR;

    if (!inputStr || !OutputStr) {
        LOG_ERROR("NULL pointer!\n");
        return ERR;
    } else if (strlen(inputStr) < 1 || OutputStrSize < 1) {
        LOG_ERROR("Invalid data!\n");
        return ERR;
    }

    memset(OutputStr, 0, OutputStrSize);
    snprintf(OutputStr, OutputStrSize - 1, "\"6F00\"");
    QString inputData(inputStr);
    QString OutputData(OutputStr);
    ret = mbimDevice.AfalMbimAtOverMbimSetSync(inputData, OutputData, 30);
    if (ret != OK) {
        LOG_ERROR("execute failed!\n");
        return ERR;
    }

    LOG_DEBUG("Original AT response: %s\n", QS(OutputData));
    memset(OutputStr, 0, OutputStrSize);
    // below code will cut the "ok" or "error" on the tail.
    snprintf(OutputStr, OutputStrSize - 1, "%s", QS(OutputData));

    LOG_DEBUG("New AT response: %s\n", OutputStr);

    return OK;
}

int LpaGetAtPresetType(void) {
    LpaDevice& _Lpadevice = LpaDevice::getInstance();
    return _Lpadevice.GetAtPresetType();
}

int LpaGetChipPlatType(void) {
    LpaDevice& _Lpadevice = LpaDevice::getInstance();
    return _Lpadevice.GetChipPlatType();
}

int LpaDevice::GetAtPresetType(void) {
    return AtPresetType;
}

int LpaDevice::SetAtPresetType(AfalLpaAtPresetEnumType type) {
    AtPresetType = (int)type;
    return OK;
}

int LpaDevice::GetChipPlatType(void) {
    return (int)ChipPlatType;
}

int LpaDevice::SetChipPlatType(AfalLpaPlatEnumType type) {
    ChipPlatType = (int)type;
    return OK;
}

LpaDevice &LpaDevice::getInstance() {
    static LpaDevice instance;
    return instance;
}

LpaDevice::~LpaDevice() {
    deinit();
}

int LpaDevice::deinit(void) {
    int ret = ERR;

    LOG_DEBUG("enter %s!\n", __func__);

    ret = SampleLPA_DeInitialize();
    return ret;
}

int LpaDevice::init(QString LogPath, QString PortName, AfalLpaPlatEnumType type) {
    int ret = ERR;
    char _LogPath[256] = {0};

    LOG_DEBUG("enter %s!\n", __func__);

    //MbimDevice& _Mbimdevice = MbimDevice::getInstance();
    //ret = _Mbimdevice.init(PortName);
    //if (ret != OK) {
    //    LOG_ERROR("mbim init failed!\n");
    //}
    snprintf(_LogPath, 255, "%s", QS(LogPath));
    SetChipPlatType(type);
    ret = SampleLPA_Initialize(_LogPath, (unsigned int)type);
    if (ret != OK) {
        LOG_ERROR("LPA init failed!\n");
        return ret;
    }

    //ret = SampleLPA_GetReader();
    //if (ret != OK) {
        //LOG_ERROR("LPA get reader failed!\n");
        //return ret;
    //}

    //SampleLPA_SetRefreshParameter(false);
    //if (ret != OK) {
        //LOG_ERROR("LPA set refresh flag failed!\n");
        //return ret;
    //}

    //SampleLPA_SendPendingNotifications();
    //if (ret != OK) {
        //LOG_ERROR("LPA set refresh flag failed!\n");
        //return ret;
    //}

    return ret;
}

int LpaDevice::AfalLpaGetProfilesInfo(vector<AfalLpaProfileInfoType> *pointer) {
    bool ret = false;

    LOG_DEBUG("enter %s!\n", __func__);

    LPA_GET_PROFILES_INFO getProfilesInfo = { 0 };
    getProfilesInfo.profileInfoList = NULL;
    ret = SampleLPA_GetProfilesInfo(&getProfilesInfo);
    if(ret)
    {
        LPA_PROFILE_INFO* profileData = (LPA_PROFILE_INFO*)getProfilesInfo.profileInfoList;
        for (size_t indexProfile = 0; indexProfile < getProfilesInfo.countProfileInfo; indexProfile++)
        {
            LOG_DEBUG("Get %d profiles info!\n", indexProfile + 1);
            char iccid[LPA_PROFILE_ICCID_BUFFER_MAX_SIZE * 2 + 1] = { 0 };
            memset(iccid, 0, sizeof(iccid));
            for (size_t indexIccid = 0; indexIccid < profileData[indexProfile].iccidSize; indexIccid++)
            {
                char tmp[3] = {0};
                snprintf(tmp, 3, "%02X", profileData[indexProfile].iccid[indexIccid]);
                tmp[2] = tmp[0];
                tmp[0] = tmp[1];
                tmp[1] = tmp[2];
                tmp[2] = 0;
                strncat(iccid, tmp, 2);
    }

        AfalLpaProfileInfoType temp;
            temp.iccid = QString(iccid);
            temp.ProfileState = (AfalLpaProfileStateEnumType)(profileData[indexProfile].profileState[0]);
            temp.ProfileName = QString((char *)profileData[indexProfile].profileName);
            temp.ProfileClass = (AfalLpaProfileClassEnumType)profileData[indexProfile].profileClass[0];
            temp.ProfileNickName = QString((char *)profileData[indexProfile].profileNickname);
        pointer->push_back(temp);

            LOG_DEBUG("Profile %d's info are as below.\n", indexProfile + 1);
        LOG_DEBUG("ProfileState: %d\n", temp.ProfileState);
        LOG_DEBUG("ProfileName: %s\n", QS(temp.ProfileName));
        LOG_DEBUG("iccid: %s\n", QS(temp.iccid));
        LOG_DEBUG("ProfileClass: %d\n", temp.ProfileClass);
        LOG_DEBUG("ProfileNickName: %s\n", QS(temp.ProfileNickName));
    }
}

    if (NULL != getProfilesInfo.profileInfoList)
    {
        free(getProfilesInfo.profileInfoList);
        getProfilesInfo.profileInfoList = NULL;
    }

    return ret ? OK : ERR;
}

int LpaDevice::AfalLpaGetEid(QString &eid) {
    int ret = ERR;
    char EID[128] = {0};

    LOG_DEBUG("enter %s!\n", __func__);

    ret = SampleLPA_GetEID(EID);
    eid.clear();
    eid.append(EID);

    return ret;
}

int LpaDevice::AfalLpaEnableProfileByIccid(QString iccid) {
    LOG_DEBUG("Enable profile: %s", QS(iccid));
    // step1 iccid string to iccid byte array
    int cnt = 0;
    std::string tmp = iccid.toStdString();
    const char* p = tmp.c_str();
    unsigned char iccid_array[LPA_PROFILE_ICCID_BUFFER_MAX_SIZE] = {0};
    int iccid_len = tmp.length() / 2;
    while (cnt < iccid_len)
    {
        int low = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                               : *p - 48;
        int high = (*(++p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                                  : *p - 48;
        iccid_array[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
        p++;
        cnt++;
    }

    // step2 enable profile
    bool ret = SampleLPA_EnableProfile(iccid_array, iccid_len);
    return ret ? OK : ERR;
}

int LpaDevice::AfalLpaDisableProfileByIccid(QString iccid) {
    LOG_DEBUG("Disable profile: %s", QS(iccid));
    // step1 iccid string to iccid byte array
    int cnt = 0;
    std::string tmp = iccid.toStdString();
    const char* p = tmp.c_str();
    unsigned char iccid_array[LPA_PROFILE_ICCID_BUFFER_MAX_SIZE] = {0};
    int iccid_len = tmp.length() / 2;
    while (cnt < iccid_len)
    {
        int low = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                               : *p - 48;
        int high = (*(++p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                                  : *p - 48;
        iccid_array[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
        p++;
        cnt++;
    }

    // step2 disable profile
    bool ret = SampleLPA_DisableProfile(iccid_array, iccid_len);
    return ret ? OK : ERR;
}

int LpaDevice::AfalLpaDownloadProfile(QString ActivationCode, bool flag) {
    LOG_DEBUG("download profile activation code: %d-%s", flag, QS(ActivationCode));
    std::string tmp = ActivationCode.toStdString();
    bool res = SampleLPA_DownloadProfile(tmp.c_str(), tmp.length(), flag);
    return res ? OK : ERR;
}

int LpaDevice::AfalLpaDeleteProfileByIccid(QString iccid) {
    LOG_DEBUG("Disable and delete profile: %s", QS(iccid));
    // step1 iccid string to iccid byte array
    int cnt = 0;
    std::string tmp = iccid.toStdString();
    const char* p = tmp.c_str();
    unsigned char iccid_array[LPA_PROFILE_ICCID_BUFFER_MAX_SIZE] = {0};
    int iccid_len = tmp.length() / 2;
    while (cnt < iccid_len)
    {
        int low = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                               : *p - 48;
        int high = (*(++p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                                  : *p - 48;
        iccid_array[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
        p++;
        cnt++;
    }

    // step2 delete profile
    bool ret = SampleLPA_DeleteProfile(iccid_array, iccid_len);
    return ret ? OK : ERR;
}

int LpaDevice::AfalLpaSetNickNameByIccid(QString iccid, QString nickname) {
    LOG_DEBUG("Disable and delete profile: %s", QS(iccid));
    // step1 iccid string to iccid byte array
    int cnt = 0;
    std::string tmp_iccid = iccid.toStdString();
    const char* p = tmp_iccid.c_str();
    unsigned char iccid_array[LPA_PROFILE_ICCID_BUFFER_MAX_SIZE] = {0};
    int iccid_len = tmp_iccid.length() / 2;
    while (cnt < iccid_len)
    {
        int low = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                               : *p - 48;
        int high = (*(++p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                                  : *p - 48;
        iccid_array[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
        p++;
        cnt++;
    }

    cnt = 0;
    p = NULL;
    std::string tmp_nickname = nickname.toStdString();
    p = tmp_nickname.c_str();
    unsigned char nickname_array[LPA_PROFILE_NICKNAME_MAX_SIZE + 1] = { 0 };
    int nickname_len = tmp_nickname.length() / 2;
    while (cnt < nickname_len)
    {
        int high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                               : *p - 48;
        int low = (*(++p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7
                                                                  : *p - 48;
        nickname_array[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
        p++;
        cnt++;
    }

    SampleLPA_SetNickname(iccid_array, iccid_len, nickname_array, nickname_len);
    return OK;
}

int LpaDevice::AfalLpaSetDefaultSMDPAddress(QString SMDPAddress) {
    std::string tmp = SMDPAddress.toStdString();
    char smdp_addr[LPA_SMDP_ADDRESS_SIZE + 1] = { 0 };
    int len = LPA_SMDP_ADDRESS_SIZE <= tmp.length() ? LPA_SMDP_ADDRESS_SIZE - 1 : tmp.length();
    memcpy(smdp_addr, tmp.c_str(), len);
    bool ret = SampleLPA_SetDefaultSMDPAddress(smdp_addr);
    return ret ? OK : ERR;
}

int LpaDevice::AfalLpaMemoryReset(void) {
    bool ret = SampleLPA_MemoryReset();
    return ret ? OK : ERR;
}

int LpaDevice::AfalLpaSetCertPath(QString CertPath) {
    std::string tmp = CertPath.toStdString();
    bool ret = SampleLPA_SetParameter_CertPath(tmp.c_str());
    return ret ? OK : ERR;
}

int LpaDevice::AfalLpaGetProfileNumber(int *ProfileNumber) {
    return SampleLPA_GetProfileInfoNum(ProfileNumber);
}


// below function only can be used on PCIOT function.
int LpaDevice::AfalLpaOpenChannel(void) {
    int  ret               = ERR;
    char OpenChannelApdu[] = "0070000001";
    char CAPDU[128]        = {0};
    char RAPDU[128]        = {0};
    char *pointer          = nullptr;
    int  channel           = 0;
    int  AtPresetType      = ERR;

    LOG_DEBUG("enter %s!\n", __func__);

    AtPresetType = GetAtPresetType();
    if (AtPresetType == 0) {
        snprintf(CAPDU, 128, "AT+CSIM=10,\"%s\"", OpenChannelApdu);
    } else if (AtPresetType == 1) {
        snprintf(CAPDU, 128, "AT+CGLA=00,10,\"%s\"", OpenChannelApdu);
    } else {
        LOG_ERROR("Invalid AT preset type: %d!\n", AtPresetType);
        return ERR;
    }

    ret = SendAtOverMbimMessage(CAPDU, RAPDU, sizeof(RAPDU));
    if (ret != OK) {
        LOG_ERROR("send apdu failed!\n");
        return ERR;
    }

    if (AtPresetType == 0 && strstr(RAPDU, "+CSIM") == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        return ERR;
    } else if (AtPresetType == 1 && strstr(RAPDU, "+CGLA") == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        return ERR;
    }

    pointer = strstr(RAPDU, "9000");
    if (pointer != nullptr) {
        LOG_DEBUG("open channel succeed!\n");
    }
    else {
        int sw1 = 0;
        pointer = strchr(RAPDU, '\"');
        // skip the \" and first two symbols, to get the sw1 sw2 value.
        pointer = pointer + 3;
        sw1 = (*(pointer) - '0') * 10 + *(pointer + 1) - '0';

        pointer = strstr(RAPDU, "OK");
        if (pointer != nullptr && sw1 == 91) {
            LOG_DEBUG("Channel open successfully, but esim have notification message!\n");
        }
        else if (pointer != nullptr) {
            LOG_ERROR("AT return OK, but close channel failed!\n");
            return ERR;
        }
        else {
            LOG_ERROR("AT return abnormally!\n");
            return ERR;
        }
    }

    // RADPU should like: +CSIM: 6,"039000"
    pointer = strchr(RAPDU, '\"');
    // skip the \" symbol, to get the channel id.
    pointer++;
    channel = (*(pointer) - '0') * 16 + *(pointer + 1) - '0';
    LOG_DEBUG("channel id: %02x\n", channel);

    if (channel != 1) {
        LOG_DEBUG("warning: seems other app is using channel!\n");
    }

    g_channel = channel;
    return channel;
}

int LpaDevice::AfalLpaCloseChannel(int channel) {
    int  ret                = ERR;
    char CloseChannelApdu[] = "007080";
    char CAPDU[128]         = {0};
    char RAPDU[128]         = {0};
    char *pointer           = nullptr;
    int  AtPresetType       = ERR;

    LOG_DEBUG("enter %s!\n", __func__);

    // channel 0 can't be closed! And open channel request should not return channel 0 by default!
    if (channel <= 0) {
        LOG_ERROR("Illegal channel ID!\n");
        return ERR;
    } else if (channel != g_channel) {
        LOG_ERROR("Mismatched channel ID, Stored channel: %02X!\n", g_channel);
        return ERR;
    }

    AtPresetType = GetAtPresetType();
    if (AtPresetType == 0) {
        snprintf(CAPDU, 128, "AT+CSIM=10,\"%s%02X00\"", CloseChannelApdu, channel);
    } else if (AtPresetType == 1) {
        snprintf(CAPDU, 128, "AT+CGLA=00,10,\"%s%02X00\"", CloseChannelApdu, channel);
    } else {
        LOG_ERROR("Invalid AT preset type: %d!\n", AtPresetType);
        return ERR;
    }

    ret = SendAtOverMbimMessage(CAPDU, RAPDU, sizeof(RAPDU));
    if (ret != OK) {
        LOG_ERROR("send apdu failed!\n");
        return ERR;
    }

    if (AtPresetType == 0 && strstr(RAPDU, "+CSIM") == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        return ERR;
    } else if (AtPresetType == 1 && strstr(RAPDU, "+CGLA") == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        return ERR;
    }

    // AT response should like: +CSIM: 4,"9000"\r\nOK\r\n
    pointer = strstr(RAPDU, "9000");
    if (pointer != nullptr) {
        g_channel = ERR;
        LOG_DEBUG("close channel finished!\n");
        return OK;
    }
    else {
        int sw1 = 0;
        pointer = strchr(RAPDU, '\"');
        // skip the \" symbol, to get the channel id.
        pointer++;
        sw1 = (*(pointer) - '0') * 10 + *(pointer + 1) - '0';

        pointer = strstr(RAPDU, "OK");
        if (pointer != nullptr && sw1 == 91) {
            LOG_DEBUG("Channel closed successfully, but esim have notification message!\n");
            return OK;
        }
        else if (pointer != nullptr && sw1 == 90) {
            LOG_ERROR("Channel closed successfully!\n");
            return OK;
        }
        else {
            LOG_ERROR("AT return abnormally!\n");
            return ERR;
        }
    }

    return OK;
}

int LpaDevice::AfalLpaSelectEsimAppid(QString aid, QString &OutputData) {
    int  ret               = ERR;
    char SelectAidApdu[]   = "00A40400";
    int  InputSize         = ERR;
    int  CAPDUSize         = ERR;
    char CAPDU[128]        = {0};
    char RAPDU[1024]       = {0};
    char *pointer          = nullptr;
    int  AtPresetType      = ERR;
    char *_pointer         = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    if (g_channel == ERR) {
        LOG_ERROR("channel not opened!\n");
        goto TAIL;
    }

    if (aid.size() % 2 != 0) {
        LOG_ERROR("illegal input param!\n");
        goto TAIL;
    }

    InputSize = aid.size() + 8 + 2;  // add header "00A40400" and length's 2 symbol.
    CAPDUSize = aid.size() / 2;  // every 2 symbols on data param will be treated as 1 symbol on the length param behind of data.

    AtPresetType = GetAtPresetType();
    if (AtPresetType == 0) {
        snprintf(CAPDU, 128, "AT+CSIM=%d,\"%s%02x%s\"", InputSize, SelectAidApdu, CAPDUSize, QS(aid));
    } else if (AtPresetType == 1) {
        snprintf(CAPDU, 128, "AT+CGLA=%d,%d,\"%s%02x%s\"", g_channel, InputSize, SelectAidApdu, CAPDUSize, QS(aid));
    } else {
        LOG_ERROR("Invalid AT preset type: %d!\n", AtPresetType);
        goto TAIL;
    }

    LOG_DEBUG("CAPDU: %s\n", CAPDU);
    // modify string to add correct CLA param.
    // we don't consider that channel id can be greater than 255!
    pointer = strchr(CAPDU, '\"');
    pointer++;
    if (g_channel % 16 >= 10) {
        *(pointer + 1) = (g_channel % 16) - 10 + 'A';
    }
    else {
        *(pointer + 1) = (g_channel % 16) + '0';
    }

    LOG_DEBUG("CAPDU: %s\n", CAPDU);
    ret = SendAtOverMbimMessage(CAPDU, RAPDU, sizeof(RAPDU));
    if (ret != OK) {
        LOG_ERROR("send apdu failed!\n");
        goto TAIL;
    }

    if (AtPresetType == 0 && strstr(RAPDU, "+CSIM") == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        goto TAIL;
    } else if (AtPresetType == 1 && strstr(RAPDU, "+CGLA") == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        goto TAIL;
    }

    // need to get RAPDU's data size and data pointer.
    // do not use malloc space here to store RAPDU! or use other function to get actual RAPDU!
    // pointer = strtok_s(RAPDU, '\"', _pointer);
    pointer = strtok(RAPDU, "\"");
    // skip the \" symbol, to get the data param.
    // pointer = strtok_s(nullptr, '\"', _pointer);
    pointer = strtok(nullptr, "\"");
    if (pointer == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        goto TAIL;
    }
    OutputData.clear();
    OutputData.append(pointer);
    return OK;

TAIL:
    OutputData.clear();
    OutputData.append("ERROR");
    return ERR;
}

int LpaDevice::AfalLpaSendApduSync(QString CAPDU, QString &RAPDU) {
    int  ret               = ERR;
    char SelectAidApdu[]   = "00A40400";
    int  InputSize         = ERR;
    char _CAPDU[1024 * 4]  = {0};
    char _RAPDU[1024 * 4]  = {0};
    char *pointer          = nullptr;
    int  AtPresetType      = ERR;
    char *_pointer         = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    if (g_channel == ERR) {
        LOG_ERROR("channel not opened!\n");
        goto TAIL;
    }

    if (CAPDU.size() % 2 != 0) {
        LOG_ERROR("illegal input param!\n");
        goto TAIL;
    }

    InputSize = CAPDU.size();  // add header "00A40400" and length's 2 symbol.

    AtPresetType = GetAtPresetType();
    if (AtPresetType == 0) {
        snprintf(_CAPDU, 1024 * 4, "AT+CSIM=%d,\"%s\"", InputSize, QS(CAPDU));
    } else if (AtPresetType == 1) {
        snprintf(_CAPDU, 1024 * 4, "AT+CGLA=%d,%d,\"%s\"", g_channel, InputSize, QS(CAPDU));
    } else {
        LOG_ERROR("Invalid AT preset type: %d!\n", AtPresetType);
        goto TAIL;
    }

    // higher service must keep input APDU is completely right!
    // don't check CLA, just send it.
    ret = SendAtOverMbimMessage(_CAPDU, _RAPDU, sizeof(_RAPDU));
    if (ret != OK) {
        LOG_ERROR("send apdu failed!\n");
        goto TAIL;
    }

    if (AtPresetType == 0 && strstr(_RAPDU, "+CSIM") == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        goto TAIL;
    } else if (AtPresetType == 1 && strstr(_RAPDU, "+CGLA") == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        goto TAIL;
    }

    // need to get RAPDU's data size and data pointer.
    // do not use malloc space here to store RAPDU! or use other function to get actual RAPDU!
    // pointer = strtok_s(RAPDU, '\"', _pointer);
    pointer = strtok(_RAPDU, "\"");
    // skip the \" symbol, to get the data param.
    // pointer = strtok_s(nullptr, '\"', _pointer);
    pointer = strtok(nullptr, "\"");
    if (pointer == nullptr) {
        LOG_ERROR("Invalid Rapdu!\n");
        goto TAIL;
    }
    RAPDU.clear();
    RAPDU.append(pointer);
    return OK;

TAIL:
    RAPDU.clear();
    RAPDU.append("6F00");
    return ERR;
}

#ifndef __LPA_H__
#define __LPA_H__

#include <string>
#include <memory>
#include <vector>
#include <list>
#include <algorithm>
#include <iostream>
#include <QMap>
#include <QString>

using std::vector;
using std::shared_ptr;

namespace afal {
    namespace lpa {

        typedef enum {
            AT_PRESET_CSIM,
            AT_PRESET_CGLA
        } AfalLpaAtPresetEnumType;

        typedef enum {
            PROFILE_STATE_DISABLED,
            PROFILE_STATE_ENABLED
        } AfalLpaProfileStateEnumType;

        typedef enum {
            PROFILE_CLASS_TEST,
            PROFILE_CLASS_PROVISIONING,
            PROFILE_CLASS_OPERATIONAL
        } AfalLpaProfileClassEnumType;

        typedef struct {
            AfalLpaProfileStateEnumType ProfileState;
            QString ProfileName;
            QString iccid;
            AfalLpaProfileClassEnumType ProfileClass;
            QString ProfileNickName;
        } AfalLpaProfileInfoType;

        typedef enum
        {
	        PLAT_TYPE_INVALID = -1,
	        PLAT_TYPE_MTK,
	        PLAT_TYPE_QC_151,
	        PLAT_TYPE_QC_135R,
        } AfalLpaPlatEnumType;

        class LpaDevice {
        private:
            // private constructor, refuse user to create the mbim instance directly.
            LpaDevice(){};
            ~LpaDevice();
            // forbidden copy constructor and "=" operator.
            LpaDevice(const LpaDevice&) = delete;
            LpaDevice& operator=(const LpaDevice&) = delete;
            int AtPresetType = 0;
            int ChipPlatType = PLAT_TYPE_INVALID;

        public:
            // function to get / set AT preset type, to help LPA_SDK to use correct preset.
            int GetAtPresetType(void);
            int SetAtPresetType(AfalLpaAtPresetEnumType type);
            int GetChipPlatType(void);
            int SetChipPlatType(AfalLpaPlatEnumType type);

            // the first step to use LPA lib, get instance.
            static LpaDevice &getInstance();
            // the second step to use LPA lib, init lpa device for further use.
            int init(QString LogPath, QString PortName, AfalLpaPlatEnumType type);
            // the last step to use LPA lib, deinit lpa device and won't use it anymore.
            int deinit(void);
            int AfalLpaGetProfilesInfo(vector<AfalLpaProfileInfoType> *pointer);
            int AfalLpaGetEid(QString &eid);
            int AfalLpaEnableProfileByIccid(QString iccid);
            int AfalLpaDisableProfileByIccid(QString iccid);
            int AfalLpaDownloadProfile(QString ActivationCode, bool flag = false);
            int AfalLpaDeleteProfileByIccid(QString iccid);
            int AfalLpaSetNickNameByIccid(QString iccid, QString nickname);
            int AfalLpaSetDefaultSMDPAddress(QString SMDPAddress);
            int AfalLpaMemoryReset(void);
            int AfalLpaSetCertPath(QString CertPath);
            int AfalLpaGetProfileNumber(int* ProfileNumber);
            // below function only can be used on PCIOT function.
            int AfalLpaOpenChannel(void);
            int AfalLpaCloseChannel(int Channel);
            int AfalLpaSelectEsimAppid(QString aid, QString &OutputData);
            int AfalLpaSendApduSync(QString CAPDU, QString &RAPDU);
        };
    }
}

#ifdef __cplusplus
extern "C" {
#endif
int SendAtOverMbimMessage(char *inputStr, char *OutputStr, int OutputStrSize);
int LpaGetAtPresetType(void);
int LpaGetChipPlatType(void);
#ifdef __cplusplus
};
#endif

#endif
#include "firmware_flash.h"
#include "proccommon.h"

#include "firmware_flash_interface.h"
#ifdef BUILD_QC
#include "firmware_flash_qc_pcie.h"
#include "firmware_flash_qc.h"
#elif BUILD_MTK
#ifdef _IS_LINUX_
#include "firmware_flash_mtk_usb.h"
#endif
#include "firmware_flash_mtk_pcie.h"
#endif

#include <QProcess>
#include <spdlog/spdlog.h>
#include "log.hpp"
#include <utility>
#include "version.h"

namespace afal {

namespace {

using namespace log;
const std::map<ModemType, QString> K_MODEM_TYPE_STR = {
    {ModemType::MTK_PCIE_GC, "MTK_PCIE_GC"},
    {},
};

} // namespace

FirmwareFlashImpl::FirmwareFlashImpl(ModemType modem_type)
    : FirmwareFlashImpl(modem_type, DeviceFactory::Create()) {}

FirmwareFlashImpl::FirmwareFlashImpl(ModemType modem_type,
                                     std::unique_ptr<DeviceFactory> dev_factory)
    : modem_type(modem_type), dev_factory(std::move(dev_factory)) {
    FMTLOG_INFO("FwFlash version : {}, Modem type : {}", FLASH_VERSION_STRING,
                GetModemTypeName(modem_type));
    //FMTLOG_DEBUG("Debug is on.");
}

bool FirmwareFlashImpl::SwitchFlashMode(FlashMode flash_mode, int retry_times) {
    for (int retry = 0; retry < retry_times; retry++)
    {
        bool ret = false;
        if (flash_mode == FlashMode::FASTBOOT)
        {
            ret = SwitchToFastbootMode();
        }
        else if (flash_mode == FlashMode::EDL)
        {
            ret = SwitchToEDLMode();
        }
        if (ret)
        {
            return true;
        }
    }
    return false;
}

void FirmwareFlashImpl::RebootModem() {
    emit RebootModemSignal();
}

void FirmwareFlashImpl::FlashImageComplete(int progress, QString messages) {
    emit FlashImageCompleteSignal(progress, std::move(messages));
}

FirmwareFlash* FirmwareFlash::Create(ModemType modem_type) {
    switch (modem_type)
    {
#ifdef BUILD_QC
        case ModemType::QC_USB_RW101:
        case ModemType::QC_USB_RW135:
        case ModemType::QC_USB_GC:
        case ModemType::QC_USB_RW135R:
        case ModemType::QC_USB_RW151:
            return new FirmwareFlashImplQcomUSB(modem_type); // NOLINT
        case ModemType::QC_PCIE_GC:
        case ModemType::QC_PCIE_RW135R:
            return new FirmwareFlashImplQcomPCIE(modem_type); 
        case ModemType::QC_PCIE_RW151:
            return new FirmwareFlashImplQcomPCIE(modem_type); 
#elif BUILD_MTK
        case ModemType::MTK_PCIE_RW350:
        case ModemType::MTK_PCIE_GC:
            //return new FirmwareFlashImplMTKPcie(modem_type); // NOLINT
            return afal::FirmwareFlashImplMTKPcie::getInstance(modem_type);
#ifdef _IS_LINUX_
        case ModemType::MTK_USB_GC:
        case ModemType::MTK_USB_RW350R:
            return new FirmwareFlashImplMTKUSB(modem_type); // NOLINT
#endif
#else
#endif
        default:
            break;
    }
    return nullptr;
}

bool SwitchFlashModeWithAT(Modem* modem, Device* flash_device,
                           const QString& cmd, int sendchannel) {
    if (flash_device->FindDevice())
    {
#ifdef _IS_LINUX_
        return true;
#else 
        return false;
#endif        
    }

    if (!modem->FindDevice())
    {
        FMTLOG_ERROR("A normal modem was not found.");
        return false;
    }

    if (sendchannel == 0)
    {
        QProcess sysprocess;    
        sysprocess.start("bash", QStringList() << "-c" << cmd);
        sysprocess.waitForFinished();
    
        if (sysprocess.exitCode() == 0) {
            LOG_DEBUG("send mbimcli command successfully!");
        } else {
            FMTLOG_ERROR("Failed to send mbimcli command:{}",sysprocess.readAllStandardError());
        }
    }
    else if (sendchannel == 1)
    {      
        if (modem->SendATCommand(cmd) == 0)
        {
            FMTLOG_WARN("send switchmode AT successfully!");
        } else {
            FMTLOG_WARN("AT command {} send failed!", cmd);
        }
    }
    else if (sendchannel == 2)
    {      
        LOG_DEBUG("send switchmode MBIM command!");  
        if (!modem->MBIMReboot())
        {
            FMTLOG_WARN("Reboot MBIM command send failed!");
            return false;
        }
    }

    if (sendchannel != 2) {
        if (!flash_device->FindDevice() && !flash_device->WaitForAction("add"))
        {
            return false;
        }
    }

    return true;
}

} // namespace afal

// using ptr = void (afal::FirmwareFlash::*)();

extern "C" {
BASE_EXPORT afal::FirmwareFlash* FwFlashCreate(afal::ModemType modem_type) {
    return afal::FirmwareFlash::Create(modem_type);
}
}
//#include "moc_firmware_flash_interface.cpp"
//#include "moc_firmware_flash.cpp"

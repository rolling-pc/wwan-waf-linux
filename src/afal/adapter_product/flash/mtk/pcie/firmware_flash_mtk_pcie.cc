#include "firmware_flash_mtk_pcie.h"
#include "proccommon.h"
#include "firmware_flash.h"
#include "firmware_flash_interface.h"
#include "flash_tools.h"
#include "log.hpp"
#include "modem.h"
#ifdef _IS_WINDOWS_
#include "5g_download_tool.h"
#endif

#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#ifdef _IS_LINUX_
#include <unistd.h>
#endif
#include <vector>
#include <QtConcurrent/QtConcurrent>
#include <QEventLoop>

namespace afal {

namespace {

#ifdef _IS_LINUX_
const std::vector<DeviceId> K_DEVICE_IDS = {
    {"0x14c3", "0x4d75"},
};
#endif
#ifdef _IS_WINDOWS_
const std::vector<DeviceId> K_DEVICE_IDS = {
    {"14c3", "4d75"},
};
#endif
const std::vector<DeviceId> K_RECOVER_MAP = {
    {"8087", "0B5D"},
};

const int K_WAIT_DEVICE_TIME = 5;
const int K_RECOVERY_TIMEOUT = 60;

using namespace log;
} // namespace

std::unique_ptr<FirmwareFlashImplMTKPcie> FirmwareFlashImplMTKPcie::instance = nullptr;
std::once_flag FirmwareFlashImplMTKPcie::initFlag;
bool FirmwareFlashImplMTKPcie::WaitEdlDevice() {
    if (!edl_device->FindDevice())
    {
        FMTLOG_DEBUG("No EDL device was found");
        return false;
    }

    auto edl_flash = dev_factory->CreateDevice("usb", K_RECOVER_MAP);
    if (!edl_flash->FindDevice())
    {
        RebootModem();
        if (!edl_flash->FindDevice() &&
            !edl_flash->WaitForAction("add", K_RECOVERY_TIMEOUT))
        {
            FMTLOG_DEBUG("No Edl device tty node was found");
            return false;
        }
    }

    return true;
}

FirmwareFlashImplMTKPcie::FirmwareFlashImplMTKPcie(ModemType modem_type)
    : FirmwareFlashImplMTKPcie(modem_type, DeviceFactory::Create()) {}

FirmwareFlashImplMTKPcie::FirmwareFlashImplMTKPcie(
    ModemType modem_type, std::unique_ptr<DeviceFactory> dev_factory)
    : FirmwareFlashImplMTKPcie(modem_type, std::move(dev_factory), nullptr) {};

FirmwareFlashImplMTKPcie::FirmwareFlashImplMTKPcie(
    ModemType modem_type, std::unique_ptr<DeviceFactory> dev_factory,
    std::unique_ptr<MTKPcieFlash> fastboot)
    : FirmwareFlashImpl(modem_type, std::move(dev_factory)),
      fastboot(std::move(fastboot)) {
    modem = this->dev_factory->CreateModem("pci", K_DEVICE_IDS);
    if (!this->fastboot) {
        this->fastboot = MTKPcieFlash::Create(modem.get());
    }
    fastboot_device =
        this->dev_factory->CreateDevice("wwan", K_DEVICE_IDS);
    edl_device = this->dev_factory->CreateDevice("usb", K_RECOVER_MAP);
#ifdef _IS_LINUX_
    fastboot_device->AddEnv("DEVNAME", "/dev/wwan0fastboot0");
#endif
}

void FirmwareFlashImplMTKPcie::progressUpdate(int progress, QString message){    
    FlashImageComplete(progress, message);
}

bool FirmwareFlashImplMTKPcie::SwitchToFastbootMode() {
    if (!modem->FindDevice())
    {
        FMTLOG_ERROR(
            "Unable to Switch fastboot mode because modem was not found");
        return false;
    }

    if (!fastboot->SwitchFlashMode())
    {
        return false;
    }

    if (!fastboot_device->FindDevice() &&
        !fastboot_device->WaitForAction("add"))
    {
        FMTLOG_ERROR("Switch Fastboot Mode failed!");
        return false;
    }

    std::this_thread::sleep_for(std::chrono::seconds(K_WAIT_DEVICE_TIME));

    if (fastboot->GetModemState() != MTKPcieFlash::State::DOWNLOAD) {
        FMTLOG_ERROR("Switch Fastboot Mode failed!");
        return false;
    }

    return true;
}

bool FirmwareFlashImplMTKPcie::PcieReset() {
    fastboot->PcieReboot();
    return true;
}
bool FirmwareFlashImplMTKPcie::SwitchModeMBIM() {
    int sendchannel= 2;
    QString command = "ModemReboot";
    LOG_ERROR("EDL flash test2");
    return SwitchFlashModeWithAT(modem.get(), edl_device.get(),
            command,sendchannel);
}
bool FirmwareFlashImplMTKPcie::SwitchToEDLMode() {
    if (!modem->FindDevice())
    {
        FMTLOG_ERROR(
            "Unable to Switch flash mode because modem was not found");
        return false;
    }

    if (SwitchModeMBIM())
    {
        return false;
    }

    return true;
}

bool FirmwareFlashImplMTKPcie::FastbootReboot() {
    return fastboot->Reboot();
}

FirmwareFlash::FlashMode FirmwareFlashImplMTKPcie::QueryFlashMode() {
    return fastboot_device->FindDevice() ? FirmwareFlash::FlashMode::FASTBOOT
                                         : FirmwareFlash::FlashMode::NONE;
}

void FirmwareFlashImplMTKPcie::EdlConfig(const QString& fw_path,
                                         const QString& log_path,
                                         const QString& partition_table,
                                         const QString& firehose_agent,
                                         const QString& auth_file,
                                         const QString& ext_xml) {
    if (!log_path.isEmpty() || !auth_file.isEmpty())
    {
        FMTLOG_WARN("log path ({}) and auth file ({}) not empty!", log_path,
                    auth_file);
    }
    if (fw_path.isEmpty())
    {
        FMTLOG_ERROR("Set Edl config failed, please check config: {}, {}, {}, "
                        "{}, {}, {}",
                        fw_path, log_path, partition_table, firehose_agent,
                        auth_file, ext_xml);
    }
    this->fw_path = fw_path;
    this->partition_table = partition_table;
    this->firehose_agent = firehose_agent;
    this->ext_xml = ext_xml;

    FMTLOG_DEBUG("Set Edl Config : {}, {}, {}, {}, {}, {}", fw_path, log_path,
                    partition_table, firehose_agent, auth_file, ext_xml);
}

bool FirmwareFlashImplMTKPcie::FastbootFlash(const QString& fw_path,
                                             const QString& fw_partition) {
    FMTLOG_DEBUG("Prepare for fastboot flash : {}, {}", fw_path, fw_partition);
    return fastboot->Flash(fw_partition, fw_path);
}

bool FirmwareFlashImplMTKPcie::EdlFlash() {
    if (fw_path.isEmpty())
    {
        FMTLOG_ERROR("Config is failed, Edl flash failed!");
        return false;
    }

    /*
    if (!WaitEdlDevice())
    {
        return false;
    }*/
    FlashImageComplete(0, "start");
    if (!SwitchModeMBIM()) {
        FMTLOG_ERROR("MBIM Reboot failed, try PLDR!");
        RebootModem();
    }

    bool ret = true;
#ifdef _IS_WINDOWS_

    ret = HandleModemFlash(this->fw_path.toStdString());
    if ( !ret )
    {
        FMTLOG_ERROR("Flash failed");
    } else
    {
        FMTLOG_DEBUG("Flash success");
    }
#endif
    FlashImageComplete(100, ret ? "Success" : "Failed"); // NOLINT
    return ret;
}
bool FirmwareFlashImplMTKPcie::FastbootErase(const QString& fw_partition) {
    FMTLOG_DEBUG("Enter Erase, {}", fw_partition.toStdString());
    return fastboot->Erase(fw_partition);
}

bool FirmwareFlashImplMTKPcie::EdlRead(const QString& /*save_path*/,
                                       std::optional<int> /*start_addr*/,
                                       std::optional<int> /*partition_size*/) {
    FMTLOG_ERROR("This feature is not supported by MTK pcie.");
    return false;
}

bool FirmwareFlashImplMTKPcie::EdlErase() {
    FMTLOG_ERROR("This feature is not supported by MTK pcie.");
    return false;
}

bool FirmwareFlashImplMTKPcie::EdlEraseAll() {
    FMTLOG_ERROR("This feature is not supported by MTK pcie.");
    return false;
};

bool FirmwareFlashImplMTKPcie::EdlErase(int /*start_addr*/,
                                        int /*partition_size*/) {
    FMTLOG_ERROR("This feature is not supported by MTK pcie.");
    return false;
}

} // namespace afal

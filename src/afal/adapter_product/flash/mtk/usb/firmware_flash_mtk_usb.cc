#include "firmware_flash_mtk_usb.h"
#include "proccommon.h"
#include "firmware_flash.h"
#include "firmware_flash_interface.h"
#include "flash_tools.h"
#include "flash_tools_mtk_usb.h"
#include "modem.h"
#include "log.hpp"
#include "osinfo.hpp"
#include <memory>
#include <optional>
#ifdef NEED_UNISTD
#include <unistd.h>
#endif
#include <vector>

namespace afal {

namespace {
    using namespace afal::osinfo;
    using namespace log;
const QString K_SWITCH_FASTBOOT_AT = "at+dlmode=1";

const std::vector<DeviceId> K_MODEM_IDS = {
  {"33f8", ""},
  {"2cb7", ""},
};

const std::vector<DeviceId> K_FASTBOOT_IDS = {
  {"33f8", "1300"},
  {"33f8", "d00d"},
  {"2cb7", "d00d"},
};

const std::vector<DeviceId> K_EDL_IDS = {
  {"0e8d", "2000"},
};

const int K_RECOVERY_TIMEOUT = 60;

} // namespace

bool FirmwareFlashImplMTKUSB::WaitEdlDevice() {
    if (!edl_device->FindDevice() || !flash_lib->IsConnect())
    {
        FMTLOG_DEBUG("No EDL device was found");
        return false;
    }
    return true;
}

FirmwareFlashImplMTKUSB::FirmwareFlashImplMTKUSB(ModemType modem_type)
    : FirmwareFlashImplMTKUSB(modem_type, DeviceFactory::Create()) {}

FirmwareFlashImplMTKUSB::FirmwareFlashImplMTKUSB(
    ModemType modem_type, std::unique_ptr<DeviceFactory> dev_factory)
    : FirmwareFlashImplMTKUSB(modem_type, std::move(dev_factory), Fastboot::Create(), MTKFlashLib::Create(this)) {};

FirmwareFlashImplMTKUSB::FirmwareFlashImplMTKUSB(ModemType modem_type,
                        std::unique_ptr<DeviceFactory> dev_factory,
                        std::unique_ptr<Fastboot> fastboot,
                        std::unique_ptr<MTKFlashLib> flash_lib)
    : FirmwareFlashImpl(modem_type, std::move(dev_factory)), fastboot(std::move(fastboot)), flash_lib(std::move(flash_lib)){
    modem = this->dev_factory->CreateModem("usb", K_MODEM_IDS);
    fastboot_device =
        this->dev_factory->CreateDevice("usb", K_FASTBOOT_IDS);
    edl_device =
        this->dev_factory->CreateDevice("usb", K_EDL_IDS);
}

bool FirmwareFlashImplMTKUSB::SwitchToFastbootMode() {

    int sendchannel= 1;
    QString command = K_SWITCH_FASTBOOT_AT;
#ifdef _IS_LINUX_
    OSType os_type = GetOSType();
    if (os_type == OSType::CROS)
    {
        command = QString("mbimcli -p -d /dev/cdc-wdm0 --fibocom-set-at-command='%1'").arg(K_SWITCH_FASTBOOT_AT);
        sendchannel=0;
    }
#endif
    return SwitchFlashModeWithAT(modem.get(), fastboot_device.get(),
            command,sendchannel);                                
}

bool FirmwareFlashImplMTKUSB::SwitchToEDLMode() {
    RebootModem();
    if (!flash_lib->Connect(auth_file.toStdString()))
    {
        FMTLOG_ERROR("Connect failed!, auth file path:{}", auth_file);
        return false;
    }
    return true;
}

bool FirmwareFlashImplMTKUSB::FastbootReboot() {
    if (!fastboot_device->FindDevice())
    {
        FMTLOG_ERROR("Not found fastboot device");
        return false;
    }
    return fastboot->Reboot();
}

bool FirmwareFlashImplMTKUSB::PcieReset() {
    return true;
}

FirmwareFlash::FlashMode FirmwareFlashImplMTKUSB::QueryFlashMode() {
    if (fastboot_device->FindDevice())
    {
        return FlashMode::FASTBOOT;
    }
    if (edl_device->FindDevice())
    {
        return FlashMode::EDL;
    }
    return FlashMode::NONE;
}

void FirmwareFlashImplMTKUSB::EdlConfig(const QString& fw_path,
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
    if (fw_path.isEmpty() || partition_table.isEmpty() ||
        firehose_agent.isEmpty() || ext_xml.isEmpty())
    {
        FMTLOG_ERROR("Set Edl config failed, please check config: {}, {}, {}, "
                     "{}, {}, {}",
                     fw_path, log_path, partition_table, firehose_agent,
                     auth_file, ext_xml);
    }
    this->fw_path = fw_path;
    this->log_path = log_path;
    this->partition_table = partition_table;
    this->auth_file = auth_file;
    this->ext_xml = ext_xml;

    flash_lib->SetLogPath(log_path.toStdString());

    FMTLOG_DEBUG("Enter Config, config: {}, {}, {}, {}, {}, {}", fw_path,
                 log_path, partition_table, firehose_agent, auth_file, ext_xml);
}

bool FirmwareFlashImplMTKUSB::FastbootFlash(const QString& fw_path,
                                            const QString& fw_partition) {
    FMTLOG_DEBUG("Enter Flash, arg: {}, {}", fw_path, fw_partition);
    return fastboot->Flash(fw_partition, fw_path);
}

bool FirmwareFlashImplMTKUSB::EdlFlash() {
    FMTLOG_DEBUG("Enter Edl Flash");
    if (fw_path.isEmpty() || partition_table.isEmpty() || auth_file.isEmpty() ||
        ext_xml.isEmpty())
    {
        FMTLOG_ERROR("Config is failed, Edl flash failed!");
        return false;
    }

    if (!WaitEdlDevice())
    {
        return false;
    }

    FlashImageComplete(0);

    if (!flash_lib->EnterFlashMode(ext_xml.toStdString()))
    {
        FMTLOG_ERROR(
            "Modem failed to enter flash mode, please check flash xml: {}",
            ext_xml);
        return false;
    }
    if (!flash_lib->FlashAll(fw_path.toStdString(),
                             partition_table.toStdString()))
    {
        FMTLOG_ERROR("Flash failed, please check path and scatter, flash_xml: "
                     "{}, scatter: {}",
                     ext_xml, partition_table);
        return false;
    }

    FlashImageComplete(100); // NOLINT(cppcoreguidelines-avoid-magic-numbers)
    return true;
}

bool FirmwareFlashImplMTKUSB::FastbootErase(const QString& fw_partition) {
    FMTLOG_DEBUG("Enter Read, {}", fw_partition.toStdString());
    return fastboot->Erase(fw_partition);
}

bool FirmwareFlashImplMTKUSB::EdlRead(const QString& save_path,
                                      std::optional<int> start_addr,
                                      std::optional<int> partition_size) {
    FMTLOG_DEBUG(
        "Enter Edl Read,save_path: {} start_addr: {}, partition_size: {}",
        save_path,
        start_addr.has_value() ? std::to_string(*start_addr) : "NONE",
        start_addr.has_value() ? std::to_string(*partition_size) : "NONE");

    if (!start_addr.has_value() || !partition_size.has_value())
    {
        FMTLOG_ERROR("The parameter start_addr or partition_size is not set! "
                     "Read partition failed!");
        return false;
    }

    std::string image_path = save_path.toStdString() + "/" +
                             fmt::format("image_start_0x{:x}_size_0x{:x}",
                                         *start_addr, *partition_size);
    return flash_lib->Read(image_path, *start_addr, *partition_size);
}

bool FirmwareFlashImplMTKUSB::EdlErase() {
    FMTLOG_DEBUG("QDL Erase");
    // QC QDL, mtk does not need to implement this function
    return false;
}

bool FirmwareFlashImplMTKUSB::EdlEraseAll() {
    FMTLOG_DEBUG("Edl Erase, MTK Erase All");

    return flash_lib->EraseAll();
};

bool FirmwareFlashImplMTKUSB::EdlErase(int start_addr, int partition_size) {
    FMTLOG_DEBUG("Enter Edl Erase, start_addr: {}, partition_size: {}",
                 start_addr, partition_size);
    return flash_lib->Erase(start_addr, partition_size);
}

void FirmwareFlashImplMTKUSB::UpdateProgress(int progress,
                                             const QString& messages) {
    FlashImageComplete(progress, messages);
}

} // namespace afal

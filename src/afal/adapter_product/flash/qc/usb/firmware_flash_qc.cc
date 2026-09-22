#include "firmware_flash_qc.h"
#include "proccommon.h"
#include "firmware_flash.h"
#include "firmware_flash_interface.h"
#include "flash_tools.h"
#include "modem.h"
#include "osinfo.hpp"
#include <memory>
#include <optional>
#include <qdom.h>
#include <qfileinfo.h>
#include <spdlog/spdlog.h>
#ifdef NEED_UNISTD
#include <unistd.h>
#endif
#include <qregularexpression.h>

namespace afal {
    using namespace afal::osinfo;
namespace {
const QString K_SWITCH_FASTBOOT_AT = "AT+SYSCMD=\"sys_reboot bootloader\"";
const QString K_SWITCH_QDL_AT = "AT+SYSCMD=\"sys_reboot edl\"";
int progress_time = 0;

const std::vector<DeviceId> K_MODEM_MAP = {
    {"33f8", ""}, {"2cb7", ""}, {"413c", ""}, // For test.
};

const std::vector<DeviceId> K_FASTBOOT_MAP = {
    {"33f8", "d00d"},
    {"2cb7", "d00d"},
    {"413c", "d00d"},
};

const std::vector<DeviceId> K_RECOVER_MAP = {
    {"05c6", "9008"},
};

const int K_RECOVERY_TIMEOUT = 60;

using namespace log;

std::pair<int, int> ReadEraseAndProgramNumberFromXml(const QString& xml_path) {
    int erase_count = 0;
    int program_count = 0;

    QFile file(xml_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        FMTLOG_ERROR("Failed to open file:{}", xml_path);
        return {erase_count, program_count};
    }

    QDomDocument doc;
    if (!doc.setContent(&file))
    {
        FMTLOG_ERROR("Failed to parse XML content.");
        file.close();
        return {erase_count, program_count};
    }
    file.close();

    QDomElement root = doc.documentElement();
    if (root.tagName() != "data")
    {
        FMTLOG_ERROR("Unexpected XML format: missing root <data> element.");
        return {erase_count, program_count};
    }

    QDomNodeList eraseNodes = root.elementsByTagName("erase");
    QDomNodeList programNodes = root.elementsByTagName("program");

    erase_count = eraseNodes.size();
    program_count = programNodes.size();

    return {erase_count, program_count};
}

} // namespace

bool FirmwareFlashImplQcomUSB::WaitEdlDevice() {
    if (!edl_device->FindDevice())
    {
        FMTLOG_DEBUG("No EDL device was found");
        return false;
    }
#ifdef _IS_WINDOWS_
    auto edl_tty = dev_factory->CreateDevice("usb", K_RECOVER_MAP);
#else
    auto edl_tty = dev_factory->CreateDevice("tty", K_RECOVER_MAP);
#endif
    if (!edl_tty->FindDevice())
    {
        RebootModem();
        if (!edl_tty->FindDevice() &&
            !edl_tty->WaitForAction("add", K_RECOVERY_TIMEOUT))
        {
            FMTLOG_DEBUG("No Edl device tty node was found");
            return false;
        }
    }

    return true;
}

FirmwareFlashImplQcomUSB::FirmwareFlashImplQcomUSB(ModemType modem_type)
    : FirmwareFlashImplQcomUSB(modem_type, DeviceFactory::Create()) {}

FirmwareFlashImplQcomUSB::FirmwareFlashImplQcomUSB(
    ModemType modem_type, std::unique_ptr<DeviceFactory> dev_factory)
    : FirmwareFlashImplQcomUSB(modem_type, std::move(dev_factory),
                               Fastboot::Create()) {}

FirmwareFlashImplQcomUSB::FirmwareFlashImplQcomUSB(
    ModemType modem_type, std::unique_ptr<DeviceFactory> dev_factory,
    std::unique_ptr<Fastboot> fastboot)
    : FirmwareFlashImpl(modem_type, std::move(dev_factory)),
      fastboot(std::move(fastboot)) {
    modem = this->dev_factory->CreateModem("usb", K_MODEM_MAP);
    fastboot_device = this->dev_factory->CreateDevice("usb", K_FASTBOOT_MAP);
    edl_device = this->dev_factory->CreateDevice("usb", K_RECOVER_MAP);
}

bool FirmwareFlashImplQcomUSB::SwitchToFastbootMode() {
    FMTLOG_DEBUG("switch to fastboot modem");
    int sendchannel= 1;
    QString command = K_SWITCH_FASTBOOT_AT;
#ifdef _IS_LINUX_
    OSType os_type = GetOSType();
    if (os_type == OSType::CROS)
    {
        command = QString("mbimcli -p -d /dev/cdc-wdm0 --fibocom-set-at-command='%1'").arg("AT+SYSCMD=sys_reboot bootloader");
        sendchannel= 0;
    }
#endif
    return SwitchFlashModeWithAT(modem.get(), fastboot_device.get(),
            command,sendchannel);
}

bool FirmwareFlashImplQcomUSB::SwitchToEDLMode() {
    int sendchannel= 1;
    QString command = K_SWITCH_QDL_AT;
#ifdef _IS_LINUX_
    OSType os_type = GetOSType();
    if (os_type == OSType::CROS)
    {
        command = QString("mbimcli -p -d /dev/cdc-wdm0 --fibocom-set-at-command='%1'").arg("AT+SYSCMD=sys_reboot edl");
        sendchannel=0;
    }
#endif
    return SwitchFlashModeWithAT(modem.get(), edl_device.get(),
            command,sendchannel);
}

bool FirmwareFlashImplQcomUSB::PcieReset() {
    return true;
}

FirmwareFlash::FlashMode FirmwareFlashImplQcomUSB::QueryFlashMode() {
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

void FirmwareFlashImplQcomUSB::EdlConfig(const QString& fw_path,
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
    this->partition_table = partition_table;
    this->firehose_agent = firehose_agent;
    this->ext_xml = ext_xml;

    FMTLOG_DEBUG("Set Edl Config : {}, {}, {}, {}, {}, {}", fw_path, log_path,
                 partition_table, firehose_agent, auth_file, ext_xml);
}

bool FirmwareFlashImplQcomUSB::EdlFlash() {
    FMTLOG_DEBUG("Enter Edl Flash");
    if (fw_path.isEmpty() || partition_table.isEmpty() ||
        firehose_agent.isEmpty() || ext_xml.isEmpty())
    {
        FMTLOG_ERROR("Config is failed, Edl flash failed!");
        return false;
    }
#if defined(_IS_LINUX_) 
    if (!WaitEdlDevice())
    {
        return false;
    }
#endif

    FlashImageComplete(0, "start");
    auto count = ReadEraseAndProgramNumberFromXml(partition_table);
    erase_count = count.first;
    program_count = count.second;
    FMTLOG_DEBUG("Erase count: {}, Program count: {}", erase_count,
                 program_count);
 
#ifdef _IS_WINDOWS_
    progress_time = 0;
    progress_step = 4;
#elif defined(_IS_LINUX_) 
    progress_step =
        100.0 / (erase_count + program_count != 0 ? erase_count + program_count
                                                  : 100.0);
#endif
    progress = 0;
    auto ret = RunFlashTools();
    if (ret)
    {
        FlashImageComplete(100, "Success"); // NOLINT
    }
    else
    {
        FMTLOG_ERROR("Qdl run failed");
    }
    return ret;
}

bool FirmwareFlashImplQcomUSB::EdlRead(const QString& save_path,
                                       std::optional<int> start_addr,
                                       std::optional<int> partition_size) {
    FMTLOG_DEBUG(
        "Enter Edl Read,save_path: {} start_addr: {}, partition_size: {}",
        save_path,
        start_addr.has_value() ? std::to_string(*start_addr) : "NONE",
        start_addr.has_value() ? std::to_string(*partition_size) : "NONE");

    if (start_addr != std::nullopt || partition_size != std::nullopt)
    {
        return false;
    }

    if (fw_path.isEmpty() || partition_table.isEmpty() ||
        firehose_agent.isEmpty())
    {
        FMTLOG_ERROR("Config is failed, Edl flash failed!");
        return false;
    }

    if (!WaitEdlDevice())
    {
        return false;
    }

    if (!RunFlashTools())
    {
        FMTLOG_ERROR("Qdl run failed");
        return false;
    }

    return true;
}

bool FirmwareFlashImplQcomUSB::EdlErase() {
    FMTLOG_DEBUG("Edl Erase, QDL Erase");

    if (partition_table.isEmpty() || firehose_agent.isEmpty())
    {
        FMTLOG_ERROR("Config is failed, Edl flash failed!");
        return false;
    }

    if (!WaitEdlDevice())
    {
        return false;
    }

    if (!RunFlashTools())
    {
        FMTLOG_ERROR("Qdl run failed");
        return false;
    }

    return true;
}

bool FirmwareFlashImplQcomUSB::EdlEraseAll() {
    FMTLOG_DEBUG("Edl Erase, MTK Erase All");
    return false;
};

bool FirmwareFlashImplQcomUSB::EdlErase(int start_addr, int partition_size) {
    FMTLOG_DEBUG("Enter Edl Erase, start_addr: {}, partition_size: {}",
                 start_addr, partition_size);
    return false;
}

void FirmwareFlashImplQcomUSB::HandleProcessOutput(QString output) {
    FMTLOG_DEBUG("QDL Flash output :progress {}, output {}", progress, output);
#ifdef _IS_WINDOWS_
    progress_time = progress_time + 1;
    if (int(progress_step * progress_time) < 100)
    {
        FlashImageComplete(int(progress_step * progress_time),
                           "Flash In Progress");
    }
    
#elif defined(_IS_LINUX_)
    QRegularExpression re(R"(\[.*\])");
    if (output.contains(re))
    {
        if (progress_step > 0 && progress_step < 100.0)
        {
            progress += progress_step;
        }
        else
        {
            progress += 1;
        }

        if (progress >= 100.0)
        {
            progress = 100.0 - 1;
        }
        FlashImageComplete(int(progress), output);
    }
#endif
}

void FirmwareFlashImplQcomUSB::HandleProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus) {
    FMTLOG_DEBUG("QDL Flash finished, exit: {}, exitstatus: {}", exitCode,
                 int(exitStatus));
}

} // namespace afal

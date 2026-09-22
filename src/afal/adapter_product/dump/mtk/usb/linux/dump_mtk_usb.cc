#include "dump_mtk_usb.h"
#include "common/modem.h"
#include "common/proccommon.h"
#include "dump_interface.h"
namespace afal {
namespace {

using namespace log;

} // namespace

#if 0
const QString SET_MODEM_CRASH_TYPE_NONE_CMD1	="at+gtshell=mrdump_tool output-set none";
const QString SET_MODEM_CRASH_TYPE_MINI_CMD1	="at+gtshell=mrdump_tool output-set mini";
const QString SET_MODEM_CRASH_TYPE_PCIE_CMD1    ="at+gtshell=mrdump_tool output-set pcie";
const QString QUERY_MODEM_CRASH_TYPE_CMD1       ="at+gtshell=mrdump_tool output-get";
#endif 

const QString kUsbAdbTool = "/usr/bin/adb";
const QString K_TRIGGER_DUMP_AT = "at+eswla=0";
const QString kUsbAdbShell = "shell";
const QString kMRDump = "mrdump_tool";
const QString kOutPutSet = "output-set";
const QString kOutPutGet = "output-get";
const QString kTimeOutSet = "timeout-set";
const QString kOutPutSetPcie = "mini";
const QString kOutPutSetNone = "none";
const int kTimeOutSetEnable = 120;
const int kTimeOutSetDisable = 0;

const QString kDumpPull = "pull";
const QString kDumpPullPath = "/data/log/aee_exp";
const QString kDumpRemovePath = "/data/log/aee_exp/*";

const QString kKillServerCmd = "kill-server";
const int K_TIMEOUT = 300 * 1000;
DumpCaptureMTKUsb::DumpCaptureMTKUsb(ModemType modem_type)
    : DumpCaptureMTKUsb(modem_type, DeviceFactory::Create()) {}

const std::vector<DeviceId> K_DEVICE_IDS = {
    {"0x14c3", "0x4d75"},
};
DumpCaptureMTKUsb::DumpCaptureMTKUsb(
    ModemType modem_type, std::unique_ptr<DeviceFactory> device_factory)
    : DumpCaptureImpl(modem_type), device_factory(std::move(device_factory)) {
    //modem = this->device_factory->CreateModem("usb", {});
    //dump_device = this->device_factory->CreateDevice("usb", K_DEVICE_IDS);
}

bool DumpCaptureMTKUsb::Initialize()
{
    try {
        if (!device_factory) {
            FMTLOG_ERROR("DeviceFactory is null");
            return false;
        }

        modem = device_factory->CreateModem("usb", K_DEVICE_IDS);
        if (!modem) {
            FMTLOG_ERROR("CreateModem returned null");
            return false;
        }
    } catch (const std::exception &e) {
        FMTLOG_ERROR("Exception in DumpCaptureMTKPcie::Initialize: {}", e.what());
        return false;
    } catch (...) {
        FMTLOG_ERROR("Unknown exception in DumpCaptureMTKPcie::Initialize");
        return false;
    }

    FMTLOG_INFO("DumpCaptureMTKPcie initialized successfully");
    return true;
}

#if 0
std::map<ModemDumpType, DumpStatus> DumpCaptureMTKUsb::Status() {
    std::map<ModemDumpType, DumpStatus> ret = {
        {ModemDumpType::AP_DUMP, DumpStatus::NONE},
        {ModemDumpType::FULL_DUMP, DumpStatus::NONE},
        {ModemDumpType::MODEM_DUMP, DumpStatus::NONE},
    };

    QString output;

    auto strAtResult = modem->SendATCommand(QUERY_MODEM_CRASH_TYPE_CMD1);

    if (strAtResult.has_value() && strAtResult->contains("none")) {
        LOG_DEBUG("Modem dump category: None\n");
    } else if (strAtResult.has_value() && strAtResult->contains("mini")) {
        ret.at(ModemDumpType::AP_DUMP) = DumpStatus::ENABLE;
        LOG_DEBUG("Modem dump category: Mini\n");
    } else if(strAtResult.has_value() && strAtResult->contains("pcie")) {
        ret.at(ModemDumpType::FULL_DUMP) = DumpStatus::ENABLE;
        LOG_DEBUG("Modem dump category: PCIe\n");
    } else if(strAtResult.has_value() && strAtResult->contains("usb")) {
        ret.at(ModemDumpType::FULL_DUMP) = DumpStatus::DISABLE;
        LOG_DEBUG("Modem dump category: USB\n");
    }

    return ret;
};
bool DumpCaptureMTKUsb::Control(ModemDumpType dump_type,
    DumpStatus dump_status) {

    if (dump_type == ModemDumpType::NONE && dump_status == DumpStatus::DISABLE)
    {    
        if (modem->SendATCommand(SET_MODEM_CRASH_TYPE_NONE_CMD1))
        {
            LOG_DEBUG("Modem dump category: None\n");
        }
    }
    else if (dump_status == DumpStatus::ENABLE)
    {        
        if (dump_type == ModemDumpType::MINI_DUMP) {
            if (modem->SendATCommand(SET_MODEM_CRASH_TYPE_MINI_CMD1))
            {
                LOG_DEBUG("Modem dump category: Mini\n");
            }
        } else if (dump_type == ModemDumpType::FULL_DUMP) {
            if (modem->SendATCommand(SET_MODEM_CRASH_TYPE_PCIE_CMD1))
            {
                LOG_DEBUG("Modem dump category: PCIe\n");
            }
        }        
    }
    else
    {
        return false;
    }

    return true;
};
#endif

std::map<ModemDumpType, DumpStatus> DumpCaptureMTKUsb::Status() {
    std::map<ModemDumpType, DumpStatus> ret = {
        {ModemDumpType::AP_DUMP, DumpStatus::NONE},
        {ModemDumpType::FULL_DUMP, DumpStatus::NONE},
        {ModemDumpType::MODEM_DUMP, DumpStatus::NONE},
    };

    QString output;

    if (0 != RunProcess(kUsbAdbTool,
                       {kUsbAdbShell, kMRDump, kOutPutGet},
                       K_TIMEOUT, &output)) {
                        FMTLOG_ERROR("{} {} failed to set, process output {}", kMRDump, kOutPutGet,
                         output);
                         return ret;
                       }

    if (output.contains("pcie")) {
        ret.at(ModemDumpType::FULL_DUMP) = DumpStatus::ENABLE;
    } else {
        ret.at(ModemDumpType::FULL_DUMP) = DumpStatus::DISABLE;
    }

    return ret;
};

bool DumpCaptureMTKUsb::Control(ModemDumpType dump_type,
                                 DumpStatus dump_status) {
#if 0                            
    if (dump_type != ModemDumpType::FULL_DUMP)
    {
        FMTLOG_DEBUG("Unsupported dump type");
        return false;
    }
#endif
    QString command = QString("%1 %2").arg(kUsbAdbTool, kKillServerCmd);
    
    if (0 !=
        RunProcess("bash", {"-c", command},
            K_TIMEOUT))
    {
        FMTLOG_ERROR("{} failed to set.", kKillServerCmd);
    }

    if (dump_status == DumpStatus::DISABLE)
    {
        command = QString("%1 %2 %3 %4 %5").arg(kUsbAdbTool, kUsbAdbShell, kMRDump, kOutPutSet, kOutPutSetNone);
        if (0 !=
            RunProcess("bash", {"-c", command},
                       K_TIMEOUT))
        {
            FMTLOG_ERROR("{} {} {} failed to set.", kMRDump, kOutPutSet,
                         kOutPutSetNone);
        }
        command = QString("%1 %2 %3 %4 %5").arg(kUsbAdbTool, kUsbAdbShell, kMRDump, kTimeOutSet,  QString::number(kTimeOutSetDisable));
        if (0 != RunProcess("bash", {"-c", command},
                            K_TIMEOUT))
        {
            FMTLOG_ERROR("{} {} {} failed to set.", kMRDump, kTimeOutSet,
                         QString::number(kTimeOutSetDisable));
        }
    }
    else if (dump_status == DumpStatus::ENABLE)
    {
        command = QString("%1 %2 %3 %4 %5").arg(kUsbAdbTool,kUsbAdbShell, kMRDump, kOutPutSet, kOutPutSetPcie);
        if (0 !=
            RunProcess("bash", {"-c", command},
                       K_TIMEOUT))
        {
            FMTLOG_ERROR("{} {} {} failed to set.", kMRDump, kOutPutSet,
                         kOutPutSetPcie);
        }
        command = QString("%1 %2 %3 %4 %5").arg(kUsbAdbTool, kUsbAdbShell, kMRDump, kTimeOutSet,  QString::number(kTimeOutSetEnable));
        if (0 != RunProcess("bash", {"-c", command},
                            K_TIMEOUT))
        {
            FMTLOG_ERROR("{} {} {} failed to set.", kMRDump, kTimeOutSet,
                QString::number(kTimeOutSetEnable));
        }
    }
    else
    {
        return false;
    }

    return true;
};

ModemDumpType DumpCaptureMTKUsb::QueryDump() {
    return dump_device->FindDevice() ? ModemDumpType::FULL_DUMP
                                     : ModemDumpType::NONE;
};

bool DumpCaptureMTKUsb::Start() {
    QString command = QString("%1 %2").arg(kUsbAdbTool, kKillServerCmd);
    
    if (0 !=
        RunProcess("bash", {"-c", command},
            K_TIMEOUT))
    {
        FMTLOG_ERROR("{} failed to set.", kKillServerCmd);
    }


    command = QString("%1 %2 %3 %4").arg(kUsbAdbTool, kDumpPull, kDumpPullPath, QString::fromStdString(config->dump_path));
    if (0 !=
        RunProcess("bash", {"-c", command},
            K_TIMEOUT))
    {
        FMTLOG_ERROR("{} {} {} failed to set.", kDumpPull, kDumpPullPath,
            config->dump_path);
        return false;
    }

    QString adbRm = "adb shell \"busybox rm -rf " + kDumpRemovePath + "\"";
    command = QString("%1").arg(adbRm);
    if (0 !=
        RunProcess("bash", {"-c", command},
            K_TIMEOUT))
    {
        FMTLOG_ERROR("{} failed to set.",adbRm);
        return false;
    }

    
    return true;
};

bool DumpCaptureMTKUsb::ForceDumpTrigger(ModemDumpType dump_type) {
        return true;
};

bool DumpCaptureMTKUsb::StartMiniDumpMonitor() {
    return true;
}

bool DumpCaptureMTKUsb::StopMiniDumpMonitor() {
    return true;
}

void DumpCaptureMTKUsb::HandleProcessOutput(QString output) {
    FMTLOG_DEBUG("{}", output);
}

void DumpCaptureMTKUsb::HandleProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus) {
    FMTLOG_INFO("{}, {}", exitCode, int(exitStatus));
}

bool DumpCaptureMTKUsb::setDebugPort(int mode)
{
    FMTLOG_DEBUG("enter {}:",__func__);
#if false
    QString command = QString("at+gtusbmode=%1").arg(mode);
    if (modem->SendATCommand(command))
    {
        LOG_DEBUG("set mode");
    }
#endif
    return true;
}
} // namespace afal

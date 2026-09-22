#include "dump_mtk_pcie.h"
#include "common/proccommon.h"
#include "common/commonprocess.h"
#include "dump_interface.h"

#include <qfileinfo.h>
#include <QString>
namespace afal {

namespace {

using namespace log;

const QString K_DUMP_STATE = "fastboot_dump";

const QString K_TRIGGER_DUMP_AT = "at+eswla=0";

const QString kDumpPull = "pull";
const QString kDumpPullPath = "/data/log/aee_exp";

const QString kDumpRemove = "rm";
const QString kDumpRemoveArg = "-r";
const QString kDumpRemovePath = "/data/log/aee_exp/*";
const QString kDumpRemovePath1 = "/data/log/aee_exp/db.*";
const QString kDumpRemovePath2 = "/data/log/aee_exp/md_log*/*";

const QString kKillServerCmd = "kill-server";
const int K_TIMEOUT = 300 * 1000;

const QString kLogToolPath = "dumplog";
const QString kLogSavePath = "-f";

const QString kPcieAdbTool = OS_PREFIX + QString("/libs/adapter_product/tools/mtk/pcie-adb");
const QString kPcieAdbDevice = "0123456mediatek";
const QString kPcieAdbToolPar ="-s";
const QString kPcieAdbShell = "shell";
const QString kMRDump = "mrdump_tool";
const QString kOutPutSet = "output-set";
const QString kOutPutGet = "output-get";
const QString kTimeOutSet = "timeout-set";
const QString kOutPutSetPcie = "mini";
const QString kOutPutSetNone = "none";

const int kTimeOutSetEnable = 120;
const int kTimeOutSetDisable = 0;
const QString SET_ADB_PORT_SECURITY_ENABLE = "at+gtsectest=1";
const QString SET_ADB_PORT_SECURITY_DISABLE = "at+gtsectest=0";

const std::vector<DeviceId> K_DEVICE_IDS = {
    {"0x14c3", "0x4d75"},
};

std::string GetDumpName(ModemDumpType dump_type) {
    switch (dump_type)
    {
        case afal::ModemDumpType::AP_DUMP:
            return "AP_DUMP";
        case afal::ModemDumpType::FULL_DUMP:
            return "FULL_DUMP";
        case afal::ModemDumpType::MODEM_DUMP:
            return "MODEM_DUMP";
        default:
            return "NONE_DUMP";
    }
}

constexpr char K_MTK_PCIE_NODE_PATH_FORMAT[] = // NOLINT
    "/sys/bus/pci/devices/{}/t7xx_mode";

} // namespace
DumpCaptureMTKPcie::DumpCaptureMTKPcie(ModemType modem_type)
    : DumpCaptureMTKPcie(modem_type, DeviceFactory::Create()) {}

DumpCaptureMTKPcie::DumpCaptureMTKPcie(
    ModemType modem_type, std::unique_ptr<DeviceFactory> device_factory)
    : DumpCaptureImpl(modem_type), device_factory(std::move(device_factory)) {
    //modem = this->device_factory->CreateModem("pci", K_DEVICE_IDS);
}

bool DumpCaptureMTKPcie::Initialize()
{
    try {
        if (!device_factory) {
            FMTLOG_ERROR("DeviceFactory is null");
            return false;
        }

        modem = device_factory->CreateModem("pci", K_DEVICE_IDS);
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
std::map<ModemDumpType, DumpStatus> DumpCaptureMTKPcie::Status() {
    std::map<ModemDumpType, DumpStatus> ret = {
        {ModemDumpType::AP_DUMP, DumpStatus::NONE},
        {ModemDumpType::FULL_DUMP, DumpStatus::NONE},
        {ModemDumpType::MODEM_DUMP, DumpStatus::NONE},
    };

    QString output;

    if (0 != RunProcess(kPcieAdbTool,
                       {kPcieAdbShell, kMRDump, kOutPutGet},
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

bool DumpCaptureMTKPcie::Control(ModemDumpType dump_type,
                                 DumpStatus dump_status) {
#if 0 
    if (dump_type != ModemDumpType::FULL_DUMP)
    {
        FMTLOG_DEBUG("Unsupported dump type : {}", GetDumpName(dump_type));
        return false;
    }
#endif
    QString command = QString("%1 %2").arg(kPcieAdbTool, kKillServerCmd);
    
    if (0 !=
        RunProcess("bash", {"-c", command},
            K_TIMEOUT))
    {
        FMTLOG_ERROR("{} failed to set.", kKillServerCmd);
    }

    if (dump_status == DumpStatus::DISABLE)
    {
        command = QString("%1 %2 %3 %4 %5 %6 %7").arg(kPcieAdbTool, kPcieAdbToolPar,kPcieAdbDevice,kPcieAdbShell, kMRDump, kOutPutSet, kOutPutSetNone);
        if (0 !=
            RunProcess("bash", {"-c", command},
                       K_TIMEOUT))
        {
            FMTLOG_ERROR("{} {} {} failed to set.", kMRDump, kOutPutSet,
                         kOutPutSetNone);
        }
        command = QString("%1 %2 %3 %4 %5 %6 %7").arg(kPcieAdbTool, kPcieAdbToolPar,kPcieAdbDevice,kPcieAdbShell, kMRDump, kTimeOutSet,  QString::number(kTimeOutSetDisable));
        if (0 != RunProcess("bash", {"-c", command},
                            K_TIMEOUT))
        {
            FMTLOG_ERROR("{} {} {} failed to set.", kMRDump, kTimeOutSet,
                         QString::number(kTimeOutSetDisable));
        }
    }
    else if (dump_status == DumpStatus::ENABLE)
    {
        command = QString("%1 %2 %3 %4 %5 %6 %7").arg(kPcieAdbTool, kPcieAdbToolPar,kPcieAdbDevice,kPcieAdbShell, kMRDump, kOutPutSet, kOutPutSetPcie);
        if (0 !=
            RunProcess("bash", {"-c", command},
                       K_TIMEOUT))
        {
            FMTLOG_ERROR("{} {} {} failed to set.", kMRDump, kOutPutSet,
                         kOutPutSetPcie);
        }
        command = QString("%1 %2 %3 %4 %5 %6 %7").arg(kPcieAdbTool, kPcieAdbToolPar,kPcieAdbDevice,kPcieAdbShell, kMRDump, kTimeOutSet,  QString::number(kTimeOutSetEnable));
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

bool DumpCaptureMTKPcie::StartMiniDumpMonitor() {
    return true;
}

bool DumpCaptureMTKPcie::StopMiniDumpMonitor() {
    return true;
}

ModemDumpType DumpCaptureMTKPcie::QueryDump() {
    auto bdf = modem->GetBDF();
    if (!bdf.has_value())
    {
        return ModemDumpType::NONE;
    }

    auto state_node =
        fmt::format(K_MTK_PCIE_NODE_PATH_FORMAT, (*bdf).toStdString());
    QFile state_node_file = QString::fromStdString(state_node);

    if (!state_node_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        FMTLOG_ERROR("Failed to open the file! {}", state_node);
        return ModemDumpType::NONE;
    }
    QString modem_state_str = state_node_file.readAll();
    state_node_file.close();

    FMTLOG_DEBUG("Modem state: {}", modem_state_str);

    modem_state_str.remove("\n");
    modem_state_str.remove("\r");
    return modem_state_str == K_DUMP_STATE ? ModemDumpType::FULL_DUMP
                                           : ModemDumpType::NONE;
};

bool DumpCaptureMTKPcie::setDebugPort(int mode)
{
    FMTLOG_DEBUG("enter:",__func__);
    if (mode != 0 && mode != 1) {
       FMTLOG_DEBUG("Invalid mode, must be 0 or 1");
       return false;
    }

    QString bdf = QString();  
    auto bdfOpt = modem->GetBDF();
    if (bdfOpt.has_value()) {
        bdf = bdfOpt.value();
        FMTLOG_DEBUG("BDF:{}", bdf);
    } else {
        FMTLOG_DEBUG("No BDF found!");
        return false;
    }

    if (0 !=RunProcess("bash",{"-c", QString("echo %1 > /sys/bus/pci/devices/%2/t7xx_debug_ports").arg(mode).arg(bdf)},K_TIMEOUT))
    {
        FMTLOG_ERROR("{} {} {} failed to set.", kMRDump, kOutPutSet,
                     kOutPutSetPcie);
        return false;
    }

    return true;
}

bool DumpCaptureMTKPcie::Start() {
    /*
    ProcessWithOutput process(
        kLogToolPath, {kLogSavePath, QString::fromStdString(config->dump_path)},
        K_TIMEOUT, this);
    return 0 == process.Run();
    */
    bool bret = true;
    if (modem->SendATCommand(SET_ADB_PORT_SECURITY_ENABLE))
    {
        LOG_DEBUG("ADB port security setting done - enable\n");
    }

    QString command = QString("%1 %2").arg(kPcieAdbTool, kKillServerCmd);
    
    if (0 !=
        RunProcess("bash", {"-c", command},
            K_TIMEOUT))
    {
        FMTLOG_ERROR("{} failed to set.", kKillServerCmd);
    }


    command = QString("%1 %2 %3 %4 %5 %6").arg(kPcieAdbTool, kPcieAdbToolPar,kPcieAdbDevice,kDumpPull, kDumpPullPath, QString::fromStdString(config->dump_path));
    if (0 !=
        RunProcess("bash", {"-c", command},
            K_TIMEOUT))
    {
        FMTLOG_ERROR("{} {} {} failed to set.", kDumpPull, kDumpPullPath,
            config->dump_path);
        bret = false;
    }

    command = QString("%1 %2 %3 %4 %5 %6 %7").arg(kPcieAdbTool, kPcieAdbToolPar,kPcieAdbDevice,kPcieAdbShell, kDumpRemove, kDumpRemoveArg,kDumpRemovePath);
    if (0 !=
        RunProcess("bash", {"-c", command},
            K_TIMEOUT))
    {
        FMTLOG_ERROR("{} {} {} failed to set.", kDumpRemove, kDumpRemoveArg,
            kDumpRemovePath);
        bret = false;
    }

    if (modem->SendATCommand(SET_ADB_PORT_SECURITY_DISABLE))
    {
        LOG_DEBUG("ADB port security setting done - disable\n");
    }

    return bret;
};

bool DumpCaptureMTKPcie::ForceDumpTrigger(ModemDumpType dump_type) {
    if (dump_type != ModemDumpType::FULL_DUMP)
    {
        FMTLOG_DEBUG("Unsupported dump type : {}", GetDumpName(dump_type));
        return false;
    }

    if (!modem->SendATCommand(K_TRIGGER_DUMP_AT))
    {
        FMTLOG_DEBUG("MTK force trigger dump failed.");
        return false;
    }
    FMTLOG_INFO("Force trigger modem dump success!");
    return true;
};

void DumpCaptureMTKPcie::HandleProcessOutput(QString output) {
    FMTLOG_DEBUG("{}", output);
}

void DumpCaptureMTKPcie::HandleProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus) {
    FMTLOG_INFO("{}, {}", exitCode, int(exitStatus));
}

} // namespace afal

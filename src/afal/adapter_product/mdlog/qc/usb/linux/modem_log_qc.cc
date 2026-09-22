#include "qc/usb/linux/modem_log_qc.h"
#include "common/modem.h"
#include "common/proccommon.h"
#include "log.hpp"
#include <QTimer>
#include <QFileInfo>
#include <memory>

namespace afal {

namespace {

const QString K_AT_DIAG_EN = "AT+GTDIAGEN?";
const QString K_AT_DIAG_EN_ON = "AT+GTDIAGEN=1,1";
const QString K_AT_DIAG_EN_OFF = "AT+GTDIAGEN=0,1";

using namespace log;

QString OfflinePullDest(const ModemLogger::Config* config) {
    if (!config || config->mdlog_path.empty()) {
        return QString();
    }
    return QFileInfo(QString::fromStdString(config->mdlog_path)).absolutePath() +
           "/offline";
}

QString OfflineIpcPullDest(const ModemLogger::Config* config) {
    if (!config || config->mdlog_path.empty()) {
        return QString();
    }
    return QFileInfo(QString::fromStdString(config->mdlog_path)).absolutePath() +
           "/offline/ipc";
}

bool IpcLogEnabled(const ModemLogger::Config* config) {
    return config && config->ipc_log_ctrl;
}

} // namespace

bool loggingStatus = false;
ModemLoggerImplQcUSB::ModemLoggerImplQcUSB(ModemType modem_type)
    : ModemLoggerImplQcUSB(modem_type, DeviceFactory::Create()) {}

ModemLoggerImplQcUSB::ModemLoggerImplQcUSB(
    ModemType modem_type, std::unique_ptr<DeviceFactory> device_factory)
    : ModemLoggerImpl(modem_type), device_factory(std::move(device_factory)) {
    modem = this->device_factory->CreateModem("usb", {});
}

bool ModemLoggerImplQcUSB::onProcessStop(){
    FMTLOG_INFO("logging tool stopped unexpectedly, try restarting...");
    if (loggingStatus) {        
        FMTLOG_INFO("logging tool restarting...");
        QTimer::singleShot(0, this, [this]() {
            if (!modem_log_tools)
                modem_log_tools = ModemLogTools::Create(modem_type, config.get());
            if (modem_log_tools)
                modem_log_tools->Start(modem_type);
            FMTLOG_INFO("logging tool restarted safely.");
        });
    }   
}

bool ModemLoggerImplQcUSB::Status() {
    FMTLOG_INFO("Status Query");
    auto result = modem->SendATCommand(GetStatusAT());

    if (!result.has_value()) {
        FMTLOG_ERROR("SendATCommand failed, empty result");
        return false;
    }
    QString resp = result.value().trimmed();  
    FMTLOG_INFO("Trimmed AT Response: [{}]", resp);
    return resp.contains(GetKeyword(), Qt::CaseInsensitive);
}

bool ModemLoggerImplQcUSB::sapStart() {
    FMTLOG_INFO("offline log: install and start on-device diag_mdlog");
    bool ok = diag_mdlog_.Install();
    if (IpcLogEnabled(config.get())) {
        FMTLOG_INFO("offline log: install and start on-device collect_ipc_log");
        ok = diag_mdlog_.IpcInstall() && diag_mdlog_.IpcStart() && ok;
    }
    return ok;
}

bool ModemLoggerImplQcUSB::sapStop() {
    FMTLOG_INFO("offline log: stop, pull, resume on-device diag_mdlog");
    bool ok = diag_mdlog_.StopPullResume(OfflinePullDest(config.get()));
    if (IpcLogEnabled(config.get())) {
        FMTLOG_INFO("offline log: stop, pull, resume on-device collect_ipc_log");
        ok = diag_mdlog_.IpcStopPullResume(OfflineIpcPullDest(config.get())) && ok;
    }
    return ok;
}

bool ModemLoggerImplQcUSB::OfflineLogUninstall() {
    FMTLOG_INFO("offline log: uninstall on-device diag_mdlog");
    return diag_mdlog_.Uninstall();
}

bool ModemLoggerImplQcUSB::OfflineIpcLogUninstall() {
    FMTLOG_INFO("offline log: uninstall on-device collect_ipc_log");
    return diag_mdlog_.IpcUninstall();
}

bool ModemLoggerImplQcUSB::Start() {
    FMTLOG_INFO("Start capture modem log");
    if (!config) {
        return false;
    }

    if (!modem_log_tools) {
        modem_log_tools = ModemLogTools::Create(modem_type, config.get());
    }

    if (!modem_log_tools) {
        FMTLOG_ERROR("Modem log tool create failed!");
        return false;
    }
    loggingStatus = true;
    connect(modem_log_tools.get(), &ModemLogTools::ToolStopSignal, this,
    &ModemLoggerImplQcUSB::onProcessStop);
    return modem_log_tools->Start(modem_type);
}

bool ModemLoggerImplQcUSB::Stop() {
    if (!modem_log_tools) {
        FMTLOG_ERROR("Modem log tool is nullptr!!!");
        return false;
    }
    loggingStatus = false;
    auto ret = modem_log_tools->Stop();
    modem_log_tools->quit();
    modem_log_tools->wait();
    modem_log_tools.reset();
    return ret;
}
bool ModemLoggerImplQcUSB::modeStatus() {
    return false;
}

bool ModemLoggerImplQcUSB::Enable() {
    auto result = modem->SendATCommand(GetEnableAT());
    return result.has_value() ?
           result.value().contains("OK", Qt::CaseInsensitive) : false;
}

bool ModemLoggerImplQcUSB::Disable() {
    auto result = modem->SendATCommand(GetDisableAT());
    return result.has_value() ?
           result.value().contains("OK", Qt::CaseInsensitive) : false;
}

bool ModemLoggerImplQcUSB::SetDebugPort(bool enable) {
    return true;
}

bool ModemLoggerImplQcUSB::CheckDebugPortStatus() {
    return true;
}

} // namespace afal
#include "modem_log_mtk_usb.h"
#include "common/modem.h"
#include "common/proccommon.h"
#include "log.hpp"
#include <memory>

namespace afal {
    using namespace log;
ModemLoggerImplMtkUSB::ModemLoggerImplMtkUSB(ModemType modem_type)
    : ModemLoggerImplPcie(modem_type)
{

}
ModemLoggerImplMtkUSB::ModemLoggerImplMtkUSB(
    ModemType modem_type,
    std::unique_ptr<DeviceFactory> device_factory)
    : ModemLoggerImplPcie(modem_type, std::move(device_factory))
{
}

bool ModemLoggerImplMtkUSB::sapStart() {
    FMTLOG_INFO("Start capture sap log");
    if (!config)
    {
        return false;
    }    

    if (!adbLogReaderUSB)
    {
        adbLogReaderUSB = std::make_unique<AdbLogReaderusb>();
    }
    adbLogReaderUSB->start(QString::fromStdString(config->mdlog_path));
    LOG_DEBUG("sap is started");

    return true;
}

bool ModemLoggerImplMtkUSB::sapStop() {
    if (adbLogReaderUSB)
    {
        adbLogReaderUSB->stop();
    }

    return true;
}

AdbLogReaderusb::AdbLogReaderusb(QObject* parent)
    : AdbLogReader(parent)
{
}
QString AdbLogReaderusb::program()
{
    return "adb";
}

QStringList AdbLogReaderusb::arguments()
{
    QStringList arguments;
    arguments << "shell" << "logread" << "-f";
    return arguments;
}

} // namespace afal
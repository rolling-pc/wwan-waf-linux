#include "qc/usb/linux/rw135R/modem_log_qc_135R.h"
#include "common/modem.h"
#include "common/proccommon.h"
#include "log.hpp"
#include <QTimer>
#include <memory>

namespace afal {
    ModemLoggerImplQc135RUSB::ModemLoggerImplQc135RUSB(
        ModemType modem_type)
        : ModemLoggerImplQc135RUSB(
              modem_type,
              DeviceFactory::Create()) {
    }
    
    ModemLoggerImplQc135RUSB::ModemLoggerImplQc135RUSB(
        ModemType modem_type,
        std::unique_ptr<DeviceFactory> device_factory)
        : ModemLoggerImplQcUSB(
              modem_type,
              std::move(device_factory)) {
    }

} // namespace afal
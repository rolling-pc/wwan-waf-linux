#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_QC_USB_LINUX_MODEM_LOG_QC_135R_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_QC_USB_LINUX_MODEM_LOG_QC_135R_H_

#include "qc/usb/linux/modem_log_qc.h"

namespace afal {

class ModemLoggerImplQc135RUSB : public ModemLoggerImplQcUSB {
  public:
    ModemLoggerImplQc135RUSB(const ModemLoggerImplQc135RUSB&) = delete;
    ModemLoggerImplQc135RUSB(ModemLoggerImplQc135RUSB&&) = delete;
    ModemLoggerImplQc135RUSB& operator=(const ModemLoggerImplQc135RUSB&) = delete;
    ModemLoggerImplQc135RUSB& operator=(ModemLoggerImplQc135RUSB&&) = delete;
    ModemLoggerImplQc135RUSB() = delete;
    ~ModemLoggerImplQc135RUSB() override = default;

    ModemLoggerImplQc135RUSB(ModemType modem_type);

    ModemLoggerImplQc135RUSB(ModemType modem_type,
                         std::unique_ptr<DeviceFactory> device_factory);

   protected:
    QString GetStatusAT() const override {
        return "AT+RWDIAGEN?";
    }

    QString GetEnableAT() const override{
        return "AT+RWDIAGEN=1,1";
    }

    QString GetDisableAT() const override{
        return "AT+RWDIAGEN=0,1";
    }

    QString GetKeyword() const override{
        return "+RWDIAGEN: 1";
    }
 };

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_MDLOG_QC_USB_LINUX_MODEM_LOG_QC_135R_H_
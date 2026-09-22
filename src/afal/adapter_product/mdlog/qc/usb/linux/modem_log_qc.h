#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_QC_USB_LINUX_MODEM_LOG_QC_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_QC_USB_LINUX_MODEM_LOG_QC_H_

#include "common.h"
#include "modem_log.h"
#include "common/modem.h"
#include "common/commonprocess.h"
#include "common/modem_log_tools.h"
#include "common/diag_mdlog.h"
#include <memory>
#include <qobjectdefs.h>
namespace afal {

class ModemLoggerImplQcUSB : public ModemLoggerImpl {
  public:
    ModemLoggerImplQcUSB(const ModemLoggerImplQcUSB&) = delete;
    ModemLoggerImplQcUSB(ModemLoggerImplQcUSB&&) = delete;
    ModemLoggerImplQcUSB& operator=(const ModemLoggerImplQcUSB&) = delete;
    ModemLoggerImplQcUSB& operator=(ModemLoggerImplQcUSB&&) = delete;
    ModemLoggerImplQcUSB() = delete;
    ~ModemLoggerImplQcUSB() override = default;

    ModemLoggerImplQcUSB(ModemType modem_type);

    ModemLoggerImplQcUSB(ModemType modem_type,
                         std::unique_ptr<DeviceFactory> device_factory);

    bool Status() override;
    bool modeStatus() override;
    bool Start() override;
    bool Stop() override;
    bool Enable() override;
    bool Disable() override;
    bool SetDebugPort(bool enable) override;
    bool CheckDebugPortStatus() override;
    bool sapStart() override;
    bool sapStop() override;
    bool OfflineLogUninstall() override;
    bool OfflineIpcLogUninstall() override;
  public slots:
    bool onProcessStop();

  protected:
    virtual QString GetStatusAT() const {
        return "AT+GTDIAGEN?";
    }

    virtual QString GetEnableAT() const {
        return "AT+GTDIAGEN=1,1";
    }

    virtual QString GetDisableAT() const {
        return "AT+GTDIAGEN=0,1";
    }

    virtual QString GetKeyword() const {
        return "+GTDIAGEN: 1";
    }

    std::unique_ptr<DeviceFactory> device_factory;
    std::unique_ptr<Modem> modem;
    std::unique_ptr<Device> diag_port;

    std::unique_ptr<ModemLogTools> modem_log_tools;
    DiagMdlog diag_mdlog_;
 };

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_MDLOG_QC_USB_LINUX_MODEM_LOG_QC_H_
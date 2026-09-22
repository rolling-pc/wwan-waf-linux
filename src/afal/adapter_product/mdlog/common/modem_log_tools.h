#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_QC_USB_LINUX_MODEM_LOG_TOOLS_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_QC_USB_LINUX_MODEM_LOG_TOOLS_H_

#include <common/commonprocess.h>
#include "modem_log_interface.h"

#include <memory>
#include <qobjectdefs.h>
#include <qthread.h>

namespace afal {

class ModemLogTools : public QThread {
    Q_OBJECT
  public:
    ModemLogTools() = default;
    ModemLogTools(const ModemLogTools&) = delete;
    ModemLogTools(ModemLogTools&&) = delete;
    ModemLogTools& operator=(const ModemLogTools&) = delete;
    ModemLogTools& operator=(ModemLogTools&&) = delete;
    ~ModemLogTools() override = default;

    static std::unique_ptr<ModemLogTools> Create(ModemType modem_type,
                                                 ModemLogger::Config* config);
    static std::unique_ptr<ModemLogTools>
    CreateTest(ModemLogger::Config* config);

    // This Start will send the signal StartSignal to inform the thread to call
    // the corresponding slot function to start the task of Capture Modem log.
    // This needs to be distinguished from QThread::start().
    virtual bool Start(ModemType modemType) = 0;
    virtual bool Stop() = 0;
  signals:
    void StartSignal();

    void StopSignal();
    void ToolStopSignal();

  protected:
    void run() override = 0;
};

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_MDLOG_QC_USB_LINUX_MODEM_LOG_TOOLS_H_
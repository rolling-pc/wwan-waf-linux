#ifndef AFAL_ADAPTER_PRODUCT_DUMP_QC_USB_LINUX_DUMP_QC_H_
#define AFAL_ADAPTER_PRODUCT_DUMP_QC_USB_LINUX_DUMP_QC_H_

#include "common/modem.h"
#include "dump/dump.h"
#include "common/commonprocess.h"

namespace afal {

class DumpCaptureQc : public DumpCaptureImpl,
                      public ProcessWithOutput::Delegate {
  public:
    DumpCaptureQc(const DumpCaptureQc&) = delete;
    DumpCaptureQc(DumpCaptureQc&&) = delete;
    DumpCaptureQc& operator=(const DumpCaptureQc&) = delete;
    DumpCaptureQc& operator=(DumpCaptureQc&&) = delete;

    DumpCaptureQc(ModemType modem_type);

    DumpCaptureQc(ModemType modem_type,
                  std::unique_ptr<DeviceFactory> device_factory);
    ~DumpCaptureQc() override = default;

    std::map<ModemDumpType, DumpStatus> Status() override;
    bool Control(ModemDumpType dump_type = ModemDumpType::FULL_DUMP,
                 DumpStatus dump_status = DumpStatus::ENABLE) override;
    ModemDumpType QueryDump() override;
    bool Start() override;
    bool ForceDumpTrigger(ModemDumpType dump_type) override;
    bool StartMiniDumpMonitor() override;
    bool StopMiniDumpMonitor() override;
    bool Initialize() override;
    // override ProcessWithOutput::Delegate

    void HandleProcessOutput(QString output) override;
    void HandleProcessFinished(int exitCode,
                               QProcess::ExitStatus exitStatus) override;
    bool setDebugPort(int mode) override;

    bool SendAdbCmd_Qt(QStringList args);
    QString GetCmdReturn_Common(QString programPath, QStringList cmd);
    bool DownloadMbnFile(QString strSN, QString mbnFilePath);
    bool StartEnableQCDump();
    
  private:
    std::unique_ptr<DeviceFactory> device_factory;
    std::unique_ptr<Modem> modem;
    std::unique_ptr<Device> dump_device;
};

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_DUMP_QC_USB_LINUX_DUMP_QC_H_

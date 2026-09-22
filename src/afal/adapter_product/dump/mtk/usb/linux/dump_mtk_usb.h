#ifndef AFAL_ADAPTER_PRODUCT_DUMP_MTK_USB_LINUX_DUMP_MTK_PCIE_H_
#define AFAL_ADAPTER_PRODUCT_DUMP_MTK_USB_LINUX_DUMP_MTK_PCIE_H_

#include "common/modem.h"
#include "dump/dump.h"
#include "common/commonprocess.h"
namespace afal {

class DumpCaptureMTKUsb : public DumpCaptureImpl,
                      public ProcessWithOutput::Delegate {
  public:
    DumpCaptureMTKUsb(const DumpCaptureMTKUsb&) = delete;
    DumpCaptureMTKUsb(DumpCaptureMTKUsb&&) = delete;
    DumpCaptureMTKUsb& operator=(const DumpCaptureMTKUsb&) = delete;
    DumpCaptureMTKUsb& operator=(DumpCaptureMTKUsb&&) = delete;

    DumpCaptureMTKUsb(ModemType modem_type);

    DumpCaptureMTKUsb(ModemType modem_type,
                  std::unique_ptr<DeviceFactory> device_factory);
    ~DumpCaptureMTKUsb() override = default;
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

  private:
    std::unique_ptr<DeviceFactory> device_factory;
    std::unique_ptr<Modem> modem;
    std::unique_ptr<Device> dump_device;
};

} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_DUMP_MTK_USB_LINUX_DUMP_MTK_PCIE_H_
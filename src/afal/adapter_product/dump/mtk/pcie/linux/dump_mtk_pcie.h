#ifndef AFAL_ADAPTER_PRODUCT_DUMP_MTK_PCIE_LINUX_DUMP_MTK_PCIE_H_
#define AFAL_ADAPTER_PRODUCT_DUMP_MTK_PCIE_LINUX_DUMP_MTK_PCIE_H_

#include "common/modem.h"
#include "dump/dump.h"
#include "common/commonprocess.h"

namespace afal {

class DumpCaptureMTKPcie : public DumpCaptureImpl,
                      public ProcessWithOutput::Delegate {
  public:
    DumpCaptureMTKPcie(const DumpCaptureMTKPcie&) = delete;
    DumpCaptureMTKPcie(DumpCaptureMTKPcie&&) = delete;
    DumpCaptureMTKPcie& operator=(const DumpCaptureMTKPcie&) = delete;
    DumpCaptureMTKPcie& operator=(DumpCaptureMTKPcie&&) = delete;

    DumpCaptureMTKPcie(ModemType modem_type);

    DumpCaptureMTKPcie(ModemType modem_type,
                  std::unique_ptr<DeviceFactory> device_factory);
    ~DumpCaptureMTKPcie() override = default;

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
};

} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_DUMP_MTK_PCIE_LINUX_DUMP_MTK_PCIE_H_
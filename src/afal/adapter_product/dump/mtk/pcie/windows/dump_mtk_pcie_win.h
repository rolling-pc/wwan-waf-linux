#ifndef AFAL_ADAPTER_PRODUCT_DUMP_MTK_PCIE_LINUX_DUMP_MTK_PCIE_H_
#define AFAL_ADAPTER_PRODUCT_DUMP_MTK_PCIE_LINUX_DUMP_MTK_PCIE_H_

#include <Windows.h>
#include <guiddef.h> 
#include <atlbase.h>
#include <atlcomcli.h>
#include "common/modem.h"
#include "dump/dump.h"
#include "common/commonprocess.h"

namespace afal {

enum FunctionReturnStates {
    RETURN_FAILED = 0,
    RETURN_SUCCESS = 1,
    UNKNOWN_ERROR
};

typedef struct MinidumpCompressFile {
    WCHAR fullPath[MAX_PATH];
    WCHAR compressedFileName[MAX_PATH];
} MINIDUMP_COMPRESS_FILES;

class DumpCaptureMTKPcie : public DumpCaptureImpl,
                      public ProcessWithOutput::Delegate {
  Q_OBJECT
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
    bool Initialize() override;
    bool Start() override;
    bool ForceDumpTrigger(ModemDumpType dump_type) override;

    void HandleProcessOutput(QString output) override;
    void HandleProcessFinished(int exitCode,
                               QProcess::ExitStatus exitStatus) override;
    bool setDebugPort(int mode) override;
    bool StartMiniDumpMonitor() override;
    bool StopMiniDumpMonitor() override;

    static unsigned __stdcall MiniDumpCollectionThread(void* p);
    static bool PullAeeExpFolder(Modem* modem, std::string& dumpPath);
    static DWORD GenLocalTime(WCHAR* timeChar, BOOL resetGlbParam);

    public slots:
    void handleDumpFinished();
  private:
    std::unique_ptr<DeviceFactory> device_factory;
    std::unique_ptr<Modem> modem;
    std::unique_ptr<Device> dump_device;
    static bool service_log_starting;
};

} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_DUMP_MTK_PCIE_LINUX_DUMP_MTK_PCIE_H_
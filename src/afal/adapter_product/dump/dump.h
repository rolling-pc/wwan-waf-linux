#ifndef AFAL_ADAPTER_PRODUCT_DUMP_DUMP_H_
#define AFAL_ADAPTER_PRODUCT_DUMP_DUMP_H_

#include "common.h"
#include "dump_interface.h"
#include <memory>

namespace afal {

class DumpCaptureImpl : public DumpCapture {
public:
  DumpCaptureImpl(const DumpCaptureImpl&) = delete;
  DumpCaptureImpl(DumpCaptureImpl&&) = delete;
  DumpCaptureImpl& operator=(const DumpCaptureImpl&) = delete;
  DumpCaptureImpl& operator=(DumpCaptureImpl&&) = delete;

  DumpCaptureImpl() = delete;

  DumpCaptureImpl(ModemType modem_type);

   ~DumpCaptureImpl() override = default;

  void SetConfig(const Config &config = Config()) override;
  std::map<ModemDumpType, DumpStatus> Status() override = 0;
  bool Control(ModemDumpType dump_type = ModemDumpType::FULL_DUMP, DumpStatus dump_status = DumpStatus::ENABLE) override = 0;
  ModemDumpType QueryDump() override = 0;
  bool Start() override = 0;
  bool ForceDumpTrigger(ModemDumpType dump_type) override = 0;
  bool StartMiniDumpMonitor() override = 0;
  bool StopMiniDumpMonitor() override = 0;
  bool Initialize() override = 0;

protected:
  ModemType modem_type;
  std::unique_ptr<Config> config;
};

} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_DUMP_DUMP_H_
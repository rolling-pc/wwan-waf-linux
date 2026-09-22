#ifndef FIRMWARE_FLASH_LIB_SRC_FIRMWARE_FLASH_QCOM_H_
#define FIRMWARE_FLASH_LIB_SRC_FIRMWARE_FLASH_QCOM_H_

#include "modem.h"
#include "commonprocess.h"
#include "firmware_flash.h"
#include "flash_tools.h"
#include <memory>

namespace afal {

class FirmwareFlashImplQcomUSB : public FirmwareFlashImpl,
                                 public ProcessWithOutput::Delegate {
  Q_OBJECT
  public:
    FirmwareFlashImplQcomUSB(const FirmwareFlashImplQcomUSB&) = delete;
    FirmwareFlashImplQcomUSB(FirmwareFlashImplQcomUSB&&) = delete;
    FirmwareFlashImplQcomUSB&
    operator=(const FirmwareFlashImplQcomUSB&) = delete;
    FirmwareFlashImplQcomUSB& operator=(FirmwareFlashImplQcomUSB&&) = delete;
    FirmwareFlashImplQcomUSB(ModemType modem_type);
    FirmwareFlashImplQcomUSB(
        ModemType modem_type,
        std::unique_ptr<DeviceFactory> dev_factory);

    FirmwareFlashImplQcomUSB(
        ModemType modem_type,
        std::unique_ptr<DeviceFactory> dev_factory,
        std::unique_ptr<Fastboot> fastboot);

    ~FirmwareFlashImplQcomUSB() override = default;

    // override FirmwareFlash
    FlashMode QueryFlashMode() override;
    void EdlConfig(const QString& fw_path, const QString& log_path,
                   const QString& partition_table,
                   const QString& firehose_agent, const QString& auth_file,
                   const QString& ext_xml) override;

    bool EdlFlash() override;
    bool EdlRead(const QString& save_path, std::optional<int> start_addr,
                 std::optional<int> partition_size) override;
    bool EdlErase() override;
    bool EdlErase(int start_addr, int partition_size) override;
    bool EdlEraseAll() override;
    bool FastbootFlash(const QString& fw_path,
                       const QString& fw_partition) override;
    bool FastbootErase(const QString& fw_partition) override;
    bool FastbootReboot() override;
    bool PcieReset() override;
    // override FirmwareFlashImpl
    bool SwitchToFastbootMode() override;
    bool SwitchToEDLMode() override;

    // override QDL::Delegate
    void HandleProcessOutput(QString output) override;
    void HandleProcessFinished(int exitCode,
                               QProcess::ExitStatus exitStatus) override;

  protected:
    std::unique_ptr<Fastboot> fastboot;
    std::unique_ptr<Modem> modem;
    std::unique_ptr<Device> fastboot_device;
    std::unique_ptr<Device> edl_device;
    // Edl Config
    QString fw_path;
    QString partition_table;
    QString firehose_agent;
    QString ext_xml;

    double progress{};
    double progress_step{};

    int erase_count{}, program_count{};

    // common
    bool WaitEdlDevice();
    bool RunFlashTools();
};

} // namespace afal

#endif // FIRMWARE_FLASH_LIB_SRC_FIRMWARE_FLASH_QCOM_H_
#ifndef AFAL_ADAPTER_PRODUCT_FLASH_MTK_USB_LINUX_RW350_FIRMWARE_FLASH_MTK_USB_H_
#define AFAL_ADAPTER_PRODUCT_FLASH_MTK_USB_LINUX_RW350_FIRMWARE_FLASH_MTK_USB_H_

#include "proccommon.h"
#include "firmware_flash.h"
#include "flash_tools.h"
#include "flash_tools_mtk_usb.h"
#include "modem.h"
#include <memory>

namespace afal {

class FirmwareFlashImplMTKUSB : public FirmwareFlashImpl, public MTKFlashLib::Delegate {
  public:
    FirmwareFlashImplMTKUSB(const FirmwareFlashImplMTKUSB&) = delete;
    FirmwareFlashImplMTKUSB(FirmwareFlashImplMTKUSB&&) = delete;
    FirmwareFlashImplMTKUSB& operator=(const FirmwareFlashImplMTKUSB&) = delete;
    FirmwareFlashImplMTKUSB& operator=(FirmwareFlashImplMTKUSB&&) = delete;
    FirmwareFlashImplMTKUSB(ModemType modem_type);
    FirmwareFlashImplMTKUSB(
        ModemType modem_type,
        std::unique_ptr<DeviceFactory> dev_factory);
    FirmwareFlashImplMTKUSB(
        ModemType modem_type,
        std::unique_ptr<DeviceFactory> dev_factory,
        std::unique_ptr<Fastboot> fastboot,
        std::unique_ptr<MTKFlashLib> flash_lib);

    ~FirmwareFlashImplMTKUSB() override = default;

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

    // override MTKFlashLib::Delegate
    void UpdateProgress(int progress, const QString& messages) override;

  protected:
    std::unique_ptr<Fastboot> fastboot;
    std::unique_ptr<MTKFlashLib> flash_lib;
    std::unique_ptr<Modem> modem;
    std::unique_ptr<Device> fastboot_device;
    std::unique_ptr<Device> edl_device;
    // Edl Config
    QString fw_path;
    QString log_path;
    QString partition_table;
    QString auth_file;
    QString ext_xml;

    // common
    bool WaitEdlDevice();
};

class Fastboot;

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_FLASH_MTK_USB_LINUX_RW350_FIRMWARE_FLASH_MTK_USB_H_
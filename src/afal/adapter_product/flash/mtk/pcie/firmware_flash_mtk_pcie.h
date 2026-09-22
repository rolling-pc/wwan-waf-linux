#ifndef AFAL_ADAPTER_PRODUCT_FLASH_MTK_PCIE_FIRMWARE_FLASH_MTK_PCIE_H_
#define AFAL_ADAPTER_PRODUCT_FLASH_MTK_PCIE_FIRMWARE_FLASH_MTK_PCIE_H_

#include "firmware_flash.h"
#include "modem.h"
#include "commonprocess.h"
#include "flash_tools_mtk_pcie.h"
namespace afal {

class FirmwareFlashImplMTKPcie : public FirmwareFlashImpl {
  public:
    FirmwareFlashImplMTKPcie(const FirmwareFlashImplMTKPcie&) = delete;
    FirmwareFlashImplMTKPcie(FirmwareFlashImplMTKPcie&&) = delete;
    FirmwareFlashImplMTKPcie& operator=(const FirmwareFlashImplMTKPcie&) = delete;
    FirmwareFlashImplMTKPcie& operator=(FirmwareFlashImplMTKPcie&&) = delete;

    static FirmwareFlashImplMTKPcie* getInstance(ModemType modem_type) {
        std::call_once(initFlag, [&]() {
            instance.reset(new FirmwareFlashImplMTKPcie(modem_type));
        });
        return instance.get();
    }
    static FirmwareFlashImplMTKPcie* getInstance() {
        if (!instance) {
            throw std::runtime_error("FirmwareFlashImplMTKPcie instance not initialized yet!");
        }
        return instance.get();
    }

    FirmwareFlashImplMTKPcie(ModemType modem_type);
    FirmwareFlashImplMTKPcie(
        ModemType modem_type,
        std::unique_ptr<DeviceFactory> dev_factory);

    FirmwareFlashImplMTKPcie(
        ModemType modem_type,
        std::unique_ptr<DeviceFactory> dev_factory,
        std::unique_ptr<MTKPcieFlash> fastboot);

    ~FirmwareFlashImplMTKPcie() override = default;

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
    void progressUpdate(int progress, QString message);

  protected:
    std::unique_ptr<MTKPcieFlash> fastboot;
    std::unique_ptr<Modem> modem;
 	  std::unique_ptr<Device> fastboot_device;
    std::unique_ptr<Device> edl_device;
    
    // Edl Config
    QString fw_path;
    QString log_path;
    QString partition_table;
    QString firehose_agent;
    QString auth_file;
    QString ext_xml;
    static std::once_flag initFlag;
    static std::unique_ptr<FirmwareFlashImplMTKPcie> instance; 
    double progress{};
    double progress_step{};
    int erase_count{}, program_count{};

    // common
    bool WaitEdlDevice();
    bool SwitchModeMBIM();
};

} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_FLASH_MTK_PCIE_FIRMWARE_FLASH_MTK_PCIE_H_
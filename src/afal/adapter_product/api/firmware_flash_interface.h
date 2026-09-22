#ifndef AFAL_ADAPTER_PRODUCT_API_FIRMWARE_FLASH_INTERFACE_H_
#define AFAL_ADAPTER_PRODUCT_API_FIRMWARE_FLASH_INTERFACE_H_

#include <QtCore/qobject.h>
#include "QtCore/qobjectdefs.h"
#include "common.h"

#include <optional>
#include <QtCore/qlibrary.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

namespace afal {

class FirmwareFlash : public QObject {
    Q_OBJECT
  public:
    FirmwareFlash() = default;
    FirmwareFlash(const FirmwareFlash&) = delete;
    FirmwareFlash(FirmwareFlash&&) = delete;
    FirmwareFlash& operator=(const FirmwareFlash&) = delete;
    FirmwareFlash& operator=(FirmwareFlash&&) = delete;
    ~FirmwareFlash() override = default;
    // Modem FlashMode
    enum class FlashMode {
        NONE,
        FASTBOOT,
        EDL,
    };

    // You must set the config before you can Switch Flash mode

    // Modem\Config | fw_path | partition_table | auth_file     | other_xml   |;
    // FM101        | fw_path | rawprogram*.xml | firehose      | patch*.xml  |;
    // FM135        | fw_path | rawprogram*.xml | firehose      | patch*.xml  |;
    // FM350_USB    | fw_path | scatter.xml     | auth_sv5.auth | flash.xml   |;
    // Please enter the absolute path of the different configurations in the
    // table above, depending on the module type
    virtual void EdlConfig(const QString& fw_path, const QString& log_path,
                           const QString& partition_table,
                           const QString& firehose_agent,
                           const QString& auth_file,
                           const QString& ext_xml) = 0;

    // switch modem to flash mode ( fastboot mode, qdl mode )
    // RW350-USB switch to EDL mode requires EdlConfig()
    [[nodiscard]] virtual bool
    SwitchFlashMode(FlashMode flash_mode = FlashMode::FASTBOOT,
                    int retry_times = 3) = 0;

    // Query flash port
    [[nodiscard]] virtual FlashMode QueryFlashMode() = 0;

    // QDL and MTK EDL flash, flash all partition
    [[nodiscard]] virtual bool EdlFlash() = 0;

    // QDL and MTK EDL Read, flash all partition
    // QDL: EdlRead()
    // MTK: EdlRead(start_addr, partition_size)
    [[nodiscard]] virtual bool
    EdlRead(const QString& save_path = "",
            std::optional<int> start_addr = std::nullopt,
            std::optional<int> partition_size = std::nullopt) = 0;

    // // MTK Read partition image
    // [[nodiscard]] virtual bool
    // EdlRead(const QString& save_path = "",
    //         const QString& partition = "ALL") = 0;

    // // MTK Get partition list
    // [[nodiscard]] virtual QStringList
    // EdlGetPartitionList() = 0;

    // QDL Erase, the contents of erase need to be configured in partition_table
    // of EdlConfig
    [[nodiscard]] virtual bool EdlErase() = 0;

    // MTK erase single partition
    [[nodiscard]] virtual bool EdlErase(int start_addr, int partition_size) = 0;

    // // MTK erase single partition
    // [[nodiscard]] virtual bool
    // EdlErase(const QString& partition) = 0;

    // MTK erase all partition
    [[nodiscard]] virtual bool EdlEraseAll() = 0;

    // Fastboot Flash
    [[nodiscard]] virtual bool
    FastbootFlash(const QString& fw_path, const QString& fw_partition = "") = 0;

    // Fastboot Erase
    [[nodiscard]] virtual bool FastbootErase(const QString& fw_partition) = 0;

    // Fastboot Reboot
    [[nodiscard]] virtual bool FastbootReboot() = 0;
    // PLDR Reboot
    [[nodiscard]] virtual bool PcieReset() = 0;

    // auto flash = PA::FirmwareFlash::Create(
    //                PA::ModemType::QCOM_USB_MODEM,
    //                nullptr);
    [[nodiscard]] static FirmwareFlash*
    Create(ModemType modem_type = ModemType::QC_USB_GC);

  signals:
    void RebootModemSignal();
    void FlashImageCompleteSignal(int progress, QString messages);

  private:
};

using FwFlashCreateType =
    afal ::FirmwareFlash* (*)(afal ::ModemType modem_type);
static FwFlashCreateType LoadFwFlashCreateFunc(const QString& lib_path) {
  static std::unique_ptr<QLibrary> flashLibrary; 
  flashLibrary = LoadLibrary(std::move(flashLibrary), lib_path);
  if (!flashLibrary) return nullptr;

  return ResolveFunc<FwFlashCreateType>(flashLibrary.get(), "FwFlashCreate");
}

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_API_FIRMWARE_FLASH_INTERFACE_H_
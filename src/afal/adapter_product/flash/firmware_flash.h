#ifndef AFAL_ADAPTER_PRODUCT_FLASH_FIRMWARE_FLASH_H_
#define AFAL_ADAPTER_PRODUCT_FLASH_FIRMWARE_FLASH_H_

#include "firmware_flash_interface.h"
#include "flash_tools.h"
#include "modem.h"

namespace afal {

class FirmwareFlashImpl : public FirmwareFlash {
    Q_OBJECT
  public:
  FirmwareFlashImpl(FirmwareFlashImpl&&) = delete;
  FirmwareFlashImpl& operator=(FirmwareFlashImpl&&) = delete;
  explicit FirmwareFlashImpl(ModemType modem_type);
  explicit FirmwareFlashImpl(ModemType modem_type,
                           std::unique_ptr<DeviceFactory> dev_factory);

  FirmwareFlashImpl(const FirmwareFlashImpl &) = delete;
  FirmwareFlashImpl &operator=(const FirmwareFlashImpl &) = delete;
  ~FirmwareFlashImpl() override = default;

  // FirmwareFlash override
  bool SwitchFlashMode(FlashMode flash_mode, int retry_times) override;
  virtual bool PcieReset() = 0;
  // Need to override
  virtual bool SwitchToFastbootMode() = 0;
  virtual bool SwitchToEDLMode() = 0;
  void FlashImageComplete(int progress, QString messages = "");

protected:
  void RebootModem();
  ModemType modem_type; 
  std::unique_ptr<DeviceFactory> dev_factory;
};

bool SwitchFlashModeWithAT(Modem *modem, Device *flash_device, const QString &cmd,  int sendchannel = 1);

} // namespace afal
#endif  // AFAL_ADAPTER_PRODUCT_FLASH_FIRMWARE_FLASH_H_
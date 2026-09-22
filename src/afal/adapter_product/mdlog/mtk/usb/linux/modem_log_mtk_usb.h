#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_LOG_MTK_USB_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_LOG_MTK_USB_H_

#include "mtk/pcie/linux/modem_log_mtk.h"
#include <QFile>

namespace afal {

class AdbLogReaderusb : public AdbLogReader
{
    Q_OBJECT

public:
    explicit AdbLogReaderusb(QObject* parent = nullptr);

protected:
    QString program() override;
    QStringList arguments() override;

};
  
 class ModemLoggerImplMtkUSB : public ModemLoggerImplPcie
  {
  public:
      ModemLoggerImplMtkUSB() = delete;
      ~ModemLoggerImplMtkUSB() override = default;
  
      ModemLoggerImplMtkUSB(ModemType modem_type);
  
      ModemLoggerImplMtkUSB(ModemType modem_type,
                             std::unique_ptr<DeviceFactory> device_factory);
  
      ModemLoggerImplMtkUSB(const ModemLoggerImplMtkUSB&) = delete;
      ModemLoggerImplMtkUSB& operator=(const ModemLoggerImplMtkUSB&) = delete;
      ModemLoggerImplMtkUSB(ModemLoggerImplMtkUSB&&) = delete;
      ModemLoggerImplMtkUSB& operator=(ModemLoggerImplMtkUSB&&) = delete;
  
  protected:
      bool sapStart() override;
      bool sapStop() override;
      std::unique_ptr<AdbLogReaderusb> adbLogReaderUSB;
  };


} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_LOG_MTK_USB_H_
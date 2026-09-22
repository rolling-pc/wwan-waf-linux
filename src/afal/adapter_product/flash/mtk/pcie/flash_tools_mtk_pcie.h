#ifndef AFAL_ADAPTER_PRODUCT_FLASH_MTK_PCIE_FWFLASH_MTK_PCIE_H_
#define AFAL_ADAPTER_PRODUCT_FLASH_MTK_PCIE_FWFLASH_MTK_PCIE_H_

#include "modem.h"
#include <qchar.h>
#include <qobject.h>
#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QSerialPort>
#include <QSerialPortInfo>

namespace afal {

// 移动到mtk pcie
class MTKPcieFlash : public QObject{

Q_OBJECT

  public:
    enum class State {
      READY,
      RESET,
      DOWNLOAD,
      UNKNOWN,
      NONE
    };

    MTKPcieFlash(const MTKPcieFlash&) = delete;
    MTKPcieFlash(MTKPcieFlash&&) = delete;
    MTKPcieFlash& operator=(const MTKPcieFlash&) = delete;
    MTKPcieFlash& operator=(MTKPcieFlash&&) = delete;
    MTKPcieFlash() = default;

     ~MTKPcieFlash() override = default;

    virtual bool SwitchFlashMode() = 0;
    virtual State GetModemState() = 0;
    virtual bool Flash(const QString& fw_partition, const QString& fw_path) = 0;
    virtual bool Erase(const QString& fw_partition) = 0;
    virtual bool Reboot() = 0;
    virtual bool PcieReboot() = 0;
    virtual bool Remove() = 0;
    virtual bool Rescan() = 0;

    [[nodiscard]] static std::unique_ptr<MTKPcieFlash> Create(Device* device);
};

}  // namespace afal
#endif  // AFAL_ADAPTER_PRODUCT_FLASH_MTK_PCIE_FWFLASH_MTK_PCIE_H_
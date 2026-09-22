#include "flash_tools_mtk_pcie.h"
#include "log.hpp"
#include "proccommon.h"
#include "file.h"

#include <QFile>
#include <fcntl.h>
#include <memory>
#include <qchar.h>
#include <qdom.h>
#include <qfileinfo.h>
#include <qobject.h>
#include <qserialport.h>
#include <QTextStream>
#include <fstream>
#include <iostream>
#include <vector>

namespace afal {

namespace {

using namespace log;

constexpr char K_MTK_PCIE_NODE_PATH_FORMAT[] = // NOLINT
    "/sys/bus/pci/devices/{}/t7xx_mode";

constexpr char K_MTK_PCIE_REMOVE_PATH[] = // NOLINT
    "/sys/bus/pci/devices/{}/remove";

constexpr char K_MTK_PCIE_RESCAN_PATH_FORMAT[] = // NOLINT
    "/sys/bus/pci/rescan";

const QString K_MTK_FASTBOOT_SWITCH = "fastboot_switching";
const QString K_MTK_FASTBOOT_PORT = "/dev/wwan0fastboot0";
const QString K_MTK_RESET = "reset";
MTKPcieFlash::State GetStateWithStr(const QString& state) {
    static const std::map<QString, MTKPcieFlash::State> K_STATE_STR = {
        {"reset", MTKPcieFlash::State::RESET},
        {"fastboot_download", MTKPcieFlash::State::DOWNLOAD},
        {"ready", MTKPcieFlash::State::READY},
        {"unknown", MTKPcieFlash::State::UNKNOWN},
    };
    auto it = K_STATE_STR.find(state);
    if (it != K_STATE_STR.end())
    {
        return it->second;
    }
    return MTKPcieFlash::State::NONE;
}

} // namespace

class MTKPcieFlashImpl : public MTKPcieFlash {
  public:
    MTKPcieFlashImpl(const MTKPcieFlashImpl&) = delete;
    MTKPcieFlashImpl(MTKPcieFlashImpl&&) = delete;
    MTKPcieFlashImpl& operator=(const MTKPcieFlashImpl&) = delete;
    MTKPcieFlashImpl& operator=(MTKPcieFlashImpl&&) = delete;
    MTKPcieFlashImpl() = delete;

    MTKPcieFlashImpl(Device* device);

    ~MTKPcieFlashImpl() override = default;

    bool SwitchFlashMode() override;
    bool PcieReboot() override;
    MTKPcieFlash::State GetModemState() override;
    bool Flash(const QString& fw_partition, const QString& fw_path) override;
    bool Erase(const QString& fw_partition) override;
    bool Reboot() override;
    bool Remove() override;
    bool Rescan() override;

  private:
    Device* device;
    std::string state_node;
    std::string remove_node;
    std::string rescan_node;

    std::mutex mtx;
    std::condition_variable cv;

    bool GetStateNode();
};

MTKPcieFlashImpl::MTKPcieFlashImpl(Device* device) : device(device){};

bool MTKPcieFlashImpl::GetStateNode() {
    if (!state_node.empty())
    {
        auto state_node_file = QFileInfo(state_node.c_str());
        if (state_node_file.exists() && state_node_file.isFile())
        {
            return true;
        }
    }

    auto bdf = device->GetBDF();

    if (!bdf.has_value())
    {
        FMTLOG_ERROR("MTK fails to obtain the modem bdf");
        return false;
    }

    state_node = fmt::format(K_MTK_PCIE_NODE_PATH_FORMAT, (*bdf).toStdString());
    remove_node = fmt::format(K_MTK_PCIE_REMOVE_PATH, (*bdf).toStdString());
    rescan_node = fmt::format(K_MTK_PCIE_RESCAN_PATH_FORMAT);
    if (!state_node.empty())
    {
        auto state_node_file = QFileInfo(state_node.c_str());
        if (state_node_file.exists() && state_node_file.isFile())
        {
            return true;
        }
    }
    return false;
}

bool MTKPcieFlashImpl::SwitchFlashMode() {
    if (!GetStateNode())
    {
        FMTLOG_ERROR("Failed to get state node!");
        return false;
    }

    if (GetModemState() == MTKPcieFlash::State::DOWNLOAD)
    {
        FMTLOG_INFO("The module is already in fastboot state.");
        return true;
    }

    QFile state_node_file(QString::fromStdString(state_node));

    if (!state_node_file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        FMTLOG_ERROR("Failed to open the file! {}", state_node);
        return false;
    }
    ScopedAutoCloseQFile auto_close_state_node(&state_node_file);

    if (state_node_file.write(K_MTK_FASTBOOT_SWITCH.toUtf8()) == -1 ||
        !state_node_file.flush())
    {
        FMTLOG_ERROR("Failed to switch fastboot!");
        return false;
    }

    return true;
}

bool MTKPcieFlashImpl::PcieReboot() {
    if (!GetStateNode())
    {
        FMTLOG_ERROR("Failed to get state node!");
        return false;
    }

    QFile state_node_file(QString::fromStdString(state_node));

    if (!state_node_file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        FMTLOG_ERROR("Failed to open the file! {}", state_node);
        return false;
    }
    ScopedAutoCloseQFile auto_close_state_node(&state_node_file);

    if (state_node_file.write(K_MTK_RESET.toUtf8()) == -1 ||
        !state_node_file.flush())
    {
        FMTLOG_ERROR("Failed to trigger PCIE Reset!");
        return false;
    }

    return true;
}

bool MTKPcieFlashImpl::Remove() {
    if (!GetStateNode())
    {
        FMTLOG_ERROR("Failed to get state node!");
        return false;
    }

    QFile remove_node_file(QString::fromStdString(remove_node));

    if (!remove_node_file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        FMTLOG_ERROR("Failed to open the file! {}", state_node);
        return false;
    }

    ScopedAutoCloseQFile auto_close_remove_node(&remove_node_file);

    FMTLOG_INFO("MTK remove");
    if (remove_node_file.write("1") == -1 || !remove_node_file.flush())
    {
        FMTLOG_ERROR("Failed to remove modem driver!");
        return false;
    }

    return true;
}

bool MTKPcieFlashImpl::Rescan() {
    FMTLOG_DEBUG("Rescan");

    QFile rescan_node_file(QString::fromStdString(rescan_node));

    if (!rescan_node_file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        FMTLOG_ERROR("Failed to open the file! {}", state_node);
        return false;
    }

    ScopedAutoCloseQFile auto_close_rescan_node(&rescan_node_file);

    FMTLOG_INFO("MTK rescan");
    if (rescan_node_file.write("1") == -1 || !rescan_node_file.flush())
    {
        FMTLOG_ERROR("Failed to rescan modem driver!");
        return false;
    }

    return true;
}

MTKPcieFlash::State MTKPcieFlashImpl::GetModemState() {
    if (!GetStateNode())
    {
        FMTLOG_ERROR("Failed to get state node!");
        return State::NONE;
    }

    QFile state_node_file(QString::fromStdString(state_node));
    if (!state_node_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        FMTLOG_ERROR("Failed to open the file! {}", state_node);
        return State::NONE;
    }
    ScopedAutoCloseQFile auto_close_state_node(&state_node_file);
    QString modem_state_str = state_node_file.readAll();

    modem_state_str.remove("\n");
    modem_state_str.remove("\r");

    FMTLOG_INFO("get MTK pcie modem state: {}", modem_state_str);

    return GetStateWithStr(modem_state_str);
}

bool MTKPcieFlashImpl::Flash(const QString& fw_partition, // NOLINT
                             const QString& fw_path) {
    if (GetModemState() != MTKPcieFlash::State::DOWNLOAD)
    {
        FMTLOG_ERROR("Flash fails and MTK modem is not in downlaod state.");
        return false;
    }

    QFile fw_file(fw_path);

    if (!fw_file.open(QIODevice::ReadOnly))
    {
        FMTLOG_ERROR("Failed to open the file! {}", fw_file.fileName());
        return false;
    }

    ScopedAutoCloseQFile auto_close_fw_file(&fw_file);
    ScopedFd fastboot_fd(open(K_MTK_FASTBOOT_PORT.toUtf8(), O_RDWR)); // NOLINT

    if (fastboot_fd.get() < 0)
    {
        FMTLOG_ERROR("fastboot port open failed!");
        return false;
    }

    std::string downlaod_size = fmt::format("download:{:08x}", fw_file.size());
    std::string flash_partition =
        fmt::format("flash:{}", fw_partition.toStdString());
    FMTLOG_DEBUG("download : {}, flash: {}", downlaod_size, flash_partition);
    if (write(fastboot_fd.get(), downlaod_size.c_str(), int(downlaod_size.size())) ==
        -1)
    {
        FMTLOG_ERROR("partition size write failed!");
        return false;
    }

    const qint64 buffer_size = 2048; // 2KB

    std::array<char, buffer_size> output{};
    auto ret = ReadWithTimeout(fastboot_fd.get(), output.data(), output.size());
    if (ret == -1)
    {
        FMTLOG_ERROR("partition size write failed!");
        return false;
    }
    output.at(ret) = '\0';

    FMTLOG_INFO("read: {}", output.data());

    std::array<char, buffer_size> buffer{};
    qint64 bytesRead = 0;

    while ((bytesRead = fw_file.read(buffer.data(), buffer_size)) > 0)
    {
        if (write(fastboot_fd.get(), buffer.data(), bytesRead) == -1)
        {
            FMTLOG_ERROR("Firmware flash failure.");
            return false;
        }
    }

    ret = ReadWithTimeout(fastboot_fd.get(), output.data(), output.size() - 1);
    if (ret == -1)
    {
        FMTLOG_ERROR("firmware flash failure.");
        return false;
    }
    output.at(ret) = '\0';

    FMTLOG_INFO("read: {}", output.data());

    if (write(fastboot_fd.get(), flash_partition.c_str(),
              int(flash_partition.size())) == -1)
    {
        FMTLOG_ERROR("partition name write failed!");
        return false;
    }

    ret = ReadWithTimeout(fastboot_fd.get(), output.data(), output.size());
    if (ret == -1)
    {
        FMTLOG_ERROR("partition name write failed!");
        return false;
    }
    output.at(ret) = '\0';

    FMTLOG_INFO("read: {}", output.data());

    return true;
}

bool MTKPcieFlashImpl::Erase(const QString& fw_partition) {
    if (GetModemState() != MTKPcieFlash::State::DOWNLOAD)
    {
        FMTLOG_ERROR("Erase fails and MTK modem is not in downlaod state.");
        return false;
    }

    QFile fastboot_port(K_MTK_FASTBOOT_PORT);

    if (!fastboot_port.open(QIODevice::WriteOnly))
    {
        FMTLOG_ERROR("Failed to open the port! {}", fastboot_port.fileName());
        return false;
    }

    ScopedAutoCloseQFile auto_close_fastboot_port(&fastboot_port);

    std::string erase_partition =
        fmt::format("erase:{}", fw_partition.toStdString());

    FMTLOG_INFO("Erase: {}", erase_partition);

    if (fastboot_port.write(erase_partition.c_str()) == -1 ||
        !fastboot_port.flush())
    {
        FMTLOG_ERROR("Erase failed!");
        return false;
    }

    return true;
}

bool MTKPcieFlashImpl::Reboot() {
    if (GetModemState() != MTKPcieFlash::State::DOWNLOAD)
    {
        FMTLOG_ERROR("Reboot fails and MTK modem is not in downlaod state.");
        return false;
    }

    QFile fastboot_port(K_MTK_FASTBOOT_PORT);

    if (!fastboot_port.open(QIODevice::WriteOnly))
    {
        FMTLOG_ERROR("Failed to open the port! {}", fastboot_port.fileName());
        return false;
    }

    ScopedAutoCloseQFile auto_close_fastboot_port(&fastboot_port);

    if (fastboot_port.write("reboot") == -1 || !fastboot_port.flush())
    {
        FMTLOG_ERROR("Reboot failed!");
        return false;
    }

    return true;
}

[[nodiscard]] std::unique_ptr<MTKPcieFlash>
MTKPcieFlash::Create(Device* device) {
    if (!device)
    {
        return nullptr;
    }
    return std::make_unique<MTKPcieFlashImpl>(device);
}

} // namespace afal

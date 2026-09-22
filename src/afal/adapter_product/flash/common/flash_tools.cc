#include "flash_tools.h"

#include "modem.h"
#include "proccommon.h"
#include "commonprocess.h"

#include "log.hpp"

#include <memory>
#include <QFile>
#include <qfileinfo.h>
#include <QDomDocument>

namespace afal {

namespace {
using namespace log;
// Fastboot
const QString K_FASTBOOT = "/usr/bin/fastboot";
const QString K_FASTBOOT_FLASH = "flash";
const QString K_FASTBOOT_REBOOT = "reboot";
const QString K_FASTBOOT_ERASE = "erase";

const int K_TIMEOUT = 60 * 1000;

} // namespace


class FastbootImpl : public Fastboot {
  public:
    FastbootImpl() = default;
    FastbootImpl(const FastbootImpl&) = default;
    FastbootImpl(FastbootImpl&&) = delete;
    FastbootImpl& operator=(const FastbootImpl&) = default;
    FastbootImpl& operator=(FastbootImpl&&) = delete;
    ~FastbootImpl() override = default;
    bool Flash(const QString& fw_partition, const QString& fw_path) override;
    bool Erase(const QString& fw_partition) override;
    bool Reboot() override;

  private:
};

bool FastbootImpl::Flash(const QString& fw_partition, const QString& fw_path) {
    QStringList arg;
    arg << K_FASTBOOT_FLASH;
    arg << fw_partition;
    arg << fw_path;
    FMTLOG_DEBUG("Run fastboot flash process : {}", arg);
    return RunProcess(K_FASTBOOT, arg, K_TIMEOUT) == 0;
}

bool FastbootImpl::Erase(const QString& fw_partition) {
    QStringList arg;
    arg << K_FASTBOOT_ERASE;
    arg << fw_partition;
    return RunProcess(K_FASTBOOT, arg, K_TIMEOUT) == 0;
}

bool FastbootImpl::Reboot() {
    QStringList arg;
    arg << K_FASTBOOT_REBOOT;
    return RunProcess(K_FASTBOOT, arg, K_TIMEOUT) == 0;
}


[[nodiscard]] std::unique_ptr<Fastboot> Fastboot::Create() {
    return std::make_unique<FastbootImpl>();
}

} // namespace afal

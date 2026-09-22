#ifndef AFAL_ADAPTER_PRODUCT_FLASH_COMMON_FLASH_TOOLS_H_
#define AFAL_ADAPTER_PRODUCT_FLASH_COMMON_FLASH_TOOLS_H_

#include "modem.h"

#include <memory>
#include <spdlog/spdlog.h>
namespace afal {

class Fastboot {
  public:
    Fastboot() = default;
    Fastboot(const Fastboot&) = default;
    Fastboot(Fastboot&&) = delete;
    Fastboot& operator=(const Fastboot&) = default;
    Fastboot& operator=(Fastboot&&) = delete;
    virtual ~Fastboot() = default;
    virtual bool Flash(const QString& fw_partition, const QString& fw_path) = 0;
    virtual bool Erase(const QString& fw_partition) = 0;
    virtual bool Reboot() = 0;

    [[nodiscard]] static std::unique_ptr<Fastboot> Create();
};

} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_FLASH_COMMON_FLASH_TOOLS_H_
#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_MTK_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_MTK_H_

#include "common/modem.h"
#include <memory>

namespace afal {

class ModemMTK : public Modem {
  public:
    ModemMTK() = default;
    ModemMTK(const ModemMTK&) = delete;
    ModemMTK(ModemMTK&&) = delete;
    ModemMTK& operator=(const ModemMTK&) = delete;
    ModemMTK& operator=(ModemMTK&&) = delete;
    ModemMTK(const Modem&) = delete;
    ModemMTK(Modem&&) = delete;
    ModemMTK& operator=(const Modem&) = delete;
    ModemMTK& operator=(Modem&&) = delete;
    ~ModemMTK() override = default;

    [[nodiscard]] virtual bool
    SetIntelTrace(int level) const = 0;
    [[nodiscard]] virtual std::optional<std::pair<int, int>>
    GetIntelTrace() const = 0;

    static std::unique_ptr<ModemMTK> Create(std::unique_ptr<Modem> modem);
};


}  // namespace afal



#endif  // AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_MTK_H_
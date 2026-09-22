#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_MODEM_LOG_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_MODEM_LOG_H_

#include "modem_log_interface.h"

namespace afal {

class ModemLoggerImpl : public ModemLogger {
  public:
    ModemLoggerImpl(const ModemLoggerImpl&) = delete;
    ModemLoggerImpl(ModemLoggerImpl&&) = delete;
    ModemLoggerImpl& operator=(const ModemLoggerImpl&) = delete;
    ModemLoggerImpl& operator=(ModemLoggerImpl&&) = delete;
    ModemLoggerImpl() = delete;
    ~ModemLoggerImpl() override = default;

    ModemLoggerImpl(ModemType modem_type);

    void SetConfig(Config config) override;

    [[nodiscard]] std::optional<QString>
    SendATCommand(const QString& cmd, int timeout = 20) const override {
        return std::nullopt;
    }

  protected:
    std::unique_ptr<Config> config;
    ModemType modem_type;
};

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_MDLOG_MODEM_LOG_H_
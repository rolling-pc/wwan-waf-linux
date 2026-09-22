#include "modem_mtk.h"
#include "common.hpp"
#include "common/proccommon.h"
#include "log.hpp"
#include "mbim.hpp"
#include <memory>

namespace afal {

namespace {

using namespace log;

}

class ModemMTKImpl : public ModemMTK {
  public:
    ModemMTKImpl(const ModemMTKImpl&) = delete;
    ModemMTKImpl(ModemMTKImpl&&) = delete;
    ModemMTKImpl& operator=(const ModemMTKImpl&) = delete;
    ModemMTKImpl& operator=(ModemMTKImpl&&) = delete;
    explicit ModemMTKImpl(std::unique_ptr<Modem> modem)
        : modem(std::move(modem)) {}
    ~ModemMTKImpl() override = default;

    [[nodiscard]] bool SetIntelTrace(int level) const override;
    [[nodiscard]] std::optional<std::pair<int, int>>
    GetIntelTrace() const override;

    [[nodiscard]] bool WaitForAction(std::optional<QString> action,
                                     int timeout) override {
        return modem->WaitForAction(action, timeout);
    };
    [[nodiscard]] bool FindDevice() override { return modem->FindDevice(); };
    void AddAttr(const QString& key, const QString& value) override {
        modem->AddAttr(key, value);
    };
    void AddEnv(const QString& key, const QString& value) override {
        modem->AddEnv(key, value);
    };
    void AddDeviceId(const QString& vid, const QString& pid) override {
        modem->AddDeviceId(vid, pid);
    }
    std::optional<QString> GetBDF() override { return modem->GetBDF(); }

    [[nodiscard]] std::optional<QString>
    SendATCommand(const QString& cmd, int timeout = K_MBIM_TIMEOUT) const override {
        return modem->SendATCommand(cmd, timeout);
    }
    bool MBIMReboot() const override { return modem->MBIMReboot(); };

  private:
    std::unique_ptr<Modem> modem;
};

bool ModemMTKImpl::SetIntelTrace(int level) const {
    int ret = error::ERR;
    QMap<QString, int> args;
    args.insert("mode", 1);
    args.insert("level", level);
    OnMbimOperation([&]() {
        ret = mbim::MbimDevice::getInstance().AfalMbimTraceLogSetSync(
            args, K_MBIM_TIMEOUT);
    });

    return ret == 0;
};

std::optional<std::pair<int, int>> ModemMTKImpl::GetIntelTrace() const {
    int ret = error::ERR;
    QMap<QString, int> args;

    args.insert("mode", 1);
    args.insert("level", 1);

    OnMbimOperation([&]() {
        ret = afal::mbim::MbimDevice::getInstance().AfalMbimTraceLogQuerySync(
            args, K_MBIM_TIMEOUT);
    });

    if (ret == error::ERR)
    {
        return std::nullopt;
    }

    int mode = 0, level = 0;

    for (auto& key : args.keys())
    {
        if (key == "mode")
        {
            mode = args[key];
        }
        else if (key == "level")
        {
            level = args[key];
        }
        else
        {
            FMTLOG_ERROR("Extra tag types: {}, value: {}", key, args[key]);
            return std::nullopt;
        }
    }

    return std::make_pair(mode, level);
};

std::unique_ptr<ModemMTK> ModemMTK::Create(std::unique_ptr<Modem> modem) {
    return std::make_unique<ModemMTKImpl>(std::move(modem));
}

} // namespace afal
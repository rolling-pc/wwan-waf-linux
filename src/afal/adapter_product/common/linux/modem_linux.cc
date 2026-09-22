#include "common.hpp"
#include "modem.h"

#include "proccommon.h"
#include "commonprocess.h"

#include "log.hpp"
#include "mbim.hpp"

#include <memory>
#include <chrono>
#include <optional>
#include <qeventloop.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qsocketnotifier.h>
#include <qthread.h>
#include <thread>
#include <utility>
#include <condition_variable>
#include <libudev.h>
#include <mutex>
#include <sys/poll.h>
#include <vector>
#include <QCoreApplication>
#include <QRegularExpression>

template <> struct fmt::formatter<std::vector<afal::DeviceId>> {
    constexpr auto
    parse( // NOLINT(readability-convert-member-functions-to-static,
           // readability-identifier-naming)
        format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format( // NOLINT(readability-convert-member-functions-to-static,
                 // readability-identifier-naming)
        const std::vector<afal::DeviceId>& p,
        FormatContext& ctx) const -> decltype(ctx.out()) {
        std::string output;
        for (const auto& id : p)
        {
            output += fmt::format("{}, ", id.ToStdString());
        }
        return fmt::format_to(ctx.out(), "{}", output);
    }
};

namespace afal {

namespace {

using namespace log;

struct UdevDeleter {
    void operator()(struct udev* udev) {
        if (udev) udev_unref(udev);
    }
};
using ScopedUdev = std::unique_ptr<struct udev, UdevDeleter>;

struct UdevEnumerateDeleter {
    void operator()(struct udev_enumerate* enumerator) {
        if (enumerator) udev_enumerate_unref(enumerator);
    }
};
using ScopedUdevEnumerate =
    std::unique_ptr<struct udev_enumerate, UdevEnumerateDeleter>;

struct UdevDeviceDeleter {
    void operator()(struct udev_device* device) {
        if (device) udev_device_unref(device);
    }
};
using ScopedUdevDevice = std::unique_ptr<struct udev_device, UdevDeviceDeleter>;

struct UdevQueueDeleter {
    void operator()(struct udev_queue* queue) {
        if (queue) udev_queue_unref(queue);
    }
};
using ScopedUdevQueue = std::unique_ptr<struct udev_queue, UdevQueueDeleter>;

struct UdevMonitorDeleter {
    void operator()(struct udev_monitor* monitor) {
        if (monitor) udev_monitor_unref(monitor);
    }
};
using ScopedUdevMonitor =
    std::unique_ptr<struct udev_monitor, UdevMonitorDeleter>;

bool PropertyMatch(struct udev_device* device, const char* key,
                   const std::string& expected_value) {
    const char* value = udev_device_get_property_value(device, key);
    if (!value) return false;
    return strcasecmp(value, expected_value.c_str()) == 0;
}

bool AttrMatch(struct udev_device* device, const char* key,
               const std::string& expected_value) {
    const char* value = udev_device_get_sysattr_value(device, key);
    if (!value) return false;
    return strcasecmp(value, expected_value.c_str()) == 0;
}

struct udev_device* MatchVidPid(const ScopedUdevDevice& device, const char* vid,
                                const char* pid, bool parent = true) {
    if (!vid || !pid) return device.get();
    struct udev_device* parent_dev = device.get();
    while (parent_dev)
    {
        const char* parent_pid =
            udev_device_get_sysattr_value(parent_dev, "idProduct");
        const char* parent_vid =
            udev_device_get_sysattr_value(parent_dev, "idVendor");
        if (parent_vid == nullptr || parent_pid == nullptr)
        {
            parent_pid = udev_device_get_sysattr_value(parent_dev, "device");
            parent_vid = udev_device_get_sysattr_value(parent_dev, "vendor");
        }
        if (parent_vid == nullptr || parent_pid == nullptr)
        {
            if (!parent)
            {
                break;
            }
            parent_dev = udev_device_get_parent(parent_dev);
            continue;
        }
        if ((std::string(parent_vid) == vid) &&
            (std::string(parent_pid) == pid || std::string(pid).empty()))
        {
            return parent_dev;
        }
        break;
    }

    return nullptr;
}

} // namespace

class DeviceFactoryImpl : public DeviceFactory {
  public:
    DeviceFactoryImpl() = default;
    DeviceFactoryImpl(const DeviceFactoryImpl&) = default;
    DeviceFactoryImpl(DeviceFactoryImpl&&) = delete;
    DeviceFactoryImpl& operator=(const DeviceFactoryImpl&) = default;
    DeviceFactoryImpl& operator=(DeviceFactoryImpl&&) = delete;
    ~DeviceFactoryImpl() override = default;

    [[nodiscard]] std::unique_ptr<Device>
    CreateDevice(const QString& sub_system,
                 const std::vector<DeviceId>& device_ids,
                 const Attributes& attrs = {}) const override;

    [[nodiscard]] std::unique_ptr<Modem>
    CreateModem(std::unique_ptr<Device> device) const override;

    [[nodiscard]] std::unique_ptr<Modem>
    CreateModem(const QString& sub_system,
                const std::vector<DeviceId>& device_ids,
                const std::map<QString, QString>& attrs = {}) const override;
};

std::unique_ptr<DeviceFactory> DeviceFactory::Create() {
    return std::make_unique<DeviceFactoryImpl>();
}


class DeviceImpl : public Device {
  public:
    DeviceImpl(const DeviceImpl&) = delete;
    DeviceImpl(DeviceImpl&&) = delete;
    DeviceImpl& operator=(const DeviceImpl&) = delete;
    DeviceImpl& operator=(DeviceImpl&&) = delete;
    explicit DeviceImpl(QString subsystem)
        : subsystem(std::move(subsystem)), udev(udev_new()) {}

    void AddAttr(const QString& key, const QString& value) override {
        attrs[key] = value;
    }

    void AddEnv(const QString& key, const QString& value) override {
        envs[key] = value;
    }

    void AddDeviceId(const QString& vid, const QString& pid) override {
        device_ids.emplace_back(vid, pid);
    }
    ~DeviceImpl() override = default;

    [[nodiscard]] std::optional<QString> GetBDF() override;

    [[nodiscard]] bool WaitForAction(std::optional<QString> action,
                                     int timeout) override;
    [[nodiscard]] bool FindDevice() override;

    [[nodiscard]] bool MatchDevice(const ScopedUdevDevice& device,
                                   bool parent = true) const;

    bool OnUdevEvent(std::optional<std::string>);

    [[nodiscard]] bool
    MatchDevicePropertyAttr(const ScopedUdevDevice& device) const;

  private:
    ScopedUdev udev;
    ScopedUdevMonitor monitor;
    ScopedUdevDevice device;
    QString subsystem;
    // either device type or vid/pid or both
    std::vector<DeviceId> device_ids;
    // optional
    std::map<QString, QString> attrs;
    std::map<QString, QString> envs;
};

class ModemImpl : public Modem {
  public:
    ModemImpl(const ModemImpl&) = delete;
    ModemImpl(ModemImpl&&) = delete;
    ModemImpl& operator=(const ModemImpl&) = delete;
    ModemImpl& operator=(ModemImpl&&) = delete;
    explicit ModemImpl(std::unique_ptr<Device> device)
        : device(std::move(device)) {}
    ~ModemImpl() override = default;
    [[nodiscard]] std::optional<QString>
    SendATCommand(const QString& cmd, int timeout) const override;
    bool MBIMReboot() const override;
    [[nodiscard]] bool WaitForAction(std::optional<QString> action,
                                     int timeout) override {
        return device->WaitForAction(action, timeout);
    };
    [[nodiscard]] bool FindDevice() override { return device->FindDevice(); };
    void AddAttr(const QString& key, const QString& value) override {
        device->AddAttr(key, value);
    };
    void AddEnv(const QString& key, const QString& value) override {
        device->AddEnv(key, value);
    };
    void AddDeviceId(const QString& vid, const QString& pid) override {
        device->AddDeviceId(vid, pid);
    }
    std::optional<QString> GetBDF() override { return device->GetBDF(); }

  private:
    std::unique_ptr<Device> device;
};

std::unique_ptr<Device>
DeviceFactoryImpl::CreateDevice(const QString& sub_system,
                                const std::vector<DeviceId>& device_ids,
                                const Attributes& attrs) const {
    auto device = std::make_unique<DeviceImpl>(sub_system);

    for (const auto& [vendor_id, product_id] : device_ids)
    {
        device->AddDeviceId(vendor_id, product_id);
    }

    for (const auto& [key, value] : attrs)
    {
        device->AddAttr(key, value);
    }

    return device;
}

std::unique_ptr<Modem>
DeviceFactoryImpl::CreateModem(std::unique_ptr<Device> device) const {
    if (!device)
    {
        return nullptr;
    }
    return std::make_unique<ModemImpl>(std::move(device));
}

std::unique_ptr<Modem>
DeviceFactoryImpl::CreateModem(const QString& sub_system,
                               const std::vector<DeviceId>& device_ids,
                               const std::map<QString, QString>& attrs) const {
    return CreateModem(CreateDevice(sub_system, device_ids, attrs));
}

[[nodiscard]] bool
DeviceImpl::MatchDevicePropertyAttr(const ScopedUdevDevice& device) const {
    if (!std::all_of(attrs.begin(), attrs.end(),
                     [this, &device](auto attr) -> bool {
                         return static_cast<bool>(
                             AttrMatch(device.get(), attr.first.toUtf8(),
                                       attr.second.toStdString()));
                     }))
    {
        return false;
    }
    if (!std::all_of(envs.begin(), envs.end(),
                     [this, &device](auto env) -> bool {
                         return static_cast<bool>(
                             PropertyMatch(device.get(), env.first.toUtf8(),
                                           env.second.toStdString()));
                     }))
    {
        return false;
    }
    return true;
}

bool DeviceImpl::MatchDevice(const ScopedUdevDevice& device,
                             bool parent) const {
    if (udev_device_get_is_initialized(device.get()) != 1) return false;
    if (device_ids.empty())
    {
        return MatchDevicePropertyAttr(device);
    }

    if (!std::any_of(device_ids.begin(), device_ids.end(),
                     [&device, &parent, this](const auto& device_id) {
                         if (MatchVidPid(device, device_id.vendor_id.toUtf8(),
                                         device_id.product_id.toUtf8(), parent))
                         {
                             return MatchDevicePropertyAttr(device);
                         }
                         return false;
                     }))
    {
        return false;
    }

    return true;
}

bool DeviceImpl::FindDevice() {
    if (!udev) return false;

    ScopedUdevEnumerate enumerator(udev_enumerate_new(udev.get()));
    if (!enumerator ||
        udev_enumerate_add_match_subsystem(enumerator.get(),
                                           subsystem.toUtf8()) < 0 ||
        udev_enumerate_scan_devices(enumerator.get()) < 0)
    {
        FMTLOG_ERROR("Create enumerate failed!");
        return false;
    }

    struct udev_list_entry* device_list = nullptr;
    udev_list_entry_foreach(device_list,
                            udev_enumerate_get_list_entry(enumerator.get())) {
        const char* device_syspath = udev_list_entry_get_name(device_list);
        ScopedUdevDevice device(
            udev_device_new_from_syspath(udev.get(), device_syspath));
        if (!device) continue;
        if (udev_device_get_is_initialized(device.get()) != 1) continue;
        if (MatchDevice(device))
        {
            FMTLOG_DEBUG("Match device_syspath: {}", device_syspath);
            this->device = std::move(device);
            return true;
        }
    }

    FMTLOG_WARN("No matching device (device_id : {}, subsystem {}) was found.",
                device_ids, subsystem);
    return false;
}

std::optional<QString> DeviceImpl::GetBDF() {
    if (!FindDevice())
    {
        return std::nullopt;
    }
    for (const auto& [vendor_id, product_id] : device_ids)
    {
        auto* match_device =
            MatchVidPid(device, vendor_id.toUtf8(), product_id.toUtf8());
        if (match_device)
        {
            return udev_device_get_sysname(match_device);
        }
    }
    return std::nullopt;
}

bool DeviceImpl::WaitForAction([[maybe_unused]] std::optional<QString> action,
                               [[maybe_unused]] int timeout) {
    if (!udev) return false;

    monitor.reset(udev_monitor_new_from_netlink(udev.get(), "udev"));
    if (!monitor ||
        udev_monitor_filter_add_match_subsystem_devtype(
            monitor.get(), subsystem.toUtf8(), nullptr) < 0 ||
        udev_monitor_filter_update(monitor.get()) < 0 ||
        udev_monitor_enable_receiving(monitor.get()) < 0)
    {
        FMTLOG_DEBUG("Create monitor failed!");
        return false;
    }

    std::vector<struct pollfd> fds = {
        {udev_monitor_get_fd(monitor.get()), POLLIN, 0},
    };

    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto remaining_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());

        FMTLOG_DEBUG("Remaining timeout : {}ms", remaining_time.count());

        if (poll(fds.data(), fds.size(), int(remaining_time.count())))
        {
            if (OnUdevEvent(action.has_value() ? std::optional<std::string>(
                                                     action->toStdString())
                                               : std::nullopt))
            {
                return true;
            }
        }
    }
    FMTLOG_WARN("{}, {}", action.has_value() ? *action : "", timeout);
    return false;
}

bool DeviceImpl::OnUdevEvent(std::optional<std::string> wait_action) {
    auto event_device =
        ScopedUdevDevice(udev_monitor_receive_device(monitor.get()));
    if (event_device)
    {
        const char* action = udev_device_get_action(event_device.get());
        const char* devnode = udev_device_get_devnode(event_device.get());
        const char* subsystem = udev_device_get_subsystem(event_device.get());
        const char* devtype = udev_device_get_devtype(event_device.get());

        FMTLOG_DEBUG(
            "Device Event: {}, DevNode: {}, Subsystem: {}, DevType: {}",
            action ? action : "", devnode ? devnode : "",
            subsystem ? subsystem : "", devtype ? devtype : "");

        if (!action || (wait_action.has_value() && action != *wait_action))
        {
            FMTLOG_DEBUG("Action not match");
            return false;
        }

        if (MatchDevice(event_device))
        {
            return true;
        }
        FMTLOG_DEBUG("Device not match");
    }
    return false;
}
void OnMbimOperation(const std::function<void()>& callback) {
    QEventLoop loop;
    std::unique_ptr<std::thread> mbim_thread;
    QTimer::singleShot(0, [&]() {
        mbim_thread = std::make_unique<std::thread>([&]() {
            callback();
            loop.quit();
        });
    });

    loop.exec();
    mbim_thread->join();
}
std::optional<QString> ModemImpl::SendATCommand(const QString& cmd,
                                                int timeout) const {
    QString output;
    FMTLOG_DEBUG("Command : {}, timeout: {}", cmd, timeout);
    int ret = error::ERR;
    OnMbimOperation([&]() {
        ret = afal::mbim::MbimDevice::getInstance().AfalMbimAtOverMbimSetSync(
            cmd, output, timeout);
    });
    if (ret != error::OK)
    {
        return std::nullopt;
    }
    return output;
}

bool ModemImpl::MBIMReboot() const {
    FMTLOG_DEBUG("Command : mbim reboot,not supported in linux");
    return false;
}

} // namespace afal

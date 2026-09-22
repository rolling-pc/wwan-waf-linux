#ifndef MODEM_LOG_INTERFACE_H_
#define MODEM_LOG_INTERFACE_H_

#include "common.h"
#include <memory>
#include <qobject.h>
#include <qobjectdefs.h>
#include <string>
#include <utility>
#include <optional>

namespace afal {

class ModemLogger : public QObject {
    //Q_OBJECT
  public:

    struct Config {
        std::string diag_port;
        std::string mdlog_path;
        int modem_level; // 1 <= modem_level <= 6 max:1 min:6
        std::string modem_cfg_path;
        int each_log_size;
        int log_max_file_num;
        bool auto_dump;
        bool syncSysTime;
        bool sapLog;
        bool offline_log_ctrl{};
        bool ipc_log_ctrl{};
        std::vector<QString> system_sap_log_list;
        afal::CompressedFormat compressed_format;

#if defined(Win32)
        Config(const std::string& diag_port, const std::string& mdlog_path,
               int modem_level = 1, const std::string& modem_cfg_path = "",
               int each_log_size = 400, int log_max_file_num = 0,bool auto_dump = true,
               bool syncSysTime = true,
               bool sapLog = false, bool offline_log_ctrl = false,
               bool ipc_log_ctrl = false,
               std::vector<QString> system_sap_log_list = {},
               afal::CompressedFormat compressed_format = CompressedFormat::XZ
               )
            : diag_port(diag_port), modem_level(modem_level),
              modem_cfg_path(modem_cfg_path), mdlog_path(mdlog_path),
              each_log_size(each_log_size), log_max_file_num(log_max_file_num),auto_dump(true),
              syncSysTime(syncSysTime),
              sapLog(sapLog),
              offline_log_ctrl(offline_log_ctrl),
              ipc_log_ctrl(ipc_log_ctrl),
              system_sap_log_list(system_sap_log_list),
              compressed_format(compressed_format) {}
#else
        Config(std::string diag_port,
               std::string mdlog_path = "/var/log/MDLog/", int modem_level = 1,
               std::string modem_cfg_path = "",
               int each_log_size = 400,  // NOLINT
               int log_max_file_num = 0, // NOLINT
               bool auto_dump = false,
               bool syncSysTime = true, bool sapLog = false,bool offline_log_ctrl = false,
               bool ipc_log_ctrl = false,
               std::vector<QString> system_sap_log_list = {},
               afal::CompressedFormat compressed_format = CompressedFormat::XZ
               )
            : diag_port(std::move(diag_port)), modem_level(modem_level),
              modem_cfg_path(std::move(modem_cfg_path)),
              mdlog_path(std::move(mdlog_path)), each_log_size(each_log_size),
              log_max_file_num(log_max_file_num),auto_dump(true),
              syncSysTime(syncSysTime),
              sapLog(sapLog), offline_log_ctrl(offline_log_ctrl),
              ipc_log_ctrl(ipc_log_ctrl),
              system_sap_log_list(system_sap_log_list),
              compressed_format(compressed_format) {}
#endif
    };

    ModemLogger(const ModemLogger&) = delete;
    ModemLogger(ModemLogger&&) = delete;
    ModemLogger& operator=(const ModemLogger&) = delete;
    ModemLogger& operator=(ModemLogger&&) = delete;
    ModemLogger() = default;
    ~ModemLogger() override = default;
    // linux e.g: modem_logger.SetConfig("/dev/ttyUSB0")
    // windows e.g:
    // modem_logger.SetConfig("com1", "C:\User\ght\AppData\fibocom\Default.cfg")
    virtual void SetConfig(Config config) = 0;

    // This function returns true if all log types are currently enabled;
    // otherwise, false.
    [[nodiscard]] virtual bool Status() = 0;
    [[nodiscard]] virtual bool modeStatus() = 0;

    // This function creates and runs a thread that captures logs from the
    // modem. The logging will continue in the background. Call Stop() to stop
    // logging and free resources.
    [[nodiscard]] virtual bool Start() = 0;

    // This function stops the logging thread and releases resources.
    [[nodiscard]] virtual bool Stop() = 0;

    // This function allows logging to start capturing modem logs.
    [[nodiscard]] virtual bool Enable() = 0;

    // This function stops logging from capturing modem logs.
    [[nodiscard]] virtual bool Disable() = 0;

    // This function is for 350 windows to switch debug port.
    [[nodiscard]] virtual bool SetDebugPort(bool enable) = 0;

    // This function is for 350 windows to check debug port status.
    [[nodiscard]] virtual bool CheckDebugPortStatus() = 0;

    [[nodiscard]] virtual std::optional<QString>
    SendATCommand(const QString& cmd, int timeout = 20) const = 0;

    // This function creates and returns a pointer to a ModemLogger object.
    // You can specify the type of modem to create.
    // You need to manage the life cycle of the return pointer.
    [[nodiscard]] static ModemLogger*
    Create(afal::ModemType modem_type = afal::ModemType::QC_USB_GC);
/*  signals:
    // TODO 需要实现这个功能
    void StorageSpaceLowSignal();*/

    virtual bool sapStart() = 0;
    virtual bool sapStop() = 0;
    // QC offline log: remove on-device diag_mdlog autostart. Default no-op.
    virtual bool OfflineLogUninstall() { return true; }
    virtual bool OfflineIpcLogUninstall() { return true; }
};

using ModemLoggerType = ModemLogger* (*)(afal ::ModemType modem_type);

[[maybe_unused]] static ModemLoggerType
LoadMdLogCreateFunc(const QString& lib_path) {
    static std::map<QString, std::unique_ptr<QLibrary>> library_map;
    auto it = library_map.find(lib_path);
    if (it == library_map.end())
    {
        auto library = LoadLibrary(nullptr, lib_path);
        if (!library)
        {
            return nullptr;
        }
        library_map.emplace(lib_path, std::move(library));
    }
    return ResolveFunc<ModemLoggerType>(library_map.at(lib_path).get(),
                                        "MdLogCreate");
}

} // namespace afal

#endif // MODEM_LOG_INTERFACE_H_

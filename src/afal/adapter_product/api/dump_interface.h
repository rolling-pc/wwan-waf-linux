#ifndef DUMP_INTERFACE_H_
#define DUMP_INTERFACE_H_
#include <QtCore/qobject.h>
#include "common.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace afal {

enum class ModemDumpType {
    AP_DUMP,
    MODEM_DUMP,
    FULL_DUMP,
    ENABLE_DUMP,
    MINI_DUMP,
    NONE,
};

enum class DumpStatus {
    ENABLE,
    DISABLE,
    NONE,
};

class DumpCapture : public QObject {
    Q_OBJECT
  public:
    DumpCapture(const DumpCapture&) = delete;
    DumpCapture(DumpCapture&&) = delete;
    DumpCapture& operator=(const DumpCapture&) = delete;
    DumpCapture& operator=(DumpCapture&&) = delete;
    DumpCapture() : QObject() {}
    virtual ~DumpCapture() = default;
    struct Config {
        std::string dump_port;
        std::string dump_path;
        CompressedFormat compressed_format;

#if defined(Win32)
        Config(std::string dump_port, std::string dump_path,
               CompressedFormat compressed_format = CompressedFormat::XZ)
            : dump_port(std::move(dump_port)), dump_path(std::move(dump_path)),
              compressed_format(compressed_format) {}
#else
        Config(std::string dump_port = "/dev/ttyUSB0",
               std::string dump_path = "/var/log/modem_dumps/",
               CompressedFormat compressed_format = CompressedFormat::XZ)
            : dump_port(std::move(dump_port)), dump_path(std::move(dump_path)),
              compressed_format(compressed_format) {}
#endif
    };

    virtual void SetConfig(const Config& config = Config()) = 0;

    [[nodiscard]] virtual std::map<ModemDumpType, DumpStatus> Status() = 0;
    [[nodiscard]] virtual bool
    Control(ModemDumpType dump_type = ModemDumpType::FULL_DUMP,
            DumpStatus dump_status = DumpStatus::ENABLE) = 0;

    [[nodiscard]] virtual ModemDumpType QueryDump() = 0;

    [[nodiscard]] virtual bool Start() = 0;

    virtual bool Initialize() = 0;
    virtual bool ForceDumpTrigger(ModemDumpType dump_type) = 0;
    virtual bool setDebugPort(int mode) = 0;
    virtual bool StartMiniDumpMonitor() = 0;
    virtual bool StopMiniDumpMonitor() = 0;

    // Create a DumpCapture object
    static DumpCapture* Create(ModemType modem_type = ModemType::QC_USB_GC);

    signals:
    void dumpFinished();
};

using DumpCaptureType = DumpCapture* (*)(afal ::ModemType modem_type);

[[maybe_unused]] static DumpCaptureType
LoadDumpCaptureCreateFunc(const QString& lib_path) {
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
    return ResolveFunc<DumpCaptureType>(library_map.at(lib_path).get(),
                                        "DumpCaptureCreate");
}
} // namespace afal

#endif // DUMP_INTERFACE_H_

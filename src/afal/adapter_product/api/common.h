#ifndef COMMON_H_
#define COMMON_H_

#include "log.hpp"
#include <QtCore/qlibrary.h>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <spdlog/spdlog.h>

#include <memory>
#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(BASE_IMPLEMENTATION)
#define BASE_EXPORT __declspec(dllexport)
#else
#define BASE_EXPORT __declspec(dllimport)
#endif // defined(BASE_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(BASE_IMPLEMENTATION)

#define BASE_EXPORT __attribute__((visibility("default")))

#else
#define BASE_EXPORT
#endif // defined(BASE_IMPLEMENTATION)
#endif
#else // defined(COMPONENT_BUILD)
#define BASE_EXPORT
#endif

namespace afal {

enum class CompressedFormat {
    NONE,
    XZ,
    ZIP,
};

enum class ModemType {
    QC_USB_GC,
    QC_USB_RW101,
    QC_USB_RW135,
    QC_USB_RW135R,
    QC_USB_RW151,
    QC_PCIE_RW135R,
    QC_PCIE_RW151,
    QC_PCIE_GC,
    MTK_USB_GC,
    MTK_USB_RW350R,
    MTK_PCIE_GC,
    MTK_PCIE_RW350
};

enum class ModeModem {
    QC_USB_DEBUG,
    QC_USB_USER,
    MTK_PCIE_DEBUG,
    MTK_USB_USER,
    MTK_USB_DEBUG,
    MTK_USB_DEBUG_IPPACKAGE
};

[[maybe_unused]] static std::unique_ptr<QLibrary>
LoadLibrary(std::unique_ptr<QLibrary> library, const QString& lib_path) {
    using namespace log;
    if (!library || !library->isLoaded())
    {
        library = std::make_unique<QLibrary>(lib_path);
    }
    FMTLOG_INFO("Attempting to load library: {}", lib_path.toStdString());
    if (!library->load())
    {
        FMTLOG_ERROR("Load library failed!, error: {}", library->errorString().toStdString());
        return nullptr;
    }
    FMTLOG_INFO("Library loaded successfully: {}", lib_path.toStdString());
    return library;
}

template <class T> T ResolveFunc(QLibrary* libraray, const char* func_name) {
    if (!libraray || !libraray->isLoaded())
    {
        using namespace log;
        FMTLOG_ERROR("library not load, {} load failed!", func_name);
        return nullptr;
    }
    auto func = reinterpret_cast<T>(libraray->resolve(func_name));
    if (!func)
    {
        using namespace log;
        FMTLOG_ERROR("Load library failed!, error: {}",
                     libraray->errorString().toStdString());
        return nullptr;
    }
    return func;
}

} // namespace afal

#endif // COMMON_H_

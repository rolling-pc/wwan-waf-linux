#include "dump.h"
#include "common/proccommon.h"

#include <iostream>
#include <memory>

#ifdef BUILD_QC
#include "qc/usb/linux/dump_qc.h"
//#include "qc/usb/windows/dump_qc.h"
#elif BUILD_MTK
#include "mtk/pcie/linux/dump_mtk_pcie.h"
#include "mtk/pcie/windows/dump_mtk_pcie_win.h"
#include "mtk/usb/linux/dump_mtk_usb.h"
#else

#endif


namespace afal {

using namespace log;

DumpCaptureImpl::DumpCaptureImpl(ModemType modem_type)
    : modem_type(modem_type) {
    FMTLOG_INFO("Dump version : {}, Modem type : {}", DUMP_VERSION_STRING,
                GetModemTypeName(modem_type));
    FMTLOG_DEBUG("Debug is on.");
}

// Consider if intput dump_port or dump_path is empty 
void DumpCaptureImpl::SetConfig(const Config& config) {
    if (!this->config)
    {
        this->config = std::make_unique<Config>(config);
        return;
    }

    Config newConfig = *this->config;

    if (!config.dump_port.empty())
    {
        newConfig.dump_port = config.dump_port;
    }

    if (!config.dump_path.empty())
    {
        newConfig.dump_path = config.dump_path;
    }

    newConfig.compressed_format = config.compressed_format;

    this->config = std::make_unique<Config>(newConfig);
}

DumpCapture* DumpCapture::Create(afal::ModemType modem_type) {
    switch (modem_type)
    {
#ifdef BUILD_QC
        case ModemType::QC_USB_RW101:
        case ModemType::QC_USB_RW135:
        case ModemType::QC_USB_GC:
        case ModemType::QC_USB_RW135R:
        case ModemType::QC_USB_RW151:
        case ModemType::QC_PCIE_RW151:
            return new DumpCaptureQc(modem_type); // NOLINT
        case ModemType::QC_PCIE_GC:
#elif BUILD_MTK
        case ModemType::MTK_PCIE_RW350:
        case ModemType::MTK_PCIE_GC:
            return new DumpCaptureMTKPcie(modem_type); // NOLINT
    #ifdef _IS_LINUX_
        case ModemType::MTK_USB_GC:
        case ModemType::MTK_USB_RW350R:
            return new DumpCaptureMTKUsb(modem_type); // NOLINT
    #endif
#else
#endif
        default:
            break;
    }
    return nullptr;
}

} // namespace afal

extern "C" {

using namespace afal::log;
BASE_EXPORT afal::DumpCapture* DumpCaptureCreate(afal::ModemType modem_type) {
    auto* ptr = afal::DumpCapture::Create(modem_type);
    return ptr;
}
}

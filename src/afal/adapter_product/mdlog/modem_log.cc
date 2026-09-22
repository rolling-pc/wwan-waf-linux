#include "modem_log.h"
#include "common/proccommon.h"

#include <memory>

#ifdef BUILD_QC
#include "qc/usb/linux/modem_log_qc.h"
#include "qc/usb/linux/rw135R/modem_log_qc_135R.h"
#elif BUILD_MTK

#ifdef _IS_WINDOWS_
#include "mtk/pcie/windows/modem_log_mtk.h"
#else
#include "mtk/pcie/linux/modem_log_mtk.h"
#include "mtk/usb/linux/modem_log_mtk_usb.h"
#endif

#else

#endif

using namespace afal::log;
namespace afal {
    ModemLoggerImpl::ModemLoggerImpl(ModemType modem_type)
        : modem_type(modem_type) {
            //FMTLOG_INFO("FwFlash version : {}, Modem type : {}", MDLOG_VERSION_STRING, GetModemTypeName(modem_type));
        FMTLOG_DEBUG("Debug is on.");

        }

    void ModemLoggerImpl::SetConfig(Config config) {
        // Mutate in place so raw Config* held by ModemLogToolsImpl stays valid.
        if (this->config) {
            *this->config = std::move(config);
        } else {
            this->config = std::make_unique<Config>(std::move(config));
        }
}

ModemLogger* ModemLogger::Create(afal::ModemType modem_type) {
    switch (modem_type)
    {
#ifdef BUILD_QC
        case ModemType::QC_USB_RW101:
        case ModemType::QC_USB_RW135:
        case ModemType::QC_USB_GC:
            LOG_DEBUG("NOW is QC Lib");
            return new ModemLoggerImplQcUSB( // NOLINT
                modem_type);
        case ModemType::QC_USB_RW135R:
        case ModemType::QC_USB_RW151:
        case ModemType::QC_PCIE_RW151:
            LOG_DEBUG("NOW is QC135R Lib");
            return new ModemLoggerImplQc135RUSB( // NOLINT
                modem_type);
        case ModemType::QC_PCIE_GC:
            break;
#elif BUILD_MTK
        case ModemType::MTK_PCIE_RW350:
#ifdef _IS_WINDOWS_
            return new ModemLoggerImplMtkPCIE(modem_type);
#endif
        case ModemType::MTK_PCIE_GC:
#ifdef _IS_LINUX_
        return new ModemLoggerImplPcie(modem_type);  //NOLINT(cppcoreguidelines-owning-memory)
#endif
        case ModemType::MTK_USB_GC:
        case ModemType::MTK_USB_RW350R:
#ifdef _IS_LINUX_
            return new ModemLoggerImplMtkUSB(modem_type);  //NOLINT(cppcoreguidelines-owning-memory)
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
    BASE_EXPORT afal::ModemLogger* MdLogCreate(afal::ModemType modem_type) {
        LOG_DEBUG("[%s] called!", __func__);
        return afal::ModemLogger::Create(modem_type);
    }
}
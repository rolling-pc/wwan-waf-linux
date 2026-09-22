#include "proccommon.h"

namespace afal {

namespace {

struct ModemState {
    ModemType modem_type;
    ModemProduct product;
    std::string type_name;
    std::string product_name;
};

const std::map<ModemType, ModemState> kModemStateMap = {
    {ModemType::QC_USB_GC, {ModemType::QC_USB_GC, ModemProduct::QC, "QC_USB_GC", "QC"}},
    {ModemType::QC_USB_RW101, {ModemType::QC_USB_RW101, ModemProduct::QC, "QC_USB_RW101", "QC"}},
    {ModemType::QC_USB_RW135, {ModemType::QC_USB_RW135, ModemProduct::QC, "QC_USB_RW135", "QC"}},
    {ModemType::QC_USB_RW151, {ModemType::QC_USB_RW151, ModemProduct::QC, "QC_USB_RW151", "QC"}},
    {ModemType::QC_PCIE_GC, {ModemType::QC_PCIE_GC, ModemProduct::QC, "QC_PCIE_GC", "QC"}},
    {ModemType::MTK_USB_GC, {ModemType::MTK_USB_GC, ModemProduct::MTK, "MTK_USB_GC", "MTK"}},
    {ModemType::MTK_USB_RW350R, {ModemType::MTK_USB_RW350R, ModemProduct::MTK, "MTK_USB_RW350R", "MTK"}},
    {ModemType::MTK_PCIE_GC, {ModemType::MTK_PCIE_GC, ModemProduct::MTK, "MTK_PCIE_GC", "MTK"}},
    {ModemType::MTK_PCIE_RW350, {ModemType::MTK_PCIE_RW350, ModemProduct::MTK, "MTK_PCIE_RW350", "MTK"}}
 };

}  // namespace

std::string GetModemTypeName(ModemType type) {
    auto it = kModemStateMap.find(type);

    if (it == kModemStateMap.end()) {
        return "Unknown";
    }

    return kModemStateMap.at(type).type_name;
}

ModemProduct GetModemProduct(ModemType type) {
    auto it = kModemStateMap.find(type);

    if (it == kModemStateMap.end()) {
        return ModemProduct::UNKNOWN;
    }

    return kModemStateMap.at(type).product;
}


}  // namespace afal
#ifndef AFAL_ADAPTER_PRODUCT_COMMON_COMMON_H_
#define AFAL_ADAPTER_PRODUCT_COMMON_COMMON_H_

#include "common.h"
#include "log.hpp"

template <> struct fmt::formatter<QStringList> {
    constexpr auto
    parse( // NOLINT(readability-convert-member-functions-to-static,
           // readability-identifier-naming)
        format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format( // NOLINT(readability-convert-member-functions-to-static,
                 // readability-identifier-naming)
        const QStringList& p, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{}", p.join(" ").toStdString());
    }
};

namespace afal {

enum class ModemProduct {
    QC,
    MTK,
    UNKNOWN,
};

std::string GetModemTypeName(ModemType type);

ModemProduct GetModemProduct(ModemType type);
} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_COMMON_COMMON_H_
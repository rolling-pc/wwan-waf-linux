#ifndef AFAL_ADAPTER_OS_API_LOG_HPP_
#define AFAL_ADAPTER_OS_API_LOG_HPP_

#include <cstdarg>
#include <string>
#include <memory>
#include <vector>
#include <cstring>
#include <type_traits>

#include "spdlog/fmt/fmt.h"

template <typename T, typename = void>
struct has_toStdString : std::false_type {};

template <typename T>
struct has_toStdString<T, std::void_t<decltype(std::declval<T>().toStdString())>> : std::true_type {};

template <typename T, typename = void>
struct has_toUtf8 : std::false_type {};

template <typename T>
struct has_toUtf8<T, std::void_t<decltype(std::declval<T>().toUtf8())>> : std::true_type {};

template <typename T>
struct fmt::formatter<T, char, std::enable_if_t<has_toStdString<T>::value>> {
    static_assert(std::is_same_v<decltype(std::declval<T>().toStdString()), std::string>,
                  "toStdString must return a std::string");

    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const T& p, FormatContext& ctx) const -> decltype(ctx.out()) {
        if constexpr (has_toUtf8<T>::value) {
            return fmt::format_to(ctx.out(), "{}", std::string(p.toUtf8()));
        }
        return fmt::format_to(ctx.out(), "{}", p.toStdString());
    }
};

namespace afal {
    namespace log {
        static constexpr int MSG_SIZE = 2048;
        enum LogLevel {
            TRACE = 0,
            DEBUG,
            INFO,
            WARN,
            ERRO,
            CRITICAL,
            OFF,
        };

        enum LogTarget {
            CONSOLE = 1,
            CUSTOM = 1 << 1,
            LINUX_SYSLOG = 1 << 2,
            WINDOWS_EVENTLOG = 1<< 3,
        };

        struct LogConfig {
            std::string filePathName;
            size_t maxFileSize;
            size_t maxFiles;
            LogConfig()
                : filePathName("afal.log"), maxFileSize(1024*1024*50), maxFiles(3) {}
            LogConfig(std::string fName, size_t fSize = 1024*1024*50, size_t fNum = 3)
                : filePathName(fName), maxFileSize(fSize), maxFiles(fNum) {}
        };

        static std::string CPrint(const char* fmt, ...) {
            std::vector<char> buffer(MSG_SIZE);
            va_list args; // NOLINT(cppcoreguidelines-pro-type-vararg)
            va_start(args, fmt); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            std::vsnprintf(buffer.data(), buffer.size(), fmt, args); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            va_end(args); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            return buffer.data();
        }

        template<typename... Args>
        std::string FmtPrint(fmt::format_string<Args...> fmt, Args ...args)
        {
            fmt::basic_memory_buffer<char, MSG_SIZE> buf;
            fmt::detail::vformat_to(buf, fmt::basic_string_view<char>(fmt), fmt::make_format_args(args...));
            return {buf.data(), buf.size()};
        }

        class Logger {
            public:
                static Logger& Instance();
                int Init(const int& target, const char* tag, const LogConfig& logConfig = LogConfig(), const bool& enc = false);
                int UnInit();
                int SetLevel(const LogLevel& level);
                int EnableEncryption(const bool& enable);
                int EnableDebugMode(const bool& enable);
                int Log(const LogLevel& level, const char* file, const int& line, const std::string& log_message);
            private:
                Logger();
                ~Logger();
                Logger(const Logger&) = delete;
                Logger& operator = (const Logger&) = delete;

                struct LogImpl;
                std::unique_ptr<LogImpl> pImpl;
        };

        #ifdef _IS_WINDOWS_
            #define PATH_SEPARATOR '\\'
        #else
            #define PATH_SEPARATOR '/'
        #endif
        #define __FILENAME__ ([] {const char* lastSep = strrchr(__FILE__, PATH_SEPARATOR);return lastSep ? lastSep + 1 : __FILE__;}()) // NOLINT
        #define LOG_DEBUG(fmt, ...) Logger::Instance().Log(DEBUG, __FILENAME__, __LINE__, CPrint(fmt, ##__VA_ARGS__)) // NOLINT(cppcoreguidelines-pro-type-vararg)
        #define LOG_INFO(fmt, ...) Logger::Instance().Log(INFO, __FILENAME__, __LINE__, CPrint(fmt, ##__VA_ARGS__)) // NOLINT(cppcoreguidelines-pro-type-vararg)
        #define LOG_WARN(fmt, ...) Logger::Instance().Log(WARN, __FILENAME__, __LINE__, CPrint(fmt, ##__VA_ARGS__)) // NOLINT(cppcoreguidelines-pro-type-vararg)
        #define LOG_ERROR(fmt, ...) Logger::Instance().Log(ERRO, __FILENAME__, __LINE__, CPrint(fmt, ##__VA_ARGS__)) // NOLINT(cppcoreguidelines-pro-type-vararg)

        #define FMTLOG_DEBUG(...)  Logger::Instance().Log(DEBUG, __FILENAME__, __LINE__, FmtPrint(__VA_ARGS__)) // NOLINT
        #define FMTLOG_INFO(...)  Logger::Instance().Log(INFO, __FILENAME__, __LINE__, FmtPrint(__VA_ARGS__)) // NOLINT
        #define FMTLOG_WARN(...)  Logger::Instance().Log(WARN, __FILENAME__, __LINE__, FmtPrint(__VA_ARGS__)) // NOLINT
        #define FMTLOG_ERROR(...)  Logger::Instance().Log(ERRO, __FILENAME__, __LINE__, FmtPrint(__VA_ARGS__)) // NOLINT

        #define SS(str) str.c_str()
        #define QS(str) SS(str.toStdString())
    }
}

#endif  // AFAL_ADAPTER_OS_API_LOG_HPP_
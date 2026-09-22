/*
#include "log.hpp"
#include "common.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#ifdef _IS_LINUX_
#include "spdlog/sinks/syslog_sink.h"
#endif
#ifdef _IS_WINDOWS_
#include "spdlog/sinks/win_eventlog_sink.h"
#endif
#include <iostream>
#include <vector>

using namespace afal::error;
namespace afal {
    namespace log {

        struct Logger::LogImpl {
            std::shared_ptr<spdlog::logger> logger; // NOLINT(misc-non-private-member-variables-in-classes)
            bool enable_encryption = false; // NOLINT(misc-non-private-member-variables-in-classes)

            static std::string Encryption(const std::string& message) {
                return message;
            }
        };

        Logger::Logger() : pImpl(std::make_unique<LogImpl>()) {}

        Logger::~Logger() = default;

        Logger& Logger::Instance() {
            static Logger instance;
            return instance;
        }

        int Logger::Init(const int& target, const char* tag, const LogConfig& logConfig, const bool& enc) {
            if (pImpl->logger != nullptr) {
                return LogError::LogNotInit;
            }

            pImpl->enable_encryption = enc;

            std::vector<spdlog::sink_ptr> sinks;
            if (target & LogTarget::CONSOLE) {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e][%n][%l][%t][%g:%#]%v%$");
                sinks.push_back(console_sink);
            }

            if (target & LogTarget::CUSTOM) {
                auto custom_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logConfig.filePathName, logConfig.maxFileSize, logConfig.maxFiles);
                custom_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e][%n][%l][%g:%#]%v%$");
                sinks.push_back(custom_sink);
            }

            if (target & LogTarget::LINUX_SYSLOG) {
                #ifdef _IS_LINUX_
                    auto syslog_sink = std::make_shared<spdlog::sinks::syslog_sink_mt>(tag, LOG_PID, LOG_USER, true);
                    syslog_sink->set_pattern("%^[%l][%g:%#]%v%$");
                    sinks.push_back(syslog_sink);
                #endif
            }

            if (target & LogTarget::WINDOWS_EVENTLOG) {
                #ifdef _IS_WINDOWS_
                    auto event_sink = std::make_shared<spdlog::sinks::win_eventlog_sink_mt>(tag);
                    event_sink->set_pattern("%^[%l][%g:%#]%v%$");
                    sinks.push_back(event_sink);
                #endif
            }

            pImpl->logger = std::make_shared<spdlog::logger>(tag, sinks.begin(), sinks.end());

            spdlog::set_default_logger(pImpl->logger);
            spdlog::set_level(spdlog::level::info);
            return ExitCode::OK;
        }

        int Logger::UnInit() {
            if (pImpl->logger != nullptr) {
                pImpl->logger->flush();
                spdlog::drop(pImpl->logger->name());
                pImpl->logger.reset();
            }
            return ExitCode::OK;
        }

        int Logger::SetLevel(const LogLevel& level) {
            if (pImpl->logger == nullptr) {
                std::cout << "Logger object has no init." << std::endl;
                return LogError::LogNotInit;
            }

            auto spdlog_level = static_cast<spdlog::level::level_enum>(level);
            pImpl->logger->set_level(spdlog_level);
            return ExitCode::OK;
        }

        int Logger::Log(const LogLevel& level, const char* file, const int& line, const std::string& log_message) {
            if (!pImpl->logger) {
                std::cout << "Logger object has no init, message:" << log_message << std::endl;
                return LogError::LogNotInit;
            }

            std::string message = log_message;
            if (pImpl->enable_encryption) {
                message = pImpl->Encryption(log_message);
            }

            auto spdlog_level = static_cast<spdlog::level::level_enum>(level);
            pImpl->logger->log(spdlog::source_loc{file, line, ""}, spdlog_level, "{}", message);
            pImpl->logger->flush();
            return ExitCode::OK;
        }

        int Logger::EnableEncryption(const bool& enable) {
            pImpl->enable_encryption = enable;
            return ExitCode::OK;
        }

        int Logger::EnableDebugMode(const bool& enable) {
            if (enable) {
                SetLevel(static_cast<LogLevel>(spdlog::level::debug));
            } else {
                SetLevel(static_cast<LogLevel>(spdlog::level::info));
            }
            return ExitCode::OK;
        }
    }
}*/

#include "log.hpp"
#include "common.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/async.h"
#ifdef _IS_LINUX_
#include "spdlog/sinks/syslog_sink.h"
#endif
#ifdef _IS_WINDOWS_
#include "spdlog/sinks/win_eventlog_sink.h"
#endif
#include <iostream>
#include <vector>

using namespace afal::error;

namespace afal {
namespace log {
struct Logger::LogImpl {
    std::shared_ptr<spdlog::logger> logger;
    bool enable_encryption = false;

    static std::string Encryption(const std::string& msg) { return msg; }
};
#ifdef _IS_LINUX_
class SafeSyslogSink : public spdlog::sinks::syslog_sink_mt {
    public:
        using syslog_base = spdlog::sinks::syslog_sink_mt;
        SafeSyslogSink(const std::string& ident, int option, int facility, bool with_pid)
            : syslog_base(ident, option, facility, with_pid) {}
    
        void sink_it_(const spdlog::details::log_msg& msg) override {
            try {
                syslog_base::sink_it_(msg);
            } catch (const std::exception& e) {
                // 后台线程捕获异常，打印到 cerr
                std::cerr << "[SafeSyslogSink] exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[SafeSyslogSink] unknown exception" << std::endl;
            }
        }
    };
#endif   
Logger::Logger() : pImpl(std::make_unique<LogImpl>()) {}
Logger::~Logger() = default;

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

// ------------------- 异步 Logger 初始化 -------------------
int Logger::Init(const int& target, const char* tag, const LogConfig& logConfig, const bool& enc) {
    if (pImpl->logger != nullptr) return LogError::LogNotInit;

    pImpl->enable_encryption = enc;

    // 1. 创建线程池 (队列大小 16384, 后台线程 1 个)
    spdlog::init_thread_pool(16384, 2);
    //spdlog::set_level(spdlog::level::info);
    std::vector<spdlog::sink_ptr> sinks;

    if (target & LogTarget::CONSOLE) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e][%n][%l][%t][%g:%#]%v%$");
        sinks.push_back(console_sink);
    }

    if (target & LogTarget::CUSTOM) {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logConfig.filePathName, logConfig.maxFileSize, logConfig.maxFiles
        );
        file_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e][%n][%l][%g:%#]%v%$");
        sinks.push_back(file_sink);
    }

#ifdef _IS_LINUX_
    if (target & LogTarget::LINUX_SYSLOG) {
        auto syslog_sink = std::make_shared<SafeSyslogSink>(tag, LOG_PID, LOG_USER, true);
        syslog_sink->set_pattern("%^[%l][%g:%#]%v%$");
        sinks.push_back(syslog_sink);
    }
#endif

#ifdef _IS_WINDOWS_
    if (target & LogTarget::WINDOWS_EVENTLOG) {
        auto event_sink = std::make_shared<spdlog::sinks::win_eventlog_sink_mt>(tag);
        event_sink->set_pattern("%^[%l][%g:%#]%v%$");
        sinks.push_back(event_sink);
    }
#endif

    // 2. 创建 async logger
    pImpl->logger = std::make_shared<spdlog::async_logger>(
        tag,
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::discard_new // 队列满时丢掉旧日志，避免阻塞
    );

    spdlog::register_logger(pImpl->logger);
    spdlog::set_default_logger(pImpl->logger);
    spdlog::set_level(spdlog::level::debug); // 默认 debug 级别

    // 3. 异步 flush 策略
    //pImpl->logger->flush_on(spdlog::level::info); // info 及以上才 flush
    pImpl->logger->flush_on(spdlog::level::err);
    spdlog::flush_every(std::chrono::seconds(5));
    return ExitCode::OK;
}

// ------------------- UnInit -------------------
int Logger::UnInit() {
    //if (pImpl->logger != nullptr) {
    //    pImpl->logger->flush(); 
    //    spdlog::drop(pImpl->logger->name());
    //    pImpl->logger.reset();
    //}
    if (pImpl->logger != nullptr) {
        try {
            pImpl->logger->flush(); 
            spdlog::set_default_logger(nullptr); 
            spdlog::drop(pImpl->logger->name());
        } catch (...) {
            
        }
        pImpl->logger.reset();
    }
    spdlog::shutdown();
    return ExitCode::OK;
}

// ------------------- SetLevel -------------------
int Logger::SetLevel(const LogLevel& level) {
    if (pImpl->logger == nullptr) {
        std::cout << "Logger object has no init." << std::endl;
        return LogError::LogNotInit;
    }

    auto spdlog_level = static_cast<spdlog::level::level_enum>(level);
    pImpl->logger->set_level(spdlog_level);
    return ExitCode::OK;
}

// ------------------- Log -------------------
int Logger::Log(const LogLevel& level, const char* file, const int& line, const std::string& log_message) {
    if (!pImpl->logger) {
        std::cerr << "Logger not init: " << log_message << std::endl;
        return LogError::LogNotInit;
    }

    std::string msg = log_message;
    if (pImpl->enable_encryption) msg = pImpl->Encryption(log_message);

    auto spd_level = static_cast<spdlog::level::level_enum>(level);
    pImpl->logger->log(spdlog::source_loc{file, line, ""}, spd_level, "{}", msg);

    return ExitCode::OK;
}


// ------------------- 其他配置 -------------------
int Logger::EnableEncryption(const bool& enable) {
    pImpl->enable_encryption = enable;
    return ExitCode::OK;
}

int Logger::EnableDebugMode(const bool& enable) {
    if (enable) {
        SetLevel(static_cast<LogLevel>(spdlog::level::debug));
    } else {
        SetLevel(static_cast<LogLevel>(spdlog::level::info));
    }
    return ExitCode::OK;
}

} // namespace log
} // namespace afal
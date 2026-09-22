#ifndef AFAL_ADAPTER_OS_API_GLOBAL_HPP_
#define AFAL_ADAPTER_OS_API_GLOBAL_HPP_
#include "cliLog.hpp"

namespace afal {
    namespace clilog {
    
        LOG_API CLILogger* GetCliLoggerInternal();
        LOG_API CLILogger* GetEventLoggerInternal();    

        extern LOG_API LoggerProxy cliLogger;
        extern LOG_API LoggerProxy eventLogger;
}
}

#define CLI_DEBUG(logger, fmt, ...) do { if(afal::clilog::logger) { (afal::clilog::logger)->Log(afal::clilog::D, QString().asprintf(fmt, ##__VA_ARGS__)); } } while(0)
#define CLI_INFO(logger, fmt, ...)  do { if(afal::clilog::logger) { (afal::clilog::logger)->Log(afal::clilog::I, QString().asprintf(fmt, ##__VA_ARGS__)); } } while(0)
#define CLI_WARN(logger, fmt, ...)  do { if(afal::clilog::logger) { (afal::clilog::logger)->Log(afal::clilog::W, QString().asprintf(fmt, ##__VA_ARGS__)); } } while(0)
#define CLI_ERROR(logger, fmt, ...) do { if(afal::clilog::logger) { (afal::clilog::logger)->Log(afal::clilog::E, QString().asprintf(fmt, ##__VA_ARGS__)); } } while(0)
#endif //AFAL_ADAPTER_OS_API_GLOBAL_HPP_
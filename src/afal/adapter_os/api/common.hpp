#ifndef AFAL_ADAPTER_OS_API_COMMON_HPP_
#define AFAL_ADAPTER_OS_API_COMMON_HPP_

namespace afal {
    namespace error {
        enum ExitCode {
            ERR = -1,
            // The modem itself answered with a definitive AT final result code
            // "ERROR" (without +CME detail). Distinct from ERR so callers can
            // tell a rejected command apart from a transport failure.
            ERR_AT_ERROR = -2,
            OK = 0,
        };
    }

    namespace log {
        enum LogError {
            LogNotInit = 1,
        };
    }
}

#endif  // AFAL_ADAPTER_OS_API_COMMON_HPP_
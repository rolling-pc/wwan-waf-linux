#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_MTK_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_MTK_H_

#include <Windows.h>
#include <guiddef.h> 
#include <atlbase.h>
#include <atlcomcli.h>
#include "common/modem.h"
#include <memory>

/*
TraceCmd == 0 (MODE)
o 0: Off
o 1: Streaming
o 2: Wrapping
*/
enum TRACE_CMD_MODE_VALUE {
    TRACE_CMD_MODE_VALUE_OFF = 0,
    TRACE_CMD_MODE_VALUE_STREAMING = 1,
    TRACE_CMD_MODE_VALUE_WRAPPING = 2,
    TRACE_CMD_MODE_VALUE_MAX
};

typedef struct _MBIM_SET_TRACE_CONFIG {
    UINT32 TraceSetCmd;
    UINT32 TraceValue;
} MBIM_SET_TRACE_CONFIG;

typedef struct _MBIM_QUERY_TRACE_CONFIG
{
	UINT32 TraceQueryCmd;
	UINT32 TraceValue;

} MBIM_QUERY_TRACE_CONFIG;

typedef enum _MBIM_TRACE_CONFIG_TRACE_CMD {

    TRACE_CONFIG_TRACECMD_MODE = 0,
    TRACE_CONFIG_TRACECMD_LEVEL = 1,
    TRACE_CONFIG_TRACECMD_LOCATION = 2,
    /*
        https://eservice.mediatek.com/eservice-portal/issue_manager/update/107186436
        ALPS05980696 [WIN] System can't sleep with MD logging enabled via PCIe
    */
    TRACE_CONFIG_TRACECMD_FLUSH_INTERVAL = 3,
    TRACE_CONFIG_TRACECMD_MAX

} MBIM_TRACE_CONFIG_TRACE_CMD;

namespace afal {


class ModemMTK : public Modem {
  public:
    ModemMTK() = default;
    ModemMTK(const ModemMTK&) = delete;
    ModemMTK(ModemMTK&&) = delete;
    ModemMTK& operator=(const ModemMTK&) = delete;
    ModemMTK& operator=(ModemMTK&&) = delete;
    ModemMTK(const Modem&) = delete;
    ModemMTK(Modem&&) = delete;
    ModemMTK& operator=(const Modem&) = delete;
    ModemMTK& operator=(Modem&&) = delete;
    ~ModemMTK() override = default;

    [[nodiscard]] virtual bool SetIntelTrace(int level) const = 0;
    [[nodiscard]] virtual std::optional<std::pair<int, int>>
    GetIntelTrace() const = 0;
    [[nodiscard]] virtual bool SwitchMdDebugPort(bool open) = 0;
    [[nodiscard]] virtual bool SyncTraceSystemTime() const = 0;
    [[nodiscard]] virtual bool SetDebugPort(bool enable) = 0;
    [[nodiscard]] virtual bool WaitforDebugPortArrival(DWORD deviceContextID,
                                               DWORD ms_timeout,
                                               DWORD ms_MINinterval) = 0;

    bool MBIMReboot() const override = 0;
    
    static std::unique_ptr<ModemMTK> Create(std::unique_ptr<Modem> modem);
};

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_MTK_H_
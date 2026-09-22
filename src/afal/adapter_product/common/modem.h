#ifndef AFAL_ADAPTER_PRODUCT_FLASH_COMMON_MODEM_H_
#define AFAL_ADAPTER_PRODUCT_FLASH_COMMON_MODEM_H_

#include <QString>
#include <QObject>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <qobjectdefs.h>
#include <qthread.h>
#ifdef _IS_WINDOWS_

#if defined(MODEM_BUILD)
#define MODEM __declspec(dllexport)
#else
#define MODEM __declspec(dllimport)
#endif

#include <initguid.h>
#include <Windows.h>
#include <SetupAPI.h>
#include <tchar.h>
#include <iostream>
#include <Qmap>
#include <Cfgmgr32.h>
#include <QMutex>
#include <powrprof.h>
#include <powerbase.h>
#ifdef WAF_SKIP_MINIDEBUGLOGGER
class ADBShell;
#else
#include "MiniDebugLogger/ADBShell.h"
#endif
#endif



namespace afal {

// Device NAME -> handle
typedef enum _DEVICE_CONTEXT_NAME {
    FLASH_PORT = 0,
    INTEL_WWAN_M80_NETADAPTER = 1,
    AT_DEBUG_PORT = 2,
    MD_LOGGING_PORT = 3,
    SAP_LOGGING_PORT = 4,
    ADB_PORT = 5,
    NPT_PORT = 6,
    META_PORT = 7,
    FULL_RAM_DUMP_PORT = 8,
    GNSS_PORT = 9,
    GNSS_TEST_PORT = 10,
    META_MD_PORT = 11
} DEVICE_CONTEXT_NAME;

#ifdef _IS_WINDOWS_
typedef struct ADB_USE_STATE {
    bool g_adb_for_sap = false; // sap logging, use in FetchSapTraceByADBThread
    bool g_adb_for_sap_boot =
        false; // sap boot logging, use in FetchSapBootTraceByADBThread
    bool g_adb_for_sap_stop = false; // sap logging stop, use in SapTraceStop
    bool g_adb_for_minidump =
        false; // minidump collection, use in MiniDumpCollectionThread
    bool g_adb_for_minidump_newpath =
        false; // minidump collection, use in MiniDumpNewPathCollectionThread
    bool g_adb_for_set_md_dump =
        false; // set modem dump type, use in SetModemDumpTypeByAdb
    bool g_adb_for_gps_log_enable = false;     // GPS_LOGGING_ENABLE
    bool g_adb_for_nvm_log_enable = false;     // SetNVRAMLogByAdb
    bool g_adb_for_nvm_log_pull = false;       // pullModemNvramTraceLogThread
    bool g_adb_for_uart_log = false;           // SetUARTLogByAdb
    bool g_adb_for_uart_pull = false;          // pullModemURATTraceLogThread
    bool g_adb_for_md_db_pull = false;         // PullModemDataBaseThread
    bool g_adb_for_modem_gnss_logging = false; // modem gnss logging
} adb_use_state; // control in ModemTraceComponent::CheckADBUseState()

extern MODEM adb_use_state _adb_use_state;
extern MODEM std::mutex mt_connectADB;
extern MODEM std::mutex mt_openADB;
extern MODEM bool tool_opened_ADB;
MODEM bool CheckADBUseState();
#endif

namespace {

constexpr int K_WAIT_ACTION_TIMEOUT = 20;
constexpr int K_MBIM_TIMEOUT = 20;

} // namespace

struct DeviceId {
    QString vendor_id;
    QString product_id;

    DeviceId(QString vid, QString pid)
        : vendor_id(std::move(vid)), product_id(std::move(pid)){};
    bool operator<(const DeviceId& other) const {
        return this->vendor_id < other.vendor_id ||
               (this->vendor_id == this->vendor_id &&
                this->product_id < other.product_id);
    };

    [[nodiscard]] std::string ToStdString() const {
        return vendor_id.toStdString() + ":" + product_id.toStdString();
    };
};

#ifdef _IS_WINDOWS_
class DeviceMonitorW;

struct DeviceContext {
    GUID guid;
    QString hwId;
    QString deviceName;
    QString interfaceId;
    bool deviceExists = false;
    int handleIndex = -1;
    HCMNOTIFICATION notificationHandle = nullptr;
    QObject* monitor = nullptr;
};

typedef enum _POWER_STATE {
    DEVICE_RESUMED,
    DEVICE_SUSPENDED,
    POWER_STATE_MAX
} POWER_STATE;

extern DWORD g_DwPowerState;
extern PVOID g_RegistrationHandle;

class DeviceMonitorW : public QObject {
    Q_OBJECT

public:
    explicit DeviceMonitorW(QObject* parent = nullptr);
    ~DeviceMonitorW() override;

    bool initializeMonitor(const GUID& guid, const QString& hwId,
                           const QString& deviceName, int handleIndex);

    bool getDeviceStatus(int handleIndex);

    BOOL IsSpecificPortOpen(QString& devicePath, const GUID& GUIDPath,
                            QString& deviceKey);

    BOOL IsNetadapterNetDeviceExist();

    QString getDevicePath(int handleIndex);

signals:
    void deviceStateChanged(CM_NOTIFY_ACTION action,
                            const DeviceContext& context);

private:
    void OnDeviceInterfaceArrival(DeviceContext& context);
    void OnDeviceInterfaceRemoval(DeviceContext& context);

    static DWORD CALLBACK deviceChangeCallback(
        HCMNOTIFICATION hNotify,
        PVOID context,
        CM_NOTIFY_ACTION action,
        PCM_NOTIFY_EVENT_DATA eventData,
        DWORD eventDataSize
    );

    void handleDeviceChange(int action, DeviceContext ctx);

    bool setDeviceInterfaceId(QString& deviceInterface,
                              const GUID& guid,
                              const QString& hwId,
                              ULONG flags);

    QMutex mutex;

    QString extractHardwareId(PCM_NOTIFY_EVENT_DATA eventData);

    std::vector<DeviceContext*> deviceContexts;
};
#endif

class Device : public QObject {
    Q_OBJECT

  public:
    Device() = default;
    Device(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(const Device&) = delete;
    Device& operator=(Device&&) = delete;
    ~Device() override = default;
    [[nodiscard]] virtual bool
    WaitForAction(std::optional<QString> action = std::nullopt,
                  int timeout = K_WAIT_ACTION_TIMEOUT) = 0;
#ifdef _IS_WINDOWS_
    [[nodiscard]] virtual bool InitializePortMonitor(const GUID& guid,
                                                     const QString& hwId,
                                                     const QString& deviceName,
                                                     int handleIndex) = 0;
    [[nodiscard]] virtual bool GetPortMonitorStatus(int handleIndex) = 0;
    [[nodiscard]] virtual QString GetDevicePath(int handleIndex) = 0;
    [[nodiscard]] virtual bool IsSpecificPortOpen(QString& devicePath,
                                          const GUID& GUIDPath,
                                          QString& deviceKey) = 0;
    [[nodiscard]] virtual bool IsNetadapterNetDeviceExist() = 0;
#endif

    [[nodiscard]] virtual bool FindDevice() = 0;
    [[nodiscard]] virtual std::optional<QString> GetBDF() = 0;
    virtual void AddAttr(const QString& key, const QString& value) = 0;
    virtual void AddEnv(const QString& key, const QString& value) = 0;
    virtual void AddDeviceId(const QString& vid, const QString& pid) = 0;
};

class Modem : public Device {
  public:
    Modem() = default;
    Modem(const Modem&) = delete;
    Modem(Modem&&) = delete;
    Modem& operator=(const Modem&) = delete;
    Modem& operator=(Modem&&) = delete;
    ~Modem() override = default;

    [[nodiscard]] virtual std::optional<QString>
    SendATCommand(const QString& cmd, int timeout = K_MBIM_TIMEOUT) const = 0;
    virtual bool MBIMReboot() const =0;
};

// XXX mbim lib might change implement later
using Attributes = std::map<QString, QString>;
void OnMbimOperation(const std::function<void()>& callback);

class DeviceFactory {
  public:
    DeviceFactory() = default;
    DeviceFactory(const DeviceFactory&) = default;
    DeviceFactory(DeviceFactory&&) = delete;
    DeviceFactory& operator=(const DeviceFactory&) = default;
    DeviceFactory& operator=(DeviceFactory&&) = delete;
    virtual ~DeviceFactory() = default;

    [[nodiscard]] virtual std::unique_ptr<Device>
    CreateDevice(const QString& sub_system, const std::vector<DeviceId> &device_ids,
                 const std::map<QString, QString>& attrs = {}) const = 0;

    [[nodiscard]] virtual std::unique_ptr<Modem>
    CreateModem(std::unique_ptr<Device> device) const = 0;

    [[nodiscard]] virtual std::unique_ptr<Modem>
    CreateModem(const QString& sub_system, const std::vector<DeviceId> &device_ids,
                const Attributes& attrs = {}) const = 0;

    static std::unique_ptr<DeviceFactory> Create();
};

#ifdef _IS_WINDOWS_
struct AdbUtil {
    static bool ConnectADBDevice(Modem* modem, ADBShell* adbShell);

    static bool DisconnectADBDevice(Modem* modem, ADBShell* adbShell);

    static bool ToolConditionOpenADB(Modem* modem);

    static bool ToolConditionCloseADB(Modem* modem, ADBShell* adbShell);
};
#endif

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_FLASH_COMMON_MODEM_H_
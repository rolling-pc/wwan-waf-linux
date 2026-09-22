#ifndef AFAL_ADAPTER_OS_API_OSINFO_HPP_
#define AFAL_ADAPTER_OS_API_OSINFO_HPP_
#include <memory>
#include <string>
#include <QString>
#include <QObject>
#include <QJsonArray>
#include <QProcess>
#ifdef _IS_WINDOWS_
#include <windows.h>
#else
#include <stdint.h>
typedef unsigned int DWORD;   
#endif
#include "global.hpp"

namespace afal {

    namespace osinfo {

        struct BIOSInfo {
            QString vendor;
            QString version;
        };

        struct DriverInfo {
            QString name;
            QString version;
        };

        struct DeviceModeData {
            int deviceMode;
            int sarTableIndex;
            int antennaTableIndex;
        };

        enum OSType {
            WINDOWS,
            CROS,
            UBUNTU,
            FEDORA,
            THINPRO,
            UNKNOWN,
        };

        extern OSType osType;
        extern QString ChromeOSName;
        extern QString ChromeOSVersion;

        /**
         * @brief This function get the OS type, "Ubuntu/Fedora..."
         * @return Return the OS type enum.
         */
        #ifdef _IS_WINDOWS_
        OSType GetOSType(); 
        bool ConditionSetServiceFlashingStatus(bool SetValue, bool bNoCondition);
        bool SetQComPcieModes(bool SetValue);
        bool QueryQComPcieMode(DWORD* outValue);
        #elif defined(_IS_LINUX_) 
        OSType GetOSType();
        #endif
        /**
         * @brief This function get the OS name, "Ubuntu 22.04...".
         * @return Return the OS name.
         */
        QString GetOSName();
        /**
         * @brief This function get the Chrome OS Name & version...
         * @return Return the Chrome OS Name & version.
         */
        void GetChromeInfo();

        /**
         * @brief This function get the OS version, "22.04" (Ubuntu 22.04)...
         * @return Return the OS version.
         */
        QString GetOSVersion();
        /**
         * @brief This function get the kernel version, "10.0.26100.2894" (Ubuntu"5.15.0-75-generic")...
         * @return Return the kernel version.
         */
        QString GetKernelVersion();
        /**
         * @brief This function get the OS time zone, "UTF-8".
         * @return Return the OS time zone.
         */

        QString GetTimeZone();
        /**
         * @brief This function get the OS build lab info, "OS BuildLab: 22621.ni_release.220506-1250".
         * @return Return the OS build lab info.
         */
        QString GetBuildLab();
         /**
         * @brief This function get the CPU info, "CPU Model Name: xxx CPU
         * Cores: xxx Siblings (Logical CPUs): xxx".
         * @return Return the CPU info.
         */
        QString GetCPUInfo();
        /**
         * @brief This function get the manufacturer, "Lenovo...".
         * @return Return the manufacturer.
         */
        QString GetManufacturer(const QString& path = "");
        /**
         * @brief This function get the SKU, "1234567#ABA".
         * @return Return the SKU.
         */
        QString GetSKU(const QString& path = "");
        /**
         * @brief This function get the product id, "8C6D".
         * @return Return the product id.
         */
        QString GetProductID(const QString& path = "");
        /**
         * @brief This function get the product name, "HP EliteBook 835 13 inch G11 Notebook PC".
         * @return Return the product name.
         */
        QString GetProductName(const QString& path = "");
        /**
         * @brief This function get the WWAN config id, "xxxx.xxxx.xxxx.xxxx".
         * @return Return the WWAN config id.
         */
        QString GetWWANConfigID(const QString& path = "");
        /**
         * @brief This function get the FCC unlock key.
         * @return Return the FCC unlock key.
         */
        QString GetFCCUnlockKey();
        /**
         * @brief This function get the OOBE state.
         * @return Return the OOBE state.
         */
        QString GetOOBEState();
        /**
         * @brief This function get the OOBE end time.
         * @return Return the OOBE end time.
         */
        QString GetOOBEEndTime();
        /**
         * @brief This function get the audit mode.
         * @return Return the audit mode.
         */
        DWORD GetAuditMode();
        /**
         * @brief This function get the flash limit reboot count value.
         * @return Return the remained count. 0 or null means no limitation.
         */
        DWORD GetFlashLimitRebootCount();        
        /**
         * @brief This function set the flash limit reboot count value.
         * @param dRebootCount The remained reboot count to set.
         * @return Return TRUE if set successfully, otherwise FALSE.
         */
#ifdef _IS_WINDOWS_
        LSTATUS SetFlashLimitRebootCount(DWORD dRebootCount);
        /**
         * @brief This function get the fwupdate option. Such as Auto, Force, No, Factory_mode, Factory_Update.
         * @return Return the fwupdate option.
         */
        QString GetFwUpdateOption();
        /**
         * @brief This function set the fwupdate option. Such as Auto, Force, No, Factory_mode, Factory_Update.
         * @param strValue The fwupdate option value to set.
         * @return Return TRUE if set successfully, otherwise FALSE.
         */
        BOOL SetFwUpdateOption(QString strValue);
        /**
         * @brief This function set FlashStatus in registry.
         * @param dFlashStatus Flash status value (0,1,2,3,4,5,9).
         *        0=INIT, 1=Start flashing, 2=Retry, 3=Final failure,
         *        4=Flash success, 5=MBIM arrived, 9=No flash needed.
         * @return Return TRUE if set successfully, otherwise FALSE.
         */
        BOOL SetFlashStatus(DWORD dFlashStatus);
        /**
         * @brief This function get the BIOS info, "vendor=Lenovo,version=xx".
         * @return Return the BIOS info.
         */
        DWORD DetectOSBuildVersion();
        /**
         * @brief This function detect OS build version, return value is build number, such as 22000, 22621.
         * @return Return the OS build version.
         */
        BOOL AddVZWRegistryKey();
        BOOL AddTMORegistryKey();
        /**
         * @brief This function add operator registry key for current device, such as VZW/TMO.
         * @return Return TRUE if add successfully, otherwise FALSE.
         */
        BOOL EnablePrivilege(LPCWSTR privilegeName);
        /**
         * @brief This function enable the privilege for current process, such as SE_SHUTDOWN_NAME.
         * @return Return TRUE if enable successfully, otherwise FALSE.
         */
         BOOL TakeOwnershipAndGrant(HKEY hRoot, LPCWSTR wszSubKey);
         void LogCurrentUserName();
#endif
        BIOSInfo GetBIOSInfo();
        /**
         * @brief This function get the net adapter driver info, "name=xx,version=xx".
         * @return Return the adapter driver info.
         */
        DriverInfo GetNetAdapterDriverInfo();
        /**
         * @brief This function get the ACPI driver info, "name=xx,version=xx".
         * @return Return the ACPI driver info.
         */
        DriverInfo GetACPIDriverInfo();
        /**
         * @brief This function get the GPS driver info, "name=xx,version=xx".
         * @return Return the GPS driver info.
         */
        DriverInfo GetGPSDriverInfo();
        /**
         * @brief This function get the flash driver info, "name=xx,version=xx".
         * @return Return the flash driver info.
         */
        DriverInfo GetFwFlashDriverInfo();
        /**
         * @brief This function get the Fw update driver info, eg: "name=FwUpdateDriver,version=1010.5902.273.1".
         * @return Return the Fw update driver info.
         */
        DriverInfo GetFwUpdateDriverInfo();
        /**
         * @brief This function get the filter driver info,
         * "name=Lenovo,version=xx".
         * @return Return the firmware driver info.
         */
        DriverInfo GetFilterDriverInfo();
        #ifdef _IS_WINDOWS_
        DriverInfo  GetDeviceInfo(QStringList &deviceInstanceInfo) ;
        QString FindVersionFromULONGLONG(ULONGLONG data);
        #endif

        enum FirmwareEvent {
            CREATE = 1,
            MODIFY,
            DELETE_,
        };

        /**
         * @class FirmwareMonitor
         * @brief This Class used to monitor firmware package create or delete, "Firmware.zip".
         *
         * FirmwareMonitor provides functions to start monitor, stop monitor, and signals of firmware package event.
         */
        class FirmwareMonitor : public QObject {
                Q_OBJECT
            public:
                FirmwareMonitor(QObject* parent = nullptr);
                ~FirmwareMonitor();
                FirmwareMonitor(const FirmwareMonitor&) = delete;
                FirmwareMonitor& operator = (const FirmwareMonitor&) = delete;

                /**
                 * @brief This function start monitor firmware package create/modify/delete event.
                 * @param filePathName The firmware package directory required to monitor.
                 */
                void StartMonitoring(const QString& filePathName);
                /**
                 * @brief This function stop monitor firmware package create/modify/delete event.
                 */
                void StopMonitoring();

            signals:
                /**
                 * @brief This function defined as a signal, when firmware package event occurs,
                 * this signal will emit.
                 * @param firmwareEvent The FirmwareEvent enum(CREATE/MODIFY/DELETE).
                 */
                void FirmwareEventOccurred(const int& firmwareEvent);
            private:
                class FirmwareMonitorImpl;
                std::unique_ptr<FirmwareMonitorImpl> pImpl;
        };

        enum DeviceModeEvent {
            TABLET = 1,
            LAPTOP,
            CLAMSHELL
        };

        /**
         * @class DeviceModeMonitor
         * @brief This Class used to monitor device mode, "tablet/laptop mode".
         *
         * DeviceModeMonitor provides functions to start monitor, stop monitor, and signals of device mode event.
         */
        class DeviceModeMonitor : public QObject {
                Q_OBJECT
            public:
                DeviceModeMonitor(QObject* parent = nullptr);
                ~DeviceModeMonitor();
                DeviceModeMonitor(const DeviceModeMonitor&) = delete;
                DeviceModeMonitor& operator = (const DeviceModeMonitor&) = delete;

                /**
                 * @brief This function start monitor device mode event.
                 */
                void StartMonitoring();
                 /**
                 * @brief This function stop monitor device mode event.
                 */
                void StopMonitoring();

            signals:
                /**
                 * @brief This function defined as a signal, when device mode event occurs,
                 * this signal will emit.
                 * @param DeviceModeData The DeviceModeEvent enum(TABLET/LAPTOP).
                 */
                //void DeviceModeEventOccurred(const int& deviceModeEvent);
                void DeviceModeEventOccurred(const DeviceModeData &data);
            private:
                class DeviceModeMonitorImpl;
                std::unique_ptr<DeviceModeMonitorImpl> pImpl;
        };

        enum OSLogType {
            NetworkIPPackage,
            WIN_NetAdapterDriverTrace,
            WIN_BootStageTrace,
            WIN_WindowsEventViewer,
            WIN_MBBLogGatherScript,
            WIN_WWANWindowsTrace,
            WIN_ACPIDriverTrace,
            WIN_FILTERDriverTrace,
            WIN_DriverFrameworkTrace,
            WIN_GEOlocationTrace,
            WIN_VerboseWWANWindowsTrace,
            WIN_NetWwanWindowsTrace,
            WIN_ComDriver,
            WIN_ResetDriver,
            WIN_GNSSDriver,
            WIN_Netsh,
            WIN_WDF,
            WIN_WMI,
            WIN_WER,
            WIN_FwUpdate,
            WIN_SetupAPI,
            WIN_MBIM,
            WIN_SystemDump,
            WIN_Power,
            WIN_QNET,
            WIN_QNETEMI,
            WIN_DATAIO,
            WIN_MHI,
            WIN_QPCIE,
            WIN_WNS,
            WIN_THERMALMDM,
            WIN_COEXMGRWPP,
            LINUX_Syslog,
            LINUX_Kernel,
            CROS_Net,
            CROS_Kernel
        };

        /**
         * @struct OSLogConfig
         * @brief This struct used to set OS log configuration, "Destination log path,
         * compress flag, usb device flag, single file limit MB".
         *
         * OSLogConfig provides parameters used to collect OS logs.
         */
        struct OSLogConfig {
            QString destLogPath;  // the destination log path which will collect log to
            QString netshCommand; // user input netsh commend 
            bool isCompressLogs;  // the compress flag
            bool isUsbDevice;  // the USB device flag
            int singleFileLimitMB;  // the size limit of single file
            int maxTraceFileNum; // the max amount of  trace files 
            OSLogConfig() : destLogPath(""), isCompressLogs(false), isUsbDevice(false), singleFileLimitMB(100), maxTraceFileNum(50) {}
        };

        /**
         * @class OSLogController
         * @brief This Class used to collect OS logs, "Linux syslog, Windows event log, driver logs...".
         *
         * OSLogController provides functions to enable log, start/stop collect.
         */

        enum LOG_CONTROL_TYPE {
            NORMAL_START,
            NORMAL_STOP,
            BOOT_START,
            BOOT_STOP
        };

        class OSLogController {
            public:
                OSLogController();
                ~OSLogController();
                /**
                 * @brief This function enable logging switch,
                 * @param logType The OSLogType enum.
                 * @param enabled The switch flag, ture=enable, false=disable.
                 */
                void SetLogTypeEnabled(OSLogType logType, bool enabled);
                /**
                 * @brief This function set the OS log configuration.
                 * @param config The OSLogConfig struct.
                 */
                void SetLogConfig(const OSLogConfig& config);
                /**
                 * @brief This function start collect OS log.
                 */
                void StartLogCapture();
                /**
                 * @brief This function stop collect OS log.
                 */
                void StopLogCapture();
                void StartTcpdump();
                void StopTcpdump();
            private:
                class OSLogControllerImpl;
                std::unique_ptr<OSLogControllerImpl> pImpl;
        };
    }
}

#endif //AFAL_ADAPTER_OS_API_OSINFO_HPP_
#include "osinfo.hpp"
#include "common.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <sys/inotify.h>
#include <cstring>
#include <memory>
#include <fstream>
#include <unordered_map>
#include <array>
#include <cstdio>
#include <sstream>
#include "log.hpp"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QMetaObject>
#include <QDateTime>
#include <QProcess>
#include <QThread>

using namespace afal::log;
bool isUSBDevice = false;
bool usbmonLoad = false;
namespace afal {
    namespace osinfo {
        constexpr int CMD_BUFFER = 128;  // Command buffer

        // WWAN config ID Index
        constexpr int WWAN_SEGMENT1_INDEX1 = 20;
        constexpr int WWAN_SEGMENT1_INDEX2 = 21;
        constexpr int WWAN_SEGMENT1_INDEX3 = 17;
        constexpr int WWAN_SEGMENT1_INDEX4 = 18;

        constexpr int WWAN_SEGMENT2_INDEX1 = 26;
        constexpr int WWAN_SEGMENT2_INDEX2 = 27;
        constexpr int WWAN_SEGMENT2_INDEX3 = 23;
        constexpr int WWAN_SEGMENT2_INDEX4 = 24;

        constexpr int WWAN_SEGMENT3_INDEX1 = 32;
        constexpr int WWAN_SEGMENT3_INDEX2 = 33;
        constexpr int WWAN_SEGMENT3_INDEX3 = 29;
        constexpr int WWAN_SEGMENT3_INDEX4 = 30;

        constexpr int WWAN_SEGMENT4_INDEX1 = 38;
        constexpr int WWAN_SEGMENT4_INDEX2 = 39;
        constexpr int WWAN_SEGMENT4_INDEX3 = 35;
        constexpr int WWAN_SEGMENT4_INDEX4 = 36;

        const std::unordered_map<std::string, std::string> DEVICE_MODE_MONITORS = {
            {"INTC1092", "/sys/devices/platform/INTC1092:00/intc_data"}
        };
        std::mutex tcpdumpMutex;
        QString GetCPUInfo() {
            // Linux read /proc/cpuinfo
            QFile file("/proc/cpuinfo");
            if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return "cpuinfo not available";
            }

            QTextStream in(&file);
            QString modelName;
            QString cpuCores;
            QString siblings;

            //while (in.status() == QTextStream::Ok) {
            while (!in.atEnd()) {
                QString line = in.readLine();

                // get model name
                if (line.startsWith("model name")) {
                    modelName = line.split(":").last().trimmed();
                }

                // get cpu cores
                if (line.startsWith("cpu cores")) {
                    cpuCores = line.split(":").last().trimmed();
                }

                // get siblings
                if (line.startsWith("siblings")) {
                    siblings = line.split(":").last().trimmed();
                }

                if (!modelName.isEmpty() && !cpuCores.isEmpty() && !siblings.isEmpty()) {
                    break;
                }
            }

            file.close();

            QString cpuInfo = QString("CPU Model Name: %1\nCPU Cores: %2\nSiblings (Logical CPUs): %3")
                              .arg(modelName)
                              .arg(cpuCores)
                              .arg(siblings);

            return cpuInfo;
        }
        
        void GetChromeInfo() {
            QFile file("/etc/lsb-release");
            if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                return;
            }

            QTextStream in(&file);
            QString releaseName, releaseVersion;

            while (!in.atEnd())
            {
                QString line = in.readLine();
                if (line.startsWith("CHROMEOS_RELEASE_NAME="))
                {
                    ChromeOSName = line.section('=', 1).trimmed();
                }
                else if (line.startsWith("CHROMEOS_RELEASE_DESCRIPTION="))
                {
                    ChromeOSVersion = line.section('=', 1).trimmed();
                }
            }
        }

        QString readFile(const QString &filePath) {
            QFile file(filePath);
            if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return "";
            }

            QTextStream in(&file);
            QString content = in.readLine();
            file.close();
            return content;
        }

        BIOSInfo GetBIOSInfo() {
            // Linux read /sys/class/dmi/id/bios_vendor
            // Linux read /sys/class/dmi/id/bios_version
            BIOSInfo biosInfo;

            biosInfo.vendor = readFile("/sys/class/dmi/id/bios_vendor");
            biosInfo.version = readFile("/sys/class/dmi/id/bios_version");
           
            if (biosInfo.version.isEmpty()) {
                QProcess process;
                process.start("crossystem fwid");
                process.waitForFinished(3000); // 等待最多3秒
        
                QString output = process.readAllStandardOutput().trimmed();
                if (!output.isEmpty()) {
                    biosInfo.version = output;
                }
            }

            if (biosInfo.vendor.isEmpty()) {
                biosInfo.vendor = "coreboot";
            }

            return biosInfo;
        }

        // DELL use SKU
        QString GetSKU(const QString& path) {
            // Linux read /sys/class/dmi/id/product_sku to get SKU
            QString file = path.isEmpty() ? "/sys/class/dmi/id/product_sku" : path;
            QString sku = readFile(file);
            return sku;
        }

        // HP use ProductID(BaseBoardProduct) and ProductName
        QString GetProductID(const QString& path) {
            // Linux read /sys/class/dmi/id/board_name
            QString file = path.isEmpty() ? "/sys/class/dmi/id/board_name" : path;
            QString productID = readFile(file);
            return productID;
        }

        QString GetProductName(const QString& path) {
            // Linux read /sys/class/dmi/id/product_name
            QString file = path.isEmpty() ? "/sys/class/dmi/id/product_name" : path;
            QString productName = readFile(file);
            return productName;
        }

        std::string runPopenCommand(const std::string& cmd) {
            std::array<char, CMD_BUFFER> buffer = {0};
            std::string result;

            std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
            if (!pipe) {
                std::cout << "run command failed" << std::endl;
                return "";
            }

            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }

            return result;
        }

        // LENOVO use WWANConfigID
        QString GetWWANConfigID(const QString& path) {
            if (!path.isEmpty()) {
                std::cout << path.toStdString() << std::endl;
            }

            const std::string command = "dmidecode -t 133 | tail -n 3";
            std::string output = runPopenCommand(command);

            if (output.length() < WWAN_SEGMENT4_INDEX2) {
                return "";
            }

            std::stringstream sstream;
            sstream << output.at(WWAN_SEGMENT1_INDEX1) << output.at(WWAN_SEGMENT1_INDEX2) << output.at(WWAN_SEGMENT1_INDEX3) << output.at(WWAN_SEGMENT1_INDEX4) << '.'
                    << output.at(WWAN_SEGMENT2_INDEX1) << output.at(WWAN_SEGMENT2_INDEX2) << output.at(WWAN_SEGMENT2_INDEX3) << output.at(WWAN_SEGMENT2_INDEX4) << '.'
                    << output.at(WWAN_SEGMENT3_INDEX1) << output.at(WWAN_SEGMENT3_INDEX2) << output.at(WWAN_SEGMENT3_INDEX3) << output.at(WWAN_SEGMENT3_INDEX4) << '.'
                    << output.at(WWAN_SEGMENT4_INDEX1) << output.at(WWAN_SEGMENT4_INDEX2) << output.at(WWAN_SEGMENT4_INDEX3) << output.at(WWAN_SEGMENT4_INDEX4);

            QString wwanConfigID = QString::fromStdString(sstream.str());
            return wwanConfigID;
        }

        QString GetFCCUnlockKey() {
            return "";
        }

        QString GetManufacturer(const QString& path) {
            // Linux read /sys/class/dmi/id/board_vendor
            QString file = path.isEmpty() ? "/sys/class/dmi/id/board_vendor" : path;
            QString manufacturer = readFile(file);
            return manufacturer;
        }

        QString GetBuildLab() {
            if (GetOSType() != OSType::CROS)
            {
                return QSysInfo::prettyProductName();
            }
            else
            {
                GetChromeInfo();
                return ChromeOSName;
            }
        }

        QString GetOOBEState() {
            // TODO: Linux don't support
            return "";
        }

        QString GetOOBEEndTime() {
            // TODO: Linux don't support
            return "";
        }

        DWORD GetAuditMode() {
            // TODO: Linux don't support
            return 0;
        }

        DWORD GetFlashLimitRebootCount() {
            // TODO: Linux don't support
            return 0;
        }    

        QString GetFwUpdateOption() {
            // TODO: Linux don't support
            return "";
        }

        DriverInfo GetNetAdapterDriverInfo() {
            // TODO: Linux don't support
            DriverInfo driverInfo;
            return driverInfo;
        }

        DriverInfo GetACPIDriverInfo() {
            // TODO: Linux don't support
            DriverInfo driverInfo;
            return driverInfo;
        }

        DriverInfo GetGPSDriverInfo() {
            // TODO: Linux don't support
            DriverInfo driverInfo;
            return driverInfo;
        }

        DriverInfo GetFwFlashDriverInfo() {
            // TODO: Linux don't support
            DriverInfo driverInfo;
            return driverInfo;
        }

        DriverInfo GetFwUpdateDriverInfo() {
            // TODO: Linux don't support
            DriverInfo driverInfo;
            return driverInfo;
        }

        class FirmwareMonitor::FirmwareMonitorImpl {
            public:
                void SetMonitor(FirmwareMonitor* pFirmwareMonitor) {
                    pMonitor = pFirmwareMonitor;
                }

                void StartMonitoring(const QString& filePathName) {
                    inotifyFd = inotify_init();
                    if (inotifyFd < 0) {
                        return;
                    }

                    watchFd = inotify_add_watch(inotifyFd, filePathName.toStdString().c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
                    if (watchFd < 0) {
                        close(inotifyFd);
                        return;
                    }

                    monitoring = true;
                    monitoringThread = std::make_unique<std::thread>(&FirmwareMonitorImpl::MonitorEvents, this);
                }

                void StopMonitoring() {
                    monitoring = false;
                    if (monitoringThread && monitoringThread->joinable()) {
                        monitoringThread->join();
                        monitoringThread.reset();
                    }
                    if (watchFd >= 0) {
                        inotify_rm_watch(inotifyFd, watchFd);
                    }
                    if (inotifyFd >= 0) {
                        close(inotifyFd);
                    }
                }

            private:
                void MonitorEvents() {
                    const size_t eventSize = sizeof(struct inotify_event);
                    const size_t bufferSize = 10 * (eventSize + NAME_MAX + 1);
                    std::array<char, bufferSize> buffer{0};
                    while (monitoring) {
                        auto length = read(inotifyFd, buffer.data(), bufferSize);
                        if (length < 0) {
                            continue;
                        }

                        size_t index = 0;
                        while(index < length) {
                            auto* event = reinterpret_cast<inotify_event*>(&buffer.at(index));
                            if (event->mask & IN_CREATE) {
                                QMetaObject::invokeMethod(pMonitor, "FirmwareEventOccurred", Qt::QueuedConnection,
                                                          Q_ARG(const int&, FirmwareEvent::CREATE));
                            }

                            if (event->mask & IN_MODIFY) {
                                QMetaObject::invokeMethod(pMonitor, "FirmwareEventOccurred", Qt::QueuedConnection,
                                    Q_ARG(const int&,
                                          FirmwareEvent::MODIFY));
                            }

                            if (event->mask & IN_DELETE) {
                                QMetaObject::invokeMethod(pMonitor, "FirmwareEventOccurred", Qt::QueuedConnection,
                                                          Q_ARG(const int&, FirmwareEvent::DELETE_));
                            }
                            index = index + eventSize + event->len;
                        }
                    }
                }

                int inotifyFd = -1;
                int watchFd = -1;
                std::unique_ptr<std::thread> monitoringThread;
                FirmwareMonitor* pMonitor = nullptr;
                std::atomic<bool> monitoring = false;
        };

        FirmwareMonitor::FirmwareMonitor(QObject* parent)
            : QObject(parent), pImpl(std::make_unique<FirmwareMonitorImpl>()) {
            pImpl->SetMonitor(this);
        }

        FirmwareMonitor::~FirmwareMonitor() {StopMonitoring();}

        void FirmwareMonitor::StartMonitoring(const QString& filePathName) {
            pImpl->StartMonitoring(filePathName);
        }

        void FirmwareMonitor::StopMonitoring() {
            pImpl->StopMonitoring();
        }

        class DeviceModeMonitor::DeviceModeMonitorImpl {
            public:
                void SetMonitor(DeviceModeMonitor* pDeviceModeMonitor) {
                    pMonitor = pDeviceModeMonitor;
                }

                void StartMonitoring() {
                    bool ret = GetDeviceModeMonitor();
                    if (!ret) {
                        return;
                    }

                    inotifyFd = inotify_init();
                    if (inotifyFd < 0) {
                        return;
                    }

                    watchFd = inotify_add_watch(inotifyFd, deviceModeMonitor.c_str(), IN_MODIFY);
                    if (watchFd < 0) {
                        close(inotifyFd);
                        return;
                    }

                    monitoring = true;
                    monitoringThread = std::make_unique<std::thread>(&DeviceModeMonitorImpl::MonitorEvents, this);
                }

                void StopMonitoring() {
                    monitoring = false;
                    if (monitoringThread && monitoringThread->joinable()) {
                        monitoringThread->join();
                        monitoringThread.reset();
                    }
                    if (watchFd >= 0) {
                        inotify_rm_watch(inotifyFd, watchFd);
                    }
                    if (inotifyFd >= 0) {
                        close(inotifyFd);
                    }
                }

            private:
                bool GetDeviceModeMonitor() {
                    for (const auto& monitor : DEVICE_MODE_MONITORS) {
                        std::ifstream file(monitor.second);

                        if (file) {
                            deviceModeMonitor = monitor.first;
                            break;
                        }
                    }

                    return !deviceModeMonitor.empty();
                }

                int GetDeviceMode_INTC1092(const std::string& filePath) {  // NOLINT
                    std::ifstream file(filePath);
                    if (!file.is_open()) {
                        std::cerr << "Failed to open the file: " << filePath << std::endl;
                        return afal::error::ERR;
                    }

                    std::string line;
                    if (std::getline(file, line)) {
                        std::istringstream iss(line);
                        int deviceMode = 0;

                        iss >> deviceMode;

                        if (iss.fail()) {
                            std::cerr << "Failed to parse device mode as an integer." << std::endl;
                            return afal::error::ERR;
                        }

                        return deviceMode;
                    }

                    file.close();
                    return afal::error::ERR;
                }

                void MonitorEvents() {
                    const size_t eventSize = sizeof(struct inotify_event);
                    const size_t bufferSize = 10 * (eventSize + NAME_MAX + 1);
                    std::array<char, bufferSize> buffer{0};

                    while (monitoring) {
                        auto length = read(inotifyFd, buffer.data(), bufferSize);
                        if (length < 0) {
                            continue;
                        }

                        size_t index = 0;
                        while(index < length) {
                            auto* event = reinterpret_cast<inotify_event*>(&buffer.at(index));
                            if (event->mask & IN_MODIFY) {
                                int deviceMode = 0;
                                if (deviceModeMonitor == "INTC1092") {
                                    deviceMode = GetDeviceMode_INTC1092(DEVICE_MODE_MONITORS.at(deviceModeMonitor));
                                }
                                
                                //QMetaObject::invokeMethod(pMonitor, "DeviceModeEventOccurred", Qt::QueuedConnection,
                                //                          Q_ARG(const int&, deviceMode));
                                DeviceModeData data {
                                    deviceMode,
                                    0,
                                    0
                                };                                
                                QMetaObject::invokeMethod(pMonitor, "DeviceModeEventOccurred",
                                    Qt::QueuedConnection,
                                    Q_ARG(DeviceModeData, data));
                            }
                            index = index + eventSize + event->len;
                        }
                    }
                }

                int inotifyFd = -1;
                int watchFd = -1;
                std::string deviceModeMonitor;
                std::unique_ptr<std::thread> monitoringThread;
                DeviceModeMonitor* pMonitor = nullptr;
                std::atomic<bool> monitoring = false;
        };

        DeviceModeMonitor::DeviceModeMonitor(QObject* parent)
            : QObject(parent), pImpl(nullptr) {
        }

        DeviceModeMonitor::~DeviceModeMonitor() {StopMonitoring();}

        void DeviceModeMonitor::StartMonitoring() {
            if (!pImpl) {
                pImpl.reset();
            }

            pImpl = std::make_unique<DeviceModeMonitorImpl>();
            pImpl->SetMonitor(this);

            pImpl->StartMonitoring();
        }

        void DeviceModeMonitor::StopMonitoring() {
            pImpl->StopMonitoring();
        }

        class OSLogController::OSLogControllerImpl {
            public:
                void SetLogTypeEnabled(OSLogType logType, bool enabled) {
                    logStatus[logType] = enabled;
                }

                void SetLogConfig(const OSLogConfig& logConfig) {
                    config = logConfig;
                }                
                void StartTcpdump()
                {
                    LOG_WARN("start to capture tcpdump");
                    if (!isUSBDevice && !config.isUsbDevice) {
                        return;
                    }
                    isUSBDevice = true;
                    std::lock_guard<std::mutex> lock(tcpdumpMutex);

                    std::cout << "start to capture tcpdump" << std::endl;
                    {
                        QProcess cleanup;
                        QString cleanupCmd = "pkill -2 tcpdump && sleep 0.5 && pkill -9 tcpdump";
                        cleanup.start("bash", QStringList() << "-c" << cleanupCmd);
                        cleanup.waitForFinished(3000); // 等待最多 3 秒
                    }

                    const QString netIpPackageDir =
                        config.destLogPath + "/" + logData + "/NetIPPackage";
                    QDir().mkpath(netIpPackageDir);
                    QString ipPackage = netIpPackageDir + "/usb_traffic.pcap";
                    QString tcpdumpCmd = QString(
                        "tcpdump -i usbmon0 -Z root -w \"%1\" -C %2 > /dev/null 2>&1 &"
                    ).arg(ipPackage).arg(config.singleFileLimitMB);

                    QProcess tcpdumpProcess;
                    tcpdumpProcess.start("bash", QStringList() << "-c" << tcpdumpCmd);
                    if (!tcpdumpProcess.waitForStarted(2000)) {
                        return;
                    }
                    QThread::msleep(200);
                }
                // Process logs that need to be captured in real time.
                void StartLogCapture() {
                    QDateTime current = QDateTime::currentDateTime();
                    logData = current.toString("yyyyMMddHHmm");

                    if (config.destLogPath.isEmpty()) {
                        config.destLogPath = "/var/log/waf/";
                    }

                    QString logDir =config.destLogPath + logData; // Default LogPath: /var/log/waf/yyyyMMddHHmm/
                    QDir dir(logDir);
                    if (!dir.exists()) {
                        if (!dir.mkpath(".")) {
                            std::cout << "Failed to create directory:" << logDir.toStdString() << std::endl;
                        } else {
                            QFile logDirFile(logDir);
                            logDirFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                                      QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
                                                      QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther);
                        }
                    }

                    for (const auto& [logType, enabled] : logStatus) {
                        if (enabled) {
                            switch (logType) {
                                case OSLogType::NetworkIPPackage:
                                    if (!usbmonLoad)
                                    {
                                        std::system("modprobe usbmon");  // NOLINT
                                        usbmonLoad = true;
                                    }
                                    StartTcpdump();
                                    //CaptureNetworkIPPackageLogs();
                                    break;
                                default:
                                    break;
                            }
                            std::cout << "Log type[" << logType <<"] start capture." << std::endl;
                        }
                    }
                }
                void StopTcpdump() {
                    std::lock_guard<std::mutex> lock(tcpdumpMutex);
                
                    QString fullCommand =
                        "pkill -2 tcpdump > /dev/null 2>&1; "
                        "sleep 0.5; "
                        "pkill -9 tcpdump > /dev/null 2>&1; ";
                
                    LOG_WARN("StopTcpdump");
                
                    QProcess process;
                    process.start("bash", QStringList() << "-c" << fullCommand);
                
                    if (!process.waitForFinished(5000)) {
                        process.kill();
                        process.waitForFinished();
                    }
                
                    LOG_DEBUG("StopTcpdump finished, exitCode={}", process.exitCode());
                }

                void StopLogCapture() {
                    for (auto& [logType, enabled] : logStatus) {
                        if (enabled) {
                            switch (logType) {
                                case OSLogType::NetworkIPPackage:
                                    /*
                                    if (tcpdumpProcess && tcpdumpProcess->state() != QProcess::NotRunning) {
                                        tcpdumpProcess->terminate();
                                        tcpdumpProcess->waitForFinished();
                                        tcpdumpProcess.reset();
                                    }
                                    */
                                   StopTcpdump();
                                    break;
                                case OSLogType::LINUX_Syslog:
                                    CopyOSLogToLogPath("/var/log/syslog*");
                                    break;
                                case OSLogType::LINUX_Kernel:
                                    CopyOSLogToLogPath("/var/log/kern.log*");
                                    break;
                                case OSLogType::CROS_Net:
                                    CopyOSLogToLogPath("/var/log/net.log*");
                                    CopyOSLogToLogPath("/var/log/messages*");
                                    break;
                                case OSLogType::CROS_Kernel:
                                    CopyOSLogToLogPath("/var/log/");
                                    CopyOSLogToLogPath("/etc/lsb-release");  
                                    //CopyOSLogToLogPath("var/spool/crash/"); 
                                    extractVariant();                                
                                    break;
                                default:
                                    break;
                            }
                            enabled = false;
                            std::cout << "Log type[" << logType <<"] stop capture." << std::endl;
                        }
                    }
                    /**/
                    if (!config.destLogPath.isEmpty() && config.isCompressLogs) {
                        QProcess tarProcess;
                        QStringList args;
                        args << "-cf" << (config.destLogPath + "/" + logData + ".tar.gz") << "-C" << (config.destLogPath + "/" + logData)<< ".";
                        tarProcess.start("tar", args);
                        if (!tarProcess.waitForFinished()) {
                            std::cout << "Process failed to finish in time." << std::endl;
                            //LOG_ERROR("Process failed to finish in time.");
                            return;
                        }

                        int ret = tarProcess.exitCode();
                        if (ret != 0) {
                            std::cout << "Compression of log file failed with exit code:" << ret << std::endl;
                        } else {
                            std::cout << "Log file compressed successfully." << std::endl;
                            QString logDir = config.destLogPath + QDir::separator() + logData;
                            QDir unCompressLogDir(logDir);
                            unCompressLogDir.removeRecursively();
                        }

                        QProcess xzProcess;
                        args.clear();
                        args << "-0" << "-T" << "0" << (config.destLogPath + "/" + logData + ".tar.gz");
                        xzProcess.start("xz", args);
                        if (!xzProcess.waitForFinished()) {
                            std::cout << "Process failed to finish in time." << std::endl;
                            //LOG_ERROR("xz Process failed to finish in time.");
                            return;
                        }

                        ret = xzProcess.exitCode();
                        if (ret != 0) {
                            std::cout << "Compression of log file failed with exit code:" << ret << std::endl;
                        } 
                    }
                }

            private:
                int CaptureNetworkIPPackageLogs() {
                    int ret = 0;
                    if (config.isUsbDevice) {                    
                        if (tcpdumpProcess) {
                            return ret;
                        }

                        std::cout << "start to capture tcpdump" << std::endl;
                        tcpdumpProcess = std::make_unique<QProcess>();
                        const QString netIpPackageDir =
                            config.destLogPath + "/" + logData + "/NetIPPackage";
                        QDir().mkpath(netIpPackageDir);
                        QString ipPackage = netIpPackageDir + "/usb_traffic.pcap";
                        QStringList args;
                        args << "-i" << "usbmon0" << "-w" << ipPackage << "-C" << QString::number(config.singleFileLimitMB);

                        tcpdumpProcess->start("tcpdump", args);
                    }

                    return ret;
                }

                int CopyDirRecursive(const QDir& sourceDir, const QDir& targetDir) {
                    int ret = 0;
                
                    if (!targetDir.exists()) {
                        if (!targetDir.mkpath(".")) {
                            std::cout << "Failed to create target directory: " << targetDir.absolutePath().toStdString() << std::endl;
                            return afal::error::ERR;
                        }
                    }
                
                    QFileInfoList entries = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
                    for (const QFileInfo& entry : entries) {
                        QString sourcePath = entry.absoluteFilePath();
                        QString targetPath = targetDir.absoluteFilePath(entry.fileName());
                
                        if (entry.isDir()) {
                            QDir subSourceDir(sourcePath);
                            QDir subTargetDir(targetPath);
                            if (CopyDirRecursive(subSourceDir, subTargetDir) != 0) {
                                ret = afal::error::ERR;
                            }
                        } else {
                            if (!QFile::copy(sourcePath, targetPath)) {
                                std::cout << "Failed to copy file: " << sourcePath.toStdString() << std::endl;
                                ret = afal::error::ERR;
                            } else {
                                std::cout << "Copied file: " << sourcePath.toStdString() << " to " << targetPath.toStdString() << std::endl;
                            }
                        }
                    }                
                    return ret;
                }
                
                int CopyOSLogToLogPath(const QString& srcPath) {
                    int ret = 0;
                
                    QFileInfo srcInfo(srcPath);
                    if (!srcInfo.exists()) {
                        std::cout << "Source does not exist: " << srcPath.toStdString() << std::endl;
                        return afal::error::ERR;
                    }
                
                    QString logDir = config.destLogPath + "/" + logData;
                    QDir targetDir(logDir);
                
                    if (srcInfo.isFile()) {
                        if (!targetDir.exists()) {
                            targetDir.mkpath(".");
                        }
                
                        QString destFilePath = targetDir.absoluteFilePath(srcInfo.fileName());
                        if (!QFile::copy(srcInfo.absoluteFilePath(), destFilePath)) {
                            std::cout << "Failed to copy file: " << srcInfo.absoluteFilePath().toStdString() << std::endl;
                            ret = afal::error::ERR;
                        } else {
                            std::cout << "Copied file: " << srcInfo.absoluteFilePath().toStdString()
                                      << " to " << destFilePath.toStdString() << std::endl;
                        }
                    } else if (srcInfo.isDir()) {
                        QDir srcDir(srcInfo.absoluteFilePath());
                        QDir dstDir(logDir + "/" + srcDir.dirName());  
                        ret = CopyDirRecursive(srcDir, dstDir);
                    }                
                    return ret;
                }

                void extractVariant() {
				
				    QString logDir = config.destLogPath + "/" + logData;
                    QDir targetDir(logDir);
					if (!targetDir.exists()) {
						targetDir.mkpath(".");
					}
                
                    QString destFilePath = targetDir.absoluteFilePath("firmwarevariant");
                    QProcess process;
                    QString command = QString("%1 %2 %3 %4 %5").arg("cros_config","/modem","firmware-variant",">",destFilePath);
                    QStringList arguments;
                    arguments << "-c"
                            << command;
                
                    process.start("bash", arguments);
                
                    if (!process.waitForFinished(5000)) {
                        std::cout <<"mmcli reset command timed out!"<< std::endl;
                        return;
                    }
                
                    int exitCode = process.exitCode();
                    if (exitCode != 0) {
                        std::cout <<"Failed to execute:{}"<< process.readAllStandardError().toStdString()<< std::endl;
                        return;
                    }
                
                    std::cout <<"Successfully copy firmware-variant!"<< std::endl; 
					arguments.clear();
					destFilePath = targetDir.absoluteFilePath("biosvariant");
					command = QString("%1 %2 %3 %4 %5").arg("dmidecode","-t","bios",">",destFilePath);
                    arguments << "-c"
                            << command;
                
                    process.start("bash", arguments);
                
                    if (!process.waitForFinished(5000)) {
                        std::cout <<"mmcli reset command timed out!"<< std::endl;
                        return;
                    }
                
                    exitCode = process.exitCode();
                    if (exitCode != 0) {
                        std::cout <<"Failed to execute:{}"<< process.readAllStandardError().toStdString()<< std::endl;
                        return;
                    }
                
                    std::cout <<"Successfully copy bios-variant!"<< std::endl; 
                }

                OSLogConfig config;
                QString logData;
                std::unordered_map<OSLogType, bool> logStatus;
                std::unique_ptr<QProcess> tcpdumpProcess;
        };

        OSLogController::OSLogController()
            : pImpl(std::make_unique<OSLogControllerImpl>()) {
        }

        OSLogController::~OSLogController() = default;

        void OSLogController::SetLogTypeEnabled(OSLogType logType, bool enabled) {
            pImpl->SetLogTypeEnabled(logType, enabled);
        }

        void OSLogController::SetLogConfig(const OSLogConfig& config) {
            pImpl->SetLogConfig(config);
        }

        void OSLogController::StartLogCapture() {
            pImpl->StartLogCapture();
        }

        void OSLogController::StopLogCapture() {
            pImpl->StopLogCapture();
        }

        void OSLogController::StartTcpdump() {
            pImpl->StartTcpdump();
        }
        void OSLogController::StopTcpdump() {
            pImpl->StopTcpdump();
        }
    }
}
#include <QDebug>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <sys/epoll.h>
#include <unistd.h>
#include "DeviceImpl.h"

#define DEVICE_ID_LENTH (9)
#define PCIE_DRIVER_FILE ("/sys/bus/pci/devices/%1/t7xx_mode")
#define OUT ("out")
#define IN ("out")

DeviceImpl* DeviceImpl::getInstance() {
    static DeviceImpl* instance = nullptr;
    if (instance == nullptr)
        instance = new(DeviceImpl); // 使用静态变量保证单例

    return instance;
}

bool DeviceImpl::start(QMap<QString, QMap<QString, PortType>> id, QMap<QString, QString> product) {
    idlist = id;
    productList = product;

    if (pipe(pipFd) == -1) {
        FMTLOG_ERROR("Failed to create pipe");
        return false;
    }

    udev = udev_new();
    if (!udev) {
        return false;
    }

    if (!monitoring) {
        monitor = udev_monitor_new_from_netlink(udev, "udev");
        udev_monitor_filter_add_match_subsystem_devtype(monitor, "usb", nullptr);
        udev_monitor_filter_add_match_subsystem_devtype(monitor, "pci", nullptr);
        udev_monitor_filter_add_match_subsystem_devtype(monitor, "wwan", nullptr);
        // udev_monitor_filter_add_match_subsystem_devtype(monitor, "net", nullptr);
        // udev_monitor_filter_add_match_subsystem_devtype(monitor, "tty", nullptr);
        udev_monitor_enable_receiving(monitor);
        monitorFd = udev_monitor_get_fd(monitor);

        monitoringThread = new QThread();

        connect(monitoringThread, &QThread::started, this, &DeviceImpl::checkDeviceEvents);
        //connect(monitoringThread, &QThread::finished, &Device::getInstance(), &Device::deleteLater);

        DeviceImpl::getInstance()->moveToThread(monitoringThread);
        monitoringThread->start();
        monitoring = true;
    }
    return true;
}

bool DeviceImpl::stop() {
    if (monitoring) {
        monitoring = false;

        if (monitoringThread) {
            ssize_t ret = write(pipFd[1], "exit", strlen("exit") + 1);
            if (ret == -1) {
                FMTLOG_ERROR("Failed to write exit signal to pipe");
            }

            close(pipFd[1]);
        }

        if (udev) {
            udev_unref(udev);
            udev = nullptr;
        }

        if (monitor) {
            udev_monitor_unref(monitor);
            monitor = nullptr;
        }
    }
    return true;
}

DeviceImpl::DeviceImpl() : udev(nullptr), monitor(nullptr), monitorFd(-1), monitoring(false), monitoringThread(nullptr) {
    atcion["add"] = DEVICE_ADDED;
    atcion["remove"] = DEVICE_REMOVED;
    atcion["change"] = DEVICE_CHANGED;
    pipFd[0] = 0;
    pipFd[1] = 0;
    qRegisterMetaType<deviceInfo>("deviceInfo");
}

DeviceImpl::~DeviceImpl() {
    stop();
}

void DeviceImpl::checkDeviceEvents() {
    char buffer[16] = {0};
    int epollFd = epoll_create1(0);
    if (epollFd == -1) {
        //LOG_ERROR("Failed to create epoll file descriptor.");
        return;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = monitorFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, monitorFd, &ev) == -1) {
        // LOG_ERROR("Failed to add file descriptor to epoll.");
        close(epollFd);
        return;
    }

    /* 监控管道的读取端,为了在程序退出时退出死循环 */
    ev.data.fd = pipFd[0];
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, pipFd[0], &ev) == -1) {
        //LOG_ERROR("Failed to add pipeFd to epoll.");
        close(epollFd);
        close(pipFd[0]);
        close(pipFd[1]);
        return;
    }

    struct epoll_event events[2];
    while (monitoring) {
        int nfds = epoll_wait(epollFd, events, 2, -1);
        if (nfds == -1) {
            LOG_ERROR("epoll_wait failed.");
            continue;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == monitorFd) {
                handleDeviceEvent();
            } else if (events[i].data.fd == pipFd[0]) {

                ssize_t ret = read(pipFd[0], buffer, sizeof(buffer));
                if (ret == -1) {
                    LOG_ERROR("[%s]:Error of read data from pip!", __func__);
                }

                close(pipFd[0]);
                LOG_INFO("Exiting event monitoring.");
            }
        }
    }

    close(epollFd);
    LOG_DEBUG("exit Thread for monitor!");
    monitoringThread->quit();
}

bool DeviceImpl::IsWhiteListedDevice(const QString &deviceId) {
    if (idlist.contains(deviceId)) {
        FMTLOG_DEBUG("Whitelisted deviceId={}", deviceId);
        return true;
    } else {
        // 遍历 idlist 的 key，匹配前 9 个字符
        for (auto it = idlist.begin(); it != idlist.end(); ++it) {
            if (it.key() == deviceId.left(DEVICE_ID_LENTH)) {
                return true;
            }
        }
    }

    return false;
}

void DeviceImpl::handleDeviceEvent() {
    QMutexLocker locker(&deviceMutex);
    deviceInfo deviceinfo;
    struct udev_device* dev = udev_monitor_receive_device(monitor);

    if (dev) {
        deviceinfo.usbnum = udev_device_get_sysname(dev);
        deviceinfo.bus = udev_device_get_subsystem(dev);
        deviceinfo.usbDevNode = udev_device_get_devnode(dev);
        deviceinfo.type = atcion.value(udev_device_get_action(dev));

        QString actiontype = udev_device_get_action(dev);
        quint32 type = atcion.count(actiontype);
        FMTLOG_DEBUG("Device event: usbnum={} bus={} usbDevNode={} action={}", deviceinfo.usbnum, deviceinfo.bus, deviceinfo.usbDevNode, udev_device_get_action(dev));

        if (type != 0) {
            const char *vid = udev_device_get_sysattr_value(dev, "idVendor");
            const char *pid = udev_device_get_sysattr_value(dev, "idProduct");
            const char *sdid = udev_device_get_sysattr_value(dev, "vendor");;
            const char *svid = udev_device_get_sysattr_value(dev, "device");

            QString tmpid = QString(vid) + ":" + QString(pid);;
            QString tmpPcieId = QString(sdid) + ":" + QString(svid);
            tmpPcieId = tmpPcieId.remove("0x");
            QString id = QString(vid) + ":" + QString(pid);

            FMTLOG_DEBUG("Device ids: usbId={} pcieId={} bus={}", tmpid, tmpPcieId, deviceinfo.bus);
            QString sysname =  udev_device_get_syspath(dev);
            FMTLOG_DEBUG("sysname:{}", sysname);
            bool is_arm = false;

            if (tmpid.length() == DEVICE_ID_LENTH || tmpPcieId.length() == DEVICE_ID_LENTH) {
                if (deviceinfo.bus == "usb") {
                    if (tmpid.length() != DEVICE_ID_LENTH) {
                        return;
                    }
                    /* 设备的sysnum中包含“:”,说明是子设备，我们不处理，我们只针对设备本身进行处理 */
                    if (deviceinfo.usbnum.contains(":")) {
                        FMTLOG_DEBUG("Skip USB child device");
                        return;
                    } else {
                        FMTLOG_DEBUG("USB sysname={}", deviceinfo.usbnum);
                    }

                    if (!deviceinfo.usbDevNode.contains("/dev/")) {
                        FMTLOG_DEBUG("Skip USB child device");
                        return;
                    } else {
                        FMTLOG_DEBUG("USB devnode={}", deviceinfo.usbDevNode);
                    }

                    if (IsWhiteListedDevice(id)) {
                        deviceinfo.deviceId = id;
                        deviceinfo.moduleName = udev_device_get_sysattr_value(dev, "product");

                        if (deviceinfo.moduleName.contains("QUSB_BULK", Qt::CaseInsensitive) ||
                            deviceinfo.moduleName.contains("101") ||
                            deviceinfo.moduleName.contains("135") ||
                            deviceinfo.moduleName.contains("151")) {
                            deviceinfo.platform = "QC";
                        } else if (deviceinfo.moduleName.contains("350") ||
                               deviceinfo.moduleName.contains("MEDIATEK")) {
                               deviceinfo.platform = "MTK";
                        }
                    } else {
                        return;
                    }
                } else if (deviceinfo.bus == "pci") {
                    if (tmpPcieId.length() == DEVICE_ID_LENTH) {
                        return;
                    }
                    const char* subsysvid = udev_device_get_sysattr_value(dev, "subsystem_vendor");
                    const char* subsyspid = udev_device_get_sysattr_value(dev, "subsystem_device");
                    QString subsysid = QString(subsysvid) + QString(subsyspid);
                    subsysid.remove("0x");

                    if (IsWhiteListedDevice(tmpPcieId)) {
                        LOG_DEBUG("[%s]: Find the id frome idlist!");
                        deviceinfo.deviceId = tmpPcieId + ":" + subsysid;

                        if (productList.contains(tmpPcieId) != 0) {
                            deviceinfo.moduleName = productList.value(tmpPcieId);
                            if (deviceinfo.moduleName.contains("350") ||
                                deviceinfo.moduleName.contains("MEDIATEK")) {
                                deviceinfo.platform = "MTK";
                            }

                            deviceinfo.moduleName = productList.value(tmpPcieId);
                            if (deviceinfo.moduleName.contains("151") ||
                                deviceinfo.moduleName.contains("135R")) {
                                deviceinfo.platform = "QC";
                                type = DUMP;
                                FMTLOG_DEBUG("Port state: DUMP");
                            }
                        }

                        //LOG_DEBUG("[%s]: >>>>>>product name:%s, deviceid:%s", QS(deviceinfo.bus), QS(deviceinfo.moduleName), QS(deviceinfo.deviceId));
                    } else {
                        FMTLOG_DEBUG("VID/PID not in whitelist, skip");
                        return;
                    }

                } else {
                    return;
                }
            } else if (actiontype != "remove") {
                if (deviceinfo.bus == "wwan") {
                    if (!sysname.contains("mbim") && !sysname.contains("fastboot") && !sysname.contains("mipc") && !sysname.contains("sahara")) {
                        FMTLOG_DEBUG("Skip wwan syspath={}", sysname);
                        return;
                    }

                    getDeviceinfoWithBus("pci", deviceinfo);

                    if (sysname.contains("sahara")) {
                        deviceinfo.portState = "DUMP-PORT";
                    }
                    if (deviceinfo.usbnum.isEmpty() || !sysname.contains(sysname)) {
                        FMTLOG_DEBUG("Missing device id, skip");
                        return;
                    }
                } else {
                    return;
                }
            } else if (actiontype == "remove") {
                if (deviceinfo.bus == "wwan") {
                    QString sysname =  udev_device_get_syspath(dev);
                    if (!sysname.contains("mbim") && !sysname.contains("fastboot") && !sysname.contains("mipc") && !sysname.contains("sahara")) {
                        return;
                    }
                    QStringList parts = sysname.split('/');

                    // 查找 'wwan' 所在的位置
                    int wwanIndex = parts.indexOf("wwan");

                    /* 如果找到了 'wwan' */
                    if (wwanIndex > 0) {
                        /* 获取 'wwan' 前面的部分 */
                        deviceinfo.usbnum  = parts[wwanIndex - 1];

                    } else {
                        FMTLOG_WARN("wwan syspath missing parent device");
                    }

                    deviceinfo.bus = "pci";
                }
            }
        } else {
            return;
        }

        FMTLOG_DEBUG("Emit device type={}", static_cast<int>(deviceinfo.type));
        FMTLOG_INFO("Device {}: module={} deviceId={} usbnum={} usbDevNode={}", deviceinfo.bus, deviceinfo.moduleName, deviceinfo.deviceId, deviceinfo.usbnum, deviceinfo.usbDevNode);

        switch (deviceinfo.type) {
            case DEVICE_ADDED:               
                emit DeviceChanged(deviceinfo);
                break;
            case DEVICE_REMOVED:
                emit DeviceChanged(deviceinfo);
                break;
            case DEVICE_CHANGED:
                if (deviceinfo.bus == "pci") {
                    emit DeviceChanged(deviceinfo);
                }
                break;
            default:
                FMTLOG_WARN("Invalid device event type");
        }

        udev_device_unref(dev);
    } else {
        FMTLOG_WARN("Failed to receive device event");
    }
}

bool DeviceImpl::getDeviceinfoWithBus(const QString bus, deviceInfo &devinfo) {
    if (bus.isEmpty()) return false;

    QByteArray byteArray = bus.toUtf8();
    const char *bustype = byteArray.constData();

    struct udev *udev = udev_new();
    if (!udev) {
        FMTLOG_ERROR("Failed to create udev");
        return false;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    if (!enumerate) {
        FMTLOG_ERROR("Failed to create udev enumerate");
        udev_unref(udev);
        return false;
    }

    udev_enumerate_add_match_subsystem(enumerate, bustype);
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry = nullptr;

    udev_list_entry_foreach(entry, devices) {
        struct udev_device *dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(entry));
        if (!dev) continue;

        QString vid, pid, tmpid, tmpPcieId;
        fetchDeviceIds(dev, vid, pid, tmpid, tmpPcieId);

        FMTLOG_DEBUG("Enumerate ids: usbId={} pcieId={} bus={}", tmpid, tmpPcieId, bus);

        if ((bus == "usb" || bus == "pci") && (tmpid.length() == DEVICE_ID_LENTH || tmpPcieId.length() == DEVICE_ID_LENTH)) {
            if (!processBusSpecific(dev, bus, devinfo, tmpid, tmpPcieId))
                continue;

            break;
        }

        if (devinfo.deviceId.isEmpty()) {
            if (!checkParentDevices(dev, bus, devinfo)) continue;
        }

        if (devinfo.bus == "usb" && devinfo.deviceId.length() != DEVICE_ID_LENTH && !IsWhiteListedDevice(devinfo.deviceId)) {
            continue;
        }

        if (devinfo.bus == "pci" && devinfo.deviceId.length() != DEVICE_ID_LENTH + 9 && !IsWhiteListedDevice(devinfo.deviceId)) {
            continue;
        }

        processDevicePorts(dev, devinfo);
        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    FMTLOG_DEBUG("Device ports count={}", devinfo.ports.size());
    return !devinfo.deviceId.isEmpty();
}

void DeviceImpl::fetchDeviceIds(udev_device *dev, QString &vid, QString &pid, QString &tmpid, QString &tmpPcieId) {
    vid = udev_device_get_sysattr_value(dev, "idVendor");
    pid = udev_device_get_sysattr_value(dev, "idProduct");
    QString sdid = udev_device_get_sysattr_value(dev, "vendor");
    QString svid = udev_device_get_sysattr_value(dev, "device");

    tmpid = QString("%1:%2").arg(vid).arg(pid);
    tmpPcieId = QString("%1:%2").arg(sdid).arg(svid).remove("0x");
}

bool DeviceImpl::processBusSpecific(udev_device *dev, const QString &bus, deviceInfo &devinfo, const QString &tmpid, const QString &tmpPcieId) {
    if (bus == "usb") {
        if (!IsWhiteListedDevice(tmpid)) {
            return false;
        }

        devinfo.deviceId = tmpid;
        devinfo.moduleName = udev_device_get_sysattr_value(dev, "product");
    } else if (bus == "pci") {
        if (!IsWhiteListedDevice(tmpPcieId)) {
            return false;
        }

        // Keep this part, as it can be used if the subsystem_ID needs to be matched.
        QString subvid = udev_device_get_sysattr_value(dev, "subsystem_vendor");
        QString subpid = udev_device_get_sysattr_value(dev, "subsystem_device");
        subvid.remove("0x");
        subpid.remove("0x");
        QString subsysid = subpid + subvid;

        LOG_DEBUG("subpid:%s subvid:%s subsystemid:%s", QS(subpid), QS(subvid), QS(subsysid));

        // if (!IsWhiteListedDevice(id)) {
        //     return false;
        // }

        devinfo.deviceId = tmpPcieId + ":" + subsysid;
        FMTLOG_DEBUG("PCIe id={}", tmpPcieId);
        if (productList.contains(tmpPcieId) != 0) {
            devinfo.moduleName = productList.value(tmpPcieId);
        }
    }

    devinfo.bus = bus;
    devinfo.usbnum = udev_device_get_sysname(dev);
    return true;
}

bool DeviceImpl::checkParentDevices(udev_device *dev, const QString &bus, deviceInfo &devinfo) {
    struct udev_device *parent = udev_device_get_parent(dev);
    while (parent) {
        QString vid = udev_device_get_sysattr_value(parent, "idVendor");
        QString pid = udev_device_get_sysattr_value(parent, "idProduct");
        QString sdid = udev_device_get_sysattr_value(parent, "vendor");
        QString svid = udev_device_get_sysattr_value(parent, "device");

        QString tmpid = QString("%1:%2").arg(vid).arg(pid);
        QString tmpPcieId = QString("%1:%2").arg(sdid).arg(svid).remove("0x");

        if (tmpid.length() == DEVICE_ID_LENTH || tmpPcieId.length() == DEVICE_ID_LENTH) {
            QString parentBus = udev_device_get_subsystem(parent);
            if ((parentBus == "usb" && IsWhiteListedDevice(tmpid))) {
                devinfo.deviceId = tmpid;
                devinfo.moduleName = udev_device_get_sysattr_value(parent, "product");
                devinfo.bus = parentBus;
                devinfo.usbnum = udev_device_get_sysname(parent);
                return true;
            } else if (parentBus == "pci" && IsWhiteListedDevice(tmpPcieId)) {
                // const char* pid = udev_device_get_sysattr_value(parent, "subsystem_vendor");
                // const char* vid = udev_device_get_sysattr_value(parent, "subsystem_device");

                // QString id = QString("%1:%2").arg(vid).arg(pid).remove("0x");

                // if (!IsWhiteListedDevice(id))
                // {
                //     return false;
                // }

                // devinfo.deviceId = id;
                devinfo.deviceId = tmpPcieId;
                if (productList.contains(tmpPcieId) != 0) {
                    devinfo.moduleName = productList.value(tmpPcieId);
                }

                devinfo.bus = parentBus;
                devinfo.usbnum = udev_device_get_sysname(parent);

                return true;
            }
        }
        parent = udev_device_get_parent(parent);
    }
    return false;
}

bool GetPortTypeFormSysNode(const QString &devicePath, PortType &type) {
    QString filePath = QString("/sys/bus/pci/devices/%1/t7xx_mode").arg(devicePath);
    QFile file(filePath);

    FMTLOG_DEBUG("t7xx_mode path={}", filePath);

    // 检查文件是否存在
    if (!file.exists()) {
        FMTLOG_DEBUG("t7xx_mode file not found");
        return false;
    }

    // 尝试打开文件
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        FMTLOG_ERROR("Failed to open t7xx_mode file");
        return false;
    }

    // 读取文件内容
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    content = content.trimmed();  // 去除文件内容中的多余空白
    if (content.contains("dump") != 0) {
        type = DUMP;
        FMTLOG_DEBUG("Port state: DUMP");
    } else if (content.contains("download") != 0){
        type = FASTBOOT;
        FMTLOG_DEBUG("Port state: FASTBOOT");
    }

    return true;
}

void DeviceImpl::processDevicePorts(udev_device *dev, deviceInfo &devinfo) {
    PortType type = UNKNOWN;
    QString sysname = QString(QLatin1String(udev_device_get_devnode(dev)));
    QString pattern = (devinfo.bus == "usb") ? "mbim" : (devinfo.bus == "pci") ? "cdc-wdm" : QString();

    if (devinfo.bus == "usb" && sysname.contains("cdc-wdm"))
    {
        addPortInfo(dev, devinfo, MBIM);
    } else if (devinfo.bus == "pci" && (sysname.contains("cdc-wdm")|| sysname.contains("mbim"))){
        addPortInfo(dev, devinfo, MBIM);
    } else if (sysname.contains("wwan0sahara0")){
        addPortInfo(dev, devinfo, DUMP);
    } else if (sysname.contains("at") || sysname.contains("ttyUSB")) {
        addPortInfo(dev, devinfo, AT);
    } else if (sysname.contains("mipc")) {
        addPortInfo(dev, devinfo, MIPC);
    } else if (sysname.contains("adb")) {
        addPortInfo(dev, devinfo, ADB);
    } else if (sysname.contains("fastboot")) {
        //如果变为fastboot,不一定是烧录口，需要根据实际情况来确定当前到底是flash还是dump
        if (!GetPortTypeFormSysNode(devinfo.usbnum, type)) {
            FMTLOG_WARN("Failed to get port type from t7xx_mode");
        }
        switch (type)
        {
            case DUMP:
            case FASTBOOT:
                addPortInfo(dev, devinfo, type);
                break;
            default:
                addPortInfo(dev, devinfo, FASTBOOT);
                break;
        }
    }
}

void DeviceImpl::addPortInfo(udev_device *dev, deviceInfo &devinfo, PortType type) {
    portInfo info;
    info.Name = QString(QLatin1String(udev_device_get_devnode(dev)));
    info.ifaceNum = extractInterfaceNumber(QString(QLatin1String(udev_device_get_syspath(dev))));
    info.type = type;
    devinfo.ports.append(info);
    FMTLOG_DEBUG("Add port name={} len={}", info.Name, info.Name.length());
}


bool DeviceImpl::getDeviceInfo(deviceInfo &info) {
    QMutexLocker locker(&deviceMutex);
    const QStringList busTypes = {"usb",  "pci", "usbmisc",
                                  "wwan", "net", "tty"};
    bool ret = false; // 初始化返回值

    for (const QString& bus : busTypes)
    {
        ret |= getDeviceinfoWithBus(bus, info); // 使用 |= 更新返回值
    }

    FMTLOG_INFO("getDeviceInfo module={}", info.moduleName);

    if (info.moduleName.contains("350") ||
        info.moduleName.contains("MEDIATEK")) {
        info.platform = "MTK";
    }else if (info.moduleName.contains("QUSB_BULK", Qt::CaseInsensitive)||
             info.moduleName.contains("101") ||
             info.moduleName.contains("135") ||
             info.moduleName.contains("151")) {
        info.platform = "QC";
    }

    return ret; // 返回是否找到任何设备
}

// 函数定义
QString DeviceImpl::extractInterfaceNumber(const QString &syspath) {
    //QRegularExpression interfaceRegex(R"((?:\d+-\d+/)*\d+-\d+:(?:\d+)\.(\d+))");
    QRegularExpression interfaceRegex(R"((?:\d+-[\d.]+):\d+\.(\d+))");
    QRegularExpressionMatch match = interfaceRegex.match(syspath);
    FMTLOG_DEBUG("syspath :{}", syspath);
    if (match.hasMatch()) {
        QString interfaceNumber = match.captured(1);
        FMTLOG_DEBUG("Matched interface={}", interfaceNumber);
        return interfaceNumber;
    }

    FMTLOG_DEBUG("No interface number matched");
    return QString();
}

bool DeviceImpl::setValue(quint32 gpio, int value) {
    return writeToFile(QString("/sys/class/gpio/gpio%1/value").arg(gpio), QString::number(value),
                       "Error setting value for GPIO:%d", gpio);
}

bool DeviceImpl::exportGPIO(quint32 gpio) {
    return writeToFile("/sys/class/gpio/export", QString::number(gpio),
                       "Error exporting GPIO:%d", gpio);
}

bool DeviceImpl::unexportGPIO(quint32 gpio) {
    return writeToFile("/sys/class/gpio/unexport", QString::number(gpio),
                       "Error unexporting GPIO:%d", gpio);
}

bool DeviceImpl::setDirection(quint32 gpio, const QString& direction) {
    return writeToFile(QString("/sys/class/gpio/gpio%1/direction").arg(gpio), direction,
                       "Error setting direction for GPIO:%d", gpio);
}

bool DeviceImpl::gpioDirectoryExists(quint32 gpio) const {
    QDir gpioDir(QString("/sys/class/gpio/gpio%1").arg(gpio));
    return gpioDir.exists();
}

bool DeviceImpl::resetUsbDevice(quint32 gpio) {
    if (!gpioDirectoryExists(gpio) && !exportGPIO(gpio)) {
        FMTLOG_ERROR("exportGPIO error!");
        return false;
    }

    // Set value to HIGH initially to avoid unexpected behavior
    if (!setValue(gpio, HIGHT)) {
        FMTLOG_ERROR("First set to HIGH failed!");
        return false;
    }

    // Set GPIO direction to "out"
    if (!setDirection(gpio, OUT)) {
        FMTLOG_ERROR("setDirection error!");
        return false;
    }

    // Set value to LOW to trigger the reset
    if (!setValue(gpio, LOW)) {
        return false;
    }

    // Retry setting value to HIGH up to 5 times if it fails
    if (!retrySetValue(gpio, HIGHT, 5)) {
        FMTLOG_ERROR("Retry setting to HIGH failed after 5 attempts!");
        return false;
    }

    return true;
}

bool DeviceImpl::writeToFile(const QString &filePath, const QString &content, const char *errorMsg, quint32 gpio) {
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream out(&file);
        out << content;

        if (out.status() != QTextStream::Ok) {
            FMTLOG_ERROR("Failed to write file={} for GPIO={}", filePath, gpio);
            file.close();
            return false;
        }

        file.close();
        return true;
    } else {
        FMTLOG_ERROR("{}", QString::asprintf(errorMsg, gpio));
        return false;
    }
}

bool DeviceImpl::retrySetValue(quint32 gpio, int value, quint32 retries) {
    for (quint32 i = 0; i < retries; i++) {
        if (setValue(gpio, value)) {
            return true;
        }
        FMTLOG_DEBUG("Retry GPIO={} set value, attempt={}", gpio, i + 1);
    }
    return false;
}


bool DeviceImpl::resetPcieDevice(const QString devicePath) {
    QString filePath = QString(PCIE_DRIVER_FILE).arg(devicePath);
    QFile file(filePath);
    if (!QFile::exists(filePath)) {
        FMTLOG_ERROR("PCIe reset path does not exist: {}", filePath);
        return false;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        FMTLOG_ERROR("Failed to open PCIe reset file: {}", filePath);
        return false;
    }

    QTextStream out(&file);
    out << "reset";

    if (out.status() != QTextStream::Ok) {
        FMTLOG_ERROR("Failed to write PCIe reset file: {}", filePath);
        file.close();
        return false;
    }

    file.close();

    return true;
}
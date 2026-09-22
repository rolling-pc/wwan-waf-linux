#include <QDebug>
#include <QCoreApplication>
#include <sys/epoll.h>
#include <unistd.h>
#include "DeviceImpl.h"

#define DEVICE_ID_LENTH (9)
#define PCIE_DRIVER_FILE ("/sys/bus/pci/devices/%1/t7xx_mode")

using namespace afal::device;

bool checkResetPara(QString bus, quint32 gpio, const QString devicePath) {
    if (bus.isEmpty()) {
        FMTLOG_ERROR("Reset failed: bus is empty");
        return false;
    }

    if (bus.toLower().contains("usb")) {
        if (gpio < 0 || gpio > 10000) {
            FMTLOG_ERROR("Reset failed: invalid GPIO={}", gpio);
            return false;
        }
    } else if (bus.toLower().contains("pci")) {
        if (devicePath.isEmpty()) {
            FMTLOG_ERROR("Reset failed: PCIe device path is empty");
            return false;
        }
    } else {
        FMTLOG_ERROR("Reset failed: unsupported bus={}", bus);
        return false;
    }

    return true;
}

Device &Device::getInstance() {
    static  Device instance;
    return instance;
}

Device::Device() {
    FMTLOG_INFO("Device() version={}", DEVICE_VERSION_STRING);
    impl = DeviceImpl::getInstance();
}

Device::~Device() {
    if (impl != nullptr) {
        delete impl;
    }
}

bool Device::start(QMap<QString, QMap<QString, PortType>> id, QMap<QString, QString> product) {
    connect(impl, &DeviceImpl::DeviceChanged, this, &Device::OnDeviceChangeHandle);
    return impl->start(id, product);
}

bool Device::stop() {
    return impl->stop();
}

bool Device::getDeviceInfo(deviceInfo& info) {
    return impl->getDeviceInfo(info);
}

bool Device::resetDevice(QString bus, quint32 gpio, const QString devicePath) {
    bool ret = false;
    if (!checkResetPara(bus, gpio, devicePath)) {
        return false;
    }

    if (bus.toLower().contains("usb")) {
        ret = impl->resetUsbDevice(gpio);
    } else if (bus.toLower().contains("pci")) {
        ret = impl->resetPcieDevice(devicePath);
    } else {
        FMTLOG_ERROR("Reset failed: unsupported bus={}", bus);
        return false;
    }

    return ret;
}

void  Device::OnDeviceChangeHandle(const deviceInfo& info) {
    switch (info.type)
    {
        case DEVICE_ADDED:
            FMTLOG_INFO("Broadcast DEVICE_ADDED module={} deviceId={}",
                        info.moduleName, info.deviceId);
            emit Added(info);
            break;
        case DEVICE_REMOVED:
            FMTLOG_INFO("Broadcast DEVICE_REMOVED module={} deviceId={}",
                        info.moduleName, info.deviceId);
            emit Removed(info);
            break;
        case DEVICE_CHANGED:
            FMTLOG_INFO("Broadcast DEVICE_CHANGED module={} deviceId={}",
                        info.moduleName, info.deviceId);
            emit Changed(info);
            break;
        default:
            return;
    }
}
/*
bool Device::getOSName(QString &name) {

        name = QString("Operating System: %1\nVersion: %2\nArchitecture: %3")
                .arg(QSysInfo::productType()).arg(QSysInfo::productVersion())
                .arg(QSysInfo::currentCpuArchitecture());
    return true;
}
*/

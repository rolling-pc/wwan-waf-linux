//
// Created by zhangxu on 11/26/24.
//

#ifndef WAF_DEVICEIMPL_H
#define WAF_DEVICEIMPL_H
#include "Device.h"
#include "version.h"
using namespace afal::device;
class DeviceImpl : public QObject {
    Q_OBJECT
  public:
    // 获取单例实例
    static DeviceImpl* getInstance();
    DeviceImpl(const DeviceImpl&) = delete;            // 禁止复制
    DeviceImpl& operator=(const DeviceImpl&) = delete; // 禁止赋值

    // 启动设备监控
    bool start(QMap<QString, QMap<QString, PortType>> id,
               QMap<QString, QString> product);
    bool stop();
    void startMonitoring(QMap<QString, QMap<QString, PortType>> id,
                         QMap<QString, QString> product);
    bool getDeviceInfo(deviceInfo& info);

    void handleDeviceEvent();
    bool IsWhiteListedDevice(const QString &deviceId);
    bool getDeviceinfoWithBus(const QString bus, deviceInfo& devinfo);

    void fetchDeviceIds(udev_device *dev, QString &vid, QString &pid, QString &tmpid, QString &tmpPcieId);
    bool processBusSpecific(udev_device *dev, const QString &bus, deviceInfo &devinfo, const QString &tmpid, const QString &tmpPcieId);
    bool checkParentDevices(udev_device *dev, const QString &bus, deviceInfo &devinfo);
    void processDevicePorts(udev_device *dev, deviceInfo &devinfo);
    void addPortInfo(udev_device *dev, deviceInfo &devinfo, PortType type);

    QString extractInterfaceNumber(const QString& syspath);

    bool resetUsbDevice(quint32 gpio);
    bool resetPcieDevice(const QString devicePath);
    bool setValue(quint32 gpio, int value);
    bool exportGPIO(quint32 gpio);
    bool unexportGPIO(quint32 gpio);
    bool setDirection(quint32 gpio, const QString& direction);
    bool gpioDirectoryExists(quint32 gpio) const;
    bool writeToFile(const QString &filePath, const QString &content, const char *errorMsg, quint32 gpio);
    bool retrySetValue(quint32 gpio, int value, quint32 retries);

    QMap<QString, SignalType> atcion;
    struct udev* udev;
    struct udev_monitor* monitor;
    QThread* monitoringThread;
    int monitorFd;
    bool monitoring;
    int pipFd[2];
    QMap<QString, QMap<QString, PortType>> idlist;
    QMap<QString, QString> productList;
    QMutex deviceMutex;

    DeviceImpl();  // 私有构造函数
    ~DeviceImpl(); // 私有析构函数

  public slots:
    void checkDeviceEvents();

  signals:
    void DeviceChanged(const deviceInfo& info);
};
#endif // WAF_DEVICEIMPL_H

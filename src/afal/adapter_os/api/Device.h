#pragma once
#include <QThread>
#include <vector>
#include <string>
#include <mutex>
#include <QMutex>
#include "log.hpp"
#include <QString>
#include <QVector>
#include <QDebug>
#include <iostream>
#include <QDir>
#include <QMap>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>

#ifdef _IS_LINUX_
    #include <libudev.h>
#else
    #include <QTimer>
    #include <QList>
    #include <QMutexLocker>
    #include <QRegularExpression>

    #include <windows.h>
    #include <wbemidl.h>
    #include <comdef.h>
    #include <SetupAPI.h>
    #include <devguid.h>
    #include <cfgmgr32.h>
    #include <tchar.h>
    #include <csignal>
    #include <dbt.h>
    #include <winsock.h>
#endif

using namespace afal::log;
class DeviceImpl;
namespace afal {
    namespace device {
        //#define SOCKET_NAME ("device-monitor-service")
        #ifdef TOOLService
          #define SOCKET_NAME "device-monitor-service_tool"
        #else
          #define SOCKET_NAME "device-monitor-service"
        #endif

        enum gpiovalue { LOW, HIGHT };

        enum FuncIdList {
            GET_DEVICE_INFO,
            RESET_DEVICE,
            EVENT,
            FUNC_ID_UNKNOWN = 0xEFFF,
        };

        enum SignalType {
            DEVICE_ADDED,   /* add */
            DEVICE_REMOVED, /* remove */
            DEVICE_CHANGED, /* change */
            DEVICE_BIND,
            DEVICE_UNBIND
        };

        enum PortType {
            MBIM,     /* MIBIM */
            AT,       /* AT */
            DIAG,     /* DIAG */
            MIPC,     /* MIPC */
            GNSS,     /* GNSS */
            FLASH,    /* EDL */
            FASTBOOT, /* 正常烧录口 */
            ADB,
            DUMP, /* DUMP */
            // This is 350 PORT
            MDLOG,
            SAPLOG,
            MDMETA,
            SAPMETA,
            NPT,
            NMEA,
            NOTUSE,
            QMUX,
            UNKNOWN
        };

        struct portInfo {
            PortType type; /* eg: AT, MBIM, MIPC, LOG */
            QString Name;  /* eg: wwan0at0, ttyUSB0 */
            QString ifaceNum;
        };

        struct deviceInfo {
            SignalType type;    /* 0: add, 1: remove, 2: change */
            QString portState;  /* NormalPort, FlashPort, FastbootPort, DumpPort */
            QString bus;        /* USB or PCIE */
            QString deviceId;   /* eg */
            QString moduleName; /* eg: FM101 FM350 RW101 RW350 ... */
            QString usbnum;
            QString platform;
            QString usbDevNode;
            QVector<portInfo> ports; /* 端口的信息组: 端口的类型，端口名称 */
        };

        /* 注册设备状态主动上报的回调函数定义 */
        typedef void (*callback)(SignalType&, deviceInfo&);
        class Device : public QObject {
            Q_OBJECT
          public:
            static Device& getInstance();
            Device(const Device&) = delete;
            Device& operator=(const Device&) = delete;
            bool start(QMap<QString, QMap<QString, PortType>> id,
                       QMap<QString, QString> product);
            bool stop();
            bool getDeviceInfo(deviceInfo& info);
#ifndef _IS_LINUX_
            void onPowerEvent(unsigned eventType);
#endif
            bool resetDevice(QString bus, quint32 gpio, const QString devicePath);

            QMap<QString, QMap<QString, PortType>> idlist;
            QMap<QString, QString> productList;
            // os adpter的函数接口，后面替换
            //bool getOSName(QString& name);

          signals:
            void Added(const deviceInfo& info);
            void Removed(const deviceInfo& info);
            void Changed(const deviceInfo& info);

          private slots:
            void OnDeviceChangeHandle(const deviceInfo& info);

          private:
            ~Device();
            DeviceImpl* impl;

          protected:
            Device();
        };
    } // namespace device
} // namespace afal

Q_DECLARE_METATYPE(afal::device::deviceInfo);
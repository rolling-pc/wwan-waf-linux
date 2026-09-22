#include "Client.h"
#include <QWaitCondition>

void AddedRevice(SignalType &type, deviceInfo &info) {
    LOG_DEBUG("======================= Ind Start : Added ======================");
    LOG_DEBUG("indication Callback called:");
    LOG_DEBUG("Bus:%s", QS(info.bus));
    LOG_DEBUG("type:%s", ((info.type == DEVICE_ADDED) ? "Add" : ((info.type == DEVICE_REMOVED) ? "Remove" : "Change")));
    LOG_DEBUG("deviceId:%s", QS(info.deviceId));
    LOG_DEBUG("moduleName:%s", QS(info.moduleName));
    LOG_DEBUG("portState:%s", QS(info.portState));
    LOG_DEBUG("platform:%s", QS(info.platform));

    QMap<PortType, QString> tmptype;
    //这里我们暂时模拟下，后续需要从xml中根据实际的信息来确定
    {
        tmptype[AT] = "AT";
        tmptype[DIAG] = "DIAG";
        tmptype[MBIM] = "MBIM";
        tmptype[MIPC] = "MIPC";
        tmptype[GNSS] = "GNSS";
        tmptype[FLASH] = "FLASH";
        tmptype[FASTBOOT] = "FASTBOOT";
        tmptype[DUMP] = "DUMP";
    }


    for (int i = 0; i < info.ports.size(); i++) {
        QString value1 = tmptype.value(info.ports.at(i).type, "");
        LOG_DEBUG("[Port]:%d,size:%d,porttype:%s", i, info.ports.size(), QS(value1.isEmpty()?"NULL":value1));
        LOG_DEBUG("[Port]:%d,size:%d,portName:%s", i, info.ports.size(), QS(info.ports.at(i).Name));
        LOG_DEBUG("[Port]:%d,size:%d,portNum :%s", i, info.ports.size(), QS(info.ports.at(i).ifaceNum));
    }
    LOG_DEBUG("======================= Ind Stop : Added========================");
}

void RemovededReved(SignalType &type, deviceInfo &info) {
    LOG_DEBUG("======================= Ind Start : Renoved ======================");
    LOG_DEBUG("indication Callback called:");
    LOG_DEBUG("Bus:%s", QS(info.bus));
    LOG_DEBUG("type:%s", ((info.type == DEVICE_ADDED) ? "Add" : ((info.type == DEVICE_REMOVED) ? "Remove" : "Change")));
    LOG_DEBUG("deviceId:%s", QS(info.deviceId));
    LOG_DEBUG("moduleName:%s", QS(info.moduleName));
    LOG_DEBUG("portState:%s", QS(info.portState));
    LOG_DEBUG("platform:%s", QS(info.platform));
    LOG_DEBUG("======================= Ind stop :  Removed  =====================");
}

void ChangedRevice(SignalType &type, deviceInfo &info) {
    LOG_DEBUG("======================= Ind Start : Changed ======================");
    LOG_DEBUG("indication Callback called:");
    LOG_DEBUG("Bus:%s", QS(info.bus));
    LOG_DEBUG("type:%s", ((info.type == DEVICE_ADDED) ? "Add" : ((info.type == DEVICE_REMOVED) ? "Remove" : "Change")));
    LOG_DEBUG("deviceId:%s", QS(info.deviceId));
    LOG_DEBUG("moduleName:%s", QS(info.moduleName));
    LOG_DEBUG("portState:%s", QS(info.portState));
    LOG_DEBUG("platform:%s", QS(info.platform));
    LOG_DEBUG("======================= Ind stop :  Changed  =====================");
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    Logger::Instance().Init(CONSOLE, "DeviceServiceLib Demo");
    Logger::Instance().EnableDebugMode(1);

    DevClient client;
    deviceInfo info;
    client.Init();

    client.SubscribeDeviceEvent(DEVICE_ADDED, AddedRevice);
    client.SubscribeDeviceEvent(DEVICE_REMOVED, RemovededReved);
    client.SubscribeDeviceEvent(DEVICE_CHANGED, ChangedRevice);

    client.GetDeviceinfo(info);

    LOG_DEBUG("======================= resp recived   =====================");
    LOG_DEBUG("GetDeviceinfo resp:");
    LOG_DEBUG("Bus:%s", QS(info.bus));
    LOG_DEBUG("type:%s", ((info.type == DEVICE_ADDED) ? "Add" : ((info.type == DEVICE_REMOVED) ? "Remove" : "Change")));
    LOG_DEBUG("deviceId:%s", QS(info.deviceId));
    LOG_DEBUG("moduleName:%s", QS(info.moduleName));
    LOG_DEBUG("portState:%s", QS(info.portState));
    LOG_DEBUG("platform:%s", QS(info.platform));

    QMap<PortType, QString> tmptype;
    //这里我们暂时模拟下，后续需要从xml中根据实际的信息来确定
    {
        tmptype[AT] = "AT";
        tmptype[DIAG] = "DIAG";
        tmptype[MBIM] = "MBIM";
        tmptype[MIPC] = "MIPC";
        tmptype[GNSS] = "GNSS";
        tmptype[FLASH] = "FLASH";
        tmptype[FASTBOOT] = "FASTBOOT";
        tmptype[DUMP] = "DUMP";
    }


    for (int i = 0; i < info.ports.size(); i++) {
        QString value1 = tmptype.value(info.ports.at(i).type, "");
        LOG_DEBUG("[Port]:%d,size:%d,porttype:%s", i, info.ports.size(), QS(value1.isEmpty()?"NULL":value1));
        LOG_DEBUG("[Port]:%d,size:%d,portName:%s", i, info.ports.size(), QS(info.ports.at(i).Name));
        LOG_DEBUG("[Port]:%d,size:%d,portNum :%s", i, info.ports.size(), QS(info.ports.at(i).ifaceNum));
    }
    LOG_DEBUG("======================= resp recived   =====================");

    {
        QTimer *timer = new QTimer();
        QObject::connect(timer, &QTimer::timeout, [&client]() {
            LOG_DEBUG("Timer timeout!   will  reset modem!");
            client.ResetDevice();
        });

        timer->start(60000); // 每60秒触发一次
    }



    app.exec();
    client.Deinit();

    return true;
}
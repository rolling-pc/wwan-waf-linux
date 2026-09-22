/*
 * This file is part of [WSF DeviceLib Project].
 *
 * Copyright (C) 2024  Rolling Wireless S.a.r.l.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ClientImpl.h"
#include "json_stream.h"
#include <QWaitCondition>
#include <QPointer>
#include <QElapsedTimer>
#include <QJsonParseError>
#include <cctype>

DevClientImpl::DevClientImpl(QObject *parent) : QObject(parent), socket(nullptr), retryTimer(nullptr),
                                                retryCount(0), stopping(false) {
    qRegisterMetaType<deviceInfo>("deviceInfo");
    connect(this, &DevClientImpl::indicationReceived, this, &DevClientImpl::onIndicationReceived);
}

/*
bool DevClientImpl::GetDeviceinfo(deviceInfo &info) {
    if (!socket->isOpen()) {
        LOG_ERROR("Server is not running!");
        return false;
    }

    // 发送请求
    {
        QMutexLocker locker(&functionMutex);
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = GET_DEVICE_INFO;

        QJsonDocument jsonDoc(jsonObj);
        socket->write(jsonDoc.toJson());
        socket->flush();
    }

    // 创建堆分配 QEventLoop，防止 lambda 捕获栈指针导致 crash
    QEventLoop* loop = new QEventLoop();
    QPointer<QEventLoop> loopPtr(loop); // 安全指针

    // 使用 shared_ptr 存储响应结果
    auto responseReceivedPtr = std::make_shared<bool>(false);
    auto infoPtr = std::make_shared<deviceInfo>();

    // 连接信号，使用 QueuedConnection 保证安全
    QMetaObject::Connection conn = connect(
        this, &DevClientImpl::deviceinfoRecived, this,
        [loopPtr, responseReceivedPtr, infoPtr](const deviceInfo& devinfo, bool result) {
            if (!loopPtr) return; // loop 已析构
            if (result) {
                LOG_DEBUG("Device info received successfully!");
                *infoPtr = devinfo;
                *responseReceivedPtr = true;
            } else {
                LOG_ERROR("Failed to receive device info.");
            }
            loopPtr->quit();
        },
        Qt::QueuedConnection
    );

    // 设置超时计时器
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, loop, &QEventLoop::quit);
    timer.start(5000); // 超时时间 5 秒

    // 阻塞等待信号或超时
    loop->exec();

    // 执行完毕立即断开连接
    QObject::disconnect(conn);
    delete loop;

    // 检查是否收到响应
    if (!(*responseReceivedPtr)) {
        if (timer.isActive()) {
            timer.stop();
            LOG_ERROR("Received response, but result=false");
        } else {
            LOG_ERROR("Timeout while waiting for device info response!");
        }
        return false;
    }

    info = *infoPtr;
    LOG_DEBUG("Device info successfully obtained.");
    return true;
}
*/

bool DevClientImpl::GetDeviceinfo(deviceInfo &info, bool forceRefresh) {
    if (!socket || !socket->isOpen()) {
        LOG_ERROR("Server is not running!");
        return false;
    }

    // GetDeviceinfo 同步读 socket 时，设备上报 (type=ind) 可能已经在缓冲区里，
    // 或与 resp 粘包到达。不能把 ind 当成 GetDeviceinfo 的应答，否则：
    // 1) 主动索取失败 (Invalid device info response)
    // 2) 订阅回调永远收不到这次 DEVICE_ADDED
    QList<deviceInfo> pendingIndications;
    bool gotResp = false;
    bool respOk = false;
    deviceInfo respInfo;
    {
        QMutexLocker locker(&ioMutex);
        socket->blockSignals(true);  // 防止 onReadyRead 抢读
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = GET_DEVICE_INFO;
        if (forceRefresh) {
            jsonObj["force_refresh"] = true;
        }
        socket->write(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
        if (!socket->waitForBytesWritten(2000)) {
            socket->blockSignals(false);
            LOG_ERROR("Write timeout!");
            return false;
        }

        QElapsedTimer timer;
        timer.start();
        const int timeoutMs = forceRefresh ? 15000 : 5000;
        while (!gotResp && timer.elapsed() < timeoutMs) {
            const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
            if (remaining <= 0) {
                break;
            }
            if (!socket->bytesAvailable() && !socket->waitForReadyRead(remaining)) {
                break;
            }
            rxBuffer += socket->readAll();
            const QList<QByteArray> messages = extractJsonObjects(rxBuffer);
            for (const QByteArray& msg : messages) {
                const QJsonDocument jsonDoc = QJsonDocument::fromJson(msg);
                LOG_DEBUG("GetDeviceinfo receive data is :%s",
                          SS(jsonDoc.toJson(QJsonDocument::Indented).toStdString()));
                const QJsonObject resObj = jsonDoc.object();
                const QString Type = resObj["type"].toString();
                bool ret = false;
                deviceInfo dev = deserializeFromJson(msg, ret);
                if (Type == "ind") {
                    LOG_DEBUG("GetDeviceinfo received indication during poll, queue it. action=%d",
                              static_cast<int>(dev.type));
                    pendingIndications.append(dev);
                    continue;
                }
                if (Type == "resp" && resObj["function_id"].toInt() == GET_DEVICE_INFO) {
                    if (!gotResp) {
                        gotResp = true;
                        respOk = ret;
                        respInfo = dev;
                    }
                    continue;
                }
                LOG_ERROR("GetDeviceinfo skipped unexpected message type=%s function_id=%d ret=%d",
                          QS(Type), resObj["function_id"].toInt(), ret);
            }
        }
        socket->blockSignals(false);
    }
    for (const deviceInfo& ind : pendingIndications) {
        onIndicationReceived(ind);
    }
    drainSocketMessages();

    if (!gotResp) {
        LOG_ERROR("Timeout while waiting for device info response!");
        return false;
    }
    if (!respOk) {
        LOG_ERROR("Invalid device info response.");
        return false;
    }
    info = respInfo;
    LOG_DEBUG("Device info successfully obtained. bus=%s id=%s portState=%s ports=%d",
              QS(info.bus), QS(info.deviceId), QS(info.portState), info.ports.size());
    return true;
}

bool DevClientImpl::ResetDevice() {
    QMutexLocker locker(&functionMutex);

    if (socket->isOpen()) {
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = RESET_DEVICE;

        QJsonDocument jsonDoc(jsonObj);

        socket->write(jsonDoc.toJson());
        socket->flush();

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        bool responseReceived = false;

        QObject::disconnect(this, &DevClientImpl::resetDeviceRecived, nullptr, nullptr);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(this, &DevClientImpl::resetDeviceRecived, this, [this, &loop, &responseReceived](bool result) {
            if (result) {
                LOG_DEBUG("resetDeviceRecived Reset device received successfully!");
                responseReceived = result; // 标记为收到响应
            } else {
                LOG_ERROR("Failed to Reset device.");
            }
            loop.quit();  // 退出事件循环
        });

        // QC PCIe WwanColdReset.exe can take up to 120s; keep a small buffer.
        constexpr int kResetDeviceTimeoutMs = 130 * 1000;
        timer.start(kResetDeviceTimeoutMs);
        loop.exec();         // 开始事件循环，等待超时或接收到服务端响应

        if (timer.isActive()) {
            timer.stop();
            LOG_DEBUG("resetDeviceRecived Response received, proceeding...");

            if (!responseReceived) {
                return false;
            }
        } else {
            LOG_ERROR("Timeout while waiting for server response!");
            return false;
        }

    }

    return true;
}

/*
bool DevClientImpl::SubscribeDeviceEvent(SignalType type, callback handleEvent) {
    QMutexLocker locker(&functionMutex);

    if (socket->isOpen()) {
        LOG_DEBUG("[%s]:enter", __func__);
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = EVENT;

        // Create QJsonArray and append elements
        QJsonArray signalsArray;
        switch (type) {
            case DEVICE_ADDED:
                func[DEVICE_ADDED] = handleEvent;
                signalsArray.append("deviceAdded");
                break;
            case DEVICE_REMOVED:
                func[DEVICE_REMOVED] = handleEvent;
                signalsArray.append("deviceRemoved");
                break;
            case DEVICE_CHANGED:
                func[DEVICE_CHANGED] = handleEvent;
                signalsArray.append("deviceChanged");
                break;
            default:
                LOG_ERROR("error of invaild type");
                return false;
        }

        jsonObj["signals"] = signalsArray;

        QJsonDocument jsonDoc(jsonObj);

        LOG_DEBUG(",will send:%s", SS(jsonDoc.toJson(QJsonDocument::Indented).toStdString()));

        socket->write(jsonDoc.toJson());
        socket->flush();
        // 创建事件循环以同步等待服务端响应
        QEventLoop loop;
        QTimer timer;
        bool responseReceived = false;
        timer.setSingleShot(true);

        QObject::disconnect(this, &DevClientImpl::SubscribeDeviceEventRecived, nullptr, nullptr);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(this, &DevClientImpl::SubscribeDeviceEventRecived, this, [this, &loop, &responseReceived](bool result) {
            if (result) {
                LOG_DEBUG("SubscribeDeviceEventRecived  received successfully!");
                responseReceived = result; // 标记为收到响应
            } else {
                LOG_ERROR("Failed to register signals.");
            }
            loop.quit();  // 退出事件循环
        }, Qt::UniqueConnection);

        timer.start(5000);  // 超时时间 5 秒
        loop.exec();         // 开始事件循环，等待超时或接收到服务端响应

        if (timer.isActive()) {
            timer.stop();
            LOG_DEBUG("SubscribeDeviceEventRecived Response received, proceeding...");

            if (!responseReceived) {
                return false;
            }
        } else {
            LOG_ERROR("Timeout while waiting for server response!");
            return false;
        }

        return true;
    }

    return false;
}*/

bool DevClientImpl::SubscribeDeviceEvent(SignalType type, callback handleEvent) {
    QJsonArray signalsArray;
    switch (type) {
        case DEVICE_ADDED:   func[DEVICE_ADDED] = handleEvent;   signalsArray.append("deviceAdded"); break;
        case DEVICE_REMOVED: func[DEVICE_REMOVED] = handleEvent; signalsArray.append("deviceRemoved"); break;
        case DEVICE_CHANGED: func[DEVICE_CHANGED] = handleEvent; signalsArray.append("deviceChanged"); break;
        default: return false;
    }

    if (!socket || !socket->isOpen()) {
        LOG_WARN("Subscribe type %d stored, socket not open yet", type);
        return true;
    }

    LOG_DEBUG("[%s]:enter for type %d", __func__, type);

    QJsonObject jsonObj;
    jsonObj["type"] = "req";
    jsonObj["function_id"] = EVENT;
    jsonObj["signals"] = signalsArray;
    QJsonDocument jsonDoc(jsonObj);

    bool success = false;
    QList<deviceInfo> pendingIndications;
    QList<QByteArray> leftoverMessages;
    {
        QMutexLocker locker(&ioMutex);
        socket->blockSignals(true);

        socket->write(jsonDoc.toJson(QJsonDocument::Compact));
        if (!socket->waitForBytesWritten(2000)) {
            LOG_ERROR("Write timeout!");
        } else {
            QElapsedTimer timer;
            timer.start();
            while (!success && timer.elapsed() < 3000) {
                const int remaining = 3000 - static_cast<int>(timer.elapsed());
                if (remaining <= 0) {
                    break;
                }
                if (!socket->bytesAvailable() && !socket->waitForReadyRead(remaining)) {
                    break;
                }
                rxBuffer += socket->readAll();
                const QList<QByteArray> messages = extractJsonObjects(rxBuffer);
                for (const QByteArray& msg : messages) {
                    const QJsonDocument resDoc = QJsonDocument::fromJson(msg);
                    LOG_DEBUG("Sync receive data is :%s", resDoc.toJson().constData());
                    const QJsonObject resObj = resDoc.object();
                    const QString msgType = resObj["type"].toString();
                    if (msgType == "ind") {
                        bool ret = false;
                        pendingIndications.append(deserializeFromJson(msg, ret));
                        continue;
                    }
                    if (msgType == "resp" && resObj["function_id"].toInt() == EVENT) {
                        if (resObj["status"].toString() == "ok") {
                            LOG_DEBUG("Subscribe type %d successfully!", type);
                            success = true;
                        } else {
                            LOG_ERROR("Server refused subscription: %s",
                                      resObj["Error_Message"].toString().toStdString().c_str());
                        }
                        continue;
                    }
                    leftoverMessages.append(msg);
                }
            }
            if (!success) {
                LOG_ERROR("Timeout waiting for server response for type %d!", type);
            }
        }
        socket->blockSignals(false);
    }
    for (const QByteArray& msg : leftoverMessages) {
        dispatchJsonMessage(msg);
    }
    for (const deviceInfo& ind : pendingIndications) {
        onIndicationReceived(ind);
    }
    drainSocketMessages();
    return success;
}

bool DevClientImpl::Init() {
    stopping = false;
    connectToServer();
    if (socket && socket->state() != QLocalSocket::ConnectedState) {
        socket->waitForConnected(3000);
    }
    if (!socket || socket->state() != QLocalSocket::ConnectedState) {
        LOG_WARN("Dev socket not ready yet, keep retrying every 5s");
        startReconnectTimer();
    }
    return true;
}

bool DevClientImpl::Deinit() {
    QMutexLocker locker(&functionMutex);
    QMutexLocker ioLocker(&ioMutex);
    stopping = true;
    rxBuffer.clear();
    stopReconnectTimer();

    if (socket && socket->isOpen()) {
        LOG_DEBUG(" Disconnecting from server...");
        socket->disconnectFromServer();
        if (!socket->waitForDisconnected(3000)) {
            LOG_ERROR("Failed to disconnect from server:%s",QS(socket->errorString()));
            socket->abort();
            return false;
        }
        LOG_DEBUG("Disconnected from server successfully.");
        socket.reset();
        return true;
    }

    LOG_ERROR("Socket is not open or null.");
    return false;
}

void DevClientImpl::onConnected() {
    LOG_DEBUG("socket state:%d", socket->state());
    {
        QMutexLocker locker(&ioMutex);
        rxBuffer.clear();
    }
    stopReconnectTimer();
    retryCount = 0;
    refreshAfterReconnect();
}

void DevClientImpl::onReadyRead() {
    QLocalSocket *sock = qobject_cast<QLocalSocket*>(sender());
    if (!sock && socket) {
        sock = socket.get();
    }
    if (!sock) {
        return;
    }
    QList<QByteArray> messages;
    {
        QMutexLocker locker(&ioMutex);
        rxBuffer += sock->readAll();
        if (socket && socket->bytesAvailable() > 0) {
            rxBuffer += socket->readAll();
        }
        messages = extractJsonObjects(rxBuffer);
    }
    for (const QByteArray& msg : messages) {
        dispatchJsonMessage(msg);
    }
}

void DevClientImpl::drainSocketMessages() {
    QList<QByteArray> messages;
    {
        QMutexLocker locker(&ioMutex);
        if (socket && socket->bytesAvailable() > 0) {
            rxBuffer += socket->readAll();
        }
        messages = extractJsonObjects(rxBuffer);
    }
    for (const QByteArray& msg : messages) {
        dispatchJsonMessage(msg);
    }
}

void DevClientImpl::dispatchJsonMessage(const QByteArray& msg) {
    bool ret = false;
    QJsonParseError parseError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(msg, &parseError);
    if (jsonDoc.isNull()) {
        LOG_ERROR("invalid JSON from Server (offset=%d err=%d size=%d): %s",
                  parseError.offset, static_cast<int>(parseError.error),
                  msg.size(), msg.left(512).constData());
        return;
    }
    LOG_DEBUG("receive data is :%s", SS(jsonDoc.toJson(QJsonDocument::Indented).toStdString()));

    const QJsonObject jsonObj = jsonDoc.object();
    const QString Type = jsonObj["type"].toString();
    LOG_DEBUG("Received data type:%s", QS(Type));

    deviceInfo dev = deserializeFromJson(msg, ret);
    LOG_DEBUG("Ret is :%d", ret);
    if (Type == "ind") {
        emit indicationReceived(dev);
    } else if (Type == "resp") {
        const qint32 functionId = jsonObj["function_id"].toInt();
        switch (functionId) {
            case GET_DEVICE_INFO:
                emit deviceinfoRecived(dev, ret);
                break;
            case RESET_DEVICE:
                emit resetDeviceRecived(ret);
                break;
            case EVENT:
                emit SubscribeDeviceEventRecived(true);
                break;
            default:
                LOG_ERROR("error invalid func id:%d", functionId);
                return;
        }
    } else {
        LOG_ERROR("invaild type of mesg from Server!");
    }
}

void DevClientImpl::ensureSocket() {
    if (socket) {
        return;
    }
    socket = std::make_unique<QLocalSocket>(this);
    connect(socket.get(), &QLocalSocket::connected, this, &DevClientImpl::onConnected);
    connect(socket.get(), &QLocalSocket::readyRead, this, &DevClientImpl::onReadyRead);
    connect(socket.get(), &QLocalSocket::disconnected, this, &DevClientImpl::onDisconnected);
    connect(socket.get(), &QLocalSocket::errorOccurred, this, &DevClientImpl::onError);
}

void DevClientImpl::startReconnectTimer() {
    if (stopping) {
        return;
    }
    if (!retryTimer) {
        retryTimer = new QTimer(this);
        retryTimer->setInterval(5000);
        connect(retryTimer, &QTimer::timeout, this, &DevClientImpl::retryConnection);
    }
    if (!retryTimer->isActive()) {
        retryTimer->start();
    }
}

void DevClientImpl::stopReconnectTimer() {
    if (retryTimer) {
        retryTimer->stop();
    }
}

void DevClientImpl::refreshAfterReconnect() {
    if (!socket || !socket->isOpen()) {
        return;
    }
    const QList<SignalType> events = {DEVICE_ADDED, DEVICE_REMOVED, DEVICE_CHANGED};
    bool hadSubscribers = false;
    for (const auto& event : events) {
        if (func.contains(event)) {
            hadSubscribers = true;
            SubscribeDeviceEvent(event, func.value(event));
        }
    }
    if (!hadSubscribers) {
        return;
    }
    deviceInfo info;
    if (!GetDeviceinfo(info)) {
        LOG_WARN("Reconnected to Dev service, GetDeviceinfo failed");
        return;
    }
    if (info.deviceId.isEmpty()
        && info.portState.compare(QLatin1String("NO-PORT"), Qt::CaseInsensitive) == 0) {
        info.type = DEVICE_REMOVED;
    } else {
        info.type = DEVICE_CHANGED;
    }
    LOG_INFO("Reconnected to Dev service, refresh deviceId=%s portState=%s",
             QS(info.deviceId), QS(info.portState));
    onIndicationReceived(info);
}

void DevClientImpl::connectToServer() {
    LOG_DEBUG("[%s]:enter", __func__);
    if (stopping) {
        return;
    }
    ensureSocket();
    if (socket->state() == QLocalSocket::ConnectedState
        || socket->state() == QLocalSocket::ConnectingState) {
        return;
    }
    if (socket->state() != QLocalSocket::UnconnectedState) {
        socket->abort();
    }
    QString socketName = "device-monitor-service";
#ifdef TOOLService
    socketName = "device-monitor-service_tool";
#endif
    socket->connectToServer(socketName);
    LOG_DEBUG("[%s]:exit", __func__);
}

void DevClientImpl::registerForSignals() {
    LOG_DEBUG("[%s]:enter", __func__);
    if (socket->isOpen()) {
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = EVENT;

        // Create QJsonArray and append elements
        QJsonArray signalsArray;
        signalsArray.append("deviceAdded");
        signalsArray.append("deviceRemoved");
        signalsArray.append("deviceChanged");

        jsonObj["signals"] = signalsArray;

        QJsonDocument jsonDoc(jsonObj);
        socket->write(jsonDoc.toJson());
        socket->flush();
    }
    LOG_DEBUG("[%s]:exit", __func__);
}

deviceInfo DevClientImpl::deserializeFromJson(const QByteArray& jsonData, bool &ret) {
    deviceInfo result;

    // 解析 JSON 数据
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    QJsonObject jsonObject = doc.object();

    // 提取顶级对象数据
    result.bus = jsonObject.value("deviceinfo").toObject().value("bus").toString();
    result.deviceId = jsonObject.value("deviceinfo").toObject().value("id").toString();   
    result.portState = jsonObject.value("deviceinfo").toObject().value("portState").toString();
    QString devId = result.deviceId.trimmed().toUpper();
    QString busType = result.bus.trimmed().toUpper();
    if (busType.contains("PCI") && devId.contains("14C3:4D75")) {
        result.platform = "MTK";
        result.moduleName = "PCI350";
    } else if (busType.contains("PCI") && devId.contains("14C3:0300")) {
        result.platform = "MTK";
        result.moduleName = "PCI330";
    } else {
        result.platform = jsonObject.value("deviceinfo").toObject().value("platform").toString();
        result.moduleName = jsonObject.value("deviceinfo").toObject().value("module").toString();
    }
    ret = (jsonObject.value("status").toString() == "ok")?true: false;
    // 提取 action 数据
    QJsonObject dataObject = jsonObject.value("data").toObject();
    int action = dataObject.value("action").toInt();
    result.type = static_cast<SignalType>(action);

    // 提取 ports 数组数据
    QJsonArray portsArray = jsonObject.value("deviceinfo").toObject().value("ports").toArray();
#ifdef _IS_WINDOWS_
/*
    if (busType.contains("PCI") && result.portState == "NORMAL-PORT") {
        portInfo port;
        port.Name = "MBIM";
        port.type = static_cast<PortType>(0);
        port.ifaceNum = "0";
        result.ports.append(port);
    }
*/
#endif
    for (const QJsonValue& value : portsArray) {
        QJsonObject portObject = value.toObject();
        portInfo port;
        quint32 portType = portObject.value("portType").toInt();
        QString portName = portObject.value("portName").toString();
        QString portNum = portObject.value("portNum").toString();
        port.Name = portName;
        port.type = static_cast<PortType>(portType);
        port.ifaceNum = portNum;
        result.ports.append(port);
    }

    return result;
}

void DevClientImpl::retryConnection() {
    if (stopping) {
        stopReconnectTimer();
        return;
    }
    ++retryCount;
    LOG_DEBUG("Dev socket reconnect attempt %d", retryCount);
    connectToServer();
}

void DevClientImpl::onDisconnected() {
    {
        QMutexLocker locker(&ioMutex);
        rxBuffer.clear();
    }
    if (stopping) {
        return;
    }
    LOG_WARN("Disconnected from Dev service, will keep retrying");
    startReconnectTimer();
}

void DevClientImpl::onError(QLocalSocket::LocalSocketError socketError) {
    LOG_DEBUG("Socket error:%d", socketError);
    if (stopping) {
        return;
    }
    if (socket && socket->state() == QLocalSocket::ConnectedState) {
        return;
    }
    startReconnectTimer();
}

void  DevClientImpl::onIndicationReceived(const deviceInfo &message) {
    SignalType type = message.type;
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_DEBUG("[timestamp]:%s,->Indication received with :%d bus=%s id=%s module=%s portState=%s ports=%d",
              QS(timestamp), message.type, QS(message.bus), QS(message.deviceId),
              QS(message.moduleName), QS(message.portState), message.ports.size());

    auto it = func.find(type);
    if (it != func.end()) {
        // 调用对应的回调函数
        it.value()(type, const_cast<deviceInfo &>(message));
    } else {
        LOG_ERROR("Function for type %d not found", type);
    }
}

void DevClientImpl::grabFromJson(const QByteArray& jsonData, bool &ret) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    QJsonObject jsonObject = doc.object();
    ret = (jsonObject["status"].toString() == "ok")?true: false;    
    return;
}
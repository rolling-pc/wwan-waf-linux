#include "wwan_func.hpp"
#include "wwan_impl.h"
#include "log.hpp"
#include <QWaitCondition>

using namespace afal::log;
#define SOCKET_NAME ("wwan-func-service")

namespace wwansvc {

WwanFuncSvcClient::WwanFuncSvcClient(QObject *parent) : QObject(parent) {
  Instance = nullptr;
  LOG_DEBUG("WwanClientFuncClient() called!");
}

bool WwanFuncSvcClient::Init() {
    Instance = new WwanClientImpl();
    return Instance->Init();
}

bool WwanFuncSvcClient::Deinit() {
    return Instance->Deinit();
}

bool WwanFuncSvcClient::SvcAvailable(bool& setresult) {
    return Instance->SvcAvailable(setresult);
}
bool WwanFuncSvcClient::TurnoffSwitch(bool& setresult) {
    return Instance->TurnoffSwitch(setresult);
}

bool WwanFuncSvcClient::TurnonSwitch(bool& setresult) {
    return Instance->TurnonSwitch(setresult);
}

bool WwanFuncSvcClient::QuerySwitchStatus(bool& service_on) {
    return Instance->QuerySwitchStatus(service_on);
}

WwanFuncSvcClient::~WwanFuncSvcClient() {
    delete Instance;
    LOG_DEBUG("~WwanClient() called!");
}

WwanClientImpl::WwanClientImpl(QObject *parent) : QObject(parent), socket(nullptr), retryTimer(
        nullptr) , retryCount(0), stopping(false), retryIntervalMs(5000){
    }
bool WwanClientImpl::SvcAvailable(bool& setresult) {
    QMutexLocker locker(&functionMutex);

    if (socket->isOpen()) {
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = FUNCTION_SVCAVAILABLE;
        QJsonDocument jsonDoc(jsonObj);

        socket->write(jsonDoc.toJson());
        socket->flush();

        LOG_DEBUG("SvcAvailable req sent successfully!");

        QEventLoop loop;
        QTimer timer;
        bool responseReceived = false;

        timer.setSingleShot(true);

        QObject::disconnect(this, &WwanClientImpl::SvcAvailableReceived, nullptr, nullptr);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(this, &WwanClientImpl::SvcAvailableReceived, this, [this, &loop, &responseReceived,&setresult](bool& opsresult,bool result) {
            if (result) {
                LOG_DEBUG("SvcAvailableReceived received successfully!");
                setresult = opsresult;
                responseReceived = result; // 标记为收到响应
            } else {
                LOG_ERROR("Failed to receive SvcAvailableReceived.");
            }
            loop.quit();  // 退出事件循环
        }, Qt::UniqueConnection);

        timer.start(5000);  // 超时时间 5 秒
        loop.exec();         // 开始事件循环，等待超时或接收到服务端响应

        if (timer.isActive()) {
            timer.stop();
            LOG_DEBUG("SvcAvailableReceivedResponse received, proceeding...");

            if (!responseReceived) {
                return false;
            }
        } else {
            LOG_ERROR("SvcAvailableReceived Timeout while waiting for server response!");
            return false;
        }

        return true;
    }

    LOG_ERROR("server is not running!");
    return false;
}
bool WwanClientImpl::TurnonSwitch(bool& setresult) {
    QMutexLocker locker(&functionMutex);

    if (socket->isOpen()) {
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = FUNCTION_SVCTRUNON;
        QJsonDocument jsonDoc(jsonObj);

        socket->write(jsonDoc.toJson());
        socket->flush();

        QEventLoop loop;
        QTimer timer;
        bool responseReceived = false;

        timer.setSingleShot(true);

        QObject::disconnect(this, &WwanClientImpl::TurnonSwitchReceived, nullptr, nullptr);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(this, &WwanClientImpl::TurnonSwitchReceived, this, [this, &loop, &responseReceived,&setresult](bool& opsresult,bool result) {
            if (result) {
                LOG_DEBUG("TurnonSwitchReceived received successfully!");
                setresult = opsresult;
                responseReceived = result; // 标记为收到响应
            } else {
                LOG_ERROR("Failed to receive TurnonSwitchReceived.");
            }
            loop.quit();  // 退出事件循环
        }, Qt::UniqueConnection);

        timer.start(5000);  // 超时时间 5 秒
        loop.exec();         // 开始事件循环，等待超时或接收到服务端响应

        if (timer.isActive()) {
            timer.stop();
            LOG_DEBUG("TurnonSwitchReceived Response received, proceeding...");

            if (!responseReceived) {
                return false;
            }
        } else {
            LOG_ERROR("TurnonSwitchReceived Timeout while waiting for server response!");
            return false;
        }

        return true;
    }

    LOG_ERROR("server is not running!");
    return false;
    // return true;
}

bool WwanClientImpl::TurnoffSwitch(bool& setresult) {
    QMutexLocker locker(&functionMutex);

    if (socket->isOpen()) {
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = FUNCTION_SVCTRUNOFF;
        QJsonDocument jsonDoc(jsonObj);

        socket->write(jsonDoc.toJson());
        socket->flush();

        QEventLoop loop;
        QTimer timer;
        bool responseReceived = false;

        timer.setSingleShot(true);

        QObject::disconnect(this, &WwanClientImpl::TurnoffSwitchReceived, nullptr, nullptr);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(this, &WwanClientImpl::TurnoffSwitchReceived, this, [this, &loop, &responseReceived,&setresult](bool& opsresult,bool result) {
            if (result) {
                LOG_DEBUG("TurnoffSwitchReceived received successfully!");
                setresult = opsresult;
                responseReceived = result; // 标记为收到响应
            } else {
                LOG_ERROR("Failed to receive TurnoffSwitchReceived.");
            }
            loop.quit();  // 退出事件循环
        }, Qt::UniqueConnection);

        timer.start(5000);  // 超时时间 5 秒
        loop.exec();         // 开始事件循环，等待超时或接收到服务端响应

        if (timer.isActive()) {
            timer.stop();
            LOG_DEBUG("TurnoffSwitchReceived Response received, proceeding...");

            if (!responseReceived) {
                return false;
            }
        } else {
            LOG_ERROR("TurnoffSwitchReceived Timeout while waiting for server response!");
            return false;
        }

        return true;
    }

    LOG_ERROR("server is not running!");
    return false;
    // return true;
}

bool WwanClientImpl::QuerySwitchStatus(bool& service_on) {
    QMutexLocker locker(&functionMutex);

    if (socket->isOpen()) {
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = FUNCTION_SVCQEURYSWITCH;
        QJsonDocument jsonDoc(jsonObj);

        socket->write(jsonDoc.toJson());
        socket->flush();

        QEventLoop loop;
        QTimer timer;
        bool responseReceived = false;

        timer.setSingleShot(true);

        QObject::disconnect(this, &WwanClientImpl::QuerySwitchReceived, nullptr,
                            nullptr);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(this, &WwanClientImpl::QuerySwitchReceived, this,
                [this, &loop, &responseReceived, &service_on](bool& opsresult,
                                                              bool status_ok) {
                    Q_UNUSED(opsresult);
                    responseReceived = true;
                    service_on = status_ok;
                    loop.quit();
                },
                Qt::UniqueConnection);

        timer.start(5000);
        loop.exec();

        if (timer.isActive()) {
            timer.stop();
            if (!responseReceived) {
                return false;
            }
        } else {
            LOG_ERROR(
                "QuerySwitchStatus timeout while waiting for server response!");
            return false;
        }

        return true;
    }

    LOG_ERROR("server is not running!");
    return false;
}

bool WwanClientImpl::Init() {
    stopping = false;
    socket = std::make_unique<QLocalSocket>(this);
    connect(socket.get(), &QLocalSocket::connected, this, &WwanClientImpl::onConnected);
    connect(socket.get(), &QLocalSocket::readyRead, this, &WwanClientImpl::onReadyRead);
    connect(socket.get(), &QLocalSocket::disconnected, this, &WwanClientImpl::onDisconnected);
    connect(socket.get(), &QLocalSocket::errorOccurred, this, &WwanClientImpl::onError);
    connectToServerInternal(0);
    if (socket->state() != QLocalSocket::ConnectedState) {
        socket->waitForConnected(3000);
    }
    if (socket->state() != QLocalSocket::ConnectedState) {
        LOG_WARN("Func socket not ready yet, keep retrying every 5s");
        startReconnectTimer();
    }
    return true;
}

bool WwanClientImpl::Deinit() {
    QMutexLocker locker(&functionMutex);
    stopping = true;
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

void WwanClientImpl::onReadyRead() {
    bool ret = false;
    QLocalSocket *socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

    LOG_DEBUG("Received data is :%s", SS(jsonDoc.toJson(QJsonDocument::Indented).toStdString()));
    
    QJsonObject jsonObj = jsonDoc.object();
    QString Type = jsonObj["type"].toString();

    LOG_DEBUG("Received data type:", QS(Type));    
    LOG_DEBUG("Ret is :%d", ret);
    // 检查是否包含关键字 "indication"
    if (Type == "ind") {
        
    } else if (Type == "resp"){
        bool finalresult = deserializeFromJson(data, ret);
        //正常的返回消息需要处理
        qint32 functionId = jsonObj["function_id"].toInt();
        switch (functionId) {
            case FuncIdList::FUNCTION_SVCAVAILABLE:
                emit SvcAvailableReceived(finalresult,ret);
                break;
            case FuncIdList::FUNCTION_SVCTRUNOFF:
                emit TurnoffSwitchReceived(finalresult,ret);
                break;
            case FuncIdList::FUNCTION_SVCTRUNON:
                emit TurnonSwitchReceived(finalresult,ret);
                break;
            case FuncIdList::FUNCTION_SVCQEURYSWITCH:
                emit QuerySwitchReceived(finalresult, ret);
                break;
            default:
                LOG_ERROR("error invalid func id:%d", functionId);
                return;
        }

    } else {
        LOG_ERROR("invaild type of mesg from Server!");
    }
}

/*
void WwanClientImpl::connectToServer() {
    LOG_DEBUG("[%s]: enter", __func__);
    connect(socket.get(), &QLocalSocket::connected, this, &WwanClientImpl::onConnected);
    connect(socket.get(), &QLocalSocket::readyRead, this, &WwanClientImpl::onReadyRead);
    connect(socket.get(), &QLocalSocket::disconnected, this, &WwanClientImpl::onDisconnected);
    connect(socket.get(), &QLocalSocket::errorOccurred, this, &WwanClientImpl::onError);
    connect(this, &WwanClientImpl::indicationReceived, this, &WwanClientImpl::onIndicationReceived);
    connectToServerInternal(0);  // 从 0 开始尝试
}*/
void WwanClientImpl::connectToServerInternal(int attempt) {
    LOG_DEBUG("Attempt %d to connect", attempt);
    if (stopping || !socket) {
        return;
    }
    if (socket->state() == QLocalSocket::ConnectedState
        || socket->state() == QLocalSocket::ConnectingState) {
        return;
    }
    if (socket->state() != QLocalSocket::UnconnectedState) {
        socket->abort();
    }
    socket->connectToServer(SOCKET_NAME);
    LOG_DEBUG("[%s]:exit", __func__);
}

void WwanClientImpl::startReconnectTimer() {
    if (stopping) {
        return;
    }
    if (!retryTimer) {
        retryTimer = new QTimer(this);
        retryTimer->setInterval(retryIntervalMs);
        connect(retryTimer, &QTimer::timeout, this, &WwanClientImpl::retryConnection);
    }
    if (!retryTimer->isActive()) {
        retryTimer->start();
    }
}

void WwanClientImpl::stopReconnectTimer() {
    if (retryTimer) {
        retryTimer->stop();
    }
}

void WwanClientImpl::retryConnection() {
    if (stopping) {
        stopReconnectTimer();
        return;
    }
    retryCount++;
    LOG_DEBUG("Func socket reconnect attempt %d", retryCount);
    connectToServerInternal(retryCount);
}

void  WwanClientImpl::onDisconnected() {
    if (stopping) {
        return;
    }
    LOG_WARN("Disconnected from Func service, will keep retrying");
    startReconnectTimer();
}

void WwanClientImpl::onError(QLocalSocket::LocalSocketError socketError) {
    LOG_DEBUG("Socket error:%d", socketError);
    if (stopping) {
        return;
    }
    if (socket && socket->state() == QLocalSocket::ConnectedState) {
        return;
    }
    startReconnectTimer();
}

bool WwanClientImpl::deserializeFromJson(const QByteArray& jsonData, bool &ret) {
    bool result;

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    QJsonObject jsonObject = doc.object();
    result = jsonObject["finalresult"].toString().toInt();
    ret = (jsonObject["status"].toString() == "ok")?true: false;    
    return result;
}

void WwanClientImpl::onConnected() {
    retryCount = 0;
    stopReconnectTimer();
    LOG_DEBUG("soceket connected: state(%d)", socket->state());
}
}
//#include "wwan_func.moc"


#include "wwan_func_tool.hpp"
#include "wwan_impl_tool.h"
#include "log.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QMetaObject>
#include <QThread>
#include <utility>
#include <chrono>
#include <thread>
#include <type_traits>

using namespace afal::log;
#define SOCKET_NAME "wwan-func-service-tool"

namespace wwansvctool {

namespace {

template <typename Fn>
auto invokeOnWorker(WwanClientImpl* instance, Fn&& fn) -> decltype(fn()) {
    using Ret = decltype(fn());
    if (!instance) {
        if constexpr (std::is_same_v<Ret, void>) {
            return;
        } else {
            return Ret{};
        }
    }
    if (QThread::currentThread() == instance->thread()) {
        return fn();
    }
    if constexpr (std::is_same_v<Ret, void>) {
        QMetaObject::invokeMethod(instance, std::forward<Fn>(fn),
                                  Qt::BlockingQueuedConnection);
    } else {
        Ret ret{};
        QMetaObject::invokeMethod(
            instance,
            [&]() { ret = fn(); },
            Qt::BlockingQueuedConnection);
        return ret;
    }
}

bool IsTerminalFlashProgressSuccess(const QString& message) {
    return !message.contains(QStringLiteral("fail"), Qt::CaseInsensitive) &&
           !message.contains(QStringLiteral("error"), Qt::CaseInsensitive);
}

}  // namespace

WwanFuncSvcClientTool::WwanFuncSvcClientTool(QObject* parent) : QObject(parent) {
    Instance = nullptr;
    worker_thread_ = nullptr;
    LOG_DEBUG("WwanClientFuncClient() called!");
}

bool WwanFuncSvcClientTool::Init() {
    if (Instance) {
        return true;
    }
   
    worker_thread_ = new QThread();
    Instance = new WwanClientImpl();
    Instance->moveToThread(worker_thread_);
    worker_thread_->start();

    bool ok = false;
    QMetaObject::invokeMethod(
        Instance,
        [this, &ok]() { ok = Instance->Init(); },
        Qt::BlockingQueuedConnection);
    LOG_INFO("WwanFuncSvcClientTool Init on worker thread, ok=%d", (int)ok);
    return ok;
}

bool WwanFuncSvcClientTool::Deinit() {
    if (!Instance) {
        return true;
    }
    bool ok = invokeOnWorker(Instance, [this]() { return Instance->Deinit(); });
    if (worker_thread_) {
        worker_thread_->quit();
        worker_thread_->wait(5000);
        delete worker_thread_;
        worker_thread_ = nullptr;
    }
    delete Instance;
    Instance = nullptr;
    return ok;
}

bool WwanFuncSvcClientTool::TriggerFlash(bool& flashresult, QString& filePath,
                                         /* QString deviceName,*/ int erasemode,
                                         int resetType) {
    if (!Instance) {
        return false;
    }
    bool local_result = false;
    bool ok = false;
    QString path = filePath;
    ok = invokeOnWorker(Instance, [this, &local_result, &path, erasemode, resetType]() {
        return Instance->TriggerFlash(local_result, path, erasemode, resetType);
    });
    flashresult = local_result;
    return ok;
}

void WwanFuncSvcClientTool::ResetFlashWait() {
    if (Instance) {
        Instance->ResetFlashWait();
    }
}

void WwanFuncSvcClientTool::AbortFlashWait() {
    if (Instance) {
        Instance->AbortFlashWait();
    }
}

void WwanFuncSvcClientTool::ForceCompleteFlashWait(bool success) {
    if (Instance) {
        Instance->ForceCompleteFlashWait(success);
    }
}

bool WwanFuncSvcClientTool::SvcAvailable(bool& setresult) {
    if (!Instance) {
        return false;
    }
    bool local = false;
    bool ok = invokeOnWorker(Instance, [this, &local]() {
        return Instance->SvcAvailable(local);
    });
    setresult = local;
    return ok;
}

bool WwanFuncSvcClientTool::TurnoffSwitch(bool& setresult) {
    if (!Instance) {
        return false;
    }
    bool local = false;
    bool ok = invokeOnWorker(Instance, [this, &local]() {
        return Instance->TurnoffSwitch(local);
    });
    setresult = local;
    return ok;
}

bool WwanFuncSvcClientTool::TurnonSwitch(bool& setresult) {
    if (!Instance) {
        return false;
    }
    bool local = false;
    bool ok = invokeOnWorker(Instance, [this, &local]() {
        return Instance->TurnonSwitch(local);
    });
    setresult = local;
    return ok;
}

bool WwanFuncSvcClientTool::QuerySwitchStatus(bool& queryresult) {
    if (!Instance) {
        return false;
    }
    bool local = false;
    bool ok = invokeOnWorker(Instance, [this, &local]() {
        return Instance->QuerySwitchStatus(local);
    });
    queryresult = local;
    return ok;
}

bool WwanFuncSvcClientTool::SubscribeFlashProgressEvent(callback handleEvent) {
    if (!Instance) {
        return false;
}
    return invokeOnWorker(Instance, [this, handleEvent]() {
        return Instance->SubscribeFlashProgressEvent(handleEvent);
    });
}

void WwanClientImpl::ResetFlashWait() {
    flash_wait_completed_.store(false);
    flash_wait_success_.store(false);
    abort_flash_wait_.store(false);
}
void WwanClientImpl::AbortFlashWait() {
    abort_flash_wait_.store(true);
    ForceCompleteFlashWait(false);
}
void WwanClientImpl::ForceCompleteFlashWait(bool success) {
    if (flash_wait_completed_.exchange(true)) {
        return;
    }
    flash_wait_success_.store(success);
    LOG_WARN("ForceCompleteFlashWait success=%d", (int)success);
    flash_wait_cv_.notify_all();
    if (flash_wait_loop_) {
        QMetaObject::invokeMethod(flash_wait_loop_, &QEventLoop::quit,
                                  Qt::QueuedConnection);
    }
}

WwanFuncSvcClientTool::~WwanFuncSvcClientTool() {
    Deinit();
    LOG_DEBUG("~WwanClient() called!");
}

WwanClientImpl::WwanClientImpl(QObject* parent)
    : QObject(parent),
      socket(nullptr),
      func(),
      retryTimer(nullptr),
      retryCount(0),
      stopping(false),
      retryIntervalMs(5000) {
    qRegisterMetaType<flashProgress>("flashProgress");
}

// Plan A: 本函数只在 worker_thread_（socket 线程）上执行；loop 里不会进 UI/设备槽
bool WwanClientImpl::TriggerFlash(bool& flashresult, QString& filePath, int erasemode,
                                  int resetType) {
    ResetFlashWait();
    flashresult = false;
    if (!socket || !socket->isOpen()) {
        LOG_ERROR("server is not running!");
        return false;
    }
    {
        QMutexLocker locker(&functionMutex);
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = FUNCTION_FLASH;
        jsonObj["filePath"] = filePath;
        jsonObj["eraseMode"] = erasemode;
        // jsonObj["deviceName"] = deviceName;
        jsonObj["resetType"] = resetType;
        socket->write(QJsonDocument(jsonObj).toJson());
        socket->flush();
    }
    QEventLoop loop;
    flash_wait_loop_ = &loop;
    struct LoopGuard {
        QEventLoop*& p;
        ~LoopGuard() { p = nullptr; }
    } guard{flash_wait_loop_};
    QObject::disconnect(this, &WwanClientImpl::flashResultReceived, nullptr, nullptr);
    const QMetaObject::Connection conn = connect(
        this, &WwanClientImpl::flashResultReceived, this,
        [this](bool& opsresult, bool transportOk) {
            LOG_INFO("flashResultReceived: ops=%d, transportOk=%d",
                     (int)opsresult, (int)transportOk);
            ForceCompleteFlashWait(opsresult && transportOk);
        },
        Qt::DirectConnection);
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, [this]() {
        LOG_ERROR("TriggerFlash timeout");
        // 超时也走统一出口，避免只 quit 却 completed=false 语义不清
        ForceCompleteFlashWait(false);
    }, Qt::QueuedConnection);
    // Keep aligned with WwanLocalSocketServer::kLibFlashTimeoutMs.
    constexpr int kTimeoutMs = 300 * 1000 * 5 + 60 * 1000;
    timer.start(kTimeoutMs);
    LOG_INFO("TriggerFlash loop.exec enter (socket worker), timeout=%dms", kTimeoutMs);
    loop.exec();
    LOG_INFO("TriggerFlash loop.exec leave, completed=%d, flashresult=%d",
             (int)flash_wait_completed_.load(), (int)flash_wait_success_.load());
    QObject::disconnect(conn);
    if (timer.isActive()) {
        timer.stop();
    }
    const bool completed = flash_wait_completed_.load();
    flashresult = flash_wait_success_.load();
    ResetFlashWait();
    return completed && flashresult;
}

bool WwanClientImpl::SvcAvailable(bool& setresult) {
    QMutexLocker locker(&functionMutex);

    if (socket->isOpen()) {
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = FUNCTION_SVCAVAILABLE_TOOL;
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
                responseReceived = result; 
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
        jsonObj["function_id"] = FUNCTION_SVCTRUNON_TOOL;
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
}


bool WwanClientImpl::TurnoffSwitch(bool& setresult) {
    QMutexLocker locker(&functionMutex);

    if (socket->isOpen()) {
        QJsonObject jsonObj;
        jsonObj["type"] = "req";
        jsonObj["function_id"] = FUNCTION_SVCTRUNOFF_TOOL;
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

bool WwanClientImpl::QuerySwitchStatus(bool& queryresult) {
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

        QObject::disconnect(this, &WwanClientImpl::QuerySwitchReceived, nullptr, nullptr);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(this, &WwanClientImpl::QuerySwitchReceived, this, [this, &loop, &responseReceived,&queryresult](bool& opsresult,bool status_ok) {
            Q_UNUSED(opsresult);
            responseReceived = true;
            queryresult = status_ok;
            loop.quit();
        }, Qt::UniqueConnection);

        timer.start(5000);  // 超时时间 5 秒
        loop.exec();         // 开始事件循环，等待超时或接收到服务端响应

        if (timer.isActive()) {
            timer.stop();
            LOG_DEBUG("QuerySwitchReceived Response received, proceeding...");

            if (!responseReceived) {
                return false;
            }
        } else {
            LOG_ERROR("QuerySwitchReceived Timeout while waiting for server response!");
            return false;
        }

        return true;
    }

    LOG_ERROR("server is not running!");
    return false;
}

bool WwanClientImpl::SubscribeFlashProgressEvent(callback handleEvent) {
    func = handleEvent;
    QMutexLocker locker(&functionMutex);

    if (!socket || !socket->isOpen()) {
        LOG_WARN("Flash progress callback stored, socket not open yet");
        return true;
    }

    LOG_DEBUG("[%s]:enter", __func__);
    QJsonObject jsonObj;
    jsonObj["type"] = "req";
    jsonObj["function_id"] = FUNCTION_EVENT;

    QJsonDocument jsonDoc(jsonObj);

    LOG_DEBUG(",will send:%s", SS(jsonDoc.toJson(QJsonDocument::Indented).toStdString()));

    socket->write(jsonDoc.toJson());
    socket->flush();
    QEventLoop loop;
    QTimer timer;
    bool responseReceived = false;
    timer.setSingleShot(true);

    QObject::disconnect(this, &WwanClientImpl::SubscribeFlashProgressEventRecived, nullptr, nullptr);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &WwanClientImpl::SubscribeFlashProgressEventRecived, this, [this, &loop, &responseReceived](bool result) {
        if (result) {
            LOG_DEBUG("SubscribeFlashProgressEventRecived  received successfully!");
            responseReceived = result;
        } else {
            LOG_ERROR("Failed to register signals.");
        }
        loop.quit();
    }, Qt::UniqueConnection);

    timer.start(5000);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
        LOG_DEBUG("SubscribeFlashProgressEventRecived Response received, proceeding...");
        if (!responseReceived) {
            return false;
        }
    } else {
        LOG_ERROR("Timeout while waiting for server response!");
        return false;
    }

    return true;
}

flashProgress WwanClientImpl::abstractFromJson(const QByteArray& jsonData) {
    flashProgress result;

    // 解析 JSON 数据
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    QJsonObject jsonObject = doc.object();
    result.progress = jsonObject.value("progress").toInt();
    result.flashMessage = jsonObject.value("flashMessage").toString(); 
    return result;
}

bool WwanClientImpl::Init() {
    stopping = false;
    socket = std::make_unique<QLocalSocket>(this);
    connect(socket.get(), &QLocalSocket::connected, this, &WwanClientImpl::onConnected);
    connect(socket.get(), &QLocalSocket::readyRead, this, &WwanClientImpl::onReadyRead);
    connect(socket.get(), &QLocalSocket::disconnected, this, &WwanClientImpl::onDisconnected);
    connect(socket.get(), &QLocalSocket::errorOccurred, this, &WwanClientImpl::onError);
    connect(this, &WwanClientImpl::indicationReceived, this, &WwanClientImpl::onIndicationReceived);
    connectToServerInternal(0);
    if (socket->state() != QLocalSocket::ConnectedState) {
        socket->waitForConnected(3000);
    }
    if (socket->state() != QLocalSocket::ConnectedState) {
        LOG_WARN("Func tool socket not ready yet, keep retrying every 5s");
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
#if 0
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
        flashProgress flashprogressinfo = abstractFromJson(data);
        emit indicationReceived(flashprogressinfo);
    } else if (Type == "resp"){
        bool finalresult = deserializeFromJson(data, ret);
        //正常的返回消息需要处理
        qint32 functionId = jsonObj["function_id"].toInt();
        switch (functionId) {
            case FuncIdList::FUNCTION_FLASH:
                emit flashResultReceived(finalresult,ret);
                break;
            case FuncIdList::FUNCTION_SVCAVAILABLE_TOOL:
                emit SvcAvailableReceived(finalresult,ret);
                break; 
            case FuncIdList::FUNCTION_SVCTRUNOFF_TOOL:
                emit TurnoffSwitchReceived(finalresult,ret);
                break;
            case FuncIdList::FUNCTION_SVCTRUNON_TOOL:
                emit TurnonSwitchReceived(finalresult,ret);
                break;
            case FuncIdList::FUNCTION_EVENT:
                emit SubscribeFlashProgressEventRecived(true);
                break;
            default:
                LOG_ERROR("error invalid func id:%d", functionId);
                return;
        }

    } else {
        LOG_ERROR("invaild type of mesg from Server!");
    }
}
#endif
void WwanClientImpl::onReadyRead() {
    QLocalSocket* localSocket = qobject_cast<QLocalSocket*>(sender());
    if (!localSocket) {
        return;
    }
    recv_buffer_.append(localSocket->readAll());
    LOG_DEBUG("onReadyRead append, buffer size=%d", recv_buffer_.size());
    while (true) {
        int start = recv_buffer_.indexOf('{');
        if (start < 0) {
            recv_buffer_.clear();
            break;
        }
        if (start > 0) {
            LOG_WARN("onReadyRead drop non-json prefix, len=%d", start);
            recv_buffer_.remove(0, start);
        }
        int depth = 0;
        int end = -1;
        bool inString = false;
        bool escape = false;
        for (int i = 0; i < recv_buffer_.size(); ++i) {
            const char c = recv_buffer_.at(i);
            if (inString) {
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    inString = false;
                }
                continue;
            }
            if (c == '"') {
                inString = true;
                continue;
            }
            if (c == '{') {
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0) {
                    end = i;
                    break;
                }
            }
        }
        if (end < 0) {
            LOG_DEBUG("onReadyRead incomplete json, wait more, size=%d",
                      recv_buffer_.size());
            break;
        }
        const QByteArray one = recv_buffer_.left(end + 1);
        recv_buffer_.remove(0, end + 1);
        handleOneSocketMessage(one);
    }
}
void WwanClientImpl::handleOneSocketMessage(const QByteArray& data) {
    bool ret = false;
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        LOG_ERROR("onReadyRead invalid json: %s, raw=%s",
                  QS(parseError.errorString()),
                  SS(data.left(256).toStdString()));
        return;
    }
    LOG_DEBUG("Received data is :%s",
              SS(jsonDoc.toJson(QJsonDocument::Indented).toStdString()));
    QJsonObject jsonObj = jsonDoc.object();
    QString type = jsonObj["type"].toString();
    LOG_DEBUG("Received data type: %s", QS(type));
    if (type == "ind") {
        flashProgress flashprogressinfo = abstractFromJson(data);
        emit indicationReceived(flashprogressinfo);
        return;
    }
    if (type == "resp") {
        bool finalresult = deserializeFromJson(data, ret);
        qint32 functionId = jsonObj["function_id"].toInt();
        LOG_INFO("Received resp function_id=%d, status_ok=%d, finalresult=%d",
                 functionId, ret, finalresult);
        switch (functionId) {
            case FuncIdList::FUNCTION_FLASH:
                emit flashResultReceived(finalresult, ret);
                break;
            case FuncIdList::FUNCTION_SVCAVAILABLE_TOOL:
                emit SvcAvailableReceived(finalresult, ret);
                break;
            case FuncIdList::FUNCTION_SVCTRUNOFF_TOOL:
                emit TurnoffSwitchReceived(finalresult, ret);
                break;
            case FuncIdList::FUNCTION_SVCTRUNON_TOOL:
                emit TurnonSwitchReceived(finalresult, ret);
                break;
            case FuncIdList::FUNCTION_SVCQEURYSWITCH:
                emit QuerySwitchReceived(finalresult, ret);
                break;
            case FuncIdList::FUNCTION_EVENT:
                emit SubscribeFlashProgressEventRecived(true);
                break;
            default:
                LOG_ERROR("error invalid func id:%d", functionId);
                break;
        }
        return;
    }
    LOG_ERROR("invalid type of mesg from Server: %s", QS(type));
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

void WwanClientImpl::refreshAfterReconnect() {
    if (!func) {
        return;
    }
    if (!socket || !socket->isOpen()) {
        return;
    }
    LOG_INFO("Reconnected to Func service, re-subscribe flash progress");
    QJsonObject jsonObj;
    jsonObj["type"] = "req";
    jsonObj["function_id"] = FUNCTION_EVENT;
    socket->write(QJsonDocument(jsonObj).toJson());
    socket->flush();
}

void WwanClientImpl::retryConnection() {
    if (stopping) {
        stopReconnectTimer();
        return;
    }
    retryCount++;
    LOG_DEBUG("Func tool socket reconnect attempt %d", retryCount);
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
void  WwanClientImpl::onIndicationReceived(const flashProgress &message) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    LOG_DEBUG("[timestamp]:%s,->FlashProgress indication received progress=%d",
              QS(timestamp), message.progress);

    if (message.progress >= 99) {
        ForceCompleteFlashWait(IsTerminalFlashProgressSuccess(message.flashMessage));
    }
    if (func) {
        func(message);
    } else {
        LOG_ERROR("func is not set! Crash avoided.");
    }
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
    refreshAfterReconnect();
}
}
//#include "wwan_func.moc"


#ifndef __WWAN_FUNC_LIB_H__
#define __WWAN_FUNC_LIB_H__

#include <QCoreApplication>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QSemaphore>
#include <QEventLoop>
#include <QTimer>
#include <functional>
#include <memory>

namespace wwansvc {

class WwanClientImpl;
class WwanFuncSvcClient : public QObject {
Q_OBJECT

public:
    explicit WwanFuncSvcClient(QObject *parent = nullptr);
    WwanFuncSvcClient(const WwanFuncSvcClient&) = delete;
    WwanFuncSvcClient(WwanFuncSvcClient&&) = delete;
    WwanFuncSvcClient& operator=(const WwanFuncSvcClient&) = delete;
    WwanFuncSvcClient& operator=(WwanFuncSvcClient&&) = delete;
    ~WwanFuncSvcClient() override;
    bool Init();
    bool Deinit();
    bool SvcAvailable(bool& setresult);
    bool TurnoffSwitch(bool& setresult);
    bool TurnonSwitch(bool& setresult);
    bool QuerySwitchStatus(bool& service_on);
private:
    WwanClientImpl *Instance;
};
}
#endif
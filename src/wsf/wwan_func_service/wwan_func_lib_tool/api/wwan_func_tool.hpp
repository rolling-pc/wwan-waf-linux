#ifndef __WWAN_FUNC_LIB_TOOL_H__
#define __WWAN_FUNC_LIB_TOOL_H__

#include <QCoreApplication>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QSemaphore>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <functional>
#include <memory>

namespace wwansvctool {

struct flashProgress {
    int progress;
    QString flashMessage; 
};

using callback = std::function<void (const flashProgress &)>;

class WwanClientImpl;
class WwanFuncSvcClientTool : public QObject {
Q_OBJECT

public:
    explicit WwanFuncSvcClientTool(QObject *parent = nullptr);
    WwanFuncSvcClientTool(const WwanFuncSvcClientTool&) = delete;
    WwanFuncSvcClientTool(WwanFuncSvcClientTool&&) = delete;
    WwanFuncSvcClientTool& operator=(const WwanFuncSvcClientTool&) = delete;
    WwanFuncSvcClientTool& operator=(WwanFuncSvcClientTool&&) = delete;
    ~WwanFuncSvcClientTool() override;
    bool Init();
    bool Deinit();
    bool TriggerFlash(bool& flashresult,QString& filePath, /*QString deviceName, */int erasemode=0, int resetType=0); 
    bool SubscribeFlashProgressEvent (callback handleEvent);
    bool SvcAvailable(bool& setresult);
    bool TurnoffSwitch(bool& setresult);
    bool TurnonSwitch(bool& setresult);
    bool QuerySwitchStatus(bool& queryresult); 
    void ResetFlashWait();
    void AbortFlashWait();
    void ForceCompleteFlashWait(bool success);

  private:
    WwanClientImpl* Instance{nullptr};
    QThread* worker_thread_{nullptr};
};
}
#endif
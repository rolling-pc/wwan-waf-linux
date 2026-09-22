#ifndef __WWAN_FUNC_IMPL_H__
#define __WWAN_FUNC_IMPL_H__

#include "wwan_func_tool.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>

class QEventLoop;
namespace wwansvctool {

enum resetType { Default = 0, PLDR, FLDR, ResetMbim };

enum eraseMode { noErase = 0, eraseAll };

enum FuncIdList {
    FUNCTION_FLASH = 1001,
    FUNCTION_SVCQEURYSWITCH = 1005,
    FUNCTION_SVCTRUNOFF_TOOL = 1006,
    FUNCTION_SVCTRUNON_TOOL = 1007,
    FUNCTION_SVCAVAILABLE_TOOL = 1008,
    FUNCTION_EVENT = 1009
};

class WwanClientImpl : public QObject {
    Q_OBJECT
  public:
    explicit WwanClientImpl(QObject* parent = nullptr);
    bool Init();
    bool Deinit();
    bool TriggerFlash(bool& flashresult, QString& filePath, /*QString deviceName,*/ int erasemode,
                      int resetType);
    bool SubscribeFlashProgressEvent(callback handleEvent);
    bool SvcAvailable(bool& setresult);
    bool TurnoffSwitch(bool& setresult);
    bool TurnonSwitch(bool& setresult);
    bool QuerySwitchStatus(bool& queryresult);     

    void ResetFlashWait();
    void AbortFlashWait();
    void ForceCompleteFlashWait(bool success);
  signals:
    void flashResultReceived(bool& flashresult, bool result);
    void indicationReceived(const flashProgress& message);
    void SubscribeFlashProgressEventRecived(bool result);
    void SvcAvailableReceived(bool& setresult, bool result);
    void TurnoffSwitchReceived(bool& setresult, bool result);
    void TurnonSwitchReceived(bool& setresult, bool result);
    void QuerySwitchReceived(bool& queryresult, bool result);

  private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QLocalSocket::LocalSocketError socketError);
    void onIndicationReceived(const flashProgress& message);

  private:
    void connectToServer();
    flashProgress abstractFromJson(const QByteArray& jsonData); // Ensure correct signature
    bool deserializeFromJson(const QByteArray& jsonData, bool& ret);
    void retryConnection();
    void connectToServerInternal(int attempt);
    void handleOneSocketMessage(const QByteArray& data);
    void startReconnectTimer();
    void stopReconnectTimer();
    void refreshAfterReconnect();

    QMutex functionMutex;
    std::unique_ptr<QLocalSocket> socket;
    callback func; // callback is a function pointer
    QByteArray recv_buffer_;

    QTimer* retryTimer;
    int retryCount;
    bool stopping;
    int retryIntervalMs;

    std::atomic<bool> flash_wait_completed_{false};
    std::atomic<bool> flash_wait_success_{false};
    std::atomic<bool> abort_flash_wait_{false};
    std::mutex flash_wait_mutex_;
    std::condition_variable flash_wait_cv_;
    QEventLoop* flash_wait_loop_{nullptr};
};
} // namespace wwansvctool
#endif //__WWAN_FUNC_IMPL_H__
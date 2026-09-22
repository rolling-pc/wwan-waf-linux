#ifndef __WWAN_FUNC_IMPL_H__
#define __WWAN_FUNC_IMPL_H__

#include "wwan_func.hpp"

namespace wwansvc {

  enum FuncIdList {
    FUNCTION_SVCAVAILABLE = 1002,
    FUNCTION_SVCTRUNOFF = 1003,
    FUNCTION_SVCTRUNON = 1004,
    FUNCTION_SVCQEURYSWITCH = 1005,
    FUNC_ID_UNKNOWN = 0xEFFF
};

class WwanClientImpl : public QObject {
    Q_OBJECT
  public:
    explicit WwanClientImpl(QObject* parent = nullptr);
    bool Init();
    bool Deinit();
    bool SvcAvailable(bool& setresult);
    bool TurnoffSwitch(bool& setresult);
    bool TurnonSwitch(bool& setresult);
    bool QuerySwitchStatus(bool& service_on);

  signals:
    void TurnoffSwitchReceived(bool& setresult, bool result);
    void TurnonSwitchReceived(bool& setresult, bool result);
    void SvcAvailableReceived(bool& setresult, bool result);
    void QuerySwitchReceived(bool& setresult, bool result);
  private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QLocalSocket::LocalSocketError socketError);

  private:
    void connectToServer();
    bool deserializeFromJson(const QByteArray& jsonData, bool& ret);
    void retryConnection();
    void connectToServerInternal(int attempt);
    void startReconnectTimer();
    void stopReconnectTimer();

    QMutex functionMutex;
    std::unique_ptr<QLocalSocket> socket;

    QTimer* retryTimer;
    int retryCount;
    bool stopping;
    int retryIntervalMs;
};
} // namespace wwansvc
#endif //__WWAN_FUNC_IMPL_H__
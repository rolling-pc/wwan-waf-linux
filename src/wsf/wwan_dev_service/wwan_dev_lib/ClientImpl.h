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

#ifndef WAF_CLIENTIMPL_H
#define WAF_CLIENTIMPL_H
#include "Client.h"
#include "version.h"
#include <QQueue>
#include <QRecursiveMutex>
class DevClientImpl : public QObject{
    Q_OBJECT
  public:
    explicit DevClientImpl(QObject *parent = nullptr);
    bool Init();
    bool Deinit();
    bool GetDeviceinfo(deviceInfo &info, bool forceRefresh = false);
    bool ResetDevice();
    bool SubscribeDeviceEvent (SignalType type, callback handleEvent);
  signals:
    void indicationReceived(const deviceInfo &message);
    void deviceinfoRecived(const deviceInfo& devinfo, bool result);
    void resetDeviceRecived(bool result);
    void SubscribeDeviceEventRecived(bool result);

  private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QLocalSocket::LocalSocketError socketError);
    void onIndicationReceived(const deviceInfo &message);

  private:
    void connectToServer();
    void ensureSocket();
    void startReconnectTimer();
    void stopReconnectTimer();
    void refreshAfterReconnect();
    void registerForSignals();
    deviceInfo deserializeFromJson(const QByteArray& jsonData, bool &ret);
    void retryConnection();
    void grabFromJson(const QByteArray& jsonData, bool &ret);
    void dispatchJsonMessage(const QByteArray& msg);
    void drainSocketMessages();
    QMutex functionMutex;
    QRecursiveMutex ioMutex;
    std::unique_ptr<QLocalSocket> socket;
    QMap<SignalType, callback> func;
    QByteArray rxBuffer;

    QTimer *retryTimer;
    int retryCount;
    bool stopping;
};
#endif // WAF_CLIENTIMPL_H

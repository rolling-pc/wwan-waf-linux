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

#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <QCoreApplication>
#include <QLocalSocket>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QSemaphore>
#include <QEventLoop>
#include <QTimer>
#include <memory>
#include "Device.h"

using namespace afal::device;


class DevClientImpl;
class DevClient : public QObject {
Q_OBJECT

public:
    explicit DevClient(QObject *parent = nullptr);
    ~DevClient();
    bool Init();
    bool Deinit();
    bool GetDeviceinfo(deviceInfo &info, bool forceRefresh = false);
    bool ResetDevice();
    bool SubscribeDeviceEvent (SignalType type, callback handleEvent);

private:
    DevClientImpl *d;
};

#endif
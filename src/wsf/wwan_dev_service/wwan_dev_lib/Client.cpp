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
#include <QWaitCondition>

DevClient::DevClient(QObject *parent) : QObject(parent) {
    d = nullptr;
    LOG_DEBUG("DevClient() called: Version %s", WWAN_DEV_LIB_VERSION_STRING);
}

bool DevClient::Init() {
    d = new DevClientImpl();
    return d->Init();
}

bool DevClient::Deinit() {
    return d->Deinit();
}

bool DevClient::GetDeviceinfo(deviceInfo &info, bool forceRefresh) {
    return d->GetDeviceinfo(info, forceRefresh);
}

bool DevClient::ResetDevice() {
    return d->ResetDevice();
}

bool DevClient::SubscribeDeviceEvent (SignalType type, callback handleEvent) {
    return d->SubscribeDeviceEvent(type, handleEvent);
}

DevClient::~DevClient() {
    //LOG_DEBUG("~DevClient() called!");
}
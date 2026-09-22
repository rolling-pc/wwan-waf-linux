/**
 * Copyright (C) 2024 Rolling Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 * @file wwan_func_main.cpp
 * @author xxx@rolling.com (xxx)
 * @brief
 * @version 1.0
 * @date 2024-08-xx
 *
 **/

#include <iostream>
#include "mbim.hpp"
#include "version.h"
#include "log.hpp"
#include <QtCore>
#include <QCoreApplication>
#include <thread>

using namespace afal::mbim;
using namespace afal::log;

#define ERR           -1
#define OK            0

// temp enable macro cause not the official compile environment
// #define _IS_WINDOWS_ 1

#ifdef _IS_WINDOWS_
#include <windows.h>
#endif

class MyRadioWatcher : public CellularRadioWatcher {
public:
    MyRadioWatcher(int id) {
        owner_id = id;
    }

    void notify(RadioData RadioNotifyData) override {
        LOG_ERROR("enter %s!\n", __func__);
        // all func's notify function will be executed on QT's APP mainloop,
        // So do NOT sleep or change input param in any notify function!
        LOG_INFO("hw state:%d\n", RadioNotifyData.hw_state);
        LOG_INFO("sw state:%d\n", RadioNotifyData.sw_state);
        return;
    }
};

class MySimWatcher : public CellularSimWatcher {
public:
    MySimWatcher(int id) {
        owner_id = id;
    }

    void notify(SimData SimNotifyData) override {
        LOG_ERROR("enter %s!\n", __func__);
        // all func's notify function will be executed on QT's APP mainloop,
        // So do NOT sleep or change input param in any notify function!
        LOG_INFO("ready_state: %d\n",  SimNotifyData.ready_state);
        LOG_INFO("imsi: %s\n",         SimNotifyData.imsi.toUtf8().data());
        LOG_INFO("iccid: %s\n",        SimNotifyData.iccid.toUtf8().data());
        LOG_INFO("local_mccmnc: %s\n", SimNotifyData.local_mccmnc.toUtf8().data());
        return;
    }
};

class MySlotWatcher : public CellularSlotWatcher {
public:
    MySlotWatcher(int id) {
        owner_id = id;
    }

    void notify(SlotData SlotNotifyData) override {
        LOG_ERROR("enter slot %s!\n", __func__);
        // all func's notify function will be executed on QT's APP mainloop,
        // So do NOT sleep or change input param in any notify function!
        LOG_INFO("ValidSlotNum: %d\n",     SlotNotifyData.ValidSlotNum);
        LOG_INFO("ValidExecutorNum: %d\n", SlotNotifyData.ValidExecutorNum);
        LOG_INFO("CurrentSlotIndex: %d\n", SlotNotifyData.CurrentSlotIndex);
        LOG_INFO("slot1: %d\n",            SlotNotifyData.slot_state[0]);
        LOG_INFO("slot2: %d\n",            SlotNotifyData.slot_state[1]);
        return;
    }
};

class MyNetworkWatcher : public CellularNetworkWatcher {
public:
    MyNetworkWatcher(int id) {
        owner_id = id;
    }

    void notify(NetworkData NetworkNotifyData) override {
        LOG_ERROR("enter network %s!\n", __func__);
        // all func's notify function will be executed on QT's APP mainloop,
        // So do NOT sleep or change input param in any notify function!
        LOG_INFO("RegisterState: %d\n", NetworkNotifyData.RegisterState);
        LOG_INFO("rssi: %d\n",          NetworkNotifyData.rssi);
        LOG_INFO("Roam mccmnc: %s\n",   NetworkNotifyData.RoamMccmnc.toUtf8().data());
        return;
    }
};

int mbimtest2() {
    int ret = -1;
    // get MbimDevice instance.
    afal::mbim::MbimDevice& mbimDevice = afal::mbim::MbimDevice::getInstance();

    QString portName = "cdc-wdm0";
    if (mbimDevice.init(portName) != 0) {
        LOG_ERROR("MbimDevice init failed!\n");
        return -1;
    }

    const int BEFORE_FIRST_TEST_WAIT_TIME = 5;
#if _IS_LINUX_
    std::chrono::seconds sleep_duration1(BEFORE_FIRST_TEST_WAIT_TIME);
    std::this_thread::sleep_for(sleep_duration1);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif

    std::shared_ptr<MyRadioWatcher> myRadioWatcher = std::make_shared<MyRadioWatcher>(1);
    ret = mbimDevice.AfalMbimRegisterNotification(myRadioWatcher);
    if (ret != 0) {
        LOG_ERROR("register event failed!\n");
        return -1;
    }
    LOG_INFO("register radio watcher succeed!\n");

    std::shared_ptr<MySimWatcher> mySimWatcher = std::make_shared<MySimWatcher>(1);
    ret = mbimDevice.AfalMbimRegisterNotification(mySimWatcher);
    if (ret != 0) {
        LOG_ERROR("register event failed!\n");
        return -1;
    }
    LOG_INFO("register sim watcher succeed!\n");

    std::shared_ptr<MySlotWatcher> mySlotWatcher = std::make_shared<MySlotWatcher>(1);
    ret = mbimDevice.AfalMbimRegisterNotification(mySlotWatcher);
    if (ret != 0) {
        LOG_ERROR("register event failed!\n");
        return -1;
    }
    LOG_INFO("register slot watcher succeed!\n");

    std::shared_ptr<MyNetworkWatcher> myNetworkWatcher = std::make_shared<MyNetworkWatcher>(1);
    ret = mbimDevice.AfalMbimRegisterNotification(myNetworkWatcher);
    if (ret != 0) {
        LOG_ERROR("register event failed!\n");
        return -1;
    }
    LOG_INFO("register network watcher succeed!\n");

    LOG_INFO("wait 60s for the incoming event!\n");
#if _IS_LINUX_
    const int WAIT_FOR_EVENT_TIME = 60;
    std::chrono::seconds sleep_duration(WAIT_FOR_EVENT_TIME);
    std::this_thread::sleep_for(sleep_duration);
#elif _IS_WINDOWS_
    Sleep(60 * 1000);
#endif

    ret = mbimDevice.AfalMbimDeregisterNotification(myRadioWatcher);
    if (ret != 0) {
        LOG_ERROR("deregister event failed!\n");
        return -1;
    }
    LOG_INFO("deregister radio watcher succeed!\n");

    ret = mbimDevice.AfalMbimDeregisterNotification(mySimWatcher);
    if (ret != 0) {
        LOG_ERROR("deregister event failed!\n");
        return -1;
    }
    LOG_INFO("deregister sim watcher succeed!\n");

    ret = mbimDevice.AfalMbimDeregisterNotification(mySlotWatcher);
    if (ret != 0) {
        LOG_ERROR("deregister event failed!\n");
        return -1;
    }
    LOG_INFO("deregister slot watcher succeed!\n");

    ret = mbimDevice.AfalMbimDeregisterNotification(myNetworkWatcher);
    if (ret != 0) {
        LOG_ERROR("deregister event failed!\n");
        return -1;
    }
    LOG_INFO("deregister network watcher succeed!\n");

    mbimDevice.deinit();
    return 0;
}

int mbimtest1() {
    afal::mbim::MbimDevice& mbimDevice = afal::mbim::MbimDevice::getInstance();
    // part 2: check mbim-device's init result.
    LOG_INFO("wait 5s to make sure mbim port ready!");
        const int BEFORE_FIRST_TEST_WAIT_TIME = 5;
#if _IS_LINUX_
    std::chrono::seconds sleep_duration1(BEFORE_FIRST_TEST_WAIT_TIME);
    std::this_thread::sleep_for(sleep_duration1);
#elif _IS_WINDOWS_
    Sleep(5 * 1000);
#endif
    int result = 0;

    // part 3: send at over mbim cmd.
    // define input and output string.
    QString inputStr = "ATI";  // convert to the command which need to be sent.
    QString outputStr;
    // set timeout time, like 5s
    int timeout = BEFORE_FIRST_TEST_WAIT_TIME;

    // call the AfalMbimAtOverMbimSetSync function.
    result = mbimDevice.AfalMbimAtOverMbimSetSync(inputStr, outputStr, timeout);
    if (result == 0) {
        LOG_INFO("AT send success, resp: %s\n", outputStr.toStdString().c_str());
    }
    else {
        LOG_ERROR("send command failed, error code: 0x%08x\n", result);
    }

#if _IS_LINUX_
    std::this_thread::sleep_for(sleep_duration1);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif

    RadioData RadioOutputData = {MBIM_RADIO_SWITCH_STATE_OFF, MBIM_RADIO_SWITCH_STATE_OFF};
    result = mbimDevice.AfalMbimRadioQuerySync(RadioOutputData, timeout);
    if (result != 0) {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }
    else {
        LOG_INFO("Hardware Radio state: %s\n", (RadioOutputData.hw_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));
        LOG_INFO("Software Radio state: %s\n", (RadioOutputData.sw_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));
    }
    const int BEFORE_NEXT_TEST_WAIT_TIME = 2;

#if _IS_LINUX_
    std::chrono::seconds sleep_duration2(BEFORE_NEXT_TEST_WAIT_TIME);
    std::this_thread::sleep_for(sleep_duration2);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif

    RadioOutputData.sw_state = MBIM_RADIO_SWITCH_STATE_OFF;
    result = mbimDevice.AfalMbimRadioSetSync(RadioOutputData, timeout);
    if (result != 0) {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }
    else {
        LOG_INFO("Hardware Radio state: %s\n", (RadioOutputData.hw_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));
        LOG_INFO("Software Radio state: %s\n", (RadioOutputData.sw_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));
    }

#if _IS_LINUX_
    std::this_thread::sleep_for(sleep_duration2);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif
    RadioOutputData.sw_state = MBIM_RADIO_SWITCH_STATE_ON;
    result = mbimDevice.AfalMbimRadioSetSync(RadioOutputData, timeout);
    if (result != 0) {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }
    else {
        LOG_INFO("Hardware Radio state: %s\n", (RadioOutputData.hw_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));
        LOG_INFO("Software Radio state: %s\n", (RadioOutputData.sw_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));
    }

#if _IS_LINUX_
    std::this_thread::sleep_for(sleep_duration2);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif

    SimData SimOutputData;
    result = mbimDevice.AfalMbimSimQuerySync(SimOutputData, timeout);
    if (result == 0) {
        LOG_INFO("send command succeed!\n");
        LOG_INFO("ready_state: %d\n", SimOutputData.ready_state);
        LOG_INFO("imsi: %s\n", SimOutputData.imsi.toUtf8().data());
        LOG_INFO("iccid: %s\n", SimOutputData.iccid.toUtf8().data());
        LOG_INFO("local_mccmnc: %s\n", SimOutputData.local_mccmnc.toUtf8().data());
    }
    else {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }

#if _IS_LINUX_
    std::this_thread::sleep_for(sleep_duration2);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif
    NetworkData NetworkOutputData;
    result = mbimDevice.AfalMbimNetworkQuerySync(NetworkOutputData, timeout);
    if (result == 0) {
        LOG_INFO("send command succeed!\n");
        LOG_INFO("RegisterState: %d\n", NetworkOutputData.RegisterState);
        LOG_INFO("rssi: %d\n", NetworkOutputData.rssi);
        LOG_INFO("Roam mccmnc: %s\n", NetworkOutputData.RoamMccmnc.toUtf8().data());
        LOG_INFO("connect state: %d\n", NetworkOutputData.ConnectState);
    }
    else {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }

#if _IS_LINUX_
    std::this_thread::sleep_for(sleep_duration2);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif
    SlotData SlotOutputData;
    result = mbimDevice.AfalMbimSlotQuerySync(SlotOutputData, timeout);
    if (result == 0) {
        LOG_INFO("send command succeed!\n");
        LOG_INFO("ValidSlotNum: %d\n", SlotOutputData.ValidSlotNum);
        LOG_INFO("ValidExecutorNum: %d\n", SlotOutputData.ValidExecutorNum);
        LOG_INFO("CurrentSlotIndex: %d\n", SlotOutputData.CurrentSlotIndex);
        LOG_INFO("slot1: %d\n", SlotOutputData.slot_state[0]);
        LOG_INFO("slot2: %d\n", SlotOutputData.slot_state[1]);
    }
    else {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }

#if _IS_LINUX_
    std::this_thread::sleep_for(sleep_duration2);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif

    SlotOutputData.CurrentSlotIndex = 1;
    result = mbimDevice.AfalMbimSlotSetSync(SlotOutputData, timeout);
    if (result == 0) {
        LOG_INFO("send command succeed!\n");
        LOG_INFO("ValidSlotNum: %d\n", SlotOutputData.ValidSlotNum);
        LOG_INFO("ValidExecutorNum: %d\n", SlotOutputData.ValidExecutorNum);
        LOG_INFO("CurrentSlotIndex: %d\n", SlotOutputData.CurrentSlotIndex);
        LOG_INFO("slot1: %d\n", SlotOutputData.slot_state[0]);
        LOG_INFO("slot2: %d\n", SlotOutputData.slot_state[1]);
    }
    else {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }

#if _IS_LINUX_
    std::this_thread::sleep_for(sleep_duration2);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif

    SlotOutputData.CurrentSlotIndex = 0;
    result = mbimDevice.AfalMbimSlotSetSync(SlotOutputData, timeout);
    if (result == 0) {
        LOG_INFO("send command succeed!\n");
        LOG_INFO("ValidSlotNum: %d\n", SlotOutputData.ValidSlotNum);
        LOG_INFO("ValidExecutorNum: %d\n", SlotOutputData.ValidExecutorNum);
        LOG_INFO("CurrentSlotIndex: %d\n", SlotOutputData.CurrentSlotIndex);
        LOG_INFO("slot1: %d\n", SlotOutputData.slot_state[0]);
        LOG_INFO("slot2: %d\n", SlotOutputData.slot_state[1]);
    }
    else {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }

#if _IS_LINUX_
    std::this_thread::sleep_for(sleep_duration2);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif

    QMap <QString, int> LogMap;
    int value = -1;

    LogMap.insert("mode", 2);
    LogMap.insert("level", 2);

    result = mbimDevice.AfalMbimTraceLogSetSync(LogMap, timeout);
    if (result == 0) {
        LOG_INFO("send command succeed!\n");

        value = LogMap.value("mode");
        LOG_INFO("mode: %d\n", value);

        value = LogMap.value("level");
        LOG_INFO("level: %d\n", value);
    }
    else {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }

#if _IS_LINUX_
    std::this_thread::sleep_for(sleep_duration2);
#elif _IS_WINDOWS_
    Sleep(2 * 1000);
#endif

    result = mbimDevice.AfalMbimTraceLogQuerySync(LogMap, timeout);
    if (result == 0) {
        LOG_INFO("send command succeed!\n");

        value = LogMap.value("mode");
        LOG_INFO("mode: %d\n", value);

        value = LogMap.value("level");
        LOG_INFO("level: %d\n", value);
    }
    else {
        LOG_ERROR("send command failed, error code: %d\n", result);
    }

// close device.
    mbimDevice.deinit();
    mbimtest2();

    return 0;
}


int main(int argc, char *argv[]) {
    QCoreApplication const APP(argc, argv); // NOLINT(misc-non-private-member-variables-in-classes)
    std::cout << "wwan_func test starting" << std::endl;
    std::cout << "wwan_func version:" << "WWAN_FUNC_SERVICE_VERSION_STRING" << std::endl;
#if _IS_LINUX_
    std::cout << "current os is Linux" << std::endl;
#elif _IS_WINDOWS_
    std::cout << "current os is windows" << std::endl;
#endif

    const bool enable = true;
    Logger::Instance().Init(CONSOLE, "WWANFUNCSRV");
    Logger::Instance().EnableDebugMode(enable);

//    Qthread will return error because Qthread depends on QTMainloop!
    // part 1: mbim-device's init process.

    // get MbimDevice instance.
    afal::mbim::MbimDevice& mbimDevice = afal::mbim::MbimDevice::getInstance();

    QString portName = "/dev/cdc-wdm0";
    // init function will depend on the QTMainlop, so main function should not be blocked.
    if (mbimDevice.init(portName) != 0)
    {
        LOG_ERROR("MbimDevice init failed!\n");
        return -1;
    }

    // Qthread will return error because Qthread depends on QTMainloop!
    std::thread mbim_thread(mbimtest1);
    return APP.exec(); //NOLINT(readability-static-accessed-through-instance)
}

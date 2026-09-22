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
#include <QThread>
#include "lpa.hpp"

using namespace afal::mbim;
using namespace afal::log;
using namespace afal::lpa;

#define ERR           -1
#define OK            0

int lpatest2() {
    afal::lpa::LpaDevice& LpaDevice    = afal::lpa::LpaDevice::getInstance();
    afal::mbim::MbimDevice& mbimDevice = afal::mbim::MbimDevice::getInstance();
    QString portName = "/dev/cdc-wdm0";
    QString eid;
    SimData SimOutputData;
    int timeout = 5;
    int result = 0;
    int channel = ERR;
    int ret = ERR;
    QString AID = "A0000005591010FFFFFFFF8900000100";
    QString resp;
    QString CAPDU;

    if (mbimDevice.init(portName) != 0)
    {
        LOG_ERROR("MbimDevice init failed!\n");
        goto TAIL;
    }

    // init function will depend on the QTMainlop, so main function should not be blocked.
    if (LpaDevice.init("/var/log", portName, PLAT_TYPE_MTK) != 0)
    {
        LOG_ERROR("LpaDevice init failed!\n");
        return -1;
    }

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
        goto TAIL;
    }

    if ((SimOutputData.ready_state != MBIM_SUBSCRIBER_READY_STATE_NO_ESIM_PROFILE) && (SimOutputData.ready_state != MBIM_SUBSCRIBER_READY_STATE_INITIALIZED)) {
        LOG_ERROR("Invalid ESIM state %d!\n", SimOutputData.ready_state);
        goto TAIL;
    }

    LOG_DEBUG("will sleep 5s!\n");
    QThread::sleep(5);

    channel = LpaDevice.AfalLpaOpenChannel();
    if (channel <= 0) {
        LOG_ERROR("Open channel failed!\n");
        goto TAIL;
    }
    else {
        LOG_DEBUG("channel: %02x\n", channel);
    }

    LOG_DEBUG("will sleep 5s!\n");
    QThread::sleep(5);

    ret = LpaDevice.AfalLpaSelectEsimAppid(AID, resp);
    if (ret != OK) {
        LOG_ERROR("AfalLpaSelectEsimAppid failed!\n");
        goto TAIL;
    }
    else {
        LOG_DEBUG("R-APDU: %s\n", QS(resp));
    }

    LOG_DEBUG("will sleep 5s!\n");
    QThread::sleep(5);

    resp.clear();
    CAPDU.clear();
    CAPDU = QString("8%1E2910006BF3E035C015A").arg(channel, 0, 16).toUpper();
    ret = LpaDevice.AfalLpaSendApduSync(CAPDU, resp);
    if (ret != OK) {
        LOG_ERROR("AfalLpaSendApduSync failed!\n");
        goto TAIL;
    }
    else {
        LOG_DEBUG("R-APDU: %s\n", QS(resp));
    }

    LOG_DEBUG("will sleep 5s!\n");
    QThread::sleep(5);

    result = LpaDevice.AfalLpaCloseChannel(channel);
    if (result != OK) {
        LOG_ERROR("Close channel failed!\n");
        goto TAIL;
    }
    else {
        LOG_DEBUG("Close channel succeed!\n");
    }

TAIL:
    if (LpaDevice.deinit() != 0)
    {
        LOG_ERROR("LpaDevice deinit failed!\n");
        return -1;
    }

    return 0;
}

int lpatest1() {
    afal::lpa::LpaDevice& LpaDevice    = afal::lpa::LpaDevice::getInstance();
    afal::mbim::MbimDevice& mbimDevice = afal::mbim::MbimDevice::getInstance();
    QString portName = "/dev/cdc-wdm0";
    QString eid;
    SimData SimOutputData;
    int timeout = 5;
    int result = 0;
    std::vector<AfalLpaProfileInfoType> data;

    // init function will depend on the QTMainlop, so main function should not be blocked.
    if (LpaDevice.init("/var/log", portName, PLAT_TYPE_MTK) != 0)
    {
        LOG_ERROR("LpaDevice init failed!\n");
        return -1;
    }

    if (mbimDevice.init(portName) != 0)
    {
        LOG_ERROR("MbimDevice init failed!\n");
        goto TAIL;
    }

    LOG_DEBUG("will sleep 5s!\n");
    QThread::sleep(5);

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
        goto TAIL;
    }

    if ((SimOutputData.ready_state != MBIM_SUBSCRIBER_READY_STATE_NO_ESIM_PROFILE) && (SimOutputData.ready_state != MBIM_SUBSCRIBER_READY_STATE_INITIALIZED)) {
        LOG_ERROR("Invalid ESIM state %d!\n", SimOutputData.ready_state);
        goto TAIL;
    }

    LOG_DEBUG("will sleep 5s!\n");
    QThread::sleep(5);

    if (LpaDevice.AfalLpaGetEid(eid) != 0) {
        LOG_ERROR("Get eid failed!\n");
        goto TAIL;
    }
    else {
        LOG_DEBUG("EID: %s\n", QS(eid));
    }

    LOG_DEBUG("will sleep 5s!\n");
    QThread::sleep(5);

    if (LpaDevice.AfalLpaGetProfilesInfo(&data) != 0) {
        LOG_ERROR("Get Profile failed!\n");
        goto TAIL;
    }
    else {
        for (auto it = data.begin(); it != data.end(); ++it) {
            LOG_DEBUG("Profile info are as below.\n");
            LOG_DEBUG("ProfileState: %d\n", it->ProfileState);
            LOG_DEBUG("ProfileName: %s\n", QS(it->ProfileName));
            LOG_DEBUG("iccid: %s\n", QS(it->iccid));
            LOG_DEBUG("ProfileClass: %d\n", it->ProfileClass);
            LOG_DEBUG("ProfileNickName: %s\n", QS(it->ProfileNickName));
            QThread::sleep(1);
        }
    }

TAIL:
    if (LpaDevice.deinit() != 0)
    {
        LOG_ERROR("LpaDevice deinit failed!\n");
        return -1;
    }

    LOG_DEBUG("Sleep 5s to begin next test!\n");
    QThread::sleep(5);

    LOG_DEBUG("Begin to test2!\n");
    lpatest2();
    LOG_DEBUG("test finished!\n");
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

    LOG_DEBUG("Begin to test!\n");
    // Qthread will return error because Qthread depends on QTMainloop!
    std::thread lpa_thread(lpatest1);
    return APP.exec(); //NOLINT(readability-static-accessed-through-instance)
}

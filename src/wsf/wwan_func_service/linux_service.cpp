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
 * @file linux_service.cpp
 * @author catherine.li@fibocom.com
 * @brief
 * @version 1.0
 * @date 2024-09-13
 *
 **/

#include "wwan_func_svc.hpp"
#include "version.h"
#include "log.hpp"
#include <QDebug>
#include <unistd.h>
#include <csignal>
#include <QtCore>
#include <QCoreApplication>

using namespace afal::log;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_DEBUG("Received :%d signal, will stop now!", signal);
        QCoreApplication::quit();
    }
}

void setup_signal_handlers() {
    struct sigaction sa; //NOLINT(cppcoreguidelines-pro-type-member-init)
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv); //NOLINT(misc-const-correctness)
    LogConfig log_cfg;
    Logger::Instance().Init(CUSTOM | LINUX_SYSLOG | WINDOWS_EVENTLOG, "wwan_func_service", log_cfg);
    Logger::Instance().SetLevel(DEBUG);

#if _IS_LINUX_
    LOG_INFO("current os is Linux");
#elif _IS_WINDOWS_
    LOG_INFO("current os is Windows");
#endif

    setup_signal_handlers();

    //QTimer::singleShot(1000, []() { // service functions run on a new thread in startservicework
    LOG_INFO("------------------ starting func service V%s-----------------", WWAN_DEV_VERSION_STRING);
    WwanFuncSvc::startWwanFuncServiceWork();

    int res = app.exec();
    WwanFuncSvc::stopWwanFuncServiceWork();
    Logger::Instance().UnInit();
    return res; //NOLINT(readability-static-accessed-through-instance)
}
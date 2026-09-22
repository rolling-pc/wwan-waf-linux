/* *****************************************************
SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0

Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
Licensed under the Unisoc General Software License, version 1.0 (the License);
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
Software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OF ANY KIND, either express or implied.
See the Unisoc General Software License, version 1.0 for more details.
******************************************************* */

#ifndef TIMERCONTRL_H
#define TIMERCONTRL_H

#include <sys/time.h>
#include <unistd.h>
#include <thread>
#include <mutex>

typedef void (*CBF)(int);

using namespace std;


class TimerContrl
{
public:
    TimerContrl();

//******毫秒******
public:
    //开启计时器   deltaT-溢出时间  tof-回调函数
    void StartTimer(unsigned long deltaT, void *tof = NULL);
    //删除计时器
    void KillTimer();
    //设置溢出时间
    void SetDeltaTime(unsigned long deltaT);
    //设置回调函数
    void SetCallbackFuc(void *tof);

public:
    unsigned long m_deltaTm;    //溢出时间 秒
    bool m_IsStopTimer;         //停止计时
    mutex m_mt_deltaTm;         //溢出时间 线程临界值

    CBF to;     //回调函数

    //计时线程
    static void ThreadTickTack(TimerContrl* tc);

//******秒******
public:
    //开启计时器 秒  deltaT-溢出时间  tof-回调函数
    void StartTimerS(unsigned long deltaT, void *tof = NULL);
    //删除计时器 秒
    void KillTimerS();
    //设置溢出时间 秒
    void SetDeltaTimeS(unsigned long deltaT);
    //设置回调函数 秒
    void SetCallbackFucS(void *tof);

public:
    unsigned long m_deltaTmS;    //溢出时间 秒
    bool m_IsStopTimerS;         //停止计时
    mutex m_mt_deltaTmS;         //溢出时间 线程临界值

    CBF toS;     //回调函数

    //秒单位 计时线程
    static void ThreadTickTackS(TimerContrl* tc);


};

#endif // TIMERCONTRL_H

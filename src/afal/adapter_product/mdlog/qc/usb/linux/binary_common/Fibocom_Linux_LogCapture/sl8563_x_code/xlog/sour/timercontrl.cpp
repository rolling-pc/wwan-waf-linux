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

#include "timercontrl.h"

TimerContrl::TimerContrl()
{
    m_deltaTm = 0;
    m_IsStopTimer = false;
    to = NULL;

    m_deltaTmS = 0;
    m_IsStopTimerS = false;
    toS = NULL;
}

//******毫秒******
void TimerContrl::StartTimer(unsigned long deltaT, void *tof)
{
    m_deltaTm = deltaT;
    to = (CBF)tof;

    thread thr(ThreadTickTack, this);
    thr.detach();
}

void TimerContrl::KillTimer()
{
    m_IsStopTimer = true;
}

void TimerContrl::SetDeltaTime(unsigned long deltaT)
{
    m_mt_deltaTm.lock();
    m_deltaTm = deltaT;
    m_mt_deltaTm.unlock();
}

void TimerContrl::SetCallbackFuc(void *tof)
{
    to = (CBF)tof;
}

void TimerContrl::ThreadTickTack(TimerContrl* tc)
{
    int ret = 0;
    bool IsRun = false;
    double t1, t2;
    struct timeval  tv;
    long dltT = 9;


    gettimeofday(&tv, NULL);
    t1 = (tv.tv_sec) * 1000. + (tv.tv_usec) / 1000. ; // convert tv_sec & tv_usec to millisecond

    IsRun = true;

    do {
        if (tc->m_IsStopTimer)
        {
            IsRun = false;
            ret = 1;
        }
        else
        {
            gettimeofday(&tv, NULL);
            t2 = (tv.tv_sec) * 1000. + (tv.tv_usec) / 1000. ; // convert tv_sec & tv_usec to millisecond

            tc->m_mt_deltaTm.lock();
            dltT = tc->m_deltaTm - (t2 - t1);
            tc->m_mt_deltaTm.unlock();
            if (dltT <= 0)
            {
                IsRun = false;
                ret = 0;
            }

            usleep(1 * 1000);
        }

    } while(IsRun);

    if (tc->to != NULL)
        tc->to(ret);
}



//******秒******
void TimerContrl::StartTimerS(unsigned long deltaT, void *tof)
{
    m_deltaTmS = deltaT;
    toS = (CBF)tof;

    thread thr(ThreadTickTackS, this);
    thr.detach();
}

void TimerContrl::KillTimerS()
{
    m_IsStopTimerS = true;
}

void TimerContrl::SetDeltaTimeS(unsigned long deltaT)
{
    m_mt_deltaTmS.lock();
    m_deltaTmS = deltaT;
    m_mt_deltaTmS.unlock();
}

void TimerContrl::SetCallbackFucS(void *tof)
{
    toS = (CBF)tof;
}

void TimerContrl::ThreadTickTackS(TimerContrl* tc)
{
    int ret = 0;
    bool IsRun = false;
    time_t t1, t2;
    long dltT = 9;


    time(&t1);
    IsRun = true;

    do {

        if (tc->m_IsStopTimerS)
        {
            IsRun = false;
            ret = 1;
        }
        else
        {
            time(&t2);

            tc->m_mt_deltaTmS.lock();
            dltT = tc->m_deltaTmS - (t2 - t1);
            tc->m_mt_deltaTmS.unlock();
            if (dltT <= 0)
            {
                IsRun = false;
                ret = 0;
            }

            usleep(1 * 1000);
        }

    } while(IsRun);

    if (tc->toS != NULL)
        tc->toS(ret);
}

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

// UserInputMonitor.cpp: implementation of the CUserInputMonitor class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "typedef.h"
#include "UserInputMonitor.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


CUserInputMonitor::CUserInputMonitor()
{
	m_bRunMonitor    = false;
	m_hMonitorThread = NULL;
}

CUserInputMonitor::~CUserInputMonitor()
{
}

void CUserInputMonitor::WaitForUserInput()
{
	m_bRunMonitor = true;

    pthread_create(&m_hMonitorThread,
				   NULL,
				   (PTHREAD_START_ROUTINE)GetMonitorFunc,
				   this);

	while(m_bRunMonitor)
    {
        usleep(500 * 1000);
    }

    pthread_cancel(m_hMonitorThread);
    pthread_join(m_hMonitorThread,NULL);
    m_hMonitorThread = NULL;
}

void CUserInputMonitor::CancelWait()
{
    m_bRunMonitor = false;
}

void* CUserInputMonitor::GetMonitorFunc(void* lpParam)
{
	CUserInputMonitor * p = (CUserInputMonitor *)lpParam;
	return p->MonitorFunc();
}

void* CUserInputMonitor::MonitorFunc()
{
    char szCmd[16] = {0};
    do {
        scanf("%s", szCmd);
        if (strcmp(szCmd, "quit") == 0)
            break;
    }
    while(1);

    m_bRunMonitor = false;
}


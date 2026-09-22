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

// UserInputMonitor.h: interface for the CUserInputMonitor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_USERINPUTMONITOR_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)
#define AFX_USERINPUTMONITOR_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_

#include <pthread.h>

class CUserInputMonitor
{
public:
	CUserInputMonitor();
	virtual ~CUserInputMonitor();

public:
	void WaitForUserInput();
	void CancelWait();

protected:
	static void* GetMonitorFunc(void* lpParam);
    void* MonitorFunc();

protected:
	bool       m_bRunMonitor;
    pthread_t  m_hMonitorThread;

};


#endif // !defined(AFX_USERINPUTMONITOR_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)

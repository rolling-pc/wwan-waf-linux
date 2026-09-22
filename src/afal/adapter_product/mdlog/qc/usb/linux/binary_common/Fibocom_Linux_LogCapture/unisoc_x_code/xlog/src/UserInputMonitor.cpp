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
        sleep(1);
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


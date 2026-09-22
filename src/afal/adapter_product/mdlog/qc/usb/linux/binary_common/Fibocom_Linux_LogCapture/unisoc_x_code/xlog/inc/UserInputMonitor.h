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

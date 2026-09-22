// DiagChannel.cpp: implementation of the CArmTracerChannel class.
//
//////////////////////////////////////////////////////////////////////

#include "DiagChan.h"
#include "ArmTracerChannel.h"

#define MSG_TIME_OUT 0xD3
#define MSG_UNKNOWN_STRUCT 0xD8

// This two functions will be implement in BaseChan.lib
extern "C" void *dlmalloc(size_t);
extern "C" void dlfree(void *);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CArmTracerChannel::CArmTracerChannel()
{
	m_lpProtocolPackage = &m_Package;
	m_szLogName[0] = '\0';
#ifdef _DEBUG
	_tcscpy(m_szLogName, _T("ArmTracerChanD"));
#else
	_tcscpy(m_szLogName, _T("ArmTracerChan"));
#endif
}

CArmTracerChannel::CArmTracerChannel(TCHAR *chName) : CBaseChannel(chName)
{
    m_lpProtocolPackage = &m_Package;
    m_outFileThreasHold = 4 * 1024 * 1024;
    m_szLogName[0] = '\0';
#ifdef _DEBUG
	sprintf(m_szLogName, "%sChanD", m_chName);
#else
	sprintf(m_szLogName, "%sChan", m_chName);
#endif
}

CArmTracerChannel::~CArmTracerChannel()
{
}

int CArmTracerChannel::GetProtocolType()
{
	return (int)PROTOCOL_TYPE_ARMTRACER;
}

char* CArmTracerChannel::GetOutFileName()
{
    struct tm *tm = NULL;
    time_t t;
    struct timeval tmv;
    gettimeofday(&tmv, NULL);
    t = time(NULL);
    tm = localtime(&t);
    {
        //8910的cp Log文件格式：name(日-时-分-秒-白分秒)[.partxx.Sn(xxxx)].tra
        //e.g: Arm(17-21-59-25-500).Part4.Sn(0).tra
        sprintf(m_pOutFileName, "%sArm(%02u-%02u-%02u-%02u-%03lu).tra", m_pOutPath,
            tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tmv.tv_usec / 1000);
    }
    return m_pOutFileName;
}

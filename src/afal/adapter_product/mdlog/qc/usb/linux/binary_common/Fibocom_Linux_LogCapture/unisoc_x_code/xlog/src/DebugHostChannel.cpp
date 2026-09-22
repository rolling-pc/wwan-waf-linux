// DiagChannel.cpp: implementation of the CDebugHostChannel class.
//
//////////////////////////////////////////////////////////////////////

#include "DiagChan.h"
#include "DebugHostChannel.h"

#define MSG_TIME_OUT 0xD3
#define MSG_UNKNOWN_STRUCT 0xD8

// This two functions will be implement in BaseChan.lib
extern "C" void *dlmalloc(size_t);
extern "C" void dlfree(void *);

typedef struct _debug_host_timestamp
{
    uint8_t sync;
    uint8_t lenM;
    uint8_t lenL;
    uint8_t flowid;
    uint32_t date;
    uint32_t ms;
} DEBUG_HOST_TIMESTAMP;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDebugHostChannel::CDebugHostChannel()
{
    m_lpProtocolPackage = &m_Package;
    m_szLogName[0] = '\0';
#ifdef _DEBUG
    _tcscpy(m_szLogName, _T("RawDataChanD"));
#else
    _tcscpy(m_szLogName, _T("RawDataChan"));
#endif
}

CDebugHostChannel::CDebugHostChannel(TCHAR *chName) : CBaseChannel(chName)
{
    m_lpProtocolPackage = &m_Package;
    m_outFileThreasHold = 1 * 1024 * 1024;
    m_szLogName[0] = '\0';
#ifdef _DEBUG
    sprintf(m_szLogName, "%sChanD", m_chName);
#else
    sprintf(m_szLogName, "%sChan", m_chName);
#endif
}

CDebugHostChannel::~CDebugHostChannel()
{
}

BOOL CDebugHostChannel::GetProperty(LONG lFlags, DWORD dwPropertyID,
                                    LPVOID pValue)
{
    if (NULL == pValue)
    {
        return false;
    }

    switch (dwPropertyID)
    {
    case PPI_DEBUG_HOST_NEED_TS:
    {
        BOOL *lpValue = (BOOL *)pValue;
        *lpValue = m_nNeedTs;
        return true;
    }
    break;
    default:
        return CBaseChannel::GetProperty(lFlags, dwPropertyID, pValue);
    }
}

BOOL CDebugHostChannel::SetProperty(LONG lFlags, DWORD dwPropertyID,
                                    LPCVOID pValue)
{
    switch (dwPropertyID)
    {
    case PPI_DEBUG_HOST_NEED_TS:
    {
        m_nNeedTs = *((BOOL *)pValue);
        m_lpProtocolPackage->SetProperty(lFlags, dwPropertyID, pValue);
        return TRUE;
    }
    break;
    default:
        return CBaseChannel::SetProperty(lFlags, dwPropertyID, pValue);
    }
}

int CDebugHostChannel::GetProtocolType()
{
    return (int)PROTOCOL_TYPE_DEBUGHOST;
}

char *CDebugHostChannel::GetOutFileName()
{
    struct tm *tm = NULL;
    time_t t;
    t = time(NULL);
    tm = localtime(&t);
    // log_-1_221101-112834.bin
    sprintf(m_pOutFileName, "%slog_-1_%02u%02u%02u-%02u%02u%02u.bin", m_pOutPath,
            1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    return m_pOutFileName;
}

uint64_t CDebugHostChannel::WriteDataToFile(char * path, LPBYTE lpBuffer, DWORD dwDataSize)
{
    size_t dwWriteSize = 0;
	if (m_pOutFilef == NULL || (0 != strcmp(path, m_pOutPath)))
	{
		strcpy(m_pOutPath, path);
		CreateOutFile();
        m_nNeedTs = TRUE;
		m_outFileSize = 0;
	}

    if (m_outFileSize + dwDataSize > m_outFileThreasHold)
    {
		std::cout << this->GetChannelName() << " over file ThreasHold " << m_outFileThreasHold << " " << m_outFileSize + dwDataSize << std::endl;
        CreateOutFile();
        m_nNeedTs = TRUE;
        m_outFileSize = 0;
    }

    if(m_nNeedTs)
	{
		struct tm *tm = NULL;
		time_t t;
		struct timeval tmv;
		gettimeofday(&tmv, NULL);

		t = time(NULL);
		tm = localtime(&t);

		DEBUG_HOST_TIMESTAMP log_ts;
		log_ts.sync= 0xAD;
		log_ts.lenM = 0;
		log_ts.lenL = 0x08;
		log_ts.flowid = 0xa2;
		log_ts.date = ((1900 + tm->tm_year) << 16) + ((tm->tm_mon + 1) << 8) + (tm->tm_mday);
		log_ts.ms = tm->tm_hour * 3600 * 1000 + tm->tm_min * 60 * 1000 + tm->tm_sec * 1000 + tmv.tv_usec / 1000;
        fwrite((void *)&log_ts, 1, sizeof(DEBUG_HOST_TIMESTAMP), m_pOutFilef);
		m_nNeedTs = FALSE;
	}

    dwWriteSize = fwrite(lpBuffer, 1, dwDataSize, m_pOutFilef);

    m_outFileSize += dwWriteSize;
	return dwWriteSize;
}
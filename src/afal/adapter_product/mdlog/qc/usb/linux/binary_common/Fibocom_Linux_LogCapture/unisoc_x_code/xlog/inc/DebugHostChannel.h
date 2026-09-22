#if !defined(__DEBUG_HOST_CHANNEL_H__)
#define __DEBUG_HOST_CHANNEL_H__

#include <stdlib.h>
#include "BaseChan.h"
#include "RawDataPackage.h"

typedef struct _ch_timestamp{
    uint8_t sync;
    uint8_t lenM;
    uint8_t lenL;
    uint8_t flowid;
    uint32_t date;
    uint32_t ms;
} CH_TIMESTAMP;

class CDebugHostChannel : public CBaseChannel
{
public:
	CDebugHostChannel();
	CDebugHostChannel(TCHAR *chName);
	virtual ~CDebugHostChannel();

	virtual BOOL GetProperty(LONG lFlags, DWORD dwPropertyID,
							 LPVOID pValue);

	virtual BOOL SetProperty(LONG lFlags, DWORD dwPropertyID,
							 LPCVOID pValue);

	virtual int GetProtocolType();

protected:
	virtual char* GetOutFileName();
	virtual uint64_t WriteDataToFile(char * path, LPBYTE lpBuffer, DWORD dwDataSize);
	CRawDataPackage m_Package;
	BOOL m_nNeedTs;
};

#endif // !defined(__DEBUG_HOST_CHANNEL_H__)

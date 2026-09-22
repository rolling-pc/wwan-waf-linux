#if !defined(__ARM_TRACER_CHANNEL_H__)
#define __ARM_TRACER_CHANNEL_H__

#include <stdlib.h>
#include "BaseChan.h"
#include "RawDataPackage.h"

class CArmTracerChannel : public CBaseChannel
{
public:
	CArmTracerChannel();
	CArmTracerChannel(TCHAR *chName);
	virtual ~CArmTracerChannel();

	virtual int GetProtocolType();

protected:
	virtual char* GetOutFileName(); 
	CRawDataPackage m_Package;
};

#endif // !defined(__ARM_TRACER_CHANNEL_H__)

// ProtocalChannelFactory.h: interface for the CProtocalChannelFactory class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROTOCALCHANNELFACTORY_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)
#define AFX_PROTOCALCHANNELFACTORY_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_

#include "BaseChan.h"
#include "DiagChannel.h"
#include "SmpChannel.h"
#include "ArmTracerChannel.h"
#include "DebugHostChannel.h"

enum {
	PROTOCOL_DIAG = 0,
	PROTOCOL_SMP,
	PROTOCOL_ARM_TRACER,
	PROTOCOL_DEBUG_HOST,
};

class CProtocalChannelFactory
{
public:
	static CBaseChannel* GetInstance(uint32_t uType);
    static void          ReleaseInstance(CBaseChannel* pInstance);
};


#endif // !defined(AFX_PROTOCALCHANNELFACTORY_H__D0A252D4_C004_463C_926C_7FAD1A3A4DBC__INCLUDED_)

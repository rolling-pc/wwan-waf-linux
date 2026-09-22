// ProtocalChannelFactory.cpp: implementation of the CProtocalChannelFactory class.
//
//////////////////////////////////////////////////////////////////////

#include "ProtocalChannelFactory.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


CBaseChannel* CProtocalChannelFactory::GetInstance(uint32_t uType)
{
	CBaseChannel* pBaseChannel = NULL;

	switch(uType)
	{
	case PROTOCOL_SMP:
		pBaseChannel = new CSmpChannel();
		break;
	case PROTOCOL_DIAG:
	default:
		pBaseChannel = new CDiagChannel();
		break;
	}

	return pBaseChannel;
}

void CProtocalChannelFactory::ReleaseInstance(CBaseChannel* pInstance)
{
    SAFE_DELETE_PTR(pInstance)
}
// SmpChannel.cpp: implementation of the CSmpChannel class.
//
//////////////////////////////////////////////////////////////////////

#include "SmpChannel.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSmpChannel::CSmpChannel()
{
	m_lpProtocolPackage = &m_Package;

	m_szLogName[0] = '\0';

#ifdef _DEBUG
	_tcscpy(m_szLogName,_T("SmpChanD"));
#else
	_tcscpy(m_szLogName,_T("SmpChan"));
#endif

}

CSmpChannel::~CSmpChannel()
{
}

int CSmpChannel::GetProtocolType()
{
    return (int)PROTOCOL_TYPE_SMP;
}

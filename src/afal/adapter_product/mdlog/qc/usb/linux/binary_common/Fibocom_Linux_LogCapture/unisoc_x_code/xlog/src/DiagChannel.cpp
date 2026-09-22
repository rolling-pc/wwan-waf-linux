// DiagChannel.cpp: implementation of the CDiagChannel class.
//
//////////////////////////////////////////////////////////////////////

#include "DiagChan.h"
#include "DiagChannel.h"

#define MSG_TIME_OUT 0xD3
#define MSG_UNKNOWN_STRUCT  0xD8

// This two functions will be implement in BaseChan.lib
extern "C" void* dlmalloc(size_t);
extern "C" void  dlfree(void*);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDiagChannel::CDiagChannel()
{
	m_lpProtocolPackage = &m_Package;
	m_nPackageType = DIAG_TYPE_NORMAL;

	m_szLogName[0] = '\0';
#ifdef _DEBUG
	_tcscpy(m_szLogName,_T("DiagChanD"));
#else
	_tcscpy(m_szLogName,_T("DiagChan"));
#endif
}

CDiagChannel::~CDiagChannel()
{

}

BOOL CDiagChannel::GetProperty( LONG lFlags, DWORD dwPropertyID,
							LPVOID pValue )
{
	if( NULL == pValue )
	{
		return false;
	}

	switch (dwPropertyID)
	{
	case PPI_DIAG_Type:
		{
			int* lpValue = (int*)pValue;
			*lpValue = m_nPackageType;
			return true;
		}
		break;
	default:
		return CBaseChannel::GetProperty( lFlags,dwPropertyID,pValue );
	}
}

BOOL CDiagChannel::SetProperty( LONG lFlags, DWORD dwPropertyID,
							LPCVOID pValue )
{
	switch (dwPropertyID)
	{
	case PPI_DIAG_Type:
		{
			if( m_bConnected )
			{
				// Different value of this property means different format
				// of diag packages,so it is only can be set before lower
				// channel is opened
				return (m_nPackageType == *((int*)pValue));
			}
			else
			{
				m_nPackageType = *((int*)pValue);
				m_lpProtocolPackage->SetProperty( lFlags,dwPropertyID,pValue );
				return TRUE;
			}
		}
		break;
	default:
		return CBaseChannel::SetProperty( lFlags,dwPropertyID,pValue );
	}
}

int CDiagChannel::GetProtocolType()
{
    return (int)PROTOCOL_TYPE_DIAG;
}

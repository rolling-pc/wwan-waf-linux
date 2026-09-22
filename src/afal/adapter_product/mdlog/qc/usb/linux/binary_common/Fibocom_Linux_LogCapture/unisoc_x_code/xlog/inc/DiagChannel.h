// DiagChannel.h: interface for the CDiagChannel class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DIAGCHANNEL_H__5D001F14_462D_4200_95DF_22B7B6A51F69__INCLUDED_)
#define AFX_DIAGCHANNEL_H__5D001F14_462D_4200_95DF_22B7B6A51F69__INCLUDED_

#include <stdlib.h>
#include "BaseChan.h"
#include "DiagPackage.h"

class CDiagChannel : public CBaseChannel
{
public:
	CDiagChannel();
	virtual ~CDiagChannel();

	virtual BOOL	GetProperty( LONG lFlags, DWORD dwPropertyID,
		LPVOID pValue );

	virtual BOOL 	SetProperty( LONG lFlags, DWORD dwPropertyID,
		LPCVOID pValue );

    virtual int GetProtocolType();

protected:
	CDiagPackage m_Package;
	int m_nPackageType;
};

#endif // !defined(AFX_DIAGCHANNEL_H__5D001F14_462D_4200_95DF_22B7B6A51F69__INCLUDED_)

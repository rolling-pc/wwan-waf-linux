// DiagPackage.h: interface for the CDiagPackage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__RAW_DATA_PACKAGE_H__)
#define __RAW_DATA_PACKAGE_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DiagChan.h"
#include "ProtoChan.h"
#include <pthread.h>

//#pragma warning( disable : 4284 )
//#pragma warning( push, 3 )
#include <deque>
//#pragma warning( pop )

#define MAX_DIAG_PKG_SIZE 65536  // 64K

class CRawDataPackage : public IProtocolPackage
{
public:
	CRawDataPackage();
	virtual ~CRawDataPackage();

public:
	virtual unsigned int Append( void* lpBuffer,unsigned int uSize );

    virtual void Clear();

    virtual unsigned int GetPackages( void* lpPackage,int size );

	virtual void FreeMem( void* lpMemBlock );

	virtual unsigned int Package( void* lpInput,int nInputSize,void** lppMemBlock,int* lpSize );

    virtual bool GetProperty( long lFlags,unsigned int dwPropertyID,void* lpValue );
    virtual bool SetProperty( long lFlags,unsigned int dwPropertyID,const void* lpValue );

protected:
	void LockPakcageList( bool bLock );
	unsigned int RawDataAppend( void* lpBuffer,unsigned int uSize  );

protected:

	PRT_BUFF* m_lpPkg;

	std::deque<PRT_BUFF*> m_Packages;		// The unpacked packages

#ifdef _WIN32
	CRITICAL_SECTION m_Section;
#else
    pthread_mutex_t  m_Section;
#endif
};


#endif // !defined(__RAW_DATA_PACKAGE_H__)

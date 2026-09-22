#include "RawDataPackage.h"
#include "dlmalloc.h"

#include <string.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRawDataPackage::CRawDataPackage()
{
#ifdef _WIN32
	InitializeCriticalSection( &m_Section );
#else
    pthread_mutex_init(&m_Section,NULL);
#endif

	m_lpPkg = NULL;
}

CRawDataPackage::~CRawDataPackage()
{
#ifdef _WIN32
	DeleteCriticalSection( &m_Section );
#else
    pthread_mutex_destroy(&m_Section);
#endif
}

unsigned int CRawDataPackage::Append( void* lpBuffer,unsigned int uSize )
{
	//std::cout << "CRawDataPackage::Append " << lpBuffer << " " << uSize << std::endl;
	if( NULL == lpBuffer || 0 == uSize )
	{
		return m_Packages.size();
	}
	//std::cout << "CRawDataPackage::Append " << uSize << std::endl;
	LockPakcageList( true );
	RawDataAppend(lpBuffer, uSize);
	LockPakcageList( false );
	return m_Packages.size();
}

void CRawDataPackage::Clear()
{
	LockPakcageList( true );
	// Remove all packages
	m_Packages.erase( m_Packages.begin(),m_Packages.end() );
	// Remove all internal data
	LockPakcageList( false );
}

unsigned int CRawDataPackage::GetPackages( void* lpPackage,int size )
{
	if( NULL == lpPackage || 0 >= size )
	{
		return 0;
	}

	PRT_BUFF** lpPkg = (PRT_BUFF**)lpPackage;

	LockPakcageList( true );
	int nCount = size < (int)m_Packages.size() ? size : (int)m_Packages.size();
	for( int i=0;i<nCount;i++ )
	{
		lpPkg[i] = m_Packages.front();
		m_Packages.pop_front();
	}
	LockPakcageList( false );

	return nCount;
}

void CRawDataPackage::FreeMem( void* lpMemBlock )
{
	dlfree( lpMemBlock );
}

bool CRawDataPackage::GetProperty( long /*lFlags*/,unsigned int dwPropertyID,void* lpValue )
{
	if( NULL == lpValue )
	{
		return false;
	}

	switch( dwPropertyID )
	{
	default:
		break;
	}

	return false;
}

bool CRawDataPackage::SetProperty( long /*lFlags*/,unsigned int dwPropertyID,const void* lpValue )
{
	switch( dwPropertyID )
	{
	default:
		break;
	}
	return false;
}

void CRawDataPackage::LockPakcageList( bool bLock )
{
#ifdef _WIN32
	if( bLock )
	{
		EnterCriticalSection( &m_Section );
	}
	else
	{
		LeaveCriticalSection( &m_Section );
	}
#else
	if( bLock )
	{
		pthread_mutex_lock(&m_Section);
	}
	else
	{
		pthread_mutex_unlock(&m_Section );
	}
#endif

}

unsigned int CRawDataPackage::Package( void* lpInput,int nInputSize,void** lppMemBlock,int* lpSize )
{
	if( NULL == lppMemBlock || NULL == lpSize )
	{
		return DP_INVALID_PARAMETER;
	}

	if( NULL == lpInput || 0 == nInputSize )
	{
		*lppMemBlock = NULL;
		*lpSize = 0;
		return 0;
	}

	// Alloc memory
	int nSize = nInputSize;
	unsigned char* lpData = (unsigned char*)dlmalloc( nSize );
	if( NULL == lpData )
	{
		return DP_NO_ENUOUPH_MEMORY;
	}
	memcpy( lpData,lpInput,nInputSize);
	*lppMemBlock = lpData;
	*lpSize = nSize;
	return 0;
}

unsigned int CRawDataPackage::RawDataAppend( void* lpBuffer,unsigned int uSize  )
{
	m_lpPkg = Alloc_PRT_BUFF( uSize );
    if (m_lpPkg)
    {
	    memcpy( m_lpPkg->lpData,lpBuffer,uSize );
    				
	    m_Packages.push_back( m_lpPkg );
	    m_lpPkg = NULL;
    }
			
	return m_Packages.size();
}


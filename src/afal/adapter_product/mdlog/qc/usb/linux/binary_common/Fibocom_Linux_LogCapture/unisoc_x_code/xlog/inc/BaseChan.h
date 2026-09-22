#ifndef BASE_CHAN_H
#define BASE_CHAN_H

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include "typedef.h"
#include "ProtoChan.h"
#include "TTYComm2.h"
#include "SpLog.h"

//#pragma warning(push,3)
#include <list>
#include <vector>
//#pragma warning(pop)

using namespace std;

typedef enum _PROTOCOL_TYPE
{
    PROTOCOL_TYPE_DIAG = 0,
    PROTOCOL_TYPE_SMP,
    PROTOCOL_TYPE_U2S,
	PROTOCOL_TYPE_ARMTRACER,
	PROTOCOL_TYPE_DEBUGHOST
}PROTOCOL_TYPE;

typedef list< PRT_BUFF* > PRT_BUFF_LIST;

struct PRT_COND_T
{
	int ref_count;
	void* lpCond;
	void* lpSync;	// Synchronization object
	PRT_BUFF_LIST list;
};

#define INVALID_OBSERVER_ID 0xFFFFFFFF

typedef vector< IProtocolObserver* > OBSERVER_LIST;

class CBaseChannel : public IProtocolObserver
{
public:
	// ctor and dtor
	CBaseChannel();
	CBaseChannel(TCHAR *chName);
	virtual ~CBaseChannel();

public:
	// IProtocolChannel interface functions
	virtual BOOL	InitLog( char * pszLogName,
			                 uint32_t uiLogLevel=INVALID_VALUE);

	virtual BOOL	SetReceiver( ULONG  ulMsgId,
				                 BOOL    bRcvThread,
                          	     LPCVOID pReceiver );

	virtual void	GetReceiver( ULONG  &ulMsgId,
			                     BOOL &bRcvThread,
			                     LPVOID &pReceiver );

	virtual BOOL	Open( PCCHANNEL_ATTRIBUTE pOpenArgument, BOOL bWaitOpen = FALSE );

	virtual void	Close();

	virtual BOOL	Clear();

	virtual DWORD	Read( LPVOID lpData, DWORD dwDataSize,
	                      DWORD dwTimeOut, DWORD dwReserved = 0 );

	virtual DWORD	Write( LPVOID lpData, DWORD dwDataSize,DWORD dwReserved = 0  );

	virtual void	FreeMem( LPVOID pMemBlock );

	virtual BOOL	GetProperty( LONG lFlags, DWORD dwPropertyID,
		                         LPVOID pValue );

	virtual BOOL 	SetProperty( LONG lFlags, DWORD dwPropertyID,
                                 LPCVOID pValue );

	virtual int     AddObserver( IProtocolObserver * lpObserver );
	virtual bool    RemoveObserver( int nObserverId );

	// IProtocolObserver functions
	virtual int     OnChannelEvent( uint32_t event,void* lpEventData );
    virtual int     OnChannelData(void* lpData, uint32_t ulDataSize,void* reserved =0 );

    virtual int     GetProtocolType();

	virtual uint64_t WriteDataToFile(char * path, LPBYTE lpBuffer, DWORD dwDataSize);

	char * GetChannelName();

	void SetOutFileThreasHold(uint64_t ts);

protected:
	IProtocolObserver* GetNextObserver( int& nPos );
	int FindObserver( IProtocolObserver* lpObserver );
	void LockObserverList( bool bLock );

	BOOL StartProcessThread();
	void StopProcessThread();

	static void* ProcessThread(void* lpParam);
    void* ProcessData();

	BOOL CreateOutFile(); 

	virtual char* GetOutFileName();

protected:
	OBSERVER_LIST m_obsList;

	IProtocolPackage* m_lpProtocolPackage;

	bool m_bProcessThreadRun;

	CSpLog m_Log;
	unsigned int m_uLogLevel;
	unsigned int m_uLogType;
	bool m_bInitLogCalled;
	TCHAR m_szLogName[MAX_PATH];
	TCHAR m_chName[MAX_CH_NAME];

	unsigned int m_nInternalPackgeCap;
	bool         m_bClearKeepPkg;
	bool         m_bConnected;
	FILE* m_pOutFilef;
	uint64_t m_outFileThreasHold;
	uint64_t m_outFileSize;
	TCHAR m_pOutPath[MAX_PATH];
	TCHAR m_pOutFileName[MAX_PATH];

protected:
	// Member variables for synchronization
	pthread_mutex_t  m_csObserver;
	pthread_t        m_hProcessThread;

	// Endian
	unsigned char m_nHiLvlEndian;
	unsigned char m_nLoLvlEndian;
	bool m_bEndianEvent;		// True means the channel just decided which endian is used,it will
								// generate a endian event

    //LPWRITEDATACALLBACK m_lpWrtCallback; // X 2016.5.9 add write data callback
    //LONG m_cbInput;

	CTTYComm2 m_LowerChannel;
};

PRT_BUFF* Alloc_PRT_BUFF( unsigned int size );

#endif

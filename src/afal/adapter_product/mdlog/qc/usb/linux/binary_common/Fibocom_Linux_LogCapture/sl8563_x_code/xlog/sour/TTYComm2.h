/* *****************************************************
SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0

Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
Licensed under the Unisoc General Software License, version 1.0 (the License);
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
Software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OF ANY KIND, either express or implied.
See the Unisoc General Software License, version 1.0 for more details.
******************************************************* */

#ifndef TTYCOMM_H
#define TTYCOMM_H

#include "SpLog.h"
#include "ICommChannel.h"
#include "MemoryMgr.h"

#include <termios.h>
#include <pthread.h>

typedef struct _TTY_CONFIG
{
    uint32_t  dwPortNum;
    uint32_t  dwBaudRate;
    char      szDevPath[64];
    char      szReserved[64];

}TTY_CONFIG,*TTY_CONFIG_PTR;

class CTTYComm2: public ICommChannel
{
    public:
        CTTYComm2();
        virtual ~CTTYComm2();
        virtual bool	InitLog( char * pszLogName,
			                      uint32_t uiLogLevel=INVALID_VALUE);
        virtual bool      Open( PCCHANNEL_ATTRIBUTE pOpenArgument, bool bWaitOpen = false );
        virtual void      Close();
        virtual bool      Clear();
        virtual bool      Drain();
        virtual uint32_t Read( uint8_t* lpData,
                        uint32_t ulDataSize,
                        uint32_t dwTimeOut,
                        uint32_t dwReserved = 0 );

        virtual uint32_t Write(uint8_t* lpData,
                        uint32_t ulDataSize,
                        uint32_t dwReserved = 0  );

        virtual void	  FreeMem( void* pMemBlock );

        virtual bool	  GetProperty( int32_t lFlags,
                               uint32_t dwPropertyID,
                               void* pValue );

        virtual bool 	  SetProperty( int32_t lFlags,
                               uint32_t dwPropertyID,
                               void* pValue );
        virtual bool    SetObserver( IProtocolObserver * lpObserver );

        virtual bool    IsConnect();

        void _Read();

    protected:
        bool OpenLogFile( int32_t dwPort , char * pDevPath);
        void CloseLogFile();
        bool LoadConfig();
        bool SetTTYAtt(int fd, uint32_t baud);

	static void* GetRecvFunc(void* lpParam);
    	void* RecvFunc();

    	bool CreateRecvThread();
    	void DestroyRecvThread();

    public:
        int         m_fdTTY;                   /* tty file descriptor */
        uint32_t   m_dwPort;
        tcflag_t    m_baudOld;
    private:
        CMemoryMgr          m_MemMgr;
        CSpLog              m_log;
        IProtocolObserver * m_pObserver;
        bool                m_bExitThread;
        pthread_t           m_hRecvThread;

        uint32_t m_dwErrorCode;

        struct termios m_tio;       /* termios structure for the port */
        struct termios m_tioOld;    /* old termios structure */

        uint32_t m_uiLogLevel;
        bool     m_bConnected;
        bool     m_bUseMempool;
        char     m_szLogName[_MAX_PATH];

        TTY_CONFIG m_tTTY_Diag;
        TTY_CONFIG m_tTTY_Smp;
};

#endif // TTYCOMM_H

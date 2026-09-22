#ifndef NETCOMM_H
#define NETCOMM_H

#include "SpLog.h"
#include "ICommChannel.h"
#include "MemoryMgr.h"

#include <termios.h>
#include <pthread.h>
#include <cstring>

using namespace std;

//网络设置
class NET_CFG {
public:
    char      szIPAds[64];   //IP地址
    uint      unPortNo;      //IP端口号
    bool      IsUse;         //是否使用

    NET_CFG()
    {
        strcpy(szIPAds, "127.0.0.1");
        unPortNo = 8080;
        IsUse = false;
    }

};

//配置信息
class CFG_INFO {
public:
    int         nDataType;                //数据类型 0-未知 1-log 2-dump
    NET_CFG     netDiag;                  //
    NET_CFG     netSMP;                   //
    NET_CFG     netU2S;                   //
    uint32_t    uiLogLevel;               //本地log等级
    uint32_t    uiAssetr;                 //自动Dump    1-自动dump
    char        szDiagLogName[_MAX_PATH]; //本地Diag的log路径名称
    char        szSMPLogName[_MAX_PATH];  //本地SMP的log路径名称
    char        szU2SLogName[_MAX_PATH];  //本地U2S的log路径名称

    CFG_INFO()
    {
        nDataType = 0;
        uiLogLevel = 5;
        uiAssetr = 0;
        memset(szDiagLogName, 0x00, _MAX_PATH);
        memset(szSMPLogName, 0x00, _MAX_PATH);
        memset(szU2SLogName, 0x00, _MAX_PATH);
    }
};

class CNETComm : public ICommChannel
{
public:
    CNETComm();
    virtual ~CNETComm();

    virtual bool InitLog( char * pszLogName, uint32_t uiLogLevel=INVALID_VALUE);
    virtual bool Open( PCCHANNEL_ATTRIBUTE pOpenArgument, bool bWaitOpen = false );
    virtual void Close();
    virtual bool Clear();
    virtual bool Drain();

    virtual uint32_t Read( uint8_t* lpData, uint32_t ulDataSize, uint32_t dwTimeOut, uint32_t dwReserved = 0 );
    virtual uint32_t Write(uint8_t* lpData, uint32_t ulDataSize, uint32_t dwReserved = 0  );

    virtual void FreeMem( void* pMemBlock );
    virtual bool GetProperty( int32_t lFlags, uint32_t dwPropertyID, void* pValue );
    virtual bool SetProperty( int32_t lFlags, uint32_t dwPropertyID, void* pValue );
    virtual bool SetObserver( IProtocolObserver * lpObserver );

protected:
    //打开本地日志文件
    bool OpenLogFile( int32_t dwPort , char * pDevPath);
    //打开本地日志文件 flen-日志文件名  uiLL-日志记录等级
    bool OpenLogFile(char * fln, int32_t uiLL);
    //关闭本地日志文件
    void CloseLogFile();

    //加载配置参数
    bool LoadConfig();

private:
    //打开Diag通讯
    bool OpenDiagComm();
    //关闭Diag通讯
    bool CloseDiagComm();

    //打开Smp通讯
    bool OpenSmpComm();
    //关闭Smp通讯
    bool CloseSmpComm();

    //打开U2S通讯
    bool OpenU2SComm();
    //关闭U2S通讯
    bool CloseU2SComm();

    //网络连接
    int connectNet(string strIPAddess, unsigned short nPortNo);
    //关闭端口
    void NetClient::closeNet()
    {
        if (m_IsNetOpen)
        {
            shutdown(m_sockfd, 2);
            m_IsNetOpen = false;
        }
    }

private:
    CFG_INFO m_cfg;         //配置参数

    bool     m_bConnected;

    uint32_t m_dwErrorCode;     //出错代码

    CMemoryMgr  m_MemMgr;

private:

    CSpLog m_log;
    IProtocolObserver* m_pObserver;




};

#endif // NETCOMM_H

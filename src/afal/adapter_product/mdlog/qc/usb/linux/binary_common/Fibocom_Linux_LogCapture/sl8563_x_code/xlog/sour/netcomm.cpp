#include "netcomm.h"

#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ExePathHelper.h"
extern "C"
{
#include "confile.h"
}

extern BOOL g_bAutoProcAssert;

CNETComm::CNETComm()
{

}

CNETComm::~CNETComm()
{

}

bool CNETComm::InitLog( char * pszLogName, uint32_t uiLogLevel )
{
    if(m_bConnected)
    {
        return false;
    }

    if(pszLogName != NULL && strlen(pszLogName)>= _MAX_PATH)
    {
        m_dwErrorCode = CH_E_INVALIDARG;
        return false;
    }

    if(pszLogName != NULL && strlen(pszLogName) != 0)
    {
        strcpy(m_cfg.szDiagLogName, pszLogName);
        strcpy(m_cfg.szSMPLogName, pszLogName);
        strcpy(m_cfg.szU2SLogName, pszLogName);
    }

    m_cfg.uiLogLevel = uiLogLevel;

    m_dwErrorCode = CH_S_OK;

    return true;
}

bool CNETComm::Open( PCCHANNEL_ATTRIBUTE pOpenArgument, bool bWaitOpen )
{
    if(NULL == pOpenArgument || CHANNEL_TYPE_SOCKET != pOpenArgument->ChannelType  )
    {
        m_dwErrorCode = CH_E_INVALIDARG;
        return false;
    }

    m_dwErrorCode = CH_S_OK;

    // first load configure
    // mainly for user can set not to create log folder and files.
    if(!LoadConfig())
    {
        m_dwErrorCode = CH_E_LOAD_CONFIG_FAILED;
        return false;
    }

    char *pDevPath  = NULL;
    uint32_t dwPort = 0;
/*
    if (NULL == pOpenArgument->tty.pDevPath)
    {
        switch(pOpenArgument->tty.nProtocolType)
        {
        case 0: // Diag
            {
                dwPort   =  m_tNET_Diag.dwPortNum;
                pDevPath =  m_tNET_Diag.szDevPath;
            }
            break;
        case 1: // Smp
            {
                dwPort   =  m_tNET_Smp.dwPortNum;
                pDevPath =  m_tNET_Smp.szDevPath;
            }
            break;
        case 2: // u2s
            {
                dwPort   =  m_tNET_Smp.dwPortNum;
                pDevPath =  m_tNET_Smp.szReserved;
            }
            break;
        default:
            break;
        }
    }
    else
    {
        dwPort   =  pOpenArgument->Socket.dwPort;
        in_addr netin;
        netin.s_addr = pOpenArgument->Socket.dwIP;
        pDevPath =  inet_ntoa(netin);
    }
*/
    assert( dwPort > 0 );

    if(dwPort == 0 || pDevPath == NULL) //lint !e774
    {
        m_dwErrorCode = CH_E_INVALIDARG;
        return false;
    }

    // second open log
    if(!OpenLogFile( dwPort, pDevPath ))
    {
        m_dwErrorCode = CH_E_OPEN_LOG_FAILED;
        return false;
    }

    // Init memory pool
    m_MemMgr.Init(m_bUseMempool);

    // opening the file

    int fd = -1;
    if(m_pObserver)
    {
        // user wants event driven reading
#if 0
        //here to wait a moment for really dev checking starting
        printf("Please Press the power key and volume down key to start downloading.");
        printf("Please input S to notice the programme that you are ready.");
        char c =' ';
        do{
        c = getchar();
        }while(c != 's' );
#endif
        if (bWaitOpen)
        {
            usleep(1 * 1000);

            while(fd == -1)
            {
                fd = socket(AF_INET, SOCK_STREAM, 0);

                if (fd == -1)
                {
                    printf("\n[Info]: specified device \"%s\" is not available, please wait...\n", pDevPath);
                    usleep(10 * 1000);
                }
            }
        }
        else
        {
            fd = open(pDevPath,O_RDWR|O_NOCTTY,0666);
        }
    }
    else
    {
        // the read/write operations will be bloking
        fd = open(pDevPath,O_RDWR|O_NOCTTY);
    }

    // oops, cannot open
    if (fd == -1) {
        printf("\n[Error]: can not open device \"%s\", error code: 0x%x,\"%s\"\n",pDevPath,errno,strerror(errno) );
        m_log.LogFmtStr(SPLOGLV_ERROR,"Open: can not open device \"%s\", error code: %d,\"%s\"",pDevPath,errno,strerror(errno));
        CloseLogFile();
        return false;
    }

    if(-1 == flock(fd, LOCK_EX|LOCK_NB))
    {
        m_log.LogFmtStr(SPLOGLV_ERROR,"device \"%s\" has been opened by other process,error code: %d,\"%s\".",pDevPath,errno,strerror(errno));
        CloseLogFile();
        close(fd);
        return false;
    }

    m_log.LogFmtStr(SPLOGLV_INFO,"Open: open channel \"%s\" success.",pDevPath);

    if( ioctl(fd,TIOCEXCL) == -1)
    {
        m_log.LogFmtStr(SPLOGLV_ERROR,"Open: set TIOCEXCL failed,error code: %d,\"%s\".",errno,strerror(errno));
        CloseLogFile();
        flock(fd, LOCK_UN);
        close(fd);
        return false;
    }

    // we remember old termios
    if(tcgetattr(fd,&(m_tioOld)) == -1)
    {
        m_log.LogFmtStr(SPLOGLV_ERROR,"Open: tcgetattr failed,error code: %d,\"%s\".",errno,strerror(errno));
        CloseLogFile();
        flock(fd, LOCK_UN);
        close(fd);
        return false;
    }

    memcpy(&m_tio,&m_tioOld,sizeof(m_tio));

    // now we set new values
    if(!SetTTYAtt(fd,dwBaud))
    {
        CloseLogFile();
        close(fd);
        return false;
    }

    m_fdTTY = fd;
    m_bConnected = true;
    if(m_pObserver)
    {
        if( !CreateRecvThread())
        {
            m_log.LogRawStr(SPLOGLV_ERROR,"Open: CreateRecvThread failed.");
            Close();
            return false;
        }
    }

    return TRUE;

}

void CNETComm::Close()
{

}
bool CNETComm::Clear()
{
    return false;
}
bool CNETComm::Drain()
{
    return false;
}

uint32_t CNETComm::Read( uint8_t* lpData, uint32_t ulDataSize, uint32_t dwTimeOut, uint32_t dwReserved )
{
    return 0;
}
uint32_t CNETComm::Write(uint8_t* lpData, uint32_t ulDataSize, uint32_t dwReserved )
{
    return 0;
}

void CNETComm::FreeMem( void* pMemBlock )
{

}
bool CNETComm::GetProperty( int32_t lFlags, uint32_t dwPropertyID, void* pValue )
{
    return fals, bool IsConnecte;
}
bool CNETComm::SetProperty( int32_t lFlags, uint32_t dwPropertyID, void* pValue )
{
    return false;
}
bool CNETComm::SetObserver( IProtocolObserver * lpObserver )
{
    return false;
}

bool CNETComm::LoadConfig()
{
    bool bret = false;
    INI_CONFIG *config = NULL;

    GetExePath helper;
    std::string strIniPath = helper.getExeDir();
    strIniPath.insert(0,"/");
    strIniPath += "Channel.ini";

    m_log.LogFmtStr(SPLOGLV_ERROR,"IniFile Path \"%s\"", strIniPath.c_str());

    config = ini_config_create_from_file(strIniPath.c_str(),0);
    if(config)
    {
        //get log level ini config
        m_cfg.uiLogLevel = ini_config_get_int(config,"Log","Level",0);

        // get diag ini config
        m_cfg.netDiag.unPortNo  = ini_config_get_int(config,"DIAG","NetPort",8080);
        strcpy(m_cfg.netDiag.szIPAds, ini_config_get_string(config,"DIAG","NetIP","NULL"));
        if (strcmp(m_cfg.netDiag.szIPAds, "NULL"))
        {
            m_cfg.netDiag.IsUse = true;

            //生成Diag本地日志文件名
            if(strlen(m_cfg.szDiagLogName) == 0 ) // not initialized, so set default value
            {
                sprintf( m_cfg.szDiagLogName, "NetComm_%s_ID%d", PathFindFileName(m_cfg.netDiag.szIPAds), m_cfg.netDiag.unPortNo);
            }
        }

        // get smp ini config
        m_cfg.netSMP.unPortNo = m_cfg.netU2S.unPortNo = ini_config_get_int(config,"SMP","NetPort", 8080);
        strcpy(m_cfg.netSMP.szIPAds, ini_config_get_string(config,"SMP","NetIP","NULL"));
        if (strcmp(m_cfg.netSMP.szIPAds, "NULL"))
        {
                m_cfg.netSMP.IsUse = true;
                m_cfg.nDataType = 1;

                //生成SMP本地日志文件名
                if(strlen(m_cfg.szSMPLogName) == 0 ) // not initialized, so set default value
                {
                    sprintf( m_cfg.szSMPLogName, "NetComm_%s_ID%d", PathFindFileName(m_cfg.netSMP.szIPAds), m_cfg.netSMP.unPortNo);
                }
        }
        else
        {
            strcpy(m_cfg.netU2S.szIPAds, ini_config_get_string(config,"SMP","NetIP_Dump_Mode","NULL"));
            if (strcmp(m_cfg.netU2S.szIPAds, "NULL"))
            {
                    m_cfg.netU2S.IsUse = true;
                    m_cfg.nDataType = 2;

                    //生成U2S本地日志文件名
                    if(strlen(m_cfg.szU2SLogName) == 0 ) // not initialized, so set default value
                    {
                        sprintf( m_cfg.szU2SLogName, "NetComm_%s_ID%d", PathFindFileName(m_cfg.netU2S.szIPAds), m_cfg.netU2S.unPortNo);
                    }
            }
        }

        // auto dump assert flag
        m_cfg.uiAssetr = g_bAutoProcAssert = ini_config_get_int(config,"ASSERT","AutoDump",1);

        ini_config_destroy(config);

        bret = true;
    }

    return bret;
}

//打开本地日志文件
bool CNETComm::OpenLogFile( int32_t dwPort , char * pDevPath)
{
    /*
    CloseLogFile();

    char szLogFileName[ _MAX_PATH ] = {0};
    if(strlen(m_cfg.szLogName)==0 ) // not initialized, so set default value
    {
        sprintf( szLogFileName, "NETComm_%s_ID%d", PathFindFileName(pDevPath), dwPort);
        strcpy(m_cfg.szLogName, szLogFileName);
    }

    if( m_log.Open(m_szLogName,m_uiLogLevel))
    {
        //GetModuleFileName( NULL,szLogFileName, MAX_PATH );
        GetExePath helper;
        std::string strDir = helper.getExeDir();
        std::string strName = helper.getExeName();

        m_log.LogFmtStr(SPLOGLV_ERROR,"===%s%s", strDir.c_str(), strName.c_str() );
        return true;
    }
    */
    return false;
}

//打开本地日志文件 flen-日志文件名  uiLL-日志记录等级
bool CNETComm::OpenLogFile(char * fln, int32_t uiLL)
{
    bool bret = false;

    CloseLogFile();

    if( m_log.Open(fln, uiLL))
    {
        GetExeP, bool IsConnectath helper;
        std::string strDir = helper.getExeDir();
        std::string strName = helper.getExeName();

        m_log.LogFmtStr(SPLOGLV_ERROR,"===%s%s", strDir.c_str(), strName.c_str() );
        return true;
    }

    return bret;
}

//网络连接
int CNETComm::connectNet(string strIPAddess, unsigned short nPortNo)
{
    int ret = 0;
    int sockRet = -1;

    struct sockaddr_in netadr;
    bzero(&netadr, sizeof(netadr));

    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (m_sockfd != -1)
    {
        netadr.sin_family = AF_INET;
        netadr.sin_addr.s_addr = inet_addr(strIPAddess.c_str());
        netadr.sin_port = htons(nPortNo);

        if (connect(m_sockfd, (struct sockaddr *)&netadr, sizeof(netadr)) != -1)
        {
            m_IsNetOpen = true;
            ret = 1;
        }
    }

    return ret;
}

//关闭端口
bool CNETComm::closeNet(int sockfd, bool IsConnect)
{
    bool bRlt = true;
    
    if (IsConnect)
    {
        shutdown(sockfd, 2);
        bRlt = false;
    }
    
    return bRlt;
}

//关闭本地日志文件
void CNETComm::CloseLogFile()
{
    m_log.Close();
}

//打开Diag通讯
bool CNETComm::OpenDiagComm()
{
    return true;
}

//关闭Diag通讯
bool CNETComm::CloseDiagComm()
{
    return true;
}

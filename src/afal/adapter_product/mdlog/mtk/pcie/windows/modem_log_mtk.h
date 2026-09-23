#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_MTK_PCIE_WINDOWS_MODEM_LOG_MTK_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_MTK_PCIE_WINDOWS_MODEM_LOG_MTK_H_

#include "modem_mtk.h"
#include "common.h"
#include "modem_log.h"
#include "common/modem.h"
#include "common/commonprocess.h"
#include "common/modem_log_tools.h"
#include "api/MiniDebugLogger/muxz.h"
#include "api/MiniDebugLogger/SpecialTraceParser.h"
#include "api/MiniDebugLogger/LogFileWriter.h"
#include "api/MiniDebugLogger/TargetExceptionParser.h"
#include "api/MiniDebugLogger/TargetExceptionReport.h"
#include "api/MiniDebugLogger/MuxzFileWriter.h"
#include "api/MiniDebugLogger/ADBShell.h"
#include <memory>
#include <qobjectdefs.h>

#define SYNC_MD_INFO_AT_EACH_FILE_CREATE

namespace afal {

    // Enumeration of command types that can be sent to the modem
enum CommandType {
    ConnectCmd = 0,            // Command to connect to the modem
    AutoMemoryDumpDHLCmd,      // Command to enable automatic memory dump on DHL
                               // trigger
    AutoMemoryDumpUPSCmd,      // Command to enable automatic memory dump on UPS trigger
    RebootMDCmd,               // Command to reboot the modem
    QueryUEInfoCmd,            // Command to query UE information
    DisconnectCmd,             // Command to disconnect from the modem
    ConnectWithoutAutoDumpCmd, // Command to connect to the modem without
                               // automatic memory dump enabled
    ComandTyepCount // Maximum value for this enumeration, used for range
                    // validation
};

enum COMPRESSTYPE {
    COMPRESS_SINGLE_FILE = 0,
    COMPRESS_FOLDER = 1,
    COMPRESS_MUTIPLE_FILES = 2,
    COMPRESS_MAX
};

struct CommandRawData {

    CommandType cmdType;
    DWORD cmdLen;
    unsigned char* const rawData;

};

class ModemLoggerImplMtkPCIE : public ModemLoggerImpl {
  public:
    bool m_bSapStop;
    bool bUserStop;
    UINT32 m_iSapFileNum;

    ModemLoggerImplMtkPCIE(const ModemLoggerImplMtkPCIE&) = delete;
    ModemLoggerImplMtkPCIE(ModemLoggerImplMtkPCIE&&) = delete;
    ModemLoggerImplMtkPCIE& operator=(const ModemLoggerImplMtkPCIE&) = delete;
    ModemLoggerImplMtkPCIE& operator=(ModemLoggerImplMtkPCIE&&) = delete;
    ModemLoggerImplMtkPCIE() = delete;
    ~ModemLoggerImplMtkPCIE() override = default;

    ModemLoggerImplMtkPCIE(ModemType modem_type);

    ModemLoggerImplMtkPCIE(ModemType modem_type,
                         std::unique_ptr<DeviceFactory> device_factory);

    bool Status() override;
    bool modeStatus() override;
    bool Start() override;
    bool Stop() override;
    bool Enable() override;
    bool Disable() override;
    bool SetDebugPort(bool enable) override;
    bool CheckDebugPortStatus() override;
    bool sapStart() override;
    bool sapStop() override;

  protected:
    std::unique_ptr<DeviceFactory> device_factory;

    std::unique_ptr<ModemMTK> modem;

    std::unique_ptr<Device> diag_port;

    std::unique_ptr<ModemLogTools> modem_log_tools;

  private:
    bool m_bModemTraceStop;
    HANDLE m_hComm;
    HANDLE m_hSap;
    HANDLE mProcessHandle;
    bool g_bPrintAfound = false;
    volatile HANDLE m_hListenThread;
    TargetExceptionParser* mTargetExceptionParser;
    TargetExceptionReport* mTargetExceptionReport;
    LogFileWriter* mLogFileWriter = NULL;
    bool OpenPort(WCHAR* devicePath);
    bool InitPort(WCHAR* devicePath);
    bool ClosePort();
    bool OpenListenMDTraceThread();
    BOOL SendStartLoggingPattern();
    std::wstring
    ExecuteLogread(ADBShell* adbShell,
                   const TCHAR* command, bool bBoot,
                                UINT32 iTimeout, WCHAR* strFilePath,
                                size_t maxFileSize);
    std::wstring
    ExecuteLogread( ADBShell* adbShell,
                   std::wstring& command, bool bBoot, UINT32 iTimeout,
                                WCHAR* strFilePath, size_t maxFileSize);
    void WaitLogreadExecute(
                            ADBShell* adbShell,
                        bool bBoot,
                            UINT32 iTimeout,
                            WCHAR* strFilePath,
                            size_t maxFileSize);
    void WaitLogreadFThreadExecute(
         ADBShell* adbShell,
        bool bBoot,
                                   UINT32 iTimeout, WCHAR* strFilePath,
                                   size_t maxFileSize);
    /**
        This is a thread to obtain sap trace by ADB tool.

        @return 0
    */
    static unsigned __stdcall FetchSapBootTraceByADBThread(void* param);

    /**
        Send pattern data command to comport.
        Normally it is for send pattern data before start modem logging.

        @param 	HANDLE hFile,
        @param 	CommandType type,
        @param 	_Out_opt_ LPDWORD lpNumberOfBytesWritten,
        @param 	_Inout_opt_ LPOVERLAPPED lpOverlapped

        @return TRUE: data send OK
        @return FALSE: data send FAILED
    */
    static BOOL SendCommand(HANDLE hFile, CommandType type,
                            _Out_opt_ LPDWORD lpNumberOfBytesWritten,
                            _Inout_opt_ LPOVERLAPPED lpOverlapped);

    /**
    Print pattern command data type

    @param CommandType type

    @return TCHAR type
    */
    static const TCHAR* GetCommandTypeText(CommandType type);

    /**
        This is the main thread implement for reading MD trace from com port.

        @param void* pParam

        @return UINT
    */
    static UINT WINAPI ListenMDTraceThread(void* pParam);

    /**
        Control MD trace file number if user enabled circular logging mode.
        Deleted file number which outof wide.
        Call this function at every file creation.

        @param UINT32 ModemFileCount: intput number.
        @return none
    */
    static void
    ModemTraceFileControl(UINT32 ModemFileCount,
                          UINT32 ModemFileMaxNum, std::string& mdPath);

    static unsigned __stdcall CompressSingleModemLogs(void* pParam);

    /**
        Save modem file name to global value

        @param std::wstring workingFile

        @return none
    */
    static void StoreModemFileInfo(std::wstring workingFile);

    /**
        Get data bytes from serial port

        @param none

        @return Remained bytes in queue
    */
    UINT GetBytesInCOM();

    /**
    Close listening thread

    @param None

    @return Success: true, failed: false
    */
    bool CloseListenTread();

    /**
     * @brief Initializes and starts the Exception Parser.
     *
     * This function creates a new Exception Parser if it doesn't already exist.
     * It sets up the Log File Writer, creates a new Target Exception Report,
     * and starts the Exception Parser.
     */
    void StartExceptionParser();

    /**
     * @brief Parses exception data using the current Exception Parser.
     *
     * This function sends the received exception data buffer to the active
     * Exception Parser for processing, if the Exception Parser exists.
     *
     * @param buf Pointer to the buffer containing the exception data.
     * @param bufSize Size of the buffer containing the exception data.
     */
    void ParserException(const unsigned char* buf, const int bufSize);

    /**
     * @brief Stops and releases the Exception Parser and its associated report
     * object.
     *
     * This function stops the active Exception Parser, releases its memory, and
     * sets its pointer to nullptr. It also releases the memory of the
     * associated Exception Report object and sets its pointer to nullptr.
     */
    void StopExceptionParser();

     	/**
        Compress ALL sap log thread

        @param none

        @return 0
        */
    static unsigned __stdcall CompressSapTraceAllThread(void* p);
    static unsigned __stdcall CompressSapTraceThread(void* pNumber);

    static void SapFileControl(UINT32 sAPFileCount,
                               UINT32 maxFileNum, std::wstring folderPath);

    /**
        sap logging start

        @param bool deviceResume: previouse device status is suspend resume

        @return 0
    */
    static unsigned __stdcall SapTraceStartThread(void* p);

   /**
    This is a thread to obtain sap trace by ADB tool.

    @param void* param: UNREFERENCED_PARAMETER

    @return 0
    */
    static unsigned __stdcall FetchSapTraceByADBThread(void* param);

    /**
        Stop sap log

        @param suspend: false means it is not a suspend case. true means it is a
       suspend case.

        @return 0
    */
    static VOID SapTraceStop(ModemLoggerImplMtkPCIE* modemLogCaptureObj);

    static unsigned __stdcall CreateThreadSapTraceStop(void* param);

   

    	/**
    Compress target file.
    targetPath -> targetPath.zip.tmp -> targetPath.zip -> delete targetPath

    @param	WCHAR *targetPath: Target folder/file path
            WCHAR *filename: Target folder/file name, use for rename
            UINT compressTimeout: Compress timeout
            COMPRESSTYPE compressType: Type, folder or file

    @return TRUE: compress OK and delete the folder
            FALSE: Something WRONG
**/
    static BOOL CompressFolderFiles(const WCHAR targetPath[MAX_PATH],
                                    const WCHAR filename[MAX_PATH],
                                    const UINT compressTimeout,
                                    const COMPRESSTYPE compressType,
                                    bool bIgnoreError = false);
};

} // namespace afal

#endif // AFAL_ADAPTER_PRODUCT_MDLOG_MTK_PCIE_WINDOWS_MODEM_LOG_MTK_H_
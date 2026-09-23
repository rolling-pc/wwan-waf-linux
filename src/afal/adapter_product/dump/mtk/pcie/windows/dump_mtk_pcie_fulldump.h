#ifndef AFAL_ADAPTER_PRODUCT_DUMP_MTK_PCIE_WIN_FULLDUMP_H_
#define AFAL_ADAPTER_PRODUCT_DUMP_MTK_PCIE_WIN_FULLDUMP_H_

#include <QThread>
#include <QSerialPort>
#include <QFile>
#include <QString>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <process.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <thread>
namespace afal {

    class FullRamDumpWorker : public QThread
    {
        Q_OBJECT
    public:
        explicit FullRamDumpWorker(const QString &portName,
                                   const QString &saveDir,
                                   QObject *parent = nullptr);
        ~FullRamDumpWorker() override;
    
        void stop();
    
    protected:
        void run() override;
    
    private:
        QString m_portName;
        QString m_saveDir;
        std::atomic<bool> m_running;
        HANDLE m_hFullRamPort;
    
        bool PrepareGetFullRamDump(HANDLE handle);
    
        UINT GetBytesInCOM(HANDLE hcom);
        bool ReadChar(char &cRecved, HANDLE hcom);

    };    

} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_DUMP_MTK_PCIE_WIN_FULLDUMP_H_
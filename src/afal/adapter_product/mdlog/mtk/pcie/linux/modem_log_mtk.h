#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_LOG_MTK_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_LOG_MTK_H_

#include "common.h"
#include "modem_log.h"
#include "common/modem.h"
#include "common/commonprocess.h"
#include "modem_mtk.h"
#include "common/modem_log_tools.h"
#include <memory>
#include <atomic>
#include <qobjectdefs.h>
#include <QFile>
#include <QMutex>
namespace afal {

  class AdbLogReader : public QObject
  {
      Q_OBJECT
  
  public:
      explicit AdbLogReader(QObject* parent = nullptr);
      ~AdbLogReader();
  
      void start(const QString& logPath);
      void stop();

  signals:
      void newLog(const QString& log);
      void errorLog(const QString& err);
      void finished();
  
  protected:
      virtual QString program();               
      virtual QStringList arguments();
      QProcess* process = nullptr;

  private slots:
      void onReadyRead();
      void onErrorRead();
      void onFinished(int exitCode, QProcess::ExitStatus status);
  
  private:
      void rotateLog();
      int getNextIndex();
      QString m_logPath; 
      QFile m_logFile;
      QByteArray m_buffer; 
      qint64 m_maxSize = 5 * 1024 * 1024; // 5MB
      void enableDeviceLog(bool enable);  
      std::atomic<bool> m_stopping{false};
      QMutex m_mutex;
      int m_flushCounter = 0;
      int m_logCounter = 0;
  };
  
class ModemLoggerImplPcie : public ModemLoggerImpl {

  public:
    ModemLoggerImplPcie(const ModemLoggerImplPcie&) = delete;
    ModemLoggerImplPcie(ModemLoggerImplPcie&&) = delete;
    ModemLoggerImplPcie& operator=(const ModemLoggerImplPcie&) = delete;
    ModemLoggerImplPcie& operator=(ModemLoggerImplPcie&&) = delete;
    ModemLoggerImplPcie() = delete;
    ~ModemLoggerImplPcie() override = default;

    ModemLoggerImplPcie(ModemType modem_type);

    ModemLoggerImplPcie(ModemType modem_type,
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
    bool originMode;
    std::unique_ptr<ModemLogTools> modem_log_tools;
    std::unique_ptr<AdbLogReader> adbLogReader;
 };


} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_LOG_MTK_H_
#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_LOG_TOOL_MTK_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_LOG_TOOL_MTK_H_

#include <common/commonprocess.h>
#include "common/file.h"
#include "common/modem_log_tools_impl.h"
#include "common/modem.h"
#include <memory>
#include <mutex>
#include <qobjectdefs.h>
#include <qthread.h>
#include <deque>

namespace afal {

class ModemLogCapture : public QObject {
    Q_OBJECT
    public:
      ModemLogCapture(const ModemLogCapture&) = delete;
      ModemLogCapture(ModemLogCapture&&) = delete;
      ModemLogCapture& operator=(const ModemLogCapture&) = delete;
      ModemLogCapture& operator=(ModemLogCapture&&) = delete;

      ModemLogCapture(QString path, QString out_path, int file_max_size);
      ~ModemLogCapture() override = default;
      
      bool Init(bool portInit);
      void Start();
      void Stop();
      void WriterLoop();
    signals:
      void LogFileCreatedSignal(QString path);
    private:
    void CreateNewFile();
      //std::unique_ptr<int> pipe_fd;
      std::unique_ptr<int[]> pipe_fd;
      ScopedFd fd, pipe_read, pipe_write;
      std::string port;
      QString out_path;
      qint64 file_max_size;
      std::unique_ptr<QFile> log_file;

      std::thread writer_thread;
      std::mutex queue_mutex;
      std::mutex file_mutex;
      std::condition_variable queue_cv;
      std::deque<std::vector<char>> data_queue;

      std::atomic<bool> stop_flag{false};
};

class ModemLogToolsMTK : public ModemLogToolsImpl {
    Q_OBJECT
  public:
    ModemLogToolsMTK() = delete;
    ModemLogToolsMTK(const ModemLogToolsMTK&) = delete;
    ModemLogToolsMTK(ModemLogToolsMTK&&) = delete;
    ModemLogToolsMTK& operator=(const ModemLogToolsMTK&) = delete;
    ModemLogToolsMTK& operator=(ModemLogToolsMTK&&) = delete;
     ~ModemLogToolsMTK() override = default;
    
    ModemLogToolsMTK(ModemLogger::Config *config);
    bool Init(bool portInit) override;
  protected:
    void run() override;
    std::unique_ptr<ModemLogCapture> capture;

    bool WaitStart(int timeout) override;
};

std::unique_ptr<ModemLogTools> CreateMTKTools(ModemType modem_type,
                                                 ModemLogger::Config* config);

} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_MDLOG_MTK_MODEM_LOG_TOOL_MTK_H_
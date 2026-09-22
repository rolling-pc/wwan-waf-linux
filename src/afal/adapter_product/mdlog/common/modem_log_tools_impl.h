#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_COMMON_MODEM_LOG_TOOLS_IMPL_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_COMMON_MODEM_LOG_TOOLS_IMPL_H_

#include "common/compress.h"
#include "modem_log_tools.h"
#include "common/proccommon.h"
#include "common/commonprocess.h"
#include "modem_log_interface.h"

#include <memory>
#include <mutex>
#include <qobjectdefs.h>
#include <set>
#include <utility>
#include <QQueue>

#include <qdatetime.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qfilesystemwatcher.h>
#include <qlist.h>
#include <QSemaphore>

namespace afal {

class ModemLogToolsImpl : public ModemLogTools,
                          public ProcessWithOutput::Delegate {
    Q_OBJECT
  public:
    ModemLogToolsImpl() = delete;
    ModemLogToolsImpl(const ModemLogToolsImpl&) = delete;
    ModemLogToolsImpl(ModemLogToolsImpl&&) = delete;
    ModemLogToolsImpl& operator=(const ModemLogToolsImpl&) = delete;
    ModemLogToolsImpl& operator=(ModemLogToolsImpl&&) = delete;

    ModemLogToolsImpl(ModemLogger::Config* config, QString program,
                      QString suffix)
        : config(config), program(std::move(program)),
          suffix(std::move(suffix)) {

    }
    ~ModemLogToolsImpl() override = default;

    void HandleProcessOutput(QString output) override;
    void HandleProcessFinished(int exitCode,
                               QProcess::ExitStatus exitStatus) override;
    void HandleCompletedLogFiles(const QFileInfo& file_info);
    bool Start(ModemType modemType) override;
    bool Stop() override;
    virtual bool Init(bool portInit = false) { return true; }
    void LastLogCompress();
    void OnDirectoryChanged(const QString& path);
  signals:
    void compressTimeout();

  protected:
    void run() override;
    void WaitCompressTask(int timeout_ms);
    virtual bool WaitStart(int timeout);

    std::unique_ptr<ProcessWithOutput> process;
    std::unique_ptr<QFileSystemWatcher> file_watcher;
    ModemLogger::Config* config{};
    QStringList args;
    QString program;
    QString suffix;

    QQueue<QFileInfo> completed_log_files; 
    QQueue<QFileInfo> compress_queue;

    QFileInfo last_log_file;
    std::set<QString> files_has_seen;
    QSemaphore semaphore {0};
    QSemaphore semaphore_stop {0};
    bool start_flag{};

    std::map<QString, std::unique_ptr<Compress>> compress_map;
};

} // namespace afal
#endif // AFAL_ADAPTER_PRODUCT_MDLOG_COMMON_MODEM_LOG_TOOLS_IMPL_H_
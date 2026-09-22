#include "modem_log_tools_impl.h"

#include "common.h"
#include "common/compress.h"
#include "common/modem.h"
#include "log.hpp"
#include "common/proccommon.h"

#include <qdir.h>
#include <qnamespace.h>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrent> 
namespace afal {

namespace {

using namespace log;

const QString kLogToolPathTest = "python3";
const QString kLogToolMdLogPathCmdTest = "../../../python/test.py";
bool compress_running = false;
#ifdef BUILD_QC
// QC
const QString kLogToolPath = QString(OS_PREFIX) + "/libs/adapter_product/tools/qc/logtool";
const QString kLogToolMdLogPathCmd = "-s";
const QString kLogToolFileSizeCmd = "-m";
const QString kLogToolCfgCmd = "-f";
const QString kLogToolPortNameCmd = "-d";
const QString kLogSuffix = "qmdl2";
const int kKillTimeout = 5000;      // 5s
const int kWaitStartTimeout = 5; // 30s
// const QString kLogToolFileMaxNumCmd = "-n";
#elif BUILD_MTK

const QString kLogToolPath ="";
const QString kLogToolMdLogPathCmd = "";
const QString kLogToolFileSizeCmd = "";
const QString kLogToolCfgCmd = "";
const QString kLogToolPortNameCmd = "";
const QString kLogSuffix = "";
const int kKillTimeout = 5000;   // 5s
const int kWaitStartTimeout = 5; // 30s

#endif

const int kDefaultCompressLevel = 6;

} // namespace


class ModemLogToolsQc : public ModemLogToolsImpl {
  public:
    ModemLogToolsQc(const ModemLogToolsQc&) = delete;
    ModemLogToolsQc(ModemLogToolsQc&&) = delete;
    ModemLogToolsQc& operator=(const ModemLogToolsQc&) = delete;
    ModemLogToolsQc& operator=(ModemLogToolsQc&&) = delete;
    ModemLogToolsQc(ModemLogger::Config* config)
        : ModemLogToolsImpl(config, kLogToolPath, kLogSuffix) {
        args = QStringList({
            kLogToolMdLogPathCmd,
            QString::fromStdString(config->mdlog_path),
            kLogToolFileSizeCmd,
            QString::fromStdString(std::to_string(config->each_log_size)),
            kLogToolCfgCmd,
            QString::fromStdString(config->modem_cfg_path),
            kLogToolPortNameCmd,
            QString::fromStdString(config->diag_port),
        });

        QSemaphore semaphore{1}; 
        QSemaphore semaphore_stop{1}; 
    }
    ~ModemLogToolsQc() override = default;
};


class ModemLogToolsTest : public ModemLogToolsImpl {
  public:
    ModemLogToolsTest(const ModemLogToolsTest&) = delete;
    ModemLogToolsTest(ModemLogToolsTest&&) = delete;
    ModemLogToolsTest& operator=(const ModemLogToolsTest&) = delete;
    ModemLogToolsTest& operator=(ModemLogToolsTest&&) = delete;
    ModemLogToolsTest(ModemLogger::Config* config)
        : ModemLogToolsImpl(config, kLogToolPath, "") {
        args = QStringList({kLogToolMdLogPathCmdTest});
    }
    ~ModemLogToolsTest() override = default;
};

bool ModemLogToolsImpl::WaitStart(int timeout) {
    return process->WaitStart(timeout);
}
bool ModemLogToolsImpl::Start(ModemType modemType) {
    FMTLOG_DEBUG("log tool start");
    semaphore.acquire();
    FMTLOG_ERROR("log tool start !");

    QDir log_dir(QString::fromStdString(config->mdlog_path));
    log_dir.mkpath(".");

    if (start_flag) {
        FMTLOG_WARN("Modem log tool is already running");
        return false;
    }
    bool portInitNeeded = false;
    if (modemType == ModemType::MTK_USB_GC || modemType == ModemType::MTK_USB_RW350R) {
        portInitNeeded = true;
    } 

    if (!Init(portInitNeeded)) {
        FMTLOG_ERROR("Init failed!");
        return false;
    }
    emit StartSignal();
    start_flag = true;    

    return WaitStart(kWaitStartTimeout);
}

bool ModemLogToolsImpl::Stop() {
    if (!start_flag)
    {
        FMTLOG_WARN("The Modem logging tool is not running and does not "
                    "need to stop");
        return false;
    }
    emit StopSignal();
    WaitCompressTask(50000);
    // Wait for the process of the modem log tool's thread to stop.
    semaphore_stop.acquire();
    start_flag = false;
    return true;
}

void ModemLogToolsImpl::WaitCompressTask(int timeout_ms) {
    /*
    for (auto &pair : compress_map) {
        FMTLOG_INFO("Wait compress file {}", pair.first);
        pair.second->wait();
    }
    compress_map.clear();
    */
    if (compress_map.empty()) {
    return;
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    int remaining = compress_map.size();

    auto compress_map_ptr = std::make_shared<std::map<QString, std::unique_ptr<Compress>>>(std::move(compress_map));

    QObject::connect(this, &ModemLogToolsImpl::compressTimeout, this, [compress_map_ptr]() {
        QtConcurrent::run([compress_map_ptr]() {
            for (auto& [path, compressTask] : *compress_map_ptr) {
                if (compressTask->isRunning()) {
                    FMTLOG_WARN("Compress task {} still running after timeout", path);
                    compressTask->Stop();
                    compressTask->quit();
                    compressTask->wait(5000);
                }
            }
            compress_map_ptr->clear();  
        });
    });

    // 超时处理
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        FMTLOG_WARN("Timeout while waiting for compress tasks. Remaining: {}", remaining);
        emit compressTimeout(); 
        loop.quit();
    });

    for (auto& [path, compressTask] : compress_map) {
        Compress* taskPtr = compressTask.get();
        QObject::connect(taskPtr, &Compress::FinishedSignal, taskPtr, [&](bool) {
            remaining--;
            if (remaining <= 0) {
                FMTLOG_INFO("All compress tasks finished before timeout");
                timeoutTimer.stop();
                emit compressTimeout(); 
                loop.quit();
            }
        });
    }

    timeoutTimer.start(timeout_ms);
    loop.exec();  
    /*
    for (auto& [_, compressTask] : compress_map) {
        if (compressTask->isRunning()) {
            FMTLOG_WARN("Compress task still running after timeout, force wait");
            compressTask->quit();
            compressTask->wait();  
        }
    }
    compress_map.clear();*/    
}

void ModemLogToolsImpl::run() {
    process = std::make_unique<ProcessWithOutput>(program, args, 0, this,
                                                  kKillTimeout,true);
    process->moveToThread(this);
    QTimer::singleShot(0, process.get(), [this]() {
        semaphore.release();
    });

    file_watcher = std::make_unique<QFileSystemWatcher>();
    file_watcher->moveToThread(this);
    file_watcher->addPath(QString::fromStdString(config->mdlog_path));
    FMTLOG_INFO("capture modem log tool thread start...");
    connect(this, &ModemLogToolsQc::StartSignal, process.get(),
            &ProcessWithOutput::Start);
    
    connect(this, &ModemLogToolsQc::StopSignal, process.get(), [this]() {        
        if (process) {
            process->ElegantKill();  
        }   
        semaphore_stop.release(); 
        FMTLOG_INFO("semaphore_stop released");
        //emit ToolStopSignal();
    });

    connect(file_watcher.get(), &QFileSystemWatcher::directoryChanged,
            file_watcher.get(),
            [this](const QString& path) { OnDirectoryChanged(path); }, Qt::QueuedConnection);
    connect(file_watcher.get(), &QFileSystemWatcher::fileChanged,
            file_watcher.get(),
            [this](const QString& path) { OnDirectoryChanged(path); }, Qt::QueuedConnection);

    exec();
    process.reset();
    file_watcher.reset();
}

void ModemLogToolsImpl::HandleProcessOutput(QString output) {
    FMTLOG_DEBUG("{}", output);
}

void ModemLogToolsImpl::HandleProcessFinished(int exitCode,
                                              QProcess::ExitStatus exitStatus) {
    FMTLOG_INFO("{}, {}", exitCode, int(exitStatus));
}

void ModemLogToolsImpl::OnDirectoryChanged(const QString& path) {
    FMTLOG_DEBUG("Directory changed:{}", path);

    QDir dir(path);
    QStringList entries =
        dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files);

    // Iterate over the directories where the file changed, and if it's a new
    // directory, add it to the watch list. If the new file is a log file, find
    // the new log file and compress the last log file.
    foreach (const QString& entry, entries)
    {
        QString fullPath = dir.absoluteFilePath(entry);
        QFileInfo file_info(fullPath);
        if (!file_info.exists())
        {
            continue;
        }
        if (file_info.isDir())
        {
            FMTLOG_DEBUG("Find a directory: {}", fullPath);
            if (!file_watcher->directories().contains(fullPath))
            {
                FMTLOG_DEBUG("New directory detected, adding to watcher: {}",
                             fullPath);
                file_watcher->addPath(fullPath);
            }
        }
        else
        {
            // 1. The file suffix is the target suffix.
            // 2. This file has not been seen before.
            if (file_info.suffix() == suffix &&
                !files_has_seen.contains(fullPath))
            {
                files_has_seen.insert(fullPath);
                if (last_log_file.absoluteFilePath().isEmpty())
                {
                    last_log_file = file_info;
                    FMTLOG_DEBUG("Find the first file: {}",
                                fullPath);
                    continue;
                }
                //LastLogCompress();
                if (!last_log_file.absoluteFilePath().isEmpty()) {
                    compress_queue.enqueue(last_log_file);
                    FMTLOG_DEBUG("Queued file for compress: {}", last_log_file.absoluteFilePath());
                }
                last_log_file = file_info;
                LastLogCompress();
            }
        }
    }
}

void ModemLogToolsImpl::LastLogCompress() {
    if (compress_running || compress_queue.isEmpty()) {
        return; 
    }

    QFileInfo file_to_compress = compress_queue.dequeue();
    compress_running = true;

    QString compress_file = file_to_compress.absoluteFilePath();
    QString compressed_file = compress_file + Compress::GetSuffix(config->compressed_format);
    FMTLOG_INFO("compress log file {}", compress_file);

    auto compress = Compress::Create(compress_file, compressed_file,
                                     config->compressed_format, kDefaultCompressLevel);
    if (!compress) {
        compress_running = false;
        LastLogCompress(); 
        return;
    }

    auto* compress_ptr = compress.get();
    compress_map.emplace(compress_file, std::move(compress));

    QObject::connect(compress_ptr, &Compress::ProgressSignal, compress_ptr,
                     [compress_file](int progress) {
                         if (progress % 50 == 0)
                             FMTLOG_DEBUG("Compress file {} progress : {}", compress_file, progress);
                     });

    QObject::connect(compress_ptr, &Compress::FinishedSignal, compress_ptr,
                     [this, compress_file, compressed_file](bool ret) mutable {
                         FMTLOG_INFO("Finished compress file {} {}", compress_file,
                                     ret ? "Success" : "Failure");
                         QFile old_file(compress_file);
                         auto remove_ret = old_file.remove();
                         FMTLOG_INFO("{} remove old log file {}", remove_ret ? "Success" : "Failure", compress_file);
                         compress_map.erase(compress_file);
                         HandleCompletedLogFiles(QFileInfo(compressed_file));

                         compress_running = false;       
                         LastLogCompress();        
                     });

    compress_ptr->Start();
}
/*
void ModemLogToolsImpl::LastLogCompress() {
    QString compress_file = last_log_file.absoluteFilePath();
    QString compressed_file = compress_file + Compress::GetSuffix(config->compressed_format);
    FMTLOG_INFO("compress log file {}", compress_file);
    auto compress =
        Compress::Create(compress_file, compressed_file,
                         config->compressed_format, kDefaultCompressLevel);
    if (!compress)
    {
        return;
    }
    compress->Start();
    auto* compress_ptr = compress.get();
    compress_map.emplace(compress_file, std::move(compress));

    QObject::connect(compress_ptr, &Compress::ProgressSignal, compress_ptr,
                     [compress_file](int progress) {
                         // print progress
                         if (progress % 50 == 0) // NOLINT
                             FMTLOG_DEBUG("Compress file {} progress : {}",
                                          compress_file, progress);
                     });
    // Once the compress is successful, you need to delete the original file and
    // leave the compress object free
    QObject::connect(
        compress_ptr, &Compress::FinishedSignal, compress_ptr,
        [compress_file, compressed_file, this](bool ret) mutable {
            FMTLOG_INFO("Finished compress file {} {}", compress_file,
                        ret ? "Success" : "Failure");
            FMTLOG_INFO("Prepare to delete the file: {}", compress_file);
            QFile old_file(compress_file);
            auto remove_ret = old_file.remove();
            FMTLOG_INFO("{} remove old log file {}",
                        remove_ret ? "Success" : "Failure", compress_file);
            auto it = compress_map.find(compress_file);
            if (it != compress_map.end())
            {
                compress_map.erase(it);
            }
            HandleCompletedLogFiles(QFileInfo(compressed_file));
        });
}
*/
void ModemLogToolsImpl::HandleCompletedLogFiles(const QFileInfo& file_info) {
    if (config->log_max_file_num == 0)
    {
        return;
    }
    completed_log_files.enqueue(file_info);
    if (completed_log_files.size() > config->log_max_file_num)
    {
        auto file = completed_log_files.dequeue();
        FMTLOG_INFO("Remove old log file: {}", file.absoluteFilePath());
        QFile old_file(file.absoluteFilePath());
        old_file.remove();
    }
}


std::unique_ptr<ModemLogTools>
ModemLogTools::Create(ModemType modem_type, ModemLogger::Config* config) {
    if (!config)
    {
        return nullptr;
    }
    std::unique_ptr<ModemLogTools> modem_log_tool;
    switch (modem_type)
    {
        case ModemType::QC_USB_GC:
        case ModemType::QC_USB_RW101:
        case ModemType::QC_USB_RW135:
        case ModemType::QC_USB_RW135R:
        case ModemType::QC_USB_RW151:
        case ModemType::QC_PCIE_RW151:
            modem_log_tool = std::make_unique<ModemLogToolsQc>(config);
            break;
        case ModemType::QC_PCIE_GC:
            break;
        case ModemType::MTK_USB_GC:
        case ModemType::MTK_USB_RW350R:
        case ModemType::MTK_PCIE_GC:
        case ModemType::MTK_PCIE_RW350:
        default:
            return nullptr;
    }
    modem_log_tool->start();
    return modem_log_tool;
}

std::unique_ptr<ModemLogTools>
ModemLogTools::CreateTest(ModemLogger::Config* config) {
    if (!config)
    {
        return nullptr;
    }
    std::unique_ptr<ModemLogTools> modem_log_tool;
    modem_log_tool = std::make_unique<ModemLogToolsTest>(config);
    modem_log_tool->start();
    return modem_log_tool;
}

} // namespace afal

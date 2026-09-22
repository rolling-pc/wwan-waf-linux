#include "modem_log_tool_mtk.h"
#include "common/modem.h"
#include "common/proccommon.h"
#include "log.hpp"
#include "modem_log_tools.h"
#include <QtSerialPort/qserialport.h>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <qnamespace.h>
#include <qobject.h>
#include <QPointer>
#include <qobjectdefs.h>
#include <qsocketnotifier.h>
#include <sys/types.h>
#include <termios.h>
#include <utility>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "common/file.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <string>
namespace afal {
namespace {

using namespace log;

const int kWaitStartTimeout = 3; // 3s
const int kWaitWhenGetZeroTime = 100;
std::mutex init_mutex;
QString GetCurrentTimeFormatted() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           now.time_since_epoch()) %
                       1000000000;    // NOLINT
    std::tm* tm = std::localtime(&t); // NOLINT
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y_%m%d_%H%M%S_");
    oss << std::setw(9) << std::setfill('0') << nanoseconds.count(); // NOLINT

    return QString::fromStdString(oss.str());
}

} // namespace

ModemLogToolsMTK::ModemLogToolsMTK(ModemLogger::Config* config)
    : ModemLogToolsImpl(config, "", ""){}

ModemLogCapture::ModemLogCapture(QString path, QString out_path,
                                 int file_max_size)
    : fd(-1), pipe_read(-1), pipe_write(-1), port(path.toStdString()),
      pipe_fd(new int[2]{-1, -1}), file_max_size(file_max_size  * 1024 * 1024), // NOLINT
      out_path(std::move(out_path)) {}
bool ModemLogCapture::Init(bool portInit)
{
    std::lock_guard<std::mutex> lock(init_mutex);
    sigset_t newmask, oldmask;

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGQUIT);  

    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) != 0) {
        FMTLOG_ERROR("sigprocmask");
        return false;
    }

    if (fd.get() != -1) {
        fd.reset();
    }
   
    if (pipe_fd) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        pipe_fd.reset();
    }
    std::string safe_port = port;
    FMTLOG_ERROR("try erial port open"); 
    int raw_fd = -1;
    constexpr int max_try = 50;
    for (int i = 0; i < max_try; ++i) {
        raw_fd = open(safe_port.c_str(), O_RDONLY | O_NOCTTY);
        if (raw_fd >= 0) break;
        FMTLOG_ERROR("open {} failed, try {} error: {}", port, i + 1, strerror(errno));
        usleep(300000); // 300ms
    }
    if (raw_fd < 0) {
        FMTLOG_ERROR("open {} ultimately failed: {}", port, strerror(errno));
        return false;
    }
    FMTLOG_ERROR("Serial port open success"); 
    fd.reset(raw_fd);
    if (portInit) {
        struct stat st{};
        if (fstat(fd.get(), &st) == 0) {
            FMTLOG_ERROR("fd mode: {}", st.st_mode);
        }
        if (fcntl(fd.get(), F_GETFD) == -1) {
            FMTLOG_ERROR("fd {} is INVALID before tcgetattr! errno={}", fd.get(), strerror(errno));
        }
        struct termios options{};
        if (!isatty(fd.get())) {
            FMTLOG_ERROR("fd {} is not a tty, skip termios", fd.get());
        } else {       
            struct termios options{};
            if (tcgetattr(fd.get(), &options) != 0) {
                FMTLOG_ERROR("tcgetattr failed: {}", strerror(errno));
                return false;
            } 
            cfmakeraw(&options);
            cfsetispeed(&options, B9600);
            cfsetospeed(&options, B9600);

            options.c_cflag |= CS8;
            options.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS | HUPCL);
            options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

            options.c_cc[VMIN]  = 1;
            options.c_cc[VTIME] = 1;
            if (fd.get() >= 0) {
                if (tcsetattr(fd.get(), TCSANOW, &options) != 0) {
                    FMTLOG_ERROR("Serial port tcsetattr failed: {}", strerror(errno));
                    return false;
                }
            }
        }  
    }

    pipe_fd = std::make_unique<int[]>(2);

    if (pipe(pipe_fd.get()) == -1) {
        FMTLOG_ERROR("Failed to create pipe: {}", strerror(errno));
        pipe_fd.reset();
        return false;
    }

    pipe_read.reset(pipe_fd[0]);
    pipe_write.reset(pipe_fd[1]);
   
    FMTLOG_DEBUG("pipe_read: {}, pipe_write: {}",
        pipe_read.get(),  
        pipe_write.get());
    
    if (sigprocmask(SIG_SETMASK, &oldmask, nullptr) != 0) {
        FMTLOG_ERROR("sigprocmask restore");
        return false;
    }
    return true;
}
bool WriteFull(QFile* file, const char* data, qint64 len)
{
    qint64 total = 0;

    while (total < len)
    {
        qint64 n = file->write(data + total, len - total);

        if (n > 0)
        {
            total += n;
            continue;
        }

        FMTLOG_ERROR("write failed or blocked: {}, errno={}",
                     file->errorString(), errno);

        return false;
    }

    return true;
}

void ModemLogCapture::WriterLoop()
{
    std::vector<char> write_buffer;
    write_buffer.reserve(256 * 1024); 

    while (true)
    {
        std::vector<char> data;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            queue_cv.wait(lock, [&]() {
                return stop_flag || !data_queue.empty();
            });

            if (stop_flag && data_queue.empty())
                break;

            if (data_queue.size() > 200)
            {
                FMTLOG_WARN("queue overflow, dropping data");
                data_queue.pop_front();
            }

            data = std::move(data_queue.front());
            data_queue.pop_front();

            queue_cv.notify_all();
        }

        if (data.empty())
            continue;

        write_buffer.insert(write_buffer.end(), data.begin(), data.end());

        if (write_buffer.size() < 128 * 1024 && !stop_flag)
            continue;

        QFile* file = nullptr;

        {
            std::lock_guard<std::mutex> lock(file_mutex);

            if (!log_file || !log_file->isOpen())
            {
                FMTLOG_ERROR("file not open before write!");
                write_buffer.clear();
                continue;
            }

            if (log_file->size() + (qint64)write_buffer.size() > file_max_size)
            {
                FMTLOG_INFO("log rotate, current size={}, incoming={}",
                            log_file->size(), write_buffer.size());

                log_file->flush();
                log_file->close();

                CreateNewFile();

                if (!log_file->open(QIODevice::WriteOnly))
                {
                    FMTLOG_ERROR("reopen log file failed: {}",
                                 log_file->errorString());
                    write_buffer.clear();
                    continue;
                }

                emit LogFileCreatedSignal(log_file->fileName());
            }

            file = log_file.get();
        }

        if (file && !write_buffer.empty())
        {
            if (!WriteFull(file, write_buffer.data(), write_buffer.size()))
            {
                FMTLOG_ERROR("write failed, drop buffer");
            }
            write_buffer.clear();
        }
    }

    FMTLOG_INFO("WriterLoop exit");
}

void ModemLogCapture::Start() {
    stop_flag = false;
    writer_thread = std::thread(&ModemLogCapture::WriterLoop, this);

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<char> buffer(BUF_SIZE);

    while (!stop_flag)
    {
        {
            std::lock_guard<std::mutex> lock(file_mutex);

            if (!log_file || !log_file->isOpen())
            {
                CreateNewFile();

                if (!log_file->open(QIODevice::WriteOnly))
                {
                    FMTLOG_ERROR("Failed to open log file {}: {}",
                                 log_file->fileName(),
                                 log_file->errorString());
                    break;
                }

                emit LogFileCreatedSignal(log_file->fileName());
            }
        }

        ssize_t read_size = ReadWithTimeout(fd.get(), buffer.data(), buffer.size(),
                                            -1, pipe_fd.get());

        if (read_size < 0)
        {
            if (!stop_flag)
            {
                FMTLOG_WARN("read failed {}, fd {}, error: {}",
                            read_size, fd.get(), strerror(errno));
            }
            return;
        }

        if (read_size == 0)
        {
            QThread::msleep(kWaitWhenGetZeroTime);
            continue;
        }

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [&]() {
                return stop_flag || data_queue.size() < 200;
            });

            if (stop_flag)
                break;

            data_queue.emplace_back(buffer.begin(), buffer.begin() + read_size);
        }

        queue_cv.notify_one();
    }
}
void ModemLogCapture::Stop() {
    FMTLOG_INFO("Modem log capture stop, will close fd");

    if (stop_flag)
        return;

    stop_flag = true;

    if (pipe_write.get() >= 0)
    {
        write(pipe_write.get(), "stop", 4);
    }

    queue_cv.notify_all();

    if (writer_thread.joinable())
        writer_thread.join();
}
bool ModemLogToolsMTK::WaitStart(int /*timeout*/) {
    return true;
}

bool ModemLogToolsMTK::Init(bool portInit) {
    FMTLOG_DEBUG("test ModemLogToolsMTK Init");
    return capture->Init(portInit);
}

void ModemLogCapture::CreateNewFile() {
    QDir log_path(out_path);
    FMTLOG_DEBUG("{}", log_path.absolutePath());

    log_file = std::make_unique<QFile>(log_path.absoluteFilePath(
        "mdlog_" + GetCurrentTimeFormatted() + ".muxraw"));
}

void ModemLogToolsMTK::run() {
    FMTLOG_INFO("Modem log tool mtk run");
    QTimer::singleShot(0, [this]() {
        semaphore.release();
    });
    QThread q_thread;
    capture = std::make_unique<ModemLogCapture>(
        QString::fromStdString(config->diag_port),
        QString::fromStdString(config->mdlog_path), config->each_log_size);
    capture->moveToThread(&q_thread);
    q_thread.start();
#if 0
    connect(
        this, &ModemLogToolsMTK::StartSignal, capture.get(),
        [this, ptr = capture.get()]() {
            FMTLOG_INFO("Modem log tools start!");
            ptr->Start();
            //semaphore_stop.release();
            //FMTLOG_INFO("Modem log tools stop!");
        },
        Qt::QueuedConnection);

    connect(this, &ModemLogToolsMTK::StopSignal, this,
             [this, ptr = capture.get()]() {
                semaphore_stop.release();
                ptr->Stop();
                FMTLOG_INFO("Modem log tools stop!");
             }, Qt::DirectConnection);
#endif
    connect(this, &ModemLogToolsMTK::StartSignal, capture.get(),
            [ptr = QPointer<ModemLogCapture>(capture.get())]() {
                FMTLOG_INFO("Modem log tools start!");
                if (ptr) ptr->Start();
            }, Qt::QueuedConnection);

    connect(this, &ModemLogToolsMTK::StopSignal, this,
            [ptr = QPointer<ModemLogCapture>(capture.get()), this]() {
                semaphore_stop.release();
                if (ptr) ptr->Stop();
                FMTLOG_INFO("Modem log tools stop!");
            }, Qt::DirectConnection);
    connect(
        capture.get(), &ModemLogCapture::LogFileCreatedSignal, &q_thread,
        [this](const QString& path) {
            FMTLOG_DEBUG("New file {}", path);
            if (!last_log_file.absoluteFilePath().isEmpty())
            { 
                compress_queue.enqueue(last_log_file);
                FMTLOG_DEBUG("Queued file for compress: {}", last_log_file.absoluteFilePath());
                LastLogCompress();
            }
            last_log_file = path;
        },
        Qt::QueuedConnection);
    FMTLOG_DEBUG("Init mutex unlock");
    exec();
    if (capture) {
        q_thread.quit();
        q_thread.wait();
    }
}

std::unique_ptr<ModemLogTools> CreateMTKTools(ModemType /*modem_type*/,
                                              ModemLogger::Config* config) {
    auto modem_log_tool = std::make_unique<ModemLogToolsMTK>(config);
    modem_log_tool->start();
    return modem_log_tool;
}

} // namespace afal
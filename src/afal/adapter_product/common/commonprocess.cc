#include "commonprocess.h"

#include "proccommon.h"
#include "log.hpp"
#include <QtCore/qdatetime.h>
#include <memory>
#include <mutex>
#include <qobject.h>
#include <utility>
#include <iostream>
#include <chrono>
#include <QFileInfo>

namespace afal {

namespace {
using namespace log;
}

class OutputRunTime {
  public:
    OutputRunTime(const OutputRunTime&) = delete;
    OutputRunTime(OutputRunTime&&) = delete;
    OutputRunTime& operator=(const OutputRunTime&) = delete;
    OutputRunTime& operator=(OutputRunTime&&) = delete;
    OutputRunTime(const std::string& message)
        : message(message), start(std::chrono::steady_clock::now()) {
        FMTLOG_DEBUG("Run {} start time: {}", message,
                     start.time_since_epoch().count());
    }
    ~OutputRunTime() {
        auto run_time = std::chrono::steady_clock::now() - start;
        FMTLOG_DEBUG("Run {} run time: {}", message, run_time.count());
    }

  private:
    std::string message;
    std::chrono::time_point<std::chrono::steady_clock> start;
};

int RunProcess(const QString& program, const QStringList& arguments,
               int timeout, QString* output, int kill_timeout) {
    FMTLOG_DEBUG("Process {}, arguments {}, timeout {}", program, arguments,
                 timeout);
    const OutputRunTime RUN_TIME(program.toStdString());
    qint64 start_process_time_sec =
        QDateTime::currentDateTime().toMSecsSinceEpoch();
    auto process = QProcess();
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);

    // Operation timeout (e.g. adb wait-for-device 60s) must not delay spawn.
    constexpr int kStartWaitMs = 5000;
    const int start_wait =
        (timeout > 0 && timeout < kStartWaitMs) ? timeout : kStartWaitMs;
    if (!process.waitForStarted(start_wait))
    {
        FMTLOG_WARN("Failed to start process, exit:{}, output:{}",
                    int(process.exitCode()),
                    process.readAllStandardOutput().toStdString());
        return -1;
    }
    qint64 current_time_msec = QDateTime::currentDateTime().toMSecsSinceEpoch();

    int remain_timeout =
        int(timeout - current_time_msec + start_process_time_sec);

    if (remain_timeout <= 0 || !process.waitForFinished(remain_timeout))
    {
        process.terminate();
        FMTLOG_WARN("Process wait timeout, try terminate it.");
        if (!process.waitForFinished(kill_timeout))
        {
            FMTLOG_WARN("Process terminate timeout, try kill it.");
            process.kill();
        }
        FMTLOG_WARN("Process timeout force kill!,, exit:{}, output: {}",
                    process.exitCode(),
                    process.readAllStandardOutput().toStdString());
        return -1;
    }

    int exit_code = process.exitCode();
    QProcess::ExitStatus exit_status = process.exitStatus();

    if (exit_status == QProcess::NormalExit && exit_code == 0)
    {
        QString p_output = process.readAllStandardOutput();
        if (output)
        {
            *output = p_output;
        }
        p_output.remove("\n");
        p_output.remove("\r");
        FMTLOG_DEBUG("Process finished successfully, output: {}", p_output);
    }
    else
    {
        QString p_output = process.readAllStandardOutput();
        if (output)
        {
            *output = p_output;
        }
        FMTLOG_ERROR("Process finished failed, exit_status: {}, exit_code: {}, "
                     "output: {}",
                     int(exit_status), exit_code,
                     p_output.toStdString());
        return exit_code;
    }

    return 0;
}

ProcessWithOutput::ProcessWithOutput(QString program, QStringList arguments,
                                     int timeout, Delegate* delegate,
                                     int kill_timeout, bool is_silent)
    : program(std::move(program)), arguments(std::move(arguments)),
      timeout(timeout), delegate(delegate), kill_timeout(kill_timeout),m_isSilent(is_silent) {
    /* QTraceCollection argv[0] is an access password — never log it. */
    if (this->program.contains("QTraceCollection") && !this->arguments.isEmpty()) {
        QStringList safeArgs = this->arguments;
        safeArgs[0] = "***";
        FMTLOG_DEBUG("Process {}, arguments {}, timeout {}", this->program,
                     safeArgs, timeout);
    } else {
        FMTLOG_DEBUG("Process {}, arguments {}, timeout {}", this->program,
                     this->arguments, timeout);
    }
    process = std::make_unique<QProcess>();
    DefaultWD = true;
    if (m_isSilent) {
        // Discard child stdout/stderr so verbose tools (e.g. logtool dump)
        // neither flood WAF logs nor block on a full pipe.
        process->setStandardOutputFile(QProcess::nullDevice());
        process->setStandardErrorFile(QProcess::nullDevice());
    } else {
        process->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(process.get(), &QProcess::readyReadStandardOutput, this,
                         &ProcessWithOutput::HandleProcessOutput);
        QObject::connect(process.get(), &QProcess::readyReadStandardError, this,
                         &ProcessWithOutput::HandleProcessOutput);
    }
    QObject::connect(process.get(), &QProcess::started, this,
                     &ProcessWithOutput::HandleProcessStarted);
    QObject::connect(
        process.get(),
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        &ProcessWithOutput::HandleProcessFinished);
}

void ProcessWithOutput::Start() {
#ifndef __linux__ //windows for process
    FMTLOG_DEBUG("Process {} start!", program);
    QFileInfo fileInfo(program);
    if (program.isEmpty()) {
        FMTLOG_ERROR("program is empty!");
    } else {
        if (!fileInfo.exists()) {
            FMTLOG_ERROR("Process exe not found: {}", program);
        }
        QString directory = fileInfo.absolutePath();
        if (DefaultWD) {
            process->setWorkingDirectory(directory);
            FMTLOG_DEBUG("Process path is :{}", directory);
        }
    }
#endif
    process->start(program, arguments);
}

int ProcessWithOutput::Wait() {
    /*
     * Qt quirk: waitForFinished() returns false if the process is ALREADY
     * finished when called. A fast-exit child (bad password, missing DLL,
     * instant crash) was mis-logged as "timeout" and then force-killed.
     */
    if (process->state() == QProcess::NotRunning) {
        if (process->error() == QProcess::FailedToStart) {
            FMTLOG_ERROR("Process failed to start: {} ({})",
                         program, process->errorString());
            return -1;
        }
        FMTLOG_WARN("Process already finished before wait: exit={} status={} err={}",
                    process->exitCode(), int(process->exitStatus()),
                    process->errorString());
        return process->exitCode();
    }

    if (!process->waitForFinished(timeout)) {
        if (process->state() == QProcess::NotRunning) {
            /* Finished during the wait window; not a timeout. */
            FMTLOG_DEBUG("Process finished during wait: exit={}",
                         process->exitCode());
            return process->exitCode();
        }
        if (process->error() == QProcess::FailedToStart) {
            FMTLOG_ERROR("Process failed to start: {} ({})",
                         program, process->errorString());
            return -1;
        }
        OnProcessTimeout();
        return -1;
    }
    return process->exitCode();
}

bool ProcessWithOutput::WaitStart(int timeout) {
    FMTLOG_DEBUG("wait start timeout:{}", timeout);
    
    if (semaphore.try_acquire_for(std::chrono::seconds(timeout))) {
        semaphore.release();
        FMTLOG_DEBUG("wait start success");
        return true;
    }
    // if (process_started) {
    //     process_started = false;
    //     return true;
    // }
    // std::unique_lock<std::mutex> lock(mutex);
    // if (cond.wait_for(lock, std::chrono::seconds(timeout), [this]() {
    //     return process_started;
    // })) {
    //     process_started = false;
    //     return true;
    // }
    // FMTLOG_ERROR("Process wait start timeout.");
    return false;
}

bool ProcessWithOutput::WaitStop(int timeout) {
    FMTLOG_DEBUG("wait stop timeout:{}", timeout);
    return process->waitForFinished(timeout*1000); // NOLINT
}

int ProcessWithOutput::Run() {
    Start();
    OutputRunTime run_time(program.toStdString());

    /* Fail fast with a clear error instead of falling into waitForFinished. */
    if (!process->waitForStarted(15000)) {
        FMTLOG_ERROR("Process waitForStarted failed: {} error={} ({})",
                     program, int(process->error()), process->errorString());
        if (!m_isSilent) {
            HandleProcessOutput();
        }
        return -1;
    }

    int exit_code = Wait();
    if (exit_code != 0) {
        if (!m_isSilent) {
            HandleProcessOutput();
        }
        FMTLOG_WARN("Process {} exited with code {} status={} error={}",
                    program, exit_code, int(process->exitStatus()),
                    process->errorString());
    }
    return exit_code;
}

void ProcessWithOutput::HandleProcessFinished(int exitCode,
                                              QProcess::ExitStatus exitStatus) {
    FMTLOG_INFO("{} {}", exitCode, int(exitStatus));
    if (!m_isSilent) {
        HandleProcessOutput();
    }
    if (delegate) {
        delegate->HandleProcessFinished(exitCode, exitStatus);
    }
    /* Only kill if still running (stuck child). Normal exit needs no kill. */
    if (process->state() == QProcess::Running) {
        ElegantKill();
    }
}

void ProcessWithOutput::HandleProcessStarted() {
    FMTLOG_INFO("Process {} Run start!", program);
    semaphore.release();
    // std::lock_guard<std::mutex> guard(mutex);
}

void ProcessWithOutput::HandleProcessOutput() {
    QString output = process->readAllStandardOutput();
    output.remove("\n");
    output.remove("\r");
    delegate->HandleProcessOutput(output);
}

void ProcessWithOutput::OnProcessTimeout() {
    FMTLOG_WARN("Process wait timeout, try elegant stop it.");
    ElegantKill();
}

void ProcessWithOutput::ElegantKill() {
    FMTLOG_WARN("Prepare to kill the process and try to Terminate first!");
    FMTLOG_WARN("Prepare to kill, timeout is:{}", kill_timeout);

    if (process->state() == QProcess::Running) {
        process->terminate();
        if (!process->waitForFinished(kill_timeout))
        {
            FMTLOG_WARN("Terminate Time out, ready to force kill it!");
            process->kill();
        }
    }

    if (!m_isSilent) {
        HandleProcessOutput();
    }
    WaitStop(0);
}

} // namespace afal
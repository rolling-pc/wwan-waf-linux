#ifndef AFAL_ADAPTER_PRODUCT_FLASH_COMMON_PROCESS_H_
#define AFAL_ADAPTER_PRODUCT_FLASH_COMMON_PROCESS_H_

#include "QtCore/qglobal.h"
#include "QtCore/qobject.h"
#include "QtCore/qobjectdefs.h"
#include "QtCore/qprocess.h"
#include "QtCore/qstringlist.h"
#include <QtCore/QTimer>
#include <condition_variable>
#include <memory>
#include <semaphore>


namespace afal {

namespace {

 constexpr int K_KILL_TIMEOUT=500;

} // namespace

int RunProcess(const QString & program, const QStringList & arguments, int timeout,
               QString* output = nullptr, int kill_timeout = K_KILL_TIMEOUT);

class ProcessWithOutput : public QObject {
    Q_OBJECT

  public:
    class Delegate {
      public:
        virtual ~Delegate() = default;
        Delegate() = default;

        Delegate(const Delegate&) = default;
        Delegate(Delegate&&) = delete;
        Delegate& operator=(const Delegate&) = default;
        Delegate& operator=(Delegate&&) = delete;
        virtual void HandleProcessOutput(QString output) = 0;
        virtual void HandleProcessFinished(int exitCode,
                                           QProcess::ExitStatus exitStatus) = 0;
    };
    ~ProcessWithOutput() override = default;
    ProcessWithOutput(const ProcessWithOutput&) = delete;
    ProcessWithOutput(ProcessWithOutput&&) = delete;
    ProcessWithOutput& operator=(const ProcessWithOutput&) = delete;
    ProcessWithOutput& operator=(ProcessWithOutput&&) = delete;
    ProcessWithOutput(QString program, QStringList arguments,
                      int timeout, Delegate* delegate, int kill_timeout = K_KILL_TIMEOUT, bool is_silent = false);
    bool DefaultWD;
    int Run();
    void Start();
    int Wait();
    bool WaitStart(int timeout);
    bool WaitStop(int timeout);

    [[nodiscard]] QProcess::ProcessState State() const {
      return process->state();
    }

    [[nodiscard]] QProcess *GetQProcess() const {
      return process.get();
    }

    void ElegantKill();

    void HandleProcessStarted();
    void HandleProcessOutput();
    void HandleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void OnProcessTimeout();
    void setWorkingDirectory(QString path) {
      DefaultWD = false;
      process->setWorkingDirectory(path);
    };

  private:
    ProcessWithOutput() = default;
    QString program;
    QStringList arguments;
    int timeout{};
    Delegate* delegate{};
    int kill_timeout{};
    std::unique_ptr<QProcess> process;
    // std::mutex mutex;
    // std::condition_variable cond;
    // bool process_started = false;

    std::counting_semaphore<1> semaphore{0};
    bool m_isSilent = false;
};

} // namespace afal

#endif  // AFAL_ADAPTER_PRODUCT_FLASH_COMMON_PROCESS_H_
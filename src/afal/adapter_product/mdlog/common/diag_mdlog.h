#ifndef AFAL_ADAPTER_PRODUCT_MDLOG_COMMON_DIAG_MDLOG_H_
#define AFAL_ADAPTER_PRODUCT_MDLOG_COMMON_DIAG_MDLOG_H_

#include <QString>
#include <QStringList>

namespace afal {

// Deploy / control on-device diag_mdlog (SOP: SDX85 / Kobuk OpenWrt).
// Logs stay on the module at /data/test_log until Pull().
class DiagMdlog {
  public:
    static constexpr const char* kLogDir = "/data/test_log";
    static constexpr const char* kCfgDir = "/data/diag";
    static constexpr const char* kInitPath = "/etc/init.d/diag-mdlog";
    static constexpr const char* kRunPath = "/etc/initscripts/diag-mdlog-run";
    static constexpr int kSizeMb = 15;
    static constexpr int kNumFiles = 2;

    // Push cfg + scripts, hook rc.local, start now.
    bool Install();
    bool Start();
    bool Stop();
    // Stop writing, pull /data/test_log to localDest, then start again.
    bool StopPullResume(const QString& localDest);
    bool Uninstall();

    // collect_ipc_log (SOP: deploy_diag_mdlog.bat <cmd> ipc)
    static constexpr const char* kIpcLogDir = "/data/ipclog";
    static constexpr const char* kIpcScriptDev = "/data/collect_ipc_log.sh";
    static constexpr const char* kIpcInitPath = "/etc/init.d/collect-ipc-log";
    static constexpr const char* kIpcRunPath = "/etc/initscripts/collect-ipc-log-run";
    static constexpr const char* kIpcMode = "pcie";

    bool IpcInstall();
    bool IpcStart();
    bool IpcStop();
    bool IpcStopPullResume(const QString& localDest);
    bool IpcUninstall();

  private:
    QString AdbBin();
    int Adb(const QStringList& args, QString* output = nullptr,
            int timeoutMs = 10000);
    // Raw stdout, no interactive shell (avoids hung `adb shell "..."`).
    QString AdbExecOut(const QString& cmd, int timeoutMs = 5000);
    QString AdbShell(const QString& cmd, int timeoutMs = 8000);
    // True if `adb devices` lists at least one "device" (not offline).
    bool HasAdbDevice(int timeoutMs = 3000);
    bool EnsureRoot();
    bool FindLocalMaskCfg(QString* localPath, QString* name);
    QString DetectDeviceMaskCfg();
    bool PushTextFile(const QString& content, const QString& remotePath);
    bool InstallRcLocalHook();
    bool RemoveRcLocalHook();
    bool InstallIpcRcLocalHook();
    bool RemoveIpcRcLocalHook();
    QString InitScript() const;
    QString RunnerScript(const QString& cfgDev) const;
    QString IpcInitScript() const;
    QString IpcRunnerScript() const;
    bool FindLocalIpcScript(QString* localPath);

    QString adb_bin_;
};

} // namespace afal

#endif

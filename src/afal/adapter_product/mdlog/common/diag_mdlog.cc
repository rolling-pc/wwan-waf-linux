#include "common/diag_mdlog.h"

#include "common/commonprocess.h"
#include "log.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>

namespace afal {
namespace {

using namespace log;

QString StripCr(QString s) {
    return s.replace('\r', QString());
}

bool IsShellUidRoot(const QString& raw) {
    const QString s = StripCr(raw).trimmed();
    if (s == QLatin1String("0")) {
        return true;
    }
    // Some devices ignore id -u and print full id output.
    return s.startsWith(QLatin1String("uid=0(")) ||
           s.startsWith(QLatin1String("uid=0 "));
}

QStringList MaskCfgSearchDirs() {
    // Source: src/afal/adapter_os/osinfo/windows/binary_common/platform-tools
    // Packaged next to adb as Utilities/platform-tools.
    const QString app = QCoreApplication::applicationDirPath();
    QStringList dirs;
    dirs << (app + "/../Utilities/platform-tools")
         << (app + "/platform-tools")
         << "C:/Program Files/Rolling/WwanToolKit/Utilities/platform-tools";
    return dirs;
}

constexpr int kAdbRootTimeoutMs = 8000;
constexpr int kAdbWaitDeviceMs = 10000;
constexpr int kAdbPushTimeoutMs = 60000;
constexpr int kAdbPullTimeoutMs = 300000;
constexpr int kUidTimeoutMs = 5000;
constexpr int kUidRetries = 3;
constexpr int kDiagMdlogPolls = 10;

} // namespace

QString DiagMdlog::AdbBin() {
    if (!adb_bin_.isEmpty()) {
        return adb_bin_;
    }

    QStringList candidates;
#if defined(_IS_WINDOWS_) || defined(_WIN32)
    const QString app = QCoreApplication::applicationDirPath();
    candidates << (app + "/../Utilities/platform-tools/adb.exe")
               << (app + "/platform-tools/adb.exe")
               << (app + "/../platform-tools/adb.exe")
               << "C:/Program Files/Rolling/WwanToolKit/Utilities/"
                    "platform-tools/adb.exe"
               << "adb.exe";
#else
    candidates << "adb" << "/usr/bin/adb";
#endif

    for (const QString& c : candidates) {
        const bool hasPath = c.contains('/') || c.contains('\\');
        if (hasPath && !QFileInfo::exists(c)) {
            continue;
        }
        QString out;
        if (RunProcess(c, {"version"}, 5000, &out) == 0) {
            adb_bin_ = c;
            FMTLOG_INFO("offline log adb: {}", adb_bin_.toStdString());
            return adb_bin_;
        }
    }

    FMTLOG_ERROR("offline log: adb not found");
    return QString();
}

int DiagMdlog::Adb(const QStringList& args, QString* output, int timeoutMs) {
    const QString bin = AdbBin();
    if (bin.isEmpty()) {
        return -1;
    }
    QString out;
    const int rc = RunProcess(bin, args, timeoutMs, &out);
    if (output) {
        *output = StripCr(out);
    }
    if (rc != 0) {
        FMTLOG_WARN("adb {} rc={} output={}", args.join(' ').toStdString(), rc,
                    StripCr(out).toStdString());
    }
    return rc;
}

QString DiagMdlog::AdbShell(const QString& cmd, int timeoutMs) {
    // Match deploy_diag_mdlog.bat: `adb shell "<cmd>"` on the device default
    // shell. `adb shell sh -c "<cmd>"` breaks here (shell: unexpected "then").
    QString out;
    Adb({"shell", cmd}, &out, timeoutMs);
    return StripCr(out).trimmed();
}

QString DiagMdlog::AdbExecOut(const QString& cmd, int timeoutMs) {
    QString out;
    Adb({"exec-out", cmd}, &out, timeoutMs);
    return StripCr(out).trimmed();
}

bool DiagMdlog::HasAdbDevice(int timeoutMs) {
    QString out;
    if (Adb({"devices"}, &out, timeoutMs) != 0) {
        return false;
    }
    const QStringList lines = StripCr(out).split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() ||
            line.startsWith(QLatin1String("List of devices")) ||
            line.startsWith(QLatin1Char('*'))) {
            continue;
        }
        QString state;
        const int tab = line.indexOf(QLatin1Char('\t'));
        if (tab >= 0) {
            state = line.mid(tab + 1).trimmed();
        } else {
            const QStringList parts =
                line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() < 2) {
                continue;
            }
            state = parts.last();
        }
        if (state == QLatin1String("device")) {
            return true;
        }
    }
    return false;
}

bool DiagMdlog::EnsureRoot() {
    // Never `adb wait-for-device` when nothing is attached: that blocks the
    // full timeout (30-60s) then kill, and StopPullResume used to stack
    // wait-for-device + root + pull into minutes on the UI thread.
    if (!HasAdbDevice()) {
        FMTLOG_ERROR("offline log: no adb device connected");
        return false;
    }

    auto queryUid = [this]() -> QString {
        QString uid = AdbExecOut("id -u", kUidTimeoutMs);
        if (!uid.isEmpty()) {
            return uid;
        }
        QString out;
        if (Adb({"shell", "id", "-u"}, &out, kUidTimeoutMs) == 0) {
            return StripCr(out).trimmed();
        }
        return QString();
    };

    QString uidOut = queryUid();
    FMTLOG_INFO("offline log: adb uid={} (before root)", uidOut.toStdString());
    if (!IsShellUidRoot(uidOut)) {
        // `adb root` restarts adbd; on some modules it never reconnects and
        // hangs forever in scripts. Only request root when shell is not root.
        FMTLOG_INFO("offline log: requesting adb root");
        const int rootRc = Adb({"root"}, nullptr, kAdbRootTimeoutMs);
        if (rootRc != 0) {
            FMTLOG_WARN("offline log: adb root rc={}", rootRc);
        }
        if (Adb({"wait-for-device"}, nullptr, kAdbWaitDeviceMs) != 0) {
            FMTLOG_ERROR("offline log: device did not reconnect after adb root");
            return false;
        }
        for (int i = 0; i < kUidRetries; ++i) {
            uidOut = queryUid();
            FMTLOG_INFO("offline log: adb uid={} (try {})",
                        uidOut.toStdString(), i + 1);
            if (IsShellUidRoot(uidOut)) {
                break;
            }
            if (!uidOut.isEmpty()) {
                break;
            }
            QThread::msleep(1000);
        }
    } else {
        FMTLOG_INFO("offline log: already root, skip adb root");
    }

    if (!IsShellUidRoot(uidOut)) {
        FMTLOG_ERROR(
            "offline log: adb shell is not root (uid='{}'); "
            "enable adb root in firmware; if adb root hangs, adbd may not "
            "support root on this build",
            uidOut.toStdString());
        return false;
    }

    const QString enforce = AdbShell("getenforce 2>/dev/null", 5000);
    if (enforce.contains("Enforcing", Qt::CaseInsensitive)) {
        FMTLOG_WARN("offline log: SELinux Enforcing, setenforce 0");
        AdbShell("setenforce 0", 5000);
    }
    return true;
}

bool DiagMdlog::FindLocalMaskCfg(QString* localPath, QString* name) {
    QStringList dirs = MaskCfgSearchDirs();
    const QString adb = AdbBin();
    if (!adb.isEmpty()) {
        dirs.prepend(QFileInfo(adb).absolutePath());
    }
    dirs.removeDuplicates();

    for (const QString& dir : dirs) {
        const QStringList matches =
            QDir(dir).entryList({"mb*_msg.cfg"}, QDir::Files, QDir::Name);
        if (matches.isEmpty()) {
            continue;
        }
        if (matches.size() > 1) {
            FMTLOG_WARN("offline log: multiple mask cfg, using {}",
                        matches.front().toStdString());
        }
        *localPath = QDir(dir).absoluteFilePath(matches.front());
        *name = matches.front();
        FMTLOG_INFO("offline log: mask cfg {}", localPath->toStdString());
        return true;
    }
    return false;
}

QString DiagMdlog::DetectDeviceMaskCfg() {
    return AdbShell("ls /data/diag/mb*_msg.cfg 2>/dev/null | head -n 1");
}

QString DiagMdlog::InitScript() const {
    return QString::fromUtf8(
        "#!/bin/sh /etc/rc.common\n"
        "\n"
        "# diag_mdlog offline logging service\n"
        "# START=99: /data mounted, diag-router (START=05) already up\n"
        "START=99\n"
        "STOP=10\n"
        "\n"
        "USE_PROCD=1\n"
        "NAME=diag-mdlog\n"
        "\n"
        "echo \"diag-mdlog init invoked, action=$1\" "
        ">> /tmp/diag-mdlog-init.log 2>/dev/null\n"
        "\n"
        "start_service() {\n"
        "    procd_open_instance\n"
        "    procd_set_param command /etc/initscripts/diag-mdlog-run\n"
        "    procd_set_param respawn 3600 5 0\n"
        "    procd_set_param stdout 1\n"
        "    procd_set_param stderr 1\n"
        "    procd_close_instance\n"
        "}\n"
        "\n"
        "stop_service() {\n"
        "    echo \"Stopping diag-mdlog\"\n"
        "}\n");
}

QString DiagMdlog::RunnerScript(const QString& cfgDev) const {
    return QString::fromUtf8(
               "#!/bin/sh\n"
               "\n"
               "CFG=%1\n"
               "LOG_DIR=%2\n"
               "DBG=/tmp/diag-mdlog-run.log\n"
               "\n"
               "exec > \"$DBG\" 2>&1\n"
               "echo \"=== diag-mdlog runner invoked ===\"\n"
               "date\n"
               "cat /proc/uptime\n"
               "\n"
               "i=0\n"
               "while [ $i -lt 60 ]; do\n"
               "    pidof diag-router >/dev/null 2>&1 && break\n"
               "    sleep 1\n"
               "    i=$((i+1))\n"
               "done\n"
               "pidof diag-router >/dev/null 2>&1 || "
               "{ echo \"FAIL: diag-router not up after $i s\"; exit 1; }\n"
               "echo \"ok: diag-router up after $i s\"\n"
               "\n"
               "i=0\n"
               "while [ $i -lt 60 ]; do\n"
               "    [ -f \"$CFG\" ] && break\n"
               "    sleep 1\n"
               "    i=$((i+1))\n"
               "done\n"
               "[ -f \"$CFG\" ] || "
               "{ echo \"FAIL: $CFG missing after $i s\"; exit 1; }\n"
               "echo \"ok: cfg present after $i s\"\n"
               "\n"
               "sleep 2\n"
               "mkdir -p \"$LOG_DIR\"\n"
               "echo \"exec diag_mdlog now\"\n"
               "\n"
               "exec >/dev/null 2>&1\n"
               "exec /usr/bin/diag_mdlog -f \"$CFG\" -o \"$LOG_DIR/\" "
               "-s %3 -n %4 -c\n")
        .arg(cfgDev, QLatin1String(kLogDir))
        .arg(kSizeMb)
        .arg(kNumFiles);
}

bool DiagMdlog::PushTextFile(const QString& content, const QString& remotePath) {
    const QString local = QDir::temp().filePath(
        QString("waf-%1").arg(QFileInfo(remotePath).fileName()));
    QFile f(local);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        FMTLOG_ERROR("offline log: cannot create {}", local.toStdString());
        return false;
    }
    f.write(content.toUtf8());
    f.close();

    const int rc = Adb({"push", local, remotePath}, nullptr, kAdbPushTimeoutMs);
    QFile::remove(local);
    if (rc != 0) {
        FMTLOG_ERROR("offline log: push {} failed", remotePath.toStdString());
        return false;
    }
    AdbShell(QString("tr -d '\\r' < %1 > %1.tmp && mv %1.tmp %1 && chmod 755 %1")
                 .arg(remotePath));
    return true;
}

bool DiagMdlog::InstallRcLocalHook() {
    const QString cmd = QString::fromUtf8(
        "RC=/etc/rc.local; "
        "[ -f \"$RC\" ] || { echo '#!/bin/sh' > \"$RC\"; chmod 755 \"$RC\"; }; "
        "[ -f \"$RC.bak-diag-mdlog\" ] || cp \"$RC\" \"$RC.bak-diag-mdlog\"; "
        "grep -v 'diag-mdlog' \"$RC\" | grep -v '^exit 0' > /tmp/rc.new; "
        "echo '/etc/init.d/diag-mdlog boot &' >> /tmp/rc.new; "
        "echo 'exit 0' >> /tmp/rc.new; "
        "cp /tmp/rc.new \"$RC\"; rm -f /tmp/rc.new; "
        "echo rc.local-hook-installed");
    const QString out = AdbShell(cmd);
    FMTLOG_INFO("offline log: rc.local hook {}", out.toStdString());
    return out.contains("rc.local-hook-installed");
}

bool DiagMdlog::RemoveRcLocalHook() {
    AdbShell(
        "RC=/etc/rc.local; "
        "[ -f \"$RC\" ] || exit 0; "
        "grep -v 'diag-mdlog' \"$RC\" > /tmp/rc.new; "
        "cp /tmp/rc.new \"$RC\"; rm -f /tmp/rc.new; "
        "echo hook-removed");
    return true;
}

bool DiagMdlog::Install() {
    FMTLOG_INFO("offline log: install diag_mdlog autostart");
    if (!EnsureRoot()) {
        return false;
    }

    const QString bin_chk = AdbShell(
        "if [ -x /usr/bin/diag_mdlog ]; then printf OK_BIN; else printf NO_BIN; fi",
        8000);
    if (bin_chk != "OK_BIN") {
        FMTLOG_ERROR(
            "offline log: /usr/bin/diag_mdlog check failed (got '{}'), "
            "expected OK_BIN; binary missing or not executable",
            bin_chk.toStdString());
        return false;
    }

    QString cfgLocal;
    QString cfgName;
    QString cfgDev;
    if (FindLocalMaskCfg(&cfgLocal, &cfgName)) {
        cfgDev = QString("%1/%2").arg(kCfgDir, cfgName);
        AdbShell(QString("mkdir -p /etc/init.d /etc/initscripts %1 %2")
                     .arg(kCfgDir, kLogDir));
        if (Adb({"push", cfgLocal, cfgDev}, nullptr, kAdbPushTimeoutMs) != 0) {
            FMTLOG_ERROR("offline log: push mask cfg failed");
            return false;
        }
        AdbShell(QString("chmod 644 %1").arg(cfgDev));
    } else {
        cfgDev = DetectDeviceMaskCfg();
        if (cfgDev.isEmpty()) {
            FMTLOG_ERROR("offline log: no mb*_msg.cfg in platform-tools "
                         "(src/afal/adapter_os/osinfo/windows/binary_common/"
                         "platform-tools), and none on device /data/diag");
            return false;
        }
        FMTLOG_WARN("offline log: using existing device cfg {}",
                    cfgDev.toStdString());
        AdbShell(QString("mkdir -p /etc/init.d /etc/initscripts %1")
                     .arg(kLogDir));
    }

    if (!PushTextFile(InitScript(), kInitPath) ||
        !PushTextFile(RunnerScript(cfgDev), kRunPath)) {
        return false;
    }

    AdbShell(QString("%1 enable").arg(kInitPath));
    if (!InstallRcLocalHook()) {
        FMTLOG_WARN("offline log: rc.local hook may have failed");
    }

    AdbShell(QString("%1 restart").arg(kInitPath), 8000);
    for (int i = 0; i < kDiagMdlogPolls; ++i) {
        if (!AdbExecOut("pidof diag_mdlog", 3000).isEmpty()) {
            FMTLOG_INFO("offline log: diag_mdlog running");
            return true;
        }
        QThread::msleep(1000);
    }
    FMTLOG_WARN("offline log: diag_mdlog not running yet "
                "(runner may still be waiting for diag-router)");
    return true;
}

bool DiagMdlog::Start() {
    if (!EnsureRoot()) {
        return false;
    }
    AdbShell(QString("%1 start").arg(kInitPath), 8000);
    QThread::msleep(2000);
    return !AdbExecOut("pidof diag_mdlog", 3000).isEmpty();
}

bool DiagMdlog::Stop() {
    if (!EnsureRoot()) {
        return false;
    }
    AdbShell(QString("[ -x %1 ] && %1 stop; killall diag_mdlog 2>/dev/null; echo stopped")
                 .arg(kInitPath),
             8000);
    QThread::msleep(200);
    return true;
}

bool DiagMdlog::StopPullResume(const QString& localDest) {
    FMTLOG_INFO("offline log: stop, pull, resume -> {}",
                localDest.toStdString());
    if (!HasAdbDevice()) {
        FMTLOG_WARN("offline log: no adb device, skip stop/pull/resume");
        return true;
    }
    if (!Stop()) {
        FMTLOG_WARN("offline log: stop failed, skip pull/resume");
        return true;
    }

    if (localDest.isEmpty()) {
        FMTLOG_ERROR("offline log: pull dest is empty");
        return false;
    }

    if (!HasAdbDevice()) {
        FMTLOG_WARN("offline log: device gone after stop, skip pull/resume");
        return true;
    }

    QDir().mkpath(localDest);
    QString out;
    // 151 offline logs can be large; this runs on the sap-stop worker, not UI.
    const int rc = Adb({"pull", kLogDir, localDest}, &out, kAdbPullTimeoutMs);
    if (rc != 0) {
        FMTLOG_ERROR("offline log: pull failed, output={}", out.toStdString());
    } else {
        FMTLOG_INFO("offline log: pulled to {}", localDest.toStdString());
    }

    if (!HasAdbDevice()) {
        FMTLOG_WARN("offline log: device gone after pull, skip resume");
        return rc == 0;
    }
    Start();
    return rc == 0;
}

bool DiagMdlog::Uninstall() {
    FMTLOG_INFO("offline log: uninstall diag_mdlog");
    if (!EnsureRoot()) {
        return false;
    }
    RemoveRcLocalHook();
    AdbShell(QString("[ -x %1 ] && { %1 stop; %1 disable; }; rm -f %1 %2; echo done")
                 .arg(kInitPath, kRunPath));
    return true;
}

bool DiagMdlog::FindLocalIpcScript(QString* localPath) {
    QStringList dirs;
    const QString app = QCoreApplication::applicationDirPath();
    dirs << (app + "/../Utilities/DeployDiagMdlog")
         << (app + "/DeployDiagMdlog")
         << "C:/Program Files/Rolling/WwanToolKit/Utilities/DeployDiagMdlog";
    const QString adb = AdbBin();
    if (!adb.isEmpty()) {
        dirs.prepend(QFileInfo(adb).absolutePath());
        dirs.prepend(QFileInfo(adb).absolutePath() + "/..");
    }
    dirs.removeDuplicates();

    for (const QString& dir : dirs) {
        const QString candidate = QDir(dir).absoluteFilePath("collect_ipc_log.sh");
        if (QFileInfo::exists(candidate)) {
            *localPath = candidate;
            FMTLOG_INFO("offline ipc log: script {}", localPath->toStdString());
            return true;
        }
    }
    return false;
}

QString DiagMdlog::IpcInitScript() const {
    return QString::fromUtf8(
        "#!/bin/sh /etc/rc.common\n"
        "\n"
        "# collect_ipc_log offline logging service\n"
        "START=99\n"
        "STOP=10\n"
        "\n"
        "USE_PROCD=1\n"
        "NAME=collect-ipc-log\n"
        "\n"
        "echo \"collect-ipc-log init invoked, action=$1\" "
        ">> /tmp/collect-ipc-log-init.log 2>/dev/null\n"
        "\n"
        "start_service() {\n"
        "    procd_open_instance\n"
        "    procd_set_param command /etc/initscripts/collect-ipc-log-run\n"
        "    procd_set_param respawn 3600 5 0\n"
        "    procd_set_param stdout 1\n"
        "    procd_set_param stderr 1\n"
        "    procd_close_instance\n"
        "}\n"
        "\n"
        "stop_service() {\n"
        "    echo \"Stopping collect-ipc-log\"\n"
        "}\n");
}

QString DiagMdlog::IpcRunnerScript() const {
    return QString::fromUtf8(
               "#!/bin/sh\n"
               "\n"
               "SCRIPT=%1\n"
               "MODE=%2\n"
               "DBG=/tmp/collect-ipc-log-run.log\n"
               "LOG_PATH=/sys/kernel/debug/ipc_logging\n"
               "\n"
               "exec > \"$DBG\" 2>&1\n"
               "echo \"=== collect-ipc-log runner invoked ===\"\n"
               "date\n"
               "cat /proc/uptime\n"
               "\n"
               "i=0\n"
               "while [ $i -lt 60 ]; do\n"
               "    [ -d \"$LOG_PATH\" ] && break\n"
               "    sleep 1\n"
               "    i=$((i+1))\n"
               "done\n"
               "[ -d \"$LOG_PATH\" ] || "
               "{ echo \"FAIL: $LOG_PATH missing after $i s\"; exit 1; }\n"
               "echo \"ok: debugfs ipc_logging up after $i s\"\n"
               "\n"
               "exec >/dev/null 2>&1\n"
               "exec sh \"$SCRIPT\" \"$MODE\"\n")
        .arg(QLatin1String(kIpcScriptDev), QLatin1String(kIpcMode));
}

bool DiagMdlog::InstallIpcRcLocalHook() {
    const QString cmd = QString::fromUtf8(
        "RC=/etc/rc.local; "
        "[ -f \"$RC\" ] || { echo '#!/bin/sh' > \"$RC\"; chmod 755 \"$RC\"; }; "
        "[ -f \"$RC.bak-diag-mdlog\" ] || cp \"$RC\" \"$RC.bak-diag-mdlog\"; "
        "grep -v 'collect-ipc-log' \"$RC\" | grep -v '^exit 0' > /tmp/rc.new; "
        "echo '/etc/init.d/collect-ipc-log boot &' >> /tmp/rc.new; "
        "echo 'exit 0' >> /tmp/rc.new; "
        "cp /tmp/rc.new \"$RC\"; rm -f /tmp/rc.new; "
        "echo rc.local-hook-installed");
    const QString out = AdbShell(cmd);
    FMTLOG_INFO("offline ipc log: rc.local hook {}", out.toStdString());
    return out.contains("rc.local-hook-installed");
}

bool DiagMdlog::RemoveIpcRcLocalHook() {
    AdbShell(
        "RC=/etc/rc.local; "
        "[ -f \"$RC\" ] || exit 0; "
        "grep -v 'collect-ipc-log' \"$RC\" > /tmp/rc.new; "
        "cp /tmp/rc.new \"$RC\"; rm -f /tmp/rc.new; "
        "echo hook-removed");
    return true;
}

bool DiagMdlog::IpcInstall() {
    FMTLOG_INFO("offline ipc log: install collect_ipc_log autostart");
    if (!EnsureRoot()) {
        return false;
    }

    QString scriptLocal;
    if (!FindLocalIpcScript(&scriptLocal)) {
        FMTLOG_ERROR("offline ipc log: collect_ipc_log.sh not found in "
                     "Utilities/DeployDiagMdlog");
        return false;
    }

    AdbShell(QString("mkdir -p /etc/init.d /etc/initscripts %1").arg(kIpcLogDir));
    if (Adb({"push", scriptLocal, kIpcScriptDev}, nullptr, kAdbPushTimeoutMs) != 0) {
        FMTLOG_ERROR("offline ipc log: push collect_ipc_log.sh failed");
        return false;
    }
    AdbShell(QString("chown root:root %1; chmod 755 %1; "
                     "tr -d '\\r' < %1 > %1.tmp && mv %1.tmp %1")
                 .arg(kIpcScriptDev));

    if (!PushTextFile(IpcInitScript(), kIpcInitPath) ||
        !PushTextFile(IpcRunnerScript(), kIpcRunPath)) {
        return false;
    }

    AdbShell(QString("%1 enable").arg(kIpcInitPath));
    if (!InstallIpcRcLocalHook()) {
        FMTLOG_WARN("offline ipc log: rc.local hook may have failed");
    }
    return true;
}

bool DiagMdlog::IpcStart() {
    if (!EnsureRoot()) {
        return false;
    }
    AdbShell(QString("%1 start").arg(kIpcInitPath), 8000);
    QThread::msleep(2000);
    return !AdbExecOut("pidof tail", 3000).isEmpty();
}

bool DiagMdlog::IpcStop() {
    if (!EnsureRoot()) {
        return false;
    }
    AdbShell(QString("[ -x %1 ] && %1 stop; killall tail 2>/dev/null; "
                      "mkdir -p %2; date >> %2/dmesg; dmesg >> %2/dmesg; "
                      "echo ipc-stopped")
                 .arg(kIpcInitPath, kIpcLogDir),
             8000);
    QThread::msleep(200);
    return true;
}

bool DiagMdlog::IpcStopPullResume(const QString& localDest) {
    FMTLOG_INFO("offline ipc log: stop, pull, resume -> {}",
                localDest.toStdString());
    if (!HasAdbDevice()) {
        FMTLOG_WARN("offline ipc log: no adb device, skip stop/pull/resume");
        return true;
    }
    if (!IpcStop()) {
        FMTLOG_WARN("offline ipc log: stop failed, skip pull/resume");
        return true;
    }

    if (localDest.isEmpty()) {
        FMTLOG_ERROR("offline ipc log: pull dest is empty");
        return false;
    }

    if (!HasAdbDevice()) {
        FMTLOG_WARN("offline ipc log: device gone after stop, skip pull/resume");
        return true;
    }

    QDir().mkpath(localDest);
    QString out;
    const int rc = Adb({"pull", kIpcLogDir, localDest}, &out, kAdbPullTimeoutMs);
    if (rc != 0) {
        FMTLOG_ERROR("offline ipc log: pull failed, output={}", out.toStdString());
    } else {
        FMTLOG_INFO("offline ipc log: pulled to {}", localDest.toStdString());
    }

    if (!HasAdbDevice()) {
        FMTLOG_WARN("offline ipc log: device gone after pull, skip resume");
        return rc == 0;
    }
    IpcStart();
    return rc == 0;
}

bool DiagMdlog::IpcUninstall() {
    FMTLOG_INFO("offline ipc log: uninstall collect_ipc_log");
    if (!EnsureRoot()) {
        return false;
    }
    RemoveIpcRcLocalHook();
    AdbShell(QString("[ -x %1 ] && { %1 stop; %1 disable; }; "
                      "killall tail 2>/dev/null; "
                      "rm -f %1 %2 %3; echo done")
                 .arg(kIpcInitPath, kIpcRunPath, kIpcScriptDev));
    return true;
}

} // namespace afal

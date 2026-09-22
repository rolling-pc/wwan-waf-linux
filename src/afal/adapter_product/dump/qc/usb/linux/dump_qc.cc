#include "dump_qc.h"
#include "common/proccommon.h"
#include "dump_interface.h"
#include <qdatetime.h>
#include <QtNetwork/QtNetwork>
#include "Device.h"
#include "mbim.hpp"
#include "global.hpp"
namespace afal {

namespace {

using namespace log;
using namespace device;
afal::clilog::CLILogger eventLogger;
const QString K_ENTER_DUMP_AT = "AT+SYSCMD=\"echo c > /proc/sysrq-trigger\"";

const QString K_ENABLE_DUMP_AT = "AT+DUMPEN=1";
const QString K_DISABLE_DUMP_AT = "AT+DUMPEN=0";
const QString K_QUERY_DUMP_AT = "AT+DUMPEN?";
const QString K_REBOOT_AT = "AT+CFUN=15";
const QString toolsFileDir = "/usr/bin";
const QString kLogToolPath =
    OS_PREFIX + QString("/libs/adapter_product/tools/qc/logtool");
const QString kLogDumpPort = "-d";
const QString kLogSavePath = "-s";
// const QString K_SWITCH_FASTBOOT_AT = "AT+SYSCMD=\"sys_reboot bootloader\"";
const int K_TIMEOUT = 300 * 1000;
deviceInfo deviceinfo;

const std::vector<DeviceId> K_RECOVER_MAP = {
    {"05c6", "900e"},
};

std::string GetDumpName(ModemDumpType dump_type) {
    switch (dump_type)
    {
        case afal::ModemDumpType::AP_DUMP:
            return "AP_DUMP";
        case afal::ModemDumpType::FULL_DUMP:
            return "FULL_DUMP";
        case afal::ModemDumpType::MODEM_DUMP:
            return "MODEM_DUMP";
        case afal::ModemDumpType::ENABLE_DUMP:
            return "ENABLE_DUMP";
        default:
            return "NONE_DUMP";
    }
}

std::string GetCurrentDate() {
    QDateTime current = QDateTime::currentDateTime();
    QString formattedDate = current.toString("yyMMddHHmm");
    return formattedDate.toStdString();
}

} // namespace
DumpCaptureQc::DumpCaptureQc(ModemType modem_type)
    : DumpCaptureQc(modem_type, DeviceFactory::Create()) {}

DumpCaptureQc::DumpCaptureQc(ModemType modem_type,
                             std::unique_ptr<DeviceFactory> device_factory)
    : DumpCaptureImpl(modem_type), device_factory(std::move(device_factory)) {
    modem = this->device_factory->CreateModem("usb", {});
    dump_device = this->device_factory->CreateDevice("usb", K_RECOVER_MAP);
}

bool DumpCaptureQc::Initialize()
{
    try {
        if (!device_factory) {
            FMTLOG_ERROR("DeviceFactory is null");
            return false;
        }

        modem = device_factory->CreateModem("usb", {});
        if (!modem) {
            FMTLOG_ERROR("CreateModem returned null");
            return false;
        }

        dump_device = device_factory->CreateDevice("usb", K_RECOVER_MAP);
        if (!dump_device) {
            FMTLOG_ERROR("CreateDevice returned null");
            return false;
        }

    } catch (const std::exception &e) {
        FMTLOG_ERROR("Exception in DumpCaptureQc::Initialize: {}", e.what());
        return false;
    } catch (...) {
        FMTLOG_ERROR("Unknown exception in DumpCaptureQc::Initialize");
        return false;
    }

    FMTLOG_INFO("DumpCaptureQc initialized successfully");
    return true;
}

std::map<ModemDumpType, DumpStatus> DumpCaptureQc::Status() {
    auto result = modem->SendATCommand(K_QUERY_DUMP_AT);

    std::map<ModemDumpType, DumpStatus> ret = {
        {ModemDumpType::AP_DUMP, DumpStatus::NONE},
        {ModemDumpType::FULL_DUMP, DumpStatus::NONE},
        {ModemDumpType::MODEM_DUMP, DumpStatus::NONE},
    };

    FMTLOG_DEBUG("result: {}", result.has_value() ? *result : "");

    if (result.has_value() &&
        result.value().contains("+DUMPEN: 0", Qt::CaseInsensitive))
    {
        ret.at(ModemDumpType::FULL_DUMP) = DumpStatus::DISABLE;
    }
    else if (result.has_value() &&
             result.value().contains("+DUMPEN: 1", Qt::CaseInsensitive))
    {
        ret.at(ModemDumpType::FULL_DUMP) = DumpStatus::ENABLE;
    }

    return ret;
};

bool DumpCaptureQc::Control(ModemDumpType dump_type, DumpStatus dump_status) {
    FMTLOG_INFO("Enter conctrl");
    if (dump_type != ModemDumpType::FULL_DUMP) //&& dump_type != ModemDumpType::ENABLE_DUMP)
    {
        FMTLOG_DEBUG("Unsupported dump type : {}", GetDumpName(dump_type));
        return false;
    }

    if (dump_status == DumpStatus::DISABLE)
    {
        FMTLOG_INFO("The Modem dump state is Enable, and the Modem needs to be Disable.");
        if (!modem->SendATCommand(K_DISABLE_DUMP_AT))
        {
            return false;
        }
        if (!modem->SendATCommand(K_REBOOT_AT)) {
             FMTLOG_WARN("The Reboot modem seems to have failed.");
        }
    }
    else if (dump_status == DumpStatus::ENABLE)
    {
        /*
        if ( dump_type == ModemDumpType::ENABLE_DUMP )
        {
            if (!StartEnableQCDump())
            {
                FMTLOG_INFO("Start EnableQCDump");
                return false;
            }
        }
        */
        FMTLOG_INFO("The Modem dump state is Disable, and the Modem needs to be Enable.");
        if (!modem->SendATCommand(K_ENABLE_DUMP_AT))
        {
             return false;
        }
        if (modem->SendATCommand(K_REBOOT_AT))
        {
             FMTLOG_WARN("The Reboot modem seems to have failed.");
        }
    }
    else
    {
        return false;
    }

    return true;
};


bool DumpCaptureQc::setDebugPort(int mode)
{
    FMTLOG_DEBUG("enter:",__func__);
    return true;
}

bool DumpCaptureQc::StartMiniDumpMonitor() {
    return true;
}

bool DumpCaptureQc::StopMiniDumpMonitor() {
    return true;
}

ModemDumpType DumpCaptureQc::QueryDump() {
    if (!dump_device->FindDevice())
    {
        return ModemDumpType::NONE;
    }
    return ModemDumpType::FULL_DUMP;
};

bool DumpCaptureQc::Start() {
    FMTLOG_INFO("start coredump");
    CLI_INFO(eventLogger,"start coredump");
    std::string dump_path_dir = config->dump_path;    
    QString timeStr = QTime::currentTime().toString("HHmmss");
    QString dateStr = QDate::currentDate().toString("yyyyMMdd");
    QString timestampDir = dateStr + "_" + timeStr;    
    QString dumpDir = QString::fromStdString(dump_path_dir) + QDir::separator() + timestampDir;
    QDir().mkpath(dumpDir);

    // Silent: dump progress goes to dump_info / dump_pull.log, not tool kits log.
    ProcessWithOutput process(
        kLogToolPath,
        {kLogDumpPort, QString::fromStdString(config->dump_port), kLogSavePath,
        dumpDir},
        K_TIMEOUT, this, 500, true);
    bool isSuccess = (0 == process.Run());
    emit dumpFinished();
    CLI_INFO(eventLogger,"coredump finished");
    return isSuccess;
};

bool DumpCaptureQc::ForceDumpTrigger(ModemDumpType dump_type) {
    if (dump_type != ModemDumpType::FULL_DUMP)
    {
        FMTLOG_WARN("Unsupported dump type : {}", GetDumpName(dump_type));
        return false;
    }

    if (!modem->SendATCommand(K_ENTER_DUMP_AT))
    {
        FMTLOG_ERROR("QC force trigger dump failed.");
        return false;
    }
    FMTLOG_INFO("Force trigger modem dump success!");
    return true;
};

void DumpCaptureQc::HandleProcessOutput(QString output) {
    Q_UNUSED(output);
}

void DumpCaptureQc::HandleProcessFinished(int exitCode,
                                          QProcess::ExitStatus exitStatus) {
    FMTLOG_INFO("{}, {}", exitCode, int(exitStatus));
}

bool DumpCaptureQc::StartEnableQCDump()
{
    FMTLOG_INFO("start Enable coredump");
    bool waitreset = false;
    QString AtCommand = "at+gtusbmode?";
    auto strAtResult = modem->SendATCommand(AtCommand);
    if (strAtResult.has_value() && !strAtResult.value().contains("64")) 
    {
        waitreset = true;
    }
    QList<QString> atcmds = { "at+gtusbmode=64", "AT+DUMPEN=1", "at+gtdiagen=1,1"};
    for ( QString atcmd : atcmds )
    {
            // QThread::msleep(1000);
            strAtResult = modem->SendATCommand(atcmd);
            if (!strAtResult.has_value() || !strAtResult.value().contains("OK"))
            {
            FMTLOG_INFO("{} send failed!", atcmd);
            return false;
            }
    }
    if (waitreset)
    {
       FMTLOG_INFO("Wait for device arrival");
       for (int i = 0; i <15; i++)
       {
        QThread::msleep(1000);
       }
    }
    QThread::msleep(1000);
    bool GetSerialNumDevice = false;
    FMTLOG_INFO("Query Serial Number ...");
    QString strSerialNum = "";
    for ( int i = 0; i < 30; i++ )
    {
        SendAdbCmd_Qt({ "remount" });
        QThread::msleep(1000);
        strSerialNum = GetCmdReturn_Common("adb", { "shell", "devmem", "0XA60A8" });
        strSerialNum = strSerialNum.trimmed();
        if (strSerialNum.contains("0x"))
        {
            GetSerialNumDevice = true;
            break;
        }
        else
        {
            FMTLOG_INFO("Failed to get Serial Num!,try again.");
        }
    }
    if (!GetSerialNumDevice)
    {
        FMTLOG_INFO("Failed to get Serial Num!");
        return false;
    }

    QString strMbnPath = toolsFileDir;
    strMbnPath.append("/");
    strMbnPath.append(strSerialNum + ".mbn");
    FMTLOG_DEBUG("Mbn file path: {}",strMbnPath);
    if ( !QFile::exists(strMbnPath) )
    {
        FMTLOG_INFO("{} does not exist, downloading", strMbnPath);
        if ( !DownloadMbnFile(strSerialNum, strMbnPath) )
        {
            FMTLOG_INFO("Download failed, Please check network!");
            return false;
        }
    }
    SendAdbCmd_Qt({ "reboot", "bootloader" });
    QThread::msleep(1000);
    QString strResult;
    bool foundDevice = false;
    for ( int i = 0; i < 89; i++ )
    {
        strResult = GetCmdReturn_Common("fastboot", { "devices" });
        if ( !strResult.isEmpty() )
        {
            FMTLOG_INFO("Fastboot devices ok");
            foundDevice = true;
            break;
        }
        else
        {
            QThread::msleep(2000);
        }
    }
    if (!foundDevice)
    {
        FMTLOG_INFO("Wait for fastboot port timeout(180s)!");
        return false;
    }
    bool flashDevice = false;
    for ( int i = 0; i < 3; i++ )
    {
        QString strBlockName = "apdp";
        strResult = GetCmdReturn_Common("fastboot", { "flash", strBlockName, strMbnPath });
        strResult = strResult.toUpper();
        if ( strResult.indexOf("FAILED") == -1 )
        {
            FMTLOG_INFO("flash Success!");
            flashDevice = true;
            break;
        }
        else
        {
            FMTLOG_INFO("Flash failed,retry again");
            QThread::msleep(2000);
        }
    }
    if (!flashDevice)
    {
        FMTLOG_INFO("Flash apdp file failed!");
        return false;
    }
    // afal::mbim::MbimDevice::getInstance().deinit();
    strResult = GetCmdReturn_Common("fastboot", { "reboot" });
    strResult = strResult.toUpper();
    if ( strResult.indexOf("FAILED") != -1 )
    {
        FMTLOG_INFO("Fastboot failed!");
        return false;
    }
     FMTLOG_INFO("Coredump is enabled!");
    return true;
}

bool DumpCaptureQc::SendAdbCmd_Qt(QStringList args)
{
    QString adbPath = toolsFileDir;
    adbPath.append("/");
    QProcess p;
    p.setWorkingDirectory(adbPath);
    p.start(adbPath.append("adb"), args);
    p.waitForStarted();
    p.waitForFinished();
    p.close();
    return true;
}

QString DumpCaptureQc::GetCmdReturn_Common(QString programPath, QStringList cmd)
{
    QProcess p;
    QString ret = "";
    QString adbPath = toolsFileDir;
    adbPath.append("/");
    p.start(adbPath.append(programPath), cmd);
    p.waitForStarted(-1);

    while ( true )
    {
        p.waitForReadyRead(-1);
        QString temp = QString::fromLocal8Bit(p.readAllStandardOutput());
        temp += QString::fromLocal8Bit(p.readAllStandardError());
        if ( temp.isEmpty() )
        {
            break;
        }
        ret += temp;
    }

    if ( ret.isEmpty() )
    {
        FMTLOG_DEBUG("cmd do not get return infomation.");
    }

    p.waitForFinished();
    p.close();
    return ret;
}

bool DumpCaptureQc::DownloadMbnFile(QString strSN, QString mbnFilePath)
{
    QNetworkAccessManager manager;
    QNetworkRequest request;

    FMTLOG_DEBUG("SN:{}",strSN);
    request.setUrl(QUrl("http://css.fibocom.com:8686/webservice/compilation/api/processDownload?serialNo=" + strSN));
    request.setRawHeader("Authorization", "Basic YXBpOkZpYm9jb21AMjAyMzEw");

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if ( reply->error() == QNetworkReply::NoError )
    {
        FMTLOG_DEBUG("Web api ok.");
    }
    else 
    {
        FMTLOG_DEBUG("External network connection failed, switch to internal network");
        reply->deleteLater();
        request.setUrl(QUrl("http://172.29.4.21:8686/webservice/compilation/api/processDownload?serialNo=" + strSN));
        reply = manager.get(request);

        QEventLoop internalLoop;
        QObject::connect(reply, &QNetworkReply::finished, &internalLoop, &QEventLoop::quit);
        internalLoop.exec();

        if ( reply->error() == QNetworkReply::NoError )
        {
            FMTLOG_DEBUG("Web api ok.");
        }
        else 
        {
            FMTLOG_DEBUG("Web api failed");
            reply->deleteLater();
            return false;
        }
    }

    QJsonObject responseData = QJsonDocument::fromJson(reply->readAll()).object();
    QJsonObject dataObj = responseData.value("data").toObject();
    QString md5 = dataObj.value("md5").toString();
    QByteArray base64String = dataObj.value("file").toString().toUtf8();
    QByteArray decodeData = QByteArray::fromBase64(base64String);

    QFile file(mbnFilePath);
    file.open(QFile::WriteOnly);
    file.write(decodeData);
    file.close();

    QFile localfile(mbnFilePath);
    localfile.open(QIODevice::ReadOnly);
    QByteArray data = localfile.readAll();
    localfile.close();

    // QByteArray data = QFile(mbnFilePath).readAll();
    QString localmd5 = QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex().toUpper();

    if ( md5 == localmd5 )
    {
        FMTLOG_DEBUG("Cheack md5 ok.");
        reply->deleteLater();
        return true;
    }
    else
    {
        FMTLOG_DEBUG("Cheack md5 failed.");
        FMTLOG_DEBUG("md5: {}", md5);
        FMTLOG_DEBUG("localmd5: {}", localmd5);
        reply->deleteLater();
        return false;
    }
}

} // namespace afal

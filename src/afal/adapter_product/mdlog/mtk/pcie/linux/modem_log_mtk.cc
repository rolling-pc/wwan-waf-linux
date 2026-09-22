#include "modem_log_mtk.h"
#include "common/modem.h"
#include "common/proccommon.h"
#include "log.hpp"
#include "modem_log_tool_mtk.h"
#include <memory>
#include <QRegularExpression>
namespace afal {

namespace {

using namespace log;

const std::vector<DeviceId> K_DEVICE_IDS = {
    {"0x14c3", "0x4d75"},
};

} // namespace

ModemLoggerImplPcie::ModemLoggerImplPcie(ModemType modem_type)
    : ModemLoggerImplPcie(modem_type, DeviceFactory::Create()) {}

ModemLoggerImplPcie::ModemLoggerImplPcie(
    ModemType modem_type, std::unique_ptr<DeviceFactory> device_factory)
    : ModemLoggerImpl(modem_type), device_factory(std::move(device_factory)),originMode(false)  {
    // TODO 将modem对象的创建抽象一个构造函数，方便单元测试
    modem = ModemMTK::Create(
        std::move(this->device_factory->CreateModem("pci", K_DEVICE_IDS))); 
}

bool ModemLoggerImplPcie::Status() {
    auto pair = modem->GetIntelTrace();
    if (!pair.has_value())
    {
        return false;
    }
    originMode = pair->first == 0;
    return pair->first == 1 && pair->second != 0;
}

bool ModemLoggerImplPcie::modeStatus() {
    return originMode;
}

bool ModemLoggerImplPcie::Start() {
    FMTLOG_INFO("Start capture modem log");
    if (!config)
    {
        return false;
    }

    if (!modem_log_tools)
    {
        modem_log_tools = CreateMTKTools(modem_type, config.get());
    }

    FMTLOG_DEBUG("Create modem log tools!");
    if (!modem_log_tools)
    {
        FMTLOG_ERROR("Modem log tool create failed!");
        return false;
    }

    return modem_log_tools->Start(modem_type);
}

bool ModemLoggerImplPcie::Stop() {
    if (!modem_log_tools)
        return true;
    auto ret = modem_log_tools->Stop();
    modem_log_tools->quit();
    modem_log_tools->wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    modem_log_tools.reset();
    return ret;
}

bool ModemLoggerImplPcie::sapStart() {
    FMTLOG_INFO("Start capture sap log");
    if (!config)
    {
        return false;
    }    

    if (!adbLogReader)
    {
        adbLogReader = std::make_unique<AdbLogReader>();
    }
    adbLogReader->start(QString::fromStdString(config->mdlog_path));
    return true;
}

bool ModemLoggerImplPcie::sapStop() {
    if (adbLogReader)
    {
        adbLogReader->stop();
    }

    return true;
}

bool ModemLoggerImplPcie::Enable() {
    FMTLOG_DEBUG("Modem logger enable.");
    return modem->SetIntelTrace(config->modem_level);
}

bool ModemLoggerImplPcie::Disable() {
    FMTLOG_DEBUG("Modem logger disable.");
    bool ret = modem->SetIntelTrace(0);
    return ret;
}

bool ModemLoggerImplPcie::SetDebugPort(bool enable) {

    return true;
} 

bool ModemLoggerImplPcie::CheckDebugPortStatus() {
    return true;
}
AdbLogReader::AdbLogReader(QObject* parent)
    : QObject(parent)
{

}

AdbLogReader::~AdbLogReader()
{
    stop();
}

QString AdbLogReader::program()
{
    return "/usr/local/opt/waf/libs/adapter_product/tools/mtk/pcie-adb";
}

QStringList AdbLogReader::arguments()
{
    QString kPcieAdbDevice = "0123456mediatek";
    QString kPcieAdbToolPar ="-s";

    QStringList arguments;
    arguments << kPcieAdbToolPar << kPcieAdbDevice << "shell" << "logread" << "-f";
    return arguments;
}

void AdbLogReader::start(const QString& logPath)
{
    FMTLOG_DEBUG("StartSapLog5");
    if (process)
    {
        stop();
    }

    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::readyRead,
            this, &AdbLogReader::onReadyRead);

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AdbLogReader::onFinished);
            
    QString parentDir = QFileInfo(logPath).absolutePath();  
    m_logPath = parentDir + "/sap/sap.log";  
    QFileInfo fileInfo(m_logPath);
    QString dirPath = fileInfo.absolutePath();
    QDir dir;
    if (!dir.exists(dirPath))
    {
        if (!dir.mkpath(dirPath))
        {
            FMTLOG_ERROR("Failed to create directory: {}", dirPath.toStdString());
            return;
        }
    }

    m_logFile.setFileName(m_logPath);
    if (!m_logFile.open(QIODevice::Append | QIODevice::Text))
    {
        FMTLOG_ERROR("Open log file failed! path: {}, error: {}",
            m_logPath.toStdString(),
            m_logFile.errorString().toStdString());
        return;
    }


    //process->start(program, arguments);
    process->start(program(), arguments());
    FMTLOG_DEBUG("{} {}",program(),arguments());

    if (!process->waitForStarted())
    {
        FMTLOG_DEBUG("Failed to start adb logread!");
        emit errorLog("Failed to start adb logread");
    }
    else
    {
        FMTLOG_DEBUG("Log started");
    }
}

void AdbLogReader::stop()
{
    FMTLOG_DEBUG("StoptSapLog5");

    if (!process)
        return;

    FMTLOG_DEBUG("Stopping process...");

    disconnect(process, nullptr, this, nullptr);

    process->kill();
    process->waitForFinished(3000);

    process->close();

    process->deleteLater();
    process = nullptr;

    {
        QMutexLocker locker(&m_mutex);

        if (m_logFile.isOpen() && !m_buffer.isEmpty())
        {
            m_logFile.write(m_buffer);
            m_buffer.clear();
            m_logFile.flush();
        }

        m_logFile.close();
    }

    FMTLOG_DEBUG("Log stopped");
}

void AdbLogReader::enableDeviceLog(bool enable)
{
    QString value = enable ? "0" : "1";
    QString kPcieAdbDevice = "0123456mediatek";
    QString kPcieAdbToolPar ="-s";

    QString program = "/usr/local/opt/waf/libs/adapter_product/tools/mtk/pcie-adb";
    QStringList arguments;

    QString cmd = QString("echo %1 > /sys/class/misc/cldma/cldma_log_enable").arg(value);

    arguments << kPcieAdbToolPar << kPcieAdbDevice << "shell" << "sh" << "-c" << cmd;

    int ret = QProcess::execute(program, arguments);

    if (ret != 0)
    {
        FMTLOG_DEBUG("Failed to set cldma_log_enable:{}",value);
    }
    else
    {
        FMTLOG_DEBUG("cldma_log_enable ={}",value);
    }
}

void AdbLogReader::onReadyRead()
{
    if (!process)
        return;

    if (process->state() == QProcess::NotRunning)
        return;

    QByteArray data = process->readAllStandardOutput();
    if (data.isEmpty())
        return;

    QMutexLocker locker(&m_mutex);

    m_buffer.append(data);
    emit newLog(data);

    constexpr int kFlushSize = 16 * 1024;

    if (m_logFile.isOpen() && m_buffer.size() >= kFlushSize)
    {
        qint64 total = 0;
        while (total < m_buffer.size())
        {
            qint64 n = m_logFile.write(m_buffer.constData() + total,
                                       m_buffer.size() - total);

            if (n <= 0)
            {
                FMTLOG_ERROR("write failed: {}", m_logFile.errorString().toStdString());
                break;
            }

            total += n;
        }

        m_buffer.clear();

        m_flushCounter++;
        if (m_flushCounter >= 10)
        {
            m_logFile.flush();
            m_flushCounter = 0;
        }

        if (m_logFile.size() >= m_maxSize)
        {
            m_logFile.flush();     
            rotateLog();           
        }
    }

    m_logCounter++;
    if (m_logCounter % 10 == 0)
    {
        QString log = QString::fromLocal8Bit(data);
        Q_UNUSED(log);
    }
}

int AdbLogReader::getNextIndex()
{
    QDir dir(QFileInfo(m_logPath).absolutePath());

    QString baseName = QFileInfo(m_logPath).completeBaseName(); // sap

    int maxIndex = 0;

    QStringList files = dir.entryList(QStringList() << baseName + "*.log",
                                       QDir::Files);

    for (const QString& file : files)
    {
        // sap1.log / sap2.log
        QRegularExpression re(QString("^%1(\\d+)\\.log$").arg(baseName));
        QRegularExpressionMatch match = re.match(file);

        if (match.hasMatch())
        {
            int idx = match.captured(1).toInt();
            maxIndex = std::max(maxIndex, idx);
        }
    }

    return maxIndex + 1;
}
void AdbLogReader::rotateLog()
{
    QMutexLocker locker(&m_mutex);

    m_logFile.flush();
    m_logFile.close();

    int nextIndex = getNextIndex();

    QString newFile = QString("%1%2.log")
                          .arg(m_logPath.replace(".log", ""))
                          .arg(nextIndex);

    if (!QFile::rename(m_logPath, newFile))
    {
        FMTLOG_ERROR("rename failed: {}", m_logPath.toStdString());
    }

    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        FMTLOG_ERROR("reopen failed!");
    }
    else
    {
        FMTLOG_DEBUG("rotated to {}", newFile.toStdString());
    }
}
void AdbLogReader::onErrorRead()
{
    QByteArray data = process->readAllStandardError();
    QString err = QString::fromLocal8Bit(data);

    emit errorLog(err);
    FMTLOG_DEBUG("Error:{}", err);
}

void AdbLogReader::onFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    FMTLOG_DEBUG("ADB Process finished:{}", exitCode);
    QMetaObject::invokeMethod(this, [this]() {
        emit finished();
    }, Qt::QueuedConnection);
}

} // namespace afal
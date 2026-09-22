
#ifndef LOG_DLL_EXPORTS
#define LOG_DLL_EXPORTS
#endif

#include "cliLog.hpp"
#include <QDateTime>
#include <QDir>
#include <cstdio>

#undef cliLogger
#undef eventLogger

namespace afal {
namespace clilog {

//CLILogger& CLILogger::Instance() {
//    static CLILogger instance;
//   return instance;
//}
CLILogger::CLILogger() {
   
}

CLILogger::~CLILogger() {
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) m_file.close(); 
}
LOG_API CLILogger* GetCliLoggerInternal() {
    static CLILogger realCliInstance; 
    return &realCliInstance;
}

LOG_API CLILogger* GetEventLoggerInternal() {
    static CLILogger realEventInstance;
    return &realEventInstance;
}

LOG_API LoggerProxy cliLogger{0};
LOG_API LoggerProxy eventLogger{1};
/*
void CLILogger::Init(const QString& logFilePath) {
    QMutexLocker locker(&m_mutex);

    m_logFilePath = logFilePath;

    QFileInfo info(logFilePath);
    QDir dir(info.path());
    if (!dir.exists()) dir.mkpath(".");

    if (m_file.isOpen()) m_file.close();

    m_file.setFileName(logFilePath);

    const bool exists = QFile::exists(logFilePath);
    auto mode = exists ? (QIODevice::Append | QIODevice::Text)
                       : (QIODevice::WriteOnly | QIODevice::Text);
    m_file.open(mode);
    m_stream.setDevice(&m_file);
}

void CLILogger::Log(CLILevel lv, const QString& msg) {
    QMutexLocker locker(&m_mutex);

    if (!m_file.isOpen()) return;

    QString time =
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = LevelToString(lv);

    m_stream << QString("[%1] [%2] %3\n").arg(time).arg(levelStr).arg(msg);
    m_stream.flush();
}*/

void CLILogger::Init(const QString& logFilePath) {
    QMutexLocker locker(&m_mutex);
    m_logFilePath = logFilePath;
    
    QFileInfo info(logFilePath);
    QDir dir(info.path());
    if (!dir.exists()) dir.mkpath(".");
    
    if (m_file.isOpen()) m_file.close();
    
    m_file.setFileName(logFilePath);
    auto mode = QFile::exists(logFilePath) ? (QIODevice::Append | QIODevice::Text)
                                           : (QIODevice::WriteOnly | QIODevice::Text);
    m_file.open(mode);
}

void CLILogger::Log(CLILevel lv, const QString& msg) {
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen()) return;

    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = LevelToString(lv);

    // 局部构造 QTextStream，规避长期持有流引发的内部状态死锁
    QTextStream stream(&m_file);
    stream << QString("[%1] [%2] %3\n").arg(time).arg(levelStr).arg(msg);
    stream.flush();
}

QString CLILogger::LevelToString(CLILevel lv) {
    switch (lv) {
        case D: return "DEBUG";
        case I: return "INFO";
        case W: return "WARN";
        case E: return "ERROR";
    }
    return "UNKNOWN";
}

} // namespace clilog
} // namespace afal
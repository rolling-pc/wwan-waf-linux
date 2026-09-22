#pragma once

#ifndef AFAL_ADAPTER_OS_API_CLILOG_HPP_
#define AFAL_ADAPTER_OS_API_CLILOG_HPP_

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>

#if defined(_WIN32) || defined(_WIN64) || defined(_IS_WINDOWS_)
    #ifdef LOG_DLL_EXPORTS
        #define LOG_API __declspec(dllexport)
    #else
        #define LOG_API __declspec(dllimport)
    #endif
#else
    #if defined(LOG_DLL_EXPORTS)
        #define LOG_API __attribute__((visibility("default")))
    #else
        #define LOG_API
    #endif
#endif

namespace afal {
namespace clilog {

    enum CLILevel { D, I, W, E};

class LOG_API CLILogger {
  public:   

    //static CLILogger& Instance();
    //void Init(const QString& logFilePath);
    //void Log(CLILevel lv, const QString& msg);

    CLILogger(); 
    ~CLILogger();

    void Init(const QString& logFilePath);
    void Log(CLILevel lv, const QString& msg);

  private:
    //() = default;
    //~CLILogger() = default;

    //CLILogger(const CLILogger&) = delete;
    //CLILogger& operator=(const CLILogger&) = delete;

    QString LevelToString(CLILevel lv);

  private:
    QString m_logFilePath;
    QFile m_file;
    QTextStream m_stream;
    QMutex m_mutex;
};

/*
#define CLI_DEBUG(fmt, ...) CLILogger::Instance().Log(afal::clilog::D, QString().asprintf(fmt, ##__VA_ARGS__))
#define CLI_INFO(fmt, ...) CLILogger::Instance().Log(afal::clilog::I, QString().asprintf(fmt, ##__VA_ARGS__))
#define CLI_WARN(fmt, ...) CLILogger::Instance().Log(afal::clilog::W, QString().asprintf(fmt, ##__VA_ARGS__))
#define CLI_ERROR(fmt, ...) CLILogger::Instance().Log(afal::clilog::E, QString().asprintf(fmt, ##__VA_ARGS__))

#define CLI_DEBUG(logger, fmt, ...) do { if(logger) { (logger)->Log(afal::clilog::D, QString().asprintf(fmt, ##__VA_ARGS__)); } } while(0)
#define CLI_INFO(logger, fmt, ...)  do { if(logger) { (logger)->Log(afal::clilog::I, QString().asprintf(fmt, ##__VA_ARGS__)); } } while(0)
#define CLI_WARN(logger, fmt, ...)  do { if(logger) { (logger)->Log(afal::clilog::W, QString().asprintf(fmt, ##__VA_ARGS__)); } } while(0)
#define CLI_ERROR(logger, fmt, ...) do { if(logger) { (logger)->Log(afal::clilog::E, QString().asprintf(fmt, ##__VA_ARGS__)); } } while(0)
*/
} // namespace clilog
} // namespace afal

namespace afal {
  namespace clilog {
  
      // 强行前置声明两个核心函数，确保代理类内部能认得它们
      LOG_API CLILogger* GetCliLoggerInternal();
      LOG_API CLILogger* GetEventLoggerInternal();
  
      // 【移到这里】全内联结构体，放在这里它就是最基础的类型，绝对不会被循环包含跳过
      struct LoggerProxy {
          int m_type; // 0: cli, 1: event
  
          CLILogger* operator->() const {
              return (m_type == 0) ? GetCliLoggerInternal() : GetEventLoggerInternal();
          }
  
          operator CLILogger*() const {
              return (m_type == 0) ? GetCliLoggerInternal() : GetEventLoggerInternal();
          }
      };
  
  } // namespace clilog
  } // namespace afal

#endif //AFAL_ADAPTER_OS_API_CLILOG_HPP_
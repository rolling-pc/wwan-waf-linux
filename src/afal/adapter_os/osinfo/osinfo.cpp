#include "osinfo.hpp"
#include <QSysInfo>
#include <QTimeZone>
#include <QSettings>
#include <QTextStream>
#include <QFile>

namespace afal {

    namespace osinfo {
        QString ChromeOSName = "chromeos";
        QString ChromeOSVersion = "";
        OSType GetOSType()
        {
            static OSType cachedType = OSType::UNKNOWN;
            static bool inited = false;
        
            if (!inited)
            {
                QString type = QSysInfo::productType();
        
                if (type.contains("ubuntu", Qt::CaseInsensitive)) {
                    cachedType = OSType::UBUNTU;
                } else if (type.contains("fedora", Qt::CaseInsensitive)) {
                    cachedType = OSType::FEDORA;
                } else if (type.contains("chromeos", Qt::CaseInsensitive) || type.contains("chromiumos", Qt::CaseInsensitive)) {
                    cachedType = OSType::CROS;
                } else if (type.contains("windows", Qt::CaseInsensitive)) {
                    cachedType = OSType::WINDOWS;
                }
        
                inited = true;
            }
        
            return cachedType;
        }

        QString GetOSName() {
            if (GetOSType() != OSType::CROS) 
            {
                return QSysInfo::prettyProductName();
            }
            else
            {
                return ChromeOSName;
            }
        }

        QString GetOSVersion() {
            if (GetOSType() != OSType::CROS)
            {
                return QSysInfo::productVersion();
            }
            else
            {
                return ChromeOSVersion;
            }
        }

        QString GetKernelVersion() {
            if(GetOSType() == OSType::WINDOWS)
            {
                QString osInfo = "";
                QProcess process;
                process.start("powershell", QStringList() << "-Command"
                                                  << "cmd.exe /c ver");
                if (!process.waitForFinished(5000))
                {
                    qDebug() << "Process timed out.";
                    return osInfo;
                }
                QString output = process.readAllStandardOutput().trimmed();
                QRegExp regex(R"(\d+\.\d+\.\d+\.\d+)");
                if (regex.indexIn(output) != -1)
                {
                    osInfo = regex.cap(0);
                }
                else
                {
                    qDebug() << "Failed to extract OS version.";
                    return osInfo;
                }
                //CLI_INFO(eventLogger,"OS version:%s",QS(osInfo));
                return osInfo;
            }
            else if(GetOSType() == OSType::CROS)
            {
                return ChromeOSVersion;
            }
            else
            {
                return QSysInfo::kernelVersion();
            }
        }

        QString GetTimeZone() {
            const int SECONDS_IN_AN_HOUR = 3600;
            QTimeZone timeZone = QTimeZone::systemTimeZone();

            int offset = timeZone.offsetFromUtc(QDateTime::currentDateTime())/SECONDS_IN_AN_HOUR;
            QString sign = (offset >= 0)? "+" : "-";

            return "UTC" + sign + QString::number(qAbs(offset));
        }       

    }
}
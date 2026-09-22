/* *****************************************************
SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0

Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
Licensed under the Unisoc General Software License, version 1.0 (the License);
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
Software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OF ANY KIND, either express or implied.
See the Unisoc General Software License, version 1.0 for more details.
******************************************************* */

#ifndef SPLOG_H
#define SPLOG_H

#include "ISpLog.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <thread>
#include <mutex>

using namespace std;

#define LOCALTIME_STRING_MAX_LEN    ( 30 )

char * PathFindFileName(char * path);

class CSpLog
{
    public:
        CSpLog();

        virtual ~CSpLog();

        virtual bool  Open(char* path,
                            uint32_t uLogLevel=SPLOGLV_INFO );

        virtual bool  Close(void);


        virtual bool  LogRawStr(uint32_t uLogLevel,
                                  const char*  str) ;

        virtual bool  LogFmtStr(uint32_t uLogLevel, const char* strFmt, ...);

        virtual bool  LogBufData(uint32_t uLogLevel,
                                  const uint8_t *pBufData,
                                  uint32_t dwBufSize,
                                  uint32_t uFlag=LOG_WRITE,
                                  const uint32_t * pUserNeedSize =NULL);

        virtual bool IsOpen();

    protected:
        // Local Time
        char*  GetLocalTime(void) const
        {
            const char* lpszFmt = "[%04d-%02d-%02d %02d:%02d:%02d:%03d]";

        #ifdef _WIN32
            SYSTEMTIME tms;
            ::GetLocalTime(&tms);
            _snprintf((char*)m_szLocalTime, sizeof(m_szLocalTime), lpszFmt, tms.wYear, tms.wMonth, tms.wDay, \
                        tms.wHour, tms.wMinute, tms.wSecond, tms.wMilliseconds);
        #else
        //Modified here by wei.zhang to use the thread safety functions.
            time_t now;
            timeval tpTime;
            struct tm tms = {0};
            gettimeofday(&tpTime,0);
            now = time(NULL);
            localtime_r(&now, &tms);

            //here put the month number adding 1 by wei.zhang
            snprintf((char *)m_szLocalTime, sizeof(m_szLocalTime), lpszFmt, tms.tm_year+1900, tms.tm_mon+1, tms.tm_mday, \
                        tms.tm_hour, tms.tm_min, tms.tm_sec, (int)(tpTime.tv_usec/1000));

        #endif

            return (char*)m_szLocalTime;
        };

        void GetLogFilePath(const char * lpszOrgFilePath,
                             char* lpszDstFilePath,
                             bool bIsTxtLogFile);

        bool LogString(const char* str);
        bool CreateMultiDirectory(const char* lpszPathName);

        static void writeLog(CSpLog* sl);

    private:
        FILE *      m_pTxtFile;
        uint32_t   m_uLogLevel;                    // Log level of 'txt log'
        uint32_t   m_nLineMaxChars;

        char        m_szModuleName[_MAX_PATH];      // The name of module file
        char        m_szLogDirPath[_MAX_PATH];      // The directory path of log file
        char        m_szDefLogName[_MAX_PATH];      // The default log file name
        char        m_szLgFilePath[_MAX_PATH];      // The log file path
        char        m_szLocalTime[LOCALTIME_STRING_MAX_LEN];
        char        m_szLogBuffer[MAX_STRING_IN_BYTES+LOCALTIME_STRING_MAX_LEN];
        pthread_mutex_t m_mutex;
        uint32_t   m_nLogNum;
        char        m_szUserLogName[_MAX_PATH];

};

#endif // SPLOG_H

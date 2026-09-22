/*
* Copyright 2018-2020 THALES group. All Rights Reserved.
*
* Project name: LPASDK.
* Platform : Windows, Linux.
* Language : C/C++
*
* Except if otherwise stated in a NOTICE file provided by Thales together with the software, below conditions are applicable by default.
*
* This computer program includes confidential and proprietary information of Thales and is a trade secret of
* Thales. All use, disclosure, and/or reproduction is prohibited unless authorized in writing by Thales.
*
* The computer program is provided "AS IS" without warranty of any kind. Thales makes no
* warranties to any person or entity with respect to the computer program and disclaims all other warranties,
* expressed or implied. Thales expressly disclaims any implied warranty of merchantability, fitness for particular
* purpose and any warranty which may arise from course of performance, course of dealing, or usage of trade. Further
* Thales does not warrant that the computer program will meet requirements or that operation of the computer program
* will be uninterrupted or error-free.
*
*/

#include "lpasdk/core/lpa_log.h"
#include "lpasdk/core/lpa_memory.h"

#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static LpaLogLevel _logLevel = SDK_LOG_LEVEL_DEBUG;

static LPA_LOG_INTERFACE _lpaLogInterface;
static bool _lpaLogInitialized = false;
static bool _logLimitationActivated = false;

#define LOG_NORMAL_MESSAGE_LINE_SIZE 1024
#define LOG_MAXIMUM_MESSAGE_LINE_SIZE 8192

static char _bufferTimeFormatting[96];
static char _bufferMessageFormatting[LOG_NORMAL_MESSAGE_LINE_SIZE];	// No static buffer > 1024 bytes (except if allocated dynamically)

// Managing log max size
#define MIN_LOG_MAX_SIZE				64 * 1024	// 64 Ko

#ifdef _DEBUG
	#define LOG_FILE_MAX_SIZE_DEFAULT	16*1024*1024		// 16 Mo
#else
	#define LOG_FILE_MAX_SIZE_DEFAULT	1024 * 1024		// 1 Mo
#endif // 

#ifdef LPA_SDK__LOG_MAX_SIZE
	#undef LOG_FILE_MAX_SIZE_DEFAULT
	#define LOG_FILE_MAX_SIZE_DEFAULT LPA_SDK__LOG_MAX_SIZE
#endif

static long _lpaLogFileMaxSize = LOG_FILE_MAX_SIZE_DEFAULT;

#define LOG_LEVEL_VERBOSE_STRING		"VER"
#define LOG_LEVEL_DEBUG_STRING			"DEB"
#define LOG_LEVEL_INFO_STRING			"INF"
#define LOG_LEVEL_WARNING_STRING		"WAR"
#define LOG_LEVEL_ERROR_STRING			"ERR"
#define LOG_LEVEL_SYSTEM_STRING			"SYS"

typedef struct
{
	LpaLogLevel logLevel;
	char* logLevelName;
	bool selectable;
}LOG_LEVEL_INFO;

static LOG_LEVEL_INFO _logLevelInfoList [] = 
{
	{ SDK_LOG_LEVEL_VERBOSE, LOG_LEVEL_VERBOSE_STRING, true },
	{ SDK_LOG_LEVEL_DEBUG, LOG_LEVEL_DEBUG_STRING, true },
	{ SDK_LOG_LEVEL_INFO, LOG_LEVEL_INFO_STRING, true },
	{ SDK_LOG_LEVEL_WARNING, LOG_LEVEL_WARNING_STRING, true },
	{ SDK_LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_STRING, true },

	{ SDK_LOG_LEVEL_SYSTEM, LOG_LEVEL_SYSTEM_STRING, false },
	// Latest entry : do not remove/update
	{0, NULL}
};

static FILE* _logFile = NULL;
static char _logFileName[LPA_MAX_PATH];
static char _backupLogFileName[LPA_MAX_PATH];

// Default implementation definition
///////////////////////////////////////////
void _lpaCoreSetLogLevel(LpaLogLevel logLevel);
LpaLogLevel _lpaCoreGetLogLevel(void);

bool _lpaCoreSetLogMaxSize(long logMaxSize);
long _lpaCoreGetLogMaxSize();

void _lpaCoreLogOpen(const char* ptrFileName, const char* prtBackupFileName);
void _lpaCoreLogFlush();
bool _lpaCoreLogIsOpened();
void _lpaCoreLogClose();

void _lpaCoreLogAppend(LpaLogLevel logLevel, const char* ptrMessage, va_list argptr);
void _lpaCoreLogAppendLongText(LpaLogLevel logLevel, const char* ptrHeaderMessage, const char* ptrLongTextToLog, const size_t LongTextToLogSize);
void _lpaCoreLogAppendByteArray(LpaLogLevel logLevel, const char* ptrMessage, const char* ptrByteArrayName, unsigned char* ptrByteArray, size_t byteArraySize);

void _lpaCoreActivateLogLimitation(bool activated);

// Internal functions
///////////////////////////////////////////

bool _isLogLevelMatching(const char* ptrLogLevel, const char* ptrLogLevelRef);
const char* _lpaCoreGetLogLevelName(LpaLogLevel logLevel, bool* ptrIsFound);

/**
 * Initialize log component with default implementation.
 * @return True if log component successfully initialized, otherwise return false
 */

bool lpaCoreLogInit()
{
	bool initDoneSuccessfully = false;

	if (!_lpaLogInitialized)
	{
            memset(&_lpaLogInterface, 0x00, sizeof(LPA_LOG_INTERFACE));

            _lpaLogInterface.openLog = _lpaCoreLogOpen;
            _lpaLogInterface.flushLog = _lpaCoreLogFlush;
            _lpaLogInterface.isLogOpened = _lpaCoreLogIsOpened;
            _lpaLogInterface.closeLog = _lpaCoreLogClose;

            _lpaLogInterface.appendToLog = _lpaCoreLogAppend;
            _lpaLogInterface.appendLongTextToLog = _lpaCoreLogAppendLongText;
            _lpaLogInterface.appendByteArrayToLog = _lpaCoreLogAppendByteArray;

            _lpaLogInterface.activateLogLimitation = _lpaCoreActivateLogLimitation;

            _lpaLogInterface.setLogLevel = _lpaCoreSetLogLevel;
            _lpaLogInterface.getLogLevel = _lpaCoreGetLogLevel;

            _lpaLogInterface.setLogMaxSize = _lpaCoreSetLogMaxSize;
            _lpaLogInterface.getLogMaxSize = _lpaCoreGetLogMaxSize;

            _lpaLogInitialized = true;
            initDoneSuccessfully = true;

            // Log limitation mecanism deactivated at startup
            _lpaLogInterface.activateLogLimitation(false);
	}

	return initDoneSuccessfully;
}

UT_EXPORT_DLL bool lpaCoreLogInitEx(LPA_LOG_INTERFACE* ptrLpaLogInterface)
{
	bool initDoneSuccessfully = false;

	if (!_lpaLogInitialized && ptrLpaLogInterface != NULL )
	{
		memset( &_lpaLogInterface, 0x00, sizeof(LPA_LOG_INTERFACE));

		_lpaLogInterface.openLog = ptrLpaLogInterface->openLog;
		_lpaLogInterface.flushLog = ptrLpaLogInterface->flushLog;
		_lpaLogInterface.closeLog = ptrLpaLogInterface->closeLog;

		_lpaLogInterface.activateLogLimitation = ptrLpaLogInterface->activateLogLimitation;

		_lpaLogInterface.setLogLevel = ptrLpaLogInterface->setLogLevel;
		_lpaLogInterface.getLogLevel = ptrLpaLogInterface->getLogLevel;

		_lpaLogInterface.setLogMaxSize = ptrLpaLogInterface->setLogMaxSize;
		_lpaLogInterface.getLogMaxSize = ptrLpaLogInterface->getLogMaxSize;

		_lpaLogInterface.appendToLog = ptrLpaLogInterface->appendToLog;
		_lpaLogInterface.appendLongTextToLog = ptrLpaLogInterface->appendLongTextToLog;
		_lpaLogInterface.appendByteArrayToLog = ptrLpaLogInterface->appendByteArrayToLog;

		_lpaLogInitialized = true;
		initDoneSuccessfully = true;

		// Log limitation mecanism deactivated at startup
		if(_lpaLogInterface.activateLogLimitation != NULL )
			_lpaLogInterface.activateLogLimitation(false);
	}

	return initDoneSuccessfully;
}

UT_EXPORT_DLL bool lpaCoreLogIsInitialized()
{
    return _lpaLogInitialized;
}

UT_EXPORT_DLL bool lpaCoreLogRelease()
{
    bool releaseDoneSuccessfully = false;

    if (_lpaLogInitialized)
    {
        memset(&_lpaLogInterface, 0x00, sizeof(LPA_LOG_INTERFACE));

        _lpaLogInitialized = false;
        releaseDoneSuccessfully = false;
    }
    return releaseDoneSuccessfully;
}

const LPA_LOG_INTERFACE* getLpaLogObject()
{
	return (_lpaLogInitialized ? &_lpaLogInterface : NULL);
}

//////////////////////////////////////////////
// Log Manager part
//////////////////////////////////////////////

UT_EXPORT_DLL void lpaCoreSetLogLevel(LpaLogLevel logLevel)
{
	if (_lpaLogInitialized && _lpaLogInterface.setLogLevel != NULL)
	{
		_lpaLogInterface.setLogLevel(logLevel);
	}
}

UT_EXPORT_DLL LpaLogLevel lpaCoreGetLogLevel(void)
{
	LpaLogLevel logLevel = SDK_LOG_LEVEL_UNKNOWN;

	if (_lpaLogInitialized && _lpaLogInterface.setLogLevel != NULL)
		logLevel = _lpaLogInterface.getLogLevel();
	
	return logLevel;
}

UT_EXPORT_DLL bool lpaCoreSetLogMaxSize(long logMaxSize)
{
	bool res = false;

	if (_lpaLogInitialized && _lpaLogInterface.setLogMaxSize != NULL)
		res = _lpaLogInterface.setLogMaxSize(logMaxSize);
	
	return res;
}

UT_EXPORT_DLL long lpaCoreGetLogMaxSize()
{
	long logMaxSize = 0L;

	if (_lpaLogInitialized && _lpaLogInterface.getLogMaxSize != NULL)
		logMaxSize = _lpaLogInterface.getLogMaxSize();

	return logMaxSize;
}

UT_EXPORT_DLL void lpaCoreActivateLogLimitation(bool activated)
{
	if (_lpaLogInitialized && _lpaLogInterface.activateLogLimitation != NULL)
		_lpaLogInterface.activateLogLimitation(activated);
}

UT_EXPORT_DLL void lpaCoreLogOpen(const char* ptrFileName, const char* prtBackupFileName)
{
	if (_lpaLogInitialized && _lpaLogInterface.openLog != NULL)
		_lpaLogInterface.openLog(ptrFileName, prtBackupFileName);
}

UT_EXPORT_DLL void lpaCoreLogFlush()
{
	if (_lpaLogInitialized && _lpaLogInterface.flushLog != NULL)
		_lpaLogInterface.flushLog();
}

UT_EXPORT_DLL void lpaCoreLogClose()
{
	if (_lpaLogInitialized && _lpaLogInterface.closeLog != NULL)
		_lpaLogInterface.closeLog();
}

UT_EXPORT_DLL bool lpaCoreLogIsOpen()
{
    bool isOpened = false;

    if (_lpaLogInitialized && _lpaLogInterface.isLogOpened != NULL)
        isOpened = _lpaLogInterface.isLogOpened();

    return isOpened;
}

UT_EXPORT_DLL void lpaCoreLogAppend(LpaLogLevel logLevel, const char* ptrMessage, ...)
{
	if (_lpaLogInitialized && _lpaLogInterface.appendToLog != NULL)
	{
		va_list argptr;
		va_start(argptr, ptrMessage);

		_lpaLogInterface.appendToLog(logLevel, ptrMessage, argptr);

		va_end(argptr);
	}
}

UT_EXPORT_DLL void lpaCoreLogAppendLongText(LpaLogLevel logLevel, const char* ptrHeaderMessage, const char* ptrLongTextToLog, const size_t LongTextToLogSize)
{
	if (_lpaLogInitialized && _lpaLogInterface.appendLongTextToLog != NULL)
	{
		_lpaLogInterface.appendLongTextToLog(logLevel, ptrHeaderMessage, ptrLongTextToLog,LongTextToLogSize);
	}
}

UT_EXPORT_DLL void lpaCoreLogAppendByteArray(LpaLogLevel logLevel, const char* ptrMessage, const char* ptrByteArrayName, unsigned char* ptrByteArray, size_t byteArraySize)
{
	if (_lpaLogInitialized && _lpaLogInterface.appendByteArrayToLog != NULL)
	{
		_lpaLogInterface.appendByteArrayToLog(logLevel, ptrMessage, ptrByteArrayName, ptrByteArray, byteArraySize);
	}
}


UT_EXPORT_DLL const char* lpaCoreGetLogLevelName(LpaLogLevel logLevel, bool* ptrIsFound)
{
	return _lpaCoreGetLogLevelName(logLevel, ptrIsFound);
}


//////////////////////////////////////////////
// Default implementation
//////////////////////////////////////////////

void _lpaCoreSetLogLevel(LpaLogLevel logLevel)
{
	_logLevel = logLevel;
}

LpaLogLevel _lpaCoreGetLogLevel(void)
{
	return _logLevel;
}

bool _lpaCoreSetLogMaxSize(long logMaxSize)
{
	bool res = false;

	if (logMaxSize > MIN_LOG_MAX_SIZE)
	{
		_lpaLogFileMaxSize = logMaxSize;
		res = true;
	}

	return res;
}

long _lpaCoreGetLogMaxSize()
{
	return _lpaLogFileMaxSize;
}

void _lpaCoreActivateLogLimitation(bool activated)
{
	_logLimitationActivated = activated;
}

void _lpaCoreManageLogFileSize()
{
	if (_logFile != NULL && _logLimitationActivated)
	{
		static size_t checkCounter = 0; 

		checkCounter++;
		if (checkCounter > 5) 
		{
			// do check all 5 call
			checkCounter = 0;

			// Check file size
			struct stat statLogFile;
			memset(&statLogFile, 0x00, sizeof(struct stat));

			bool getFileSize = false;

#ifdef LPA_SDK__PLATFORM_WIN

			int descriptorFile = _fileno(_logFile);

			if (descriptorFile != -1 && fstat(descriptorFile, &statLogFile) >= 0)
				getFileSize = true;
#else
			// Special code for Linux (Warning: fileno() is POSIX, not C99 !!
#if defined(__POSIX_VISIBLE) && (__POSIX_VISIBLE > 0)
			if (fstat(fileno(_logFile), &statLogFile) >= 0)
				getFileSize = true;
#else
                        // Works fine under Cygwin, TBV for other platforms
                        if (stat(_logFileName, &statLogFile) >= 0)
                                getFileSize = true;

#endif // __POSIX_VISIBLE
#endif // LPA_SDK__PLATFORM_WIN

			if (getFileSize)
			{
				// File exist => check size
				if (statLogFile.st_size > _lpaLogFileMaxSize)
				{
					fclose(_logFile);
					_logFile = NULL;

					if (stat(_backupLogFileName, &statLogFile) >= 0)
					{
						remove(_backupLogFileName);
					}

					if (rename(_logFileName, _backupLogFileName) != 0)
					{
						fprintf(stderr, "Unable to rename log file (errno=%d) !", errno);

						// Unable to rename file => reopen it but content is lost
						_logFile = fopen(_logFileName, "wt");
					}
					else
					{
						// log file renamed successfully
						_logFile = fopen(_logFileName, "at");
					}
				}
			}
		}
	}
}

void _lpaCoreLogOpen( const char* ptrFileName, const char* prtBackupFileName)
{
    if( _logFile == NULL )
    {
		if (ptrFileName != NULL && prtBackupFileName != NULL && strlen(ptrFileName) < LPA_MAX_PATH && strlen(ptrFileName) < LPA_MAX_PATH)
		{
			// Save log file name & backup log file name
			snprintf(_logFileName, LPA_MAX_PATH, "%s", ptrFileName);
			snprintf(_backupLogFileName, LPA_MAX_PATH, "%s", prtBackupFileName);

			// Check file size
			struct stat statLogFile;

			if (_logLimitationActivated && stat(ptrFileName, &statLogFile) >= 0)
			{
				// File exist => check size
				if (statLogFile.st_size > _lpaLogFileMaxSize)
				{
					if (stat(prtBackupFileName, &statLogFile) >= 0)
					{
						remove(prtBackupFileName);
					}

					if (rename(ptrFileName, prtBackupFileName) != 0)
						fprintf(stderr, "Unable to rename log file (errno=%d) !", errno);
				}
			}

			_logFile = fopen(ptrFileName, "at");
		}
    }
    else
    {
        // Log file already opened
    }
}

bool _lpaCoreLogIsOpened()
{
    return (_logFile != NULL);
}

void _lpaCoreLogFlush()
{
    if( _logFile != NULL )
        fflush(_logFile);
}

void _lpaCoreLogAppend(LpaLogLevel logLevel, const char* ptrMessage, va_list argptr)
{
	bool not_big_logging = true;
	
    if (_logFile != NULL && logLevel >= _logLevel && ptrMessage != NULL)
    {
        time_t rawTime;
        struct tm *infoTime = NULL;
        int formattedStringSize = 0;

        // Buffers reset
        memset(_bufferTimeFormatting, 0, sizeof(_bufferTimeFormatting));
        memset(_bufferMessageFormatting, 0, sizeof(_bufferMessageFormatting));        
        
        // Manage header message
        /////////////////////////////

        // manage time formatting
        time(&rawTime);
        infoTime = localtime(&rawTime);
        strftime(_bufferTimeFormatting, sizeof(_bufferTimeFormatting), "%d/%m/%Y %H:%M:%S", infoTime);

        // Manage message formatting
        /////////////////////////////

        formattedStringSize = vsnprintf(_bufferMessageFormatting, sizeof(_bufferMessageFormatting) - 1, ptrMessage, argptr);
//		vsprintf_s(bufferMessageFormatting, sizeof(bufferMessageFormatting), message, valist); // Not C99
                
        // Check if line to log exceeds normal buffer. If yes try to allocate a bigger buffer and retrieve log inside
        if(formattedStringSize > (LOG_NORMAL_MESSAGE_LINE_SIZE - 1))
        {
            int bigFormattedStringSize = formattedStringSize + 1;   // + End of string
                    
            // Check not exceed maximum allowed
            if(bigFormattedStringSize > LOG_MAXIMUM_MESSAGE_LINE_SIZE)
                bigFormattedStringSize = LOG_MAXIMUM_MESSAGE_LINE_SIZE;
                    
            char * bigBufferMessageFormatting = lpaCoreMemoryAlloc(bigFormattedStringSize);
                    
            // If data buffer cannot be allocated, will return to log on one truncated line, size limited to LOG_NORMAL_MESSAGE_LINE_SIZE
            if(bigBufferMessageFormatting != NULL)
            {
                not_big_logging = false;
                
                // Write header part - Done here to avoid log interleave with Memory allocation log
		fprintf(_logFile, "%s | %s", _bufferTimeFormatting, _lpaCoreGetLogLevelName(logLevel, NULL));
                        
                // Manage message formatting
                formattedStringSize = vsnprintf(bigBufferMessageFormatting, bigFormattedStringSize, ptrMessage, argptr);
					
				if (formattedStringSize > 0 )
                    fprintf(_logFile, " | %s\n", bigBufferMessageFormatting);
            	else
                    fprintf(_logFile, " | ---Unable to format message --- \n");

                // Big buffer cleanup
                lpaCoreMemoryFree(bigBufferMessageFormatting);
                bigBufferMessageFormatting = NULL;
            }
        }
                
        // Perform logging of normal buffer if not been done by big one due to memory problem
        if(not_big_logging)
        {
            // Write header part - Done here to avoid log interleave with Memory allocation log
            fprintf(_logFile, "%s | %s", _bufferTimeFormatting, _lpaCoreGetLogLevelName(logLevel, NULL));
            
            if (formattedStringSize > 0 )
                fprintf(_logFile, " | %s\n", _bufferMessageFormatting);
            else
                fprintf(_logFile, " | ---Unable to format message --- \n");
        }
		
        // In first release, flush automatically each log entry
		fflush(_logFile);

		_lpaCoreManageLogFileSize();
    }  
}


/**
 * Log text in log file, in multiple lines - This functions does not support formatting characters
 * @param logLevel Log level to apply on log item
 * @param ptrHeaderMessage Message header to write in log
 * @param ptrLongTextToLog Pointer on long text to log in log file
 * @param LongTextToLogSize Long text to log size
 */

void _lpaCoreLogAppendLongText(LpaLogLevel logLevel, const char* ptrHeaderMessage, const char* ptrLongTextToLog, const size_t LongTextToLogSize)
{
    if (_logFile != NULL && logLevel >= _logLevel && ptrHeaderMessage != NULL && ptrLongTextToLog != NULL && LongTextToLogSize > 0)
    {
        const char* logLevelName = _lpaCoreGetLogLevelName(logLevel, NULL);
        
        time_t rawTime;
        struct tm *infoTime = NULL;

        // manage time formatting
        time(&rawTime);
        infoTime = localtime(&rawTime);
        strftime(_bufferTimeFormatting, sizeof(_bufferTimeFormatting), "%d/%m/%Y %H:%M:%S", infoTime);

        // Log Header
        fprintf(_logFile, "%s | %s | %s\n", _bufferTimeFormatting, logLevelName, ptrHeaderMessage);

        size_t currentPositionInText = 0;
        size_t currentPositionInLine = 0;
        
        while(currentPositionInText < LongTextToLogSize)
        {
            // Start a new line, print new line header
            if(currentPositionInLine == 0)
            {
		        fprintf(_logFile, "%s | %s |    ", _bufferTimeFormatting, logLevelName);
            }
            
            // Print text character in current log line
            fprintf(_logFile, "%c", *(ptrLongTextToLog + currentPositionInText));
            
            currentPositionInText++;
            currentPositionInLine++;
            
            // Check end of log line reached, if yes go to the next line and flush current line
            if(currentPositionInLine > LOG_NORMAL_MESSAGE_LINE_SIZE)
            {
                fprintf(_logFile, "\n");
                currentPositionInLine = 0;
                
                fflush(_logFile);
            }
        }
        
        // Terminate last line, only if not already done
        if(currentPositionInLine > 0)
            fprintf(_logFile, "\n");
        
        fflush(_logFile);
        _lpaCoreManageLogFileSize();
    }
}


void _lpaCoreLogAppendByteArray(LpaLogLevel logLevel, const char* ptrMessage, const char* ptrByteArrayName, unsigned char* ptrByteArray, size_t byteArraySize)
{
	if (_logFile != NULL && logLevel >= _logLevel)
	{
		if (ptrMessage != NULL)
		{
			// Write message in first
			lpaCoreLogAppend(logLevel, ptrMessage);
		}

		if (ptrByteArray != NULL && byteArraySize > 0 && ptrByteArrayName != NULL )
		{

			// Write Byte array (one or more bloc)
			size_t maxBytesByBlocK = sizeof(_bufferMessageFormatting) / 4;
			size_t offsetByteArray = 0;

			time_t rawTime;
			struct tm *infoTime = NULL;
			const char* logLevelName = _lpaCoreGetLogLevelName(logLevel, NULL);

			while (offsetByteArray < byteArraySize)
			{
				size_t currentBlockSize = ((offsetByteArray + maxBytesByBlocK) < byteArraySize ? maxBytesByBlocK : (byteArraySize - offsetByteArray));

				// manage time formatting
				time(&rawTime);
				infoTime = localtime(&rawTime);
				strftime(_bufferTimeFormatting, sizeof(_bufferTimeFormatting), "%d/%m/%Y %H:%M:%S", infoTime);

				if (currentBlockSize > 1)
				{
					snprintf(_bufferMessageFormatting, sizeof(_bufferMessageFormatting), "%s [%d-%d] : ", ptrByteArrayName, (int) offsetByteArray, (int) (offsetByteArray + currentBlockSize - 1));
					fprintf(_logFile, "%s | %s | %s", _bufferTimeFormatting, logLevelName, _bufferMessageFormatting);


					// Manage data
					for (size_t offset = 0; offset < currentBlockSize; offset++)
						fprintf(_logFile, " %02X", ptrByteArray[offsetByteArray + offset]);

					fprintf(_logFile, "\n");
				}
				else
				{
					snprintf(_bufferMessageFormatting, sizeof(_bufferMessageFormatting), "%s [%d] : %02X", ptrByteArrayName, (int) offsetByteArray, ptrByteArray[offsetByteArray]);
					fprintf(_logFile, "%s | %s | %s \n", _bufferTimeFormatting, logLevelName, _bufferMessageFormatting);
				}

				fflush(_logFile);

				offsetByteArray += currentBlockSize;
			}

			_lpaCoreManageLogFileSize();
		}
	}
}

void _lpaCoreLogClose()
{
    if( _logFile != NULL )
    {
        fclose(_logFile);
        _logFile = NULL;
    }
}

bool lpaCoreSetLogLevelString(const char* logLevelString)
{
	bool res = false;

	if (logLevelString != NULL)
	{
		size_t index = 0;

		while ( !res && _logLevelInfoList[index].logLevelName != NULL)
		{
			if ( _isLogLevelMatching(logLevelString, _logLevelInfoList[index].logLevelName))
			{
				if (_logLevelInfoList[index].selectable)
				{
					if (_logLevel != _logLevelInfoList[index].logLevel)
					{

						lpaCoreLogAppend(SDK_LOG_LEVEL_SYSTEM, "Updating log level to '%s' ( '%s' previously)",
							_lpaCoreGetLogLevelName(_logLevelInfoList[index].logLevel, NULL),
							_lpaCoreGetLogLevelName(_logLevel, NULL));
						_logLevel = _logLevelInfoList[index].logLevel;
					}
					else
						lpaCoreLogAppend(SDK_LOG_LEVEL_SYSTEM, "Log level already to '%s'", _lpaCoreGetLogLevelName(_logLevel, NULL));

					res = true;
				}
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_SYSTEM, "Unable to select '%s' log level  (internal usage only)",
							_lpaCoreGetLogLevelName(_logLevelInfoList[index].logLevel, NULL));
				break;
			}
			else
				index++;
		}
	}

	return res;
}


const char* _lpaCoreGetLogLevelName(LpaLogLevel logLevel, bool* ptrIsFound)
{
	char* ptrLogLevelName = "??";
	bool isFound = false;
	size_t index = 0;

	while (!isFound && _logLevelInfoList[index].logLevelName != NULL)
	{
		if (logLevel == _logLevelInfoList[index].logLevel)
		{
			ptrLogLevelName = _logLevelInfoList[index].logLevelName;
			isFound = true;
			break;
		}
		else
			index++;
	}

	if (ptrIsFound != NULL)
		*ptrIsFound = isFound;

	return ptrLogLevelName;
}

bool _isLogLevelMatching(const char* ptrLogLevel, const char* ptrLogLevelRef )
{
	if (ptrLogLevel == NULL || ptrLogLevelRef == NULL || strlen(ptrLogLevel) < strlen(ptrLogLevelRef))
		return false;

	size_t stringSize = strlen(ptrLogLevelRef);
	for (size_t index = 0; index < stringSize; index++)
	{
		if (tolower(ptrLogLevel[index]) != tolower(ptrLogLevelRef[index]))
			return false;
	}
	return true;
}




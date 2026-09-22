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

#include "lpasdk/core/lpa_config_file.h"
#include "lpasdk/core/lpa_manager.h"

#include "lpasdk/core/lpa_log.h"

#include <string.h>
#include <ctype.h>

static bool _isLoaded = false;

void _trimString(char* ptrString);
bool _manageConfigLineEntry(char* ptrString);
bool _isEmptyOrCommentedLine(const char* ptrString);

bool lpaConfigFileLoad(const char* ptrConfigFileName, bool* ptrConfigFilePresent)
{
	FILE *ptrFileConfig = NULL;

	lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "lpaConfigFileLoad() ...");

	if (!_isLoaded)
	{

		if (ptrConfigFileName != NULL && ptrConfigFilePresent != NULL)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "opening '%s' configuration file ...", ptrConfigFileName);

			ptrFileConfig = fopen(ptrConfigFileName, "r");
			if (ptrFileConfig != NULL)
			{
				char bufferReadFile[LPA_CFG_LINE_ENTRY_MAX_SIZE];
				int16_t lineCounter = 0;

				*ptrConfigFilePresent = true;
				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "loading '%s' configuration file ...", ptrConfigFileName);


				// Read line by line (parsing error on one line do not stop parsing loop)
				while ( !feof(ptrFileConfig))
				{
					if (fgets(bufferReadFile, 1024, ptrFileConfig) == NULL)
						break;
					lineCounter++;

					_trimString(bufferReadFile);
					if (!_isEmptyOrCommentedLine(bufferReadFile))
					{
						if (!_manageConfigLineEntry(bufferReadFile))
							lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Unable to parse successfully line number %d !", lineCounter);
					}
				}

				_isLoaded = true;
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Closing configuration file ..." );

				fclose(ptrFileConfig);
				ptrFileConfig = NULL;
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "unable to open configuration file");
				*ptrConfigFilePresent = false;
			}
		}
	}
	else
	{
		// Configuration file already loaded
		lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "configuration file already loaded");

		if ( ptrConfigFilePresent != NULL)
			*ptrConfigFilePresent = true;
	}

	return _isLoaded;
}
bool _isEmptyOrCommentedLine(const char* ptrLine)
{
	bool isEmptyOrCommentedLine = false;

	if (NULL == ptrLine || strlen(ptrLine) == 0)
		isEmptyOrCommentedLine = true;
	else
	{
		if (ptrLine[0] == '#' || ptrLine[0] == '/' || ptrLine[0] == ';')
			isEmptyOrCommentedLine = true;
	}

	return isEmptyOrCommentedLine;
}

bool _manageConfigLineEntry(char* ptrLine)
{
	bool res = true;

	if (NULL != ptrLine && strlen(ptrLine) > 0)
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "Parsing configuration entry '%s' ...", ptrLine);
		size_t posEqual = strcspn(ptrLine, "=");
		if (posEqual < strlen(ptrLine))
		{

			// Split the line into 2 parts : Key <-> Value
			ptrLine[posEqual] = 0x00;

			char* ptrParameterKeyName = ptrLine;
			char* ptrParameterKeyValue = &ptrLine[posEqual + 1];

			_trimString(ptrParameterKeyName);
			_trimString(ptrParameterKeyValue);

			if (strlen(ptrParameterKeyName) > 0 && strlen(ptrParameterKeyValue) > 0)
			{
				LPA_PARAMETER_TYPE parameterType = LPA_PARAMETER_TYPE_UNKNOWN;
				bool isParameterExist = false;
				bool isAccessGranted = false;

				lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "managing LPA parameter : '%s'='%s'", ptrParameterKeyName, ptrParameterKeyValue);

				if (lpaManagerIsConfigParameterExist(ptrParameterKeyName, &parameterType, &isParameterExist, &isAccessGranted) && isParameterExist )
				{
					if(isAccessGranted)
					{
						// Internal call to lpaManagerSetConfigParameter() function
						if (lpaManagerSetConfigParameter(ptrParameterKeyName, LPA_PARAMETER_TYPE_STRING, ptrParameterKeyValue, true))
						{
							lpaCoreLogAppend(SDK_LOG_LEVEL_INFO, "LPA parameter '%s' configured successfully with value '%s'", ptrParameterKeyName, ptrParameterKeyValue);
						}
						else
							lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Unable to set LPA parameter '%s' with value '%s' !", ptrParameterKeyName, ptrParameterKeyValue);
					}
					else
						lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "No access right to update LPA parameter '%s' with value '%s' !", ptrParameterKeyName, ptrParameterKeyValue);
				}
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "parameter '%s' not exit or not supported", ptrParameterKeyName);
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Incorrect configuration entry !");
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "Separator not found on Configuration entry!");
	}

	return res;
}

void _trimString(char* ptrString)
{
	if (ptrString != NULL && strlen(ptrString) > 0 )
	{
		char* ptrBeginString = ptrString;

		size_t posSlash = strcspn(ptrBeginString, "\r\n");
		if (posSlash != strlen(ptrBeginString))
			ptrBeginString[posSlash] = 0x00;

		if (strlen(ptrString) > 0)
		{
			// Step1 : remove all space before first digit
			if (isspace((unsigned char)(ptrBeginString[0])))
			{
				// One or more space at begin
				size_t countSpace = 0;
				while (isspace( (unsigned char) (ptrBeginString[countSpace])))
					countSpace++;

				// Now remove it (or them)
				size_t remainingString = strlen(ptrBeginString) - countSpace;
				if (remainingString > 0)
				{
					for (size_t idx = 0; idx < remainingString; idx++)
						ptrBeginString[idx] = ptrBeginString[idx + countSpace];
					ptrBeginString[remainingString] = 0x0;
				}
				else
					ptrBeginString[0] = 0x0;
			}
		}
	}
}

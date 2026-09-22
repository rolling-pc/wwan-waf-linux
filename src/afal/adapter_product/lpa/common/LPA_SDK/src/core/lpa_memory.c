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

#include "lpasdk/core/lpa_memory.h"
#include "lpasdk/core/lpa_log.h"

#ifdef LPA_SDK__MEMORY


#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lpasdk/core/lpa_core.h"


    // Activate it to trace (Verbose mode all memory management operation)
    #ifdef LPA_SDK__MEMORY_MONITORING
static const bool _activateLogForMemoryManagementOperation = true;
    #else
static const bool _activateLogForMemoryManagementOperation = false;
    #endif //

// Memory stat
// /////////////////////

static long _countMemoryAllocCall = 0;
static long _countMemoryFreeCall = 0;
static long _currentMemoryBlockAllocated = 0;


static long _currentMemoryAllocated = 0;
static long _totalMemoryAllocated = 0;

static long _maxMemoryAllocated = 0;
static long _maxMemoryBlockAllocated = 0;

#define		INVALID_MONITORING_INDEX			0xFFFFFF

// Simulating Memory Error if requested
// If value <= 0, do nothing
static long _generateErrIfMemoryCounterEQValue = 0;
static long _generateErrIfMemoryCounterGTValue = 0;
static long _generateErrIfMemoryCounterGEValue = 0;

static long _generateErrIfMemorySizeRequestedEQValue = 0;
static long _generateErrIfMemorySizeRequestedGTValue = 0;
static long _generateErrIfMemorySizeRequestedGEValue = 0;

// Internal function
// /////////////////////

size_t	_lpaCoreMemoryGetBlockSize(void* ptrMemoryBlock, bool* ptrIsValidMemoryBlock);

void*	_lpaCoreMemoryAlloc(size_t size, bool isAllocationSource, char* ptrFileName, int fileLine);
void*	_lpaCoreMemoryRealloc(void* ptrMemoryOldBlock, size_t newSize, bool isAllocationSource, char* ptrFileName, int fileLine);
void*	_lpaCoreMemoryCalloc(size_t count, size_t size, bool isAllocationSource, char* ptrFileName, int fileLine);
void	_lpaCoreMemoryFree(void* ptrMemoryBlock, bool isAllocationSource, char* ptrFileName, int fileLine);

void _memoryMonitoringBreakpointDebugger();
bool _isSimulateMemoryErrorAllocation(size_t size);

#define		MEMORY_CONTENT_INIT_PATTERN			0xBD	// Pattern used to initialize LPASDK memory block

#endif // LPA_SDK__MEMORY

// This function is available whatever the configuration
UT_EXPORT_DLL void lpaCoreMemoryInitialize()
{
	// Do internal module initialization (eg mutex when supported)
}

#ifdef LPA_SDK__MEMORY

#ifdef LPA_SDK__MEMORY_MONITORING

	void	_lpaCoreMemoryCheckMemoryAllocated(bool trace);

	#define		MAX_MEMORY_BLOCK_INFORMATION		4096
	#define		MAX_MEMORY_ALLOCATION_INFORMATION	1024
	#define		ALLOCATION_FILE_NAME_SIZE			64

	// Internal structure definition
	// /////////////////////

	// Contains @ of memory block allocated + entry in MEMORY_ALLOCATION_INFORMATION table (for managing memory leak)
	typedef struct
	{
		void*	memoryBlockAllocated;				// Internal @ of Memory block allocated
		size_t	memorySize;							// memory size requested
		size_t	indexMemoryAllocationInformation;	// -> In MEMORY_ALLOCATION_INFORMATION table
	} MEMORY_BLOCK_INFORMATION;

	// Contains memory allocation source
	typedef struct
	{
		char	allocationFileName[ALLOCATION_FILE_NAME_SIZE];
		size_t	allocationLine;
		size_t	allocationCounter;
	} MEMORY_ALLOCATION_INFORMATION;

	// Array that contains
	MEMORY_BLOCK_INFORMATION*		_ptrMemoryBlockInformation = NULL; // MAX_MEMORY_BLOCK_INFORMATION;
	MEMORY_ALLOCATION_INFORMATION*	_ptrMemoryAllocationInformation = NULL; //MAX_MEMORY_ALLOCATION_INFORMATION];

	size_t _addMemoryAllocationMonitoring(char* ptrFileName, int fileLine, void* ptrMem, size_t size);
	void _freeMemoryAllocationMonitoring(void *ptrMemoryBlock, size_t index);
	void _initMemoryAllocationMonitoring();
	void _writeLogMemoryAllocationBlockBefore(void* ptrMemoryBlock);
	
	bool _getMemoryAllocationInformation(MEMORY_ALLOCATION_INFORMATION* ptrMemoryAllocationInformation, size_t index);

	// Monitoring of allocation source
    ////////////////////////////////////////////////////

    UT_EXPORT_DLL void* lpaCoreMemoryMonitorAlloc(char* ptrFilename, int line, size_t size)
    {
		return _lpaCoreMemoryAlloc(size, true, ptrFilename, line);
	}

    //////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////

    UT_EXPORT_DLL void* lpaCoreMemoryMonitorCalloc(char* ptrFilename, int line, size_t count, size_t size)
    {
		return _lpaCoreMemoryCalloc(count, size, true, ptrFilename, line);
	}

    //////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////

    UT_EXPORT_DLL void* lpaCoreMemoryMonitorRealloc(char* ptrFilename, int line, void* ptrMemoryOldBlock, size_t newSize)
    {
		return _lpaCoreMemoryRealloc(ptrMemoryOldBlock, newSize, true, ptrFilename, line);
	}

    //////////////////////////////////////////////////////////////
    // free memory
    //////////////////////////////////////////////////////////////

    UT_EXPORT_DLL void lpaCoreMemoryMonitorFree(char* ptrFilename, int line, void* ptrMemoryBlock)
    {
			_lpaCoreMemoryFree(ptrMemoryBlock, true, ptrFilename, line);
	}

#endif // LPA_SDK__MEMORY_MONITORING

UT_EXPORT_DLL void lpaCoreMemoryResetParamGenerateErr()
{
	_generateErrIfMemoryCounterEQValue = 0;
	_generateErrIfMemoryCounterGTValue = 0;
	_generateErrIfMemoryCounterGEValue = 0;

	_generateErrIfMemorySizeRequestedEQValue = 0;
	_generateErrIfMemorySizeRequestedGTValue = 0;
	_generateErrIfMemorySizeRequestedGEValue = 0;
}

UT_EXPORT_DLL bool lpaCoreMemorySetParamGenerateErr(uint8_t param, long value)
{
	bool result = false;

	switch (param)
	{
		case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_EQ:
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] update LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_EQ <%d> to <%d>", _generateErrIfMemoryCounterEQValue, value );
			_generateErrIfMemoryCounterEQValue = value;
			result = true;
		break;

		case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GT:
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] update LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GT <%d> to <%d>", _generateErrIfMemoryCounterGTValue, value);
			_generateErrIfMemoryCounterGTValue = value;
			result = true;
		break;

		case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GE:
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] update LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GE <%d> to <%d>", _generateErrIfMemoryCounterGEValue, value);
			_generateErrIfMemoryCounterGEValue = value;
			result = true;
		break;
		
		///////////////////////////////////

		case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_EQ:
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] update LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_EQ <%d> to <%d>", _generateErrIfMemorySizeRequestedEQValue, value);
			_generateErrIfMemorySizeRequestedEQValue = value;
			result = true;
		break;

		case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GT:
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] update LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GT <%d> to <%d>", _generateErrIfMemorySizeRequestedGTValue, value);
			_generateErrIfMemorySizeRequestedGTValue = value;
			result = true;
		break;

		case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GE:
			lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] update LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GE <%d> to <%d>", _generateErrIfMemorySizeRequestedGEValue, value);
			_generateErrIfMemorySizeRequestedGEValue = value;
			result = true;
		break;

		default:
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] lpaCoreMemorySetParamGenerateErr() => Invalid param %d", param);
		break;
	}

	return result;
}

bool lpaCoreMemoryGetParamGenerateErr(uint8_t param, long* ptrValue)
{
	bool result = false;

	if (ptrValue != NULL)
	{
		switch (param)
		{
			case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_EQ:
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] get LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_EQ <%d>", _generateErrIfMemoryCounterEQValue);
				*ptrValue = _generateErrIfMemoryCounterEQValue;
				result = true;
			break;

			case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GT:
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] get LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GT <%d>", _generateErrIfMemoryCounterGTValue);
				*ptrValue = _generateErrIfMemoryCounterGTValue;
				result = true;
			break;

			case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GE:
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] get LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_COUNTER_GE <%d>", _generateErrIfMemoryCounterGEValue);
				*ptrValue = _generateErrIfMemoryCounterGEValue;
				result = true;
			break;

			///////////////////////////////////

			case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_EQ:
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] get LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_EQ <%d>", _generateErrIfMemorySizeRequestedEQValue);
				*ptrValue = _generateErrIfMemorySizeRequestedEQValue;
				result = true;
			break;

			case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GT:
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] get LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GT <%d>", _generateErrIfMemorySizeRequestedGTValue);
				*ptrValue = _generateErrIfMemorySizeRequestedGTValue;
				result = true;
			break;

			case LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GE:
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] get LPA_CORE_MEMORY_GENERATE_ERR_IF_MEMORY_SIZE_REQUESTED_GE <%d>", _generateErrIfMemorySizeRequestedGEValue);
				*ptrValue = _generateErrIfMemorySizeRequestedGEValue;
				result = true;
			break;

			default:
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] lpaCoreMemoryGetGenerateErr() => Invalid param %d", param);
			break;
		}
	}
	return result;
}

    #ifndef LPA_SDK__MEMORY_MONITORING

// No monitoring of allocation source
////////////////////////////////////////////////////

void* lpaCoreMemoryAlloc(size_t size)
{
	return _lpaCoreMemoryAlloc(size, false, NULL, 0);
}

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

void* lpaCoreMemoryCalloc(size_t count, size_t size)
{
	return _lpaCoreMemoryCalloc(count, size, false, NULL, 0);
}

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

void* lpaCoreMemoryRealloc(void* ptrMemoryOldBlock, size_t newSize)
{
	return _lpaCoreMemoryRealloc(ptrMemoryOldBlock, newSize, false, NULL, 0);
}

//////////////////////////////////////////////////////////////
// free memory
//////////////////////////////////////////////////////////////

void lpaCoreMemoryFree(void* ptrMemoryBlock)
{
	_lpaCoreMemoryFree(ptrMemoryBlock, false, NULL, 0);
}


    #endif // LPA_SDK__MEMORY_MONITORING NOT DEFINED

//////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////

 #ifdef LPA_SDK__USING_EX_API
UT_EXPORT_DLL bool lpaCoreGetMemoryStatus(LPA_MEMORY_STATUS* prtMemoryStatus)
{
	bool res = false;

	if (prtMemoryStatus != NULL)
	{
		prtMemoryStatus->countMemoryAllocCall = _countMemoryAllocCall;
		prtMemoryStatus->countMemoryFreeCall = _countMemoryFreeCall;
		prtMemoryStatus->currentMemoryBlockAllocated = _currentMemoryBlockAllocated;

		prtMemoryStatus->currentMemoryAllocated = _currentMemoryAllocated;
		prtMemoryStatus->totalMemoryAllocated = _totalMemoryAllocated;

		prtMemoryStatus->maxMemoryAllocated = _maxMemoryAllocated;
		prtMemoryStatus->maxMemoryBlockAllocated = _maxMemoryBlockAllocated;

		res = true;
	}

	return res;
}

#endif // LPA_SDK__USING_EX_API

//////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////

void lpaCoreMemoryDumpStatusIntoLog()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] === Memory status ===");

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] * countMemoryAllocCall : %ld", _countMemoryAllocCall);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] * countMemoryFreeCall : %ld", _countMemoryFreeCall);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] * currentMemoryBlockAllocated : %ld", _currentMemoryBlockAllocated);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] * currentMemoryAllocated : %ld bytes", _currentMemoryAllocated);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] * maxMemoryBlockAllocated : %ld", _maxMemoryBlockAllocated);
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] * maxMemoryAllocated : %ld bytes", _maxMemoryAllocated);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] * totalMemoryAllocated : %ld bytes", _totalMemoryAllocated);

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "=== ============= ===");

#ifdef LPA_SDK__MEMORY_MONITORING

	size_t indexEntryMemoryAlloc = 0;
	size_t indexEntryMemoryBlock = 0;

	// Parse _memoryAllocationInformation to display some potential memory leak
	// ///////////////////////////////////////////////////////////////////////////
	if (_ptrMemoryAllocationInformation != NULL && _ptrMemoryBlockInformation != NULL)
	{
		for (indexEntryMemoryAlloc = 0; indexEntryMemoryAlloc < MAX_MEMORY_ALLOCATION_INFORMATION; indexEntryMemoryAlloc ++)
		{
			if (_ptrMemoryAllocationInformation[indexEntryMemoryAlloc].allocationCounter > 0)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] <%d> allocation done at %s:%d",
					_ptrMemoryAllocationInformation[indexEntryMemoryAlloc].allocationCounter, _ptrMemoryAllocationInformation[indexEntryMemoryAlloc].allocationFileName, _ptrMemoryAllocationInformation[indexEntryMemoryAlloc].allocationLine);

				for (indexEntryMemoryBlock = 0; indexEntryMemoryBlock < MAX_MEMORY_BLOCK_INFORMATION; indexEntryMemoryBlock ++)
				{
					if (_ptrMemoryBlockInformation[indexEntryMemoryBlock].indexMemoryAllocationInformation == indexEntryMemoryAlloc && _ptrMemoryBlockInformation[indexEntryMemoryBlock].memoryBlockAllocated != NULL )
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM]   - memory block 0x%08lX => %d bytes", _ptrMemoryBlockInformation[indexEntryMemoryBlock].memoryBlockAllocated, _ptrMemoryBlockInformation[indexEntryMemoryBlock].memorySize);
					}
				}
			}
		}
	}
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "=== ============= ===");
#endif // LPA_SDK__MEMORY_MONITORING
}

//////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////

void lpaCoreMemoryCheckMemoryAllocated()
{
	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] === Check Memory Allocated ===");

#ifdef LPA_SDK__MEMORY_MONITORING
	_lpaCoreMemoryCheckMemoryAllocated(true);
#endif // LPA_SDK__MEMORY_MONITORING

	lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "=== ============= ===");
}

//////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////

bool _isSimulateMemoryErrorAllocation(size_t size)
{
	bool simulateMemoryErrorAllocation = false;

	if (!simulateMemoryErrorAllocation && _generateErrIfMemoryCounterEQValue > 0 && _countMemoryAllocCall == _generateErrIfMemoryCounterEQValue )
		simulateMemoryErrorAllocation = true;

	if (!simulateMemoryErrorAllocation && _generateErrIfMemoryCounterGTValue > 0 && _countMemoryAllocCall > _generateErrIfMemoryCounterGTValue)
		simulateMemoryErrorAllocation = true;

	if (!simulateMemoryErrorAllocation && _generateErrIfMemoryCounterGEValue > 0 && _countMemoryAllocCall >= _generateErrIfMemoryCounterGEValue )
		simulateMemoryErrorAllocation = true;

	if (!simulateMemoryErrorAllocation && _generateErrIfMemorySizeRequestedEQValue > 0 && (size == ((size_t)_generateErrIfMemorySizeRequestedEQValue)) )
		simulateMemoryErrorAllocation = true;

	if (!simulateMemoryErrorAllocation && _generateErrIfMemorySizeRequestedGTValue > 0 && (size > ((size_t)_generateErrIfMemorySizeRequestedGTValue)) )
		simulateMemoryErrorAllocation = true;

	if (!simulateMemoryErrorAllocation && _generateErrIfMemorySizeRequestedGEValue > 0 && (size >= ((size_t)_generateErrIfMemorySizeRequestedGEValue)) )
		simulateMemoryErrorAllocation = true;

	return simulateMemoryErrorAllocation;
}

void* _lpaCoreMemoryAlloc(size_t size, bool isAllocationSource, char* ptrFileName, int fileLine)
{
	void* ptrMemoryBlock = NULL;

#ifdef LPA_SDK__MEMORY_MONITORING
	if (_countMemoryAllocCall == 0)
		_initMemoryAllocationMonitoring();
#endif // LPA_SDK__MEMORY_MONITORING

#ifdef LPA_SDK__PAMPERS
	_lpaCoreMemoryCheckMemoryAllocated(false);
#endif //

	_countMemoryAllocCall++;

	if (size > 0)
	{
		if (!_isSimulateMemoryErrorAllocation(size))
		{
			unsigned char* ptrMem = malloc(size + 18);

			// 2 bytes => Tag Begin
			// 4 bytes => data size
			// 1 byte => status : 0x11 ==> Allocated - 0x22 => Free, Other code are forbidden
			// 3 byte => Index on _memory table (used if LPA_SDK__MEMORY_MONITORING)
			// 4 bytes => Not yet used (Must be 0x00 for the moment)
			// 2 bytes => Tag Middle
			// xx bytes => real memory requested
			// 2 bytes => Tag End


			if (ptrMem != NULL)
			{
				// Tag begin
				ptrMem[0] = 0xA5;
				ptrMem[1] = 0x5A;

				// Data size
				ptrMem[2] = (unsigned char)((size & 0xFF000000) >> 24);
				ptrMem[3] = (unsigned char)((size & 0x00FF0000) >> 16);
				ptrMem[4] = (unsigned char)((size & 0x0000FF00) >> 8);
				ptrMem[5] = (unsigned char)(size & 0x000000FF);

				// Status
				ptrMem[6] = (unsigned char)0x11;	// Allocated

				// Index info
				size_t indexMemoryBlockInfo = INVALID_MONITORING_INDEX; // By default, No information

#ifdef LPA_SDK__MEMORY_MONITORING
				if (isAllocationSource && ptrFileName != NULL)
				{
					indexMemoryBlockInfo = _addMemoryAllocationMonitoring(ptrFileName, fileLine, ptrMem, size);
				}
#endif // LPA_SDK__MEMORY_MONITORING

				ptrMem[7] = (unsigned char)((indexMemoryBlockInfo & 0xFF0000) >> 16);
				ptrMem[8] = (unsigned char)((indexMemoryBlockInfo & 0x00FF00) >> 8);
				ptrMem[9] = (unsigned char)((indexMemoryBlockInfo & 0x0000FF));

				// Not yet used : padding
				ptrMem[10] = 0x00;
				ptrMem[11] = 0x00;
				ptrMem[12] = 0x00;
				ptrMem[13] = 0x00;

				// Tag : middle
				ptrMem[14] = 0x16;
				ptrMem[15] = 0x64;

				ptrMemoryBlock = ptrMem + 16;

				if (_activateLogForMemoryManagementOperation)
					lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "[LPASDKMEM] _lpaCoreMemoryAlloc(%d bytes) => mem:0x%08lX (real memory @ 0x%08lX)", size, ptrMem, ptrMemoryBlock);

				// Initialize LPA SDK memory content
				memset(ptrMemoryBlock, MEMORY_CONTENT_INIT_PATTERN, size);

				// Tag End
				ptrMem[size + 16] = 0xAA;
				ptrMem[size + 17] = 0x55;

				_currentMemoryBlockAllocated++;
				_currentMemoryAllocated += size;
				_totalMemoryAllocated += size;


				if (_currentMemoryBlockAllocated > _maxMemoryBlockAllocated)
				{
					_maxMemoryBlockAllocated = _currentMemoryBlockAllocated;
					lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "[LPASDKMEM] _lpaCoreMemoryAlloc() => maxMemoryBlockAllocated updated to %d ", _maxMemoryBlockAllocated);
				}
				//tracking memory evolution.
				lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] Current MemoAllocated %d", _currentMemoryAllocated);
				if (_currentMemoryAllocated > _maxMemoryAllocated)
				{
					_maxMemoryAllocated = _currentMemoryAllocated;
					lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "[LPASDKMEM] _lpaCoreMemoryAlloc() => maxMemoryAllocated updated to %d bytes", _maxMemoryAllocated);
				}
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_ERROR, "[LPASDKMEM] _lpaCoreMemoryAlloc() do not allocate memory ( requested %d bytes)", _currentMemoryBlockAllocated );
			}
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryAlloc() do not allocate memory as requested");
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryAlloc() => No memory allocation for 0 bytes !!");

	return ptrMemoryBlock;
}

#ifdef LPA_SDK__MEMORY_MONITORING

void _initMemoryAllocationMonitoring()
{
	if (_ptrMemoryBlockInformation == NULL)
	{
		_ptrMemoryBlockInformation = malloc(sizeof(MEMORY_BLOCK_INFORMATION) * MAX_MEMORY_BLOCK_INFORMATION);
		if (_ptrMemoryBlockInformation != NULL)
			memset(_ptrMemoryBlockInformation, 0x00, sizeof(MEMORY_BLOCK_INFORMATION) * MAX_MEMORY_BLOCK_INFORMATION);
	}

	if (_ptrMemoryAllocationInformation == NULL)
	{
		_ptrMemoryAllocationInformation = malloc(sizeof(MEMORY_ALLOCATION_INFORMATION) * MAX_MEMORY_ALLOCATION_INFORMATION);
		if (_ptrMemoryAllocationInformation != NULL)
			memset(_ptrMemoryAllocationInformation, 0x00, sizeof(MEMORY_ALLOCATION_INFORMATION) * MAX_MEMORY_ALLOCATION_INFORMATION);
	}
}

size_t _addMemoryAllocationMonitoring(char* ptrFileName, int fileLine, void* ptrMem, size_t size)
{
	size_t indexArray = 0;	// Using to parse different array
	size_t indexMemoryBlockInfo = INVALID_MONITORING_INDEX;	// By default, no entry available
	size_t indexMemoryAllocationInfo = INVALID_MONITORING_INDEX;	// By default, no entry available

	if (_ptrMemoryAllocationInformation != NULL && _ptrMemoryBlockInformation != NULL && ptrFileName != NULL && ptrMem != NULL )
	{
		// Step 1 : Search if source file+line entry already present
		for (indexArray = 0; indexArray < MAX_MEMORY_ALLOCATION_INFORMATION; indexArray++)
		{
			if (_ptrMemoryAllocationInformation[indexArray].allocationCounter > 0)
			{
				// Record is not free => check if match with source file+line
				if (_ptrMemoryAllocationInformation[indexArray].allocationLine == fileLine)
				{
					char shortFileName[ALLOCATION_FILE_NAME_SIZE];
					if (strlen(ptrFileName) < ALLOCATION_FILE_NAME_SIZE)
						snprintf(shortFileName, ALLOCATION_FILE_NAME_SIZE, "%s", ptrFileName);
					else
						snprintf(shortFileName, ALLOCATION_FILE_NAME_SIZE, "%s", &ptrFileName[strlen(ptrFileName) - ALLOCATION_FILE_NAME_SIZE]);

					if (strcmp(shortFileName, _ptrMemoryAllocationInformation[indexArray].allocationFileName) == 0)
					{
						// Same source
						indexMemoryAllocationInfo = indexArray;
						break;
					}
				}
			}
		}

		if (indexMemoryAllocationInfo == INVALID_MONITORING_INDEX)
		{
			// Step 2 : Search empty record inside _ptrMemoryAllocationInformation
			for (indexArray = 0; indexArray < MAX_MEMORY_ALLOCATION_INFORMATION; indexArray++)
			{
				if (_ptrMemoryAllocationInformation[indexArray].allocationCounter == 0)
				{
					// Record is free
					indexMemoryAllocationInfo = indexArray;
					break;
				}
			}
		}

		if (indexMemoryAllocationInfo != INVALID_MONITORING_INDEX)
		{
			// Step 3 : Update _ptrMemoryAllocationInformation
			if (_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationCounter == 0)
			{
				// First call from this source code
				if (_activateLogForMemoryManagementOperation)
					lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "[LPASDKMEM] _lpaCoreMemoryAlloc() => Using entry <%d> on _ptrMemoryAllocationInformation object", indexMemoryAllocationInfo);

				_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationCounter = 1;
				_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationLine = fileLine;

				if (strlen(ptrFileName) < ALLOCATION_FILE_NAME_SIZE)
					snprintf(_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationFileName, ALLOCATION_FILE_NAME_SIZE, "%s", ptrFileName);
				else
					snprintf(_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationFileName, ALLOCATION_FILE_NAME_SIZE, "%s", &ptrFileName[strlen(ptrFileName) - ALLOCATION_FILE_NAME_SIZE]);
			}
			else
			{

				// Some call from this source code already under monitoring
				_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationCounter++;
			}
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryAlloc() => Unable to obtain a free entry on _ptrMemoryAllocationInformation object !");


		// Step 4 : Search empty record on _ptrMemoryBlockInformation
		for (indexArray = 0; indexArray < MAX_MEMORY_BLOCK_INFORMATION; indexArray++)
		{
			if (_ptrMemoryBlockInformation[indexArray].memoryBlockAllocated == NULL)
			{
				// Record is free
				indexMemoryBlockInfo = indexArray;
				break;
			}
		}

		if (indexMemoryBlockInfo != INVALID_MONITORING_INDEX)
		{
			_ptrMemoryBlockInformation[indexMemoryBlockInfo].memoryBlockAllocated = ptrMem;
			_ptrMemoryBlockInformation[indexMemoryBlockInfo].memorySize = size;
			_ptrMemoryBlockInformation[indexMemoryBlockInfo].indexMemoryAllocationInformation = indexMemoryAllocationInfo;
		}
		else
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryAlloc() => Unable to obtain a free entry on _ptrMemoryBlockInformation object !");

	}

	return indexMemoryBlockInfo;
}

void _writeLogMemoryAllocationBlockBefore(void *ptrMemoryBlock)
{
	// Search block allocated just before
	size_t indexArray = 0;	// Using to parse different array
	void *ptrMemoryBlockBefore = NULL;
	size_t indexBlockBefore = INVALID_MONITORING_INDEX;

	if (ptrMemoryBlock != NULL)
	{
		for (indexArray = 0; indexArray < MAX_MEMORY_BLOCK_INFORMATION; indexArray++)
		{
			if (_ptrMemoryBlockInformation[indexArray].memoryBlockAllocated != NULL)
			{
				if (ptrMemoryBlockBefore < _ptrMemoryBlockInformation[indexArray].memoryBlockAllocated && _ptrMemoryBlockInformation[indexArray].memoryBlockAllocated < ptrMemoryBlock)
				{
					ptrMemoryBlock = _ptrMemoryBlockInformation[indexArray].memoryBlockAllocated;
					indexBlockBefore = indexArray;
				}
			}
		}

		if (indexBlockBefore != INVALID_MONITORING_INDEX)
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] Memory block before at real memory @ 0x%08lX (%d bytes)", ptrMemoryBlock, _ptrMemoryBlockInformation[indexBlockBefore].memorySize );
			size_t indexMemoryAllocationInfo = _ptrMemoryBlockInformation[indexBlockBefore].indexMemoryAllocationInformation;
			if (indexMemoryAllocationInfo != INVALID_MONITORING_INDEX)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] Memory block before allocated at %s:%d",
					_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationFileName, _ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationLine);
			}
		}
	}
}

#endif // LPA_SDK__MEMORY_MONITORING

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

void* _lpaCoreMemoryCalloc(size_t count, size_t size, bool isAllocationSource, char* ptrFileName, int fileLine)
{
	void* ptrMemoryBlock = NULL;
	size_t memorySizeRequested = count * size;

	if (_activateLogForMemoryManagementOperation)
		lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "[LPASDKMEM] _lpaCoreMemoryCalloc(%d*%d=%d) => calling lpaCoreMemoryAlloc ...", count, size, memorySizeRequested);

	if (memorySizeRequested > 0)
	{
		ptrMemoryBlock = lpaCoreMemoryAlloc(memorySizeRequested);
		if (ptrMemoryBlock != NULL)
		{
			// Clear memory bloc allocated
			memset(ptrMemoryBlock, 0x00, memorySizeRequested);
		}
	}

	return ptrMemoryBlock;
}

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

void* _lpaCoreMemoryRealloc(void* ptrMemoryOldBlock, size_t newSize, bool isAllocationSource, char* ptrFileName, int fileLine)
{
	void* ptrNewMemoryBlock = NULL;

	if (_activateLogForMemoryManagementOperation)
		lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "[LPASDKMEM] _lpaCoreMemoryRealloc(%08lX,%d)", ptrMemoryOldBlock, newSize);

	if (ptrMemoryOldBlock == NULL)
	{
		if (newSize > 0)
		{
			// Case 1.1 : As malloc function
			ptrNewMemoryBlock = lpaCoreMemoryAlloc(newSize);
		}
		else
		{
			// Case 1.2 : No old memory bloc and no memory allocation needed
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryRealloc() => No memory allocation for 0 bytes !!");
		}
	}
	else
	{
		// case 2 
		if (newSize == 0)
		{
			// Case 2.1 : As free memory
			lpaCoreMemoryFree(ptrMemoryOldBlock);
			ptrMemoryOldBlock = NULL;
		}
		else
		{
			// Case 2.2 : Normal use case
			bool isValidMemoryBlock = false;
			size_t oldMemoryBlocSize = _lpaCoreMemoryGetBlockSize(ptrMemoryOldBlock, &isValidMemoryBlock);
			if (isValidMemoryBlock)
			{
				if (oldMemoryBlocSize != newSize)
				{
					ptrNewMemoryBlock = lpaCoreMemoryAlloc(newSize);
					if (ptrNewMemoryBlock != NULL)
					{
						memcpy(ptrNewMemoryBlock, ptrMemoryOldBlock, (newSize < oldMemoryBlocSize ? newSize : oldMemoryBlocSize));
					}
					else
						lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryRealloc() => Unable to allocate requested (%ld bytes) memory size !!", newSize);

					// free old memory bloc
					lpaCoreMemoryFree(ptrMemoryOldBlock);
					ptrMemoryOldBlock = NULL;
				}
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] _lpaCoreMemoryRealloc() : previous and requested size are identical => do nothing");
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryRealloc() => Do nothing because memory block is invalid !!");
		}
	}

	return ptrNewMemoryBlock;
}


//////////////////////////////////////////////////////////////
// free memory
//////////////////////////////////////////////////////////////

void _lpaCoreMemoryFree(void* ptrMemoryBlockUser, bool isAllocationSource, char* ptrFileName, int fileLine)
{
	_countMemoryFreeCall++;

	if (ptrMemoryBlockUser != NULL)
	{
		unsigned char* ptrMemoryBlockBegin = ((unsigned char*)ptrMemoryBlockUser) - 16;

		// Check Tag Begin
		if (ptrMemoryBlockBegin[0] == 0xA5 && ptrMemoryBlockBegin[1] == 0x5A)
		{
			// Begin tag is correct => Check if allocated or free
			if (ptrMemoryBlockBegin[6] == 0x11)
			{
				// retrieve data size
				size_t memoryBlocSize = (size_t)(ptrMemoryBlockBegin[2] << 24) + (size_t)(ptrMemoryBlockBegin[3] << 16) + (size_t)(ptrMemoryBlockBegin[4] << 8) + ptrMemoryBlockBegin[5];
				
				if (_activateLogForMemoryManagementOperation )
					lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "[LPASDKMEM] _lpaCoreMemoryFree(0x%08lX) => memoryBlockSize:%d - mem:0x%08lX", ptrMemoryBlockUser, memoryBlocSize, ptrMemoryBlockBegin);

				// Check Tag middle
				if (ptrMemoryBlockBegin[14] == 0x16 && ptrMemoryBlockBegin[15] == 0x64)
				{
					bool isCurrentMemoryAllocationInformation = false;

#ifdef LPA_SDK__MEMORY_MONITORING
					size_t indexMemoryBlock = (size_t)(ptrMemoryBlockBegin[7] << 16) + (size_t)(ptrMemoryBlockBegin[8] << 8) + ptrMemoryBlockBegin[9];

					MEMORY_ALLOCATION_INFORMATION currentMemoryAllocationInformation;
					memset( &currentMemoryAllocationInformation, 0x00, sizeof(MEMORY_ALLOCATION_INFORMATION));
					
					if (indexMemoryBlock == 0xFFFFFF)
						lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryFree() => memory block 0x%08lX not contains entry on _memoryAllocationInformation object", ptrMemoryBlockBegin);
					else
					{
						isCurrentMemoryAllocationInformation = _getMemoryAllocationInformation( &currentMemoryAllocationInformation, indexMemoryBlock);
						_freeMemoryAllocationMonitoring(ptrMemoryBlockBegin, indexMemoryBlock);
					}
#endif // LPA_SDK__MEMORY_MONITORING

					if (ptrMemoryBlockBegin[memoryBlocSize + 16] == 0xAA && ptrMemoryBlockBegin[memoryBlocSize + 17] == 0x55)
					{
						_currentMemoryAllocated -= memoryBlocSize;
						_currentMemoryBlockAllocated--;

						// Check memory usage
						if (memoryBlocSize > 0)
						{
							for (size_t countMemoryContent = 0; countMemoryContent < memoryBlocSize; countMemoryContent ++)
							{
								if ( (((unsigned char*)ptrMemoryBlockUser)[memoryBlocSize - countMemoryContent - 1]) != MEMORY_CONTENT_INIT_PATTERN)
								{
									if (countMemoryContent > 0)
									{
										lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] _lpaCoreMemoryFree() => last %d bytes seems not be used (memory block size : %d)",
											countMemoryContent, memoryBlocSize );
										
										if (isCurrentMemoryAllocationInformation)
										{
											lpaCoreLogAppend(SDK_LOG_LEVEL_DEBUG, "[LPASDKMEM] _lpaCoreMemoryFree() => Memory block allocated at %s, line %d",
												currentMemoryAllocationInformation.allocationFileName, currentMemoryAllocationInformation.allocationLine);
										}
										lpaCoreLogFlush();
									}
									break;
								}
							}
						}

						// And free this memory
						ptrMemoryBlockBegin[6] = 0x22;	// Mark as free
						free(ptrMemoryBlockBegin);

						ptrMemoryBlockBegin = NULL;
					}
					else
					{
						lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryFree() => invalid End Tag for memory block 0x%08lX (real memory @ 0x%08lX)", ptrMemoryBlockUser, ptrMemoryBlockBegin);
						lpaCoreLogFlush();
						_memoryMonitoringBreakpointDebugger();
					}
				}
				else
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryFree() => invalid Middle Tag for memory block 0x%08lX (real memory @ 0x%08lX)", ptrMemoryBlockUser, ptrMemoryBlockBegin);
					lpaCoreLogFlush();
					_memoryMonitoringBreakpointDebugger();
				}
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryFree() => memory block 0x%08lX (real memory @ 0x%08lX) already free", ptrMemoryBlockUser, ptrMemoryBlockBegin);
				lpaCoreLogFlush();
				_memoryMonitoringBreakpointDebugger();
			}
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryFree() => invalid Begin Tag for memory block 0x%08lX (real memory @ 0x%08lX)", ptrMemoryBlockUser, ptrMemoryBlockBegin);
			#ifdef LPA_SDK__MEMORY_MONITORING
                            _writeLogMemoryAllocationBlockBefore(ptrMemoryBlockBegin);
                        #endif
			lpaCoreLogFlush();
			_memoryMonitoringBreakpointDebugger();
		}

#ifdef LPA_SDK__PAMPERS
		_lpaCoreMemoryCheckMemoryAllocated(false);
#endif //
	}
	else
	{
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryFree() => try to free NULL memory block");
		lpaCoreLogFlush();
		_memoryMonitoringBreakpointDebugger();
	}

}

#ifdef LPA_SDK__MEMORY_MONITORING
void _freeMemoryAllocationMonitoring(void *ptrMemoryBlock, size_t indexMemoryBlockInfo)
{
	if (_ptrMemoryAllocationInformation != NULL && _ptrMemoryBlockInformation != NULL && ptrMemoryBlock != NULL)
	{
		// Step 1 : Update _ptrMemoryBlockInformation
		if (indexMemoryBlockInfo != INVALID_MONITORING_INDEX && indexMemoryBlockInfo < MAX_MEMORY_BLOCK_INFORMATION)
		{
			if (_ptrMemoryBlockInformation[indexMemoryBlockInfo].memoryBlockAllocated == ptrMemoryBlock)
			{
				size_t indexMemoryAllocationInfo = _ptrMemoryBlockInformation[indexMemoryBlockInfo].indexMemoryAllocationInformation;

				if (indexMemoryAllocationInfo != INVALID_MONITORING_INDEX && indexMemoryAllocationInfo < MAX_MEMORY_ALLOCATION_INFORMATION)
				{
					if (_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationCounter > 0)
					{
						_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationCounter--;
					}
				}
				else
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _freeMemoryAllocationMonitoring() => indexMemoryAllocationInfo <%d> is invalid !", indexMemoryBlockInfo);

				// Record is now free
				_ptrMemoryBlockInformation[indexMemoryBlockInfo].memoryBlockAllocated = NULL;
				_ptrMemoryBlockInformation[indexMemoryBlockInfo].indexMemoryAllocationInformation = INVALID_MONITORING_INDEX;
			}
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _freeMemoryAllocationMonitoring() => indexMemoryBlockInfo <%d> is invalid !", indexMemoryBlockInfo);
}

bool _getMemoryAllocationInformation(MEMORY_ALLOCATION_INFORMATION* ptrMemoryAllocationInformation, size_t indexMemoryBlockInfo)
{
	bool isMemoryAllocationInformation = false;

	if (_ptrMemoryAllocationInformation != NULL && _ptrMemoryBlockInformation != NULL && ptrMemoryAllocationInformation != NULL)
	{
		if (indexMemoryBlockInfo != INVALID_MONITORING_INDEX && indexMemoryBlockInfo < MAX_MEMORY_BLOCK_INFORMATION)
		{
			size_t indexMemoryAllocationInfo = _ptrMemoryBlockInformation[indexMemoryBlockInfo].indexMemoryAllocationInformation;

			if (indexMemoryAllocationInfo != INVALID_MONITORING_INDEX && indexMemoryAllocationInfo < MAX_MEMORY_ALLOCATION_INFORMATION)
			{
				if (_ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationCounter > 0)
				{

					ptrMemoryAllocationInformation->allocationCounter = _ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationCounter;
					ptrMemoryAllocationInformation->allocationLine = _ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationLine;
					memcpy(ptrMemoryAllocationInformation->allocationFileName, _ptrMemoryAllocationInformation[indexMemoryAllocationInfo].allocationFileName, sizeof(ptrMemoryAllocationInformation->allocationFileName));

					isMemoryAllocationInformation = true;
				}
			}
			else
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _getMemoryAllocationInformation() => indexMemoryAllocationInfo <%d> is invalid !", indexMemoryBlockInfo);
		}
	}
	else
		lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _getMemoryAllocationInformation() => invalid parameter !");

	return isMemoryAllocationInformation;
}

#endif // LPA_SDK__MEMORY_MONITORING

size_t _lpaCoreMemoryGetBlockSize(void* ptrMemoryBlock, bool* ptrIsValidMemoryBlock)
{
	size_t memoryBlockSize = 0;

	if (ptrMemoryBlock != NULL && ptrIsValidMemoryBlock != NULL)
	{
		unsigned char* dataBegin = ((unsigned char*)ptrMemoryBlock) - 16;
		*ptrIsValidMemoryBlock = false;

		// Check Tag Begin
		if (dataBegin[0] == 0xA5 && dataBegin[1] == 0x5A)
		{
			// Begin tag is correct => check if block always allocated
			if (dataBegin[6] == 0x11)
			{
				// retrieve data size
				memoryBlockSize = (size_t)(dataBegin[2] << 24) + (size_t)(dataBegin[3] << 16) + (size_t)(dataBegin[4] << 8) + dataBegin[5];

				if (dataBegin[memoryBlockSize + 16] != 0xAA || dataBegin[memoryBlockSize + 17] != 0x55)
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryGetBlocSize() => invalid End Tag for memory block 0x%08lX (real memory @ 0x%08lX)", ptrMemoryBlock, dataBegin);
					lpaCoreLogFlush();
					_memoryMonitoringBreakpointDebugger();
				}
				else
					*ptrIsValidMemoryBlock = true;
			}
			else
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryGetBlocSize() => Memory block 0x%08lx (real memory @ 0x%08lx) already free", ptrMemoryBlock, dataBegin);
				lpaCoreLogFlush();
				_memoryMonitoringBreakpointDebugger();
			}
		}
		else
		{
			lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryGetBlocSize() => invalid Begin Tag for memory block 0x%08lx (real memory @ 0x%08lx)", ptrMemoryBlock, dataBegin);
			#ifdef LPA_SDK__MEMORY_MONITORING
                            _writeLogMemoryAllocationBlockBefore(dataBegin);
                        #endif
			lpaCoreLogFlush();
			_memoryMonitoringBreakpointDebugger();
		}
	}

	return memoryBlockSize;
}

void _memoryMonitoringBreakpointDebugger()
{
#ifdef LPA_SDK__MEMORY_MONITORING
	#if defined(WIN32) && defined(_DEBUG) && defined(_WINDOWS)
		__debugbreak();
	#endif

	#ifdef LPA_SDK__PLATFORM_CYGWIN
		__builtin_trap();
	#endif // LPA_SDK__PLATFORM_CYGWIN
#endif // LPA_SDK__MEMORY_MONITORING
}


#ifdef LPA_SDK__MEMORY_MONITORING
void _lpaCoreMemoryCheckMemoryAllocated(bool trace)
{
	size_t indexEntryMemoryBlock = 0;

	for (indexEntryMemoryBlock = 0; indexEntryMemoryBlock < MAX_MEMORY_BLOCK_INFORMATION; indexEntryMemoryBlock++)
	{
		unsigned char* ptrMemoryBlockBegin = _ptrMemoryBlockInformation[indexEntryMemoryBlock].memoryBlockAllocated;
		size_t memorySize = _ptrMemoryBlockInformation[indexEntryMemoryBlock].memorySize;

		if (ptrMemoryBlockBegin != NULL)
		{
			bool isblockErrorDetected = false;
			if (trace)
				lpaCoreLogAppend(SDK_LOG_LEVEL_VERBOSE, "[LPASDKMEM] _lpaCoreMemoryCheckMemoryAllocated() --> checking _ptrMemoryBlockInformation <%d> ...", indexEntryMemoryBlock);

			// Check Tag Begin
			if (ptrMemoryBlockBegin[0] != 0xA5 || ptrMemoryBlockBegin[1] != 0x5A)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryCheckMemoryAllocated() => real memory @ 0x%08lx : Invalid Begin Tag", ptrMemoryBlockBegin);
				isblockErrorDetected = true;

				_writeLogMemoryAllocationBlockBefore(ptrMemoryBlockBegin);
			}

			// Check if allocated or free
			if (!isblockErrorDetected && ptrMemoryBlockBegin[6] != 0x11)
			{
				lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryCheckMemoryAllocated() => real memory @ 0x%08lx : Invalid Block status", ptrMemoryBlockBegin);
				isblockErrorDetected = true;
			}

			if (!isblockErrorDetected)
			{
				// retrieve data size
				size_t memoryRegisteredSize = (size_t)(ptrMemoryBlockBegin[2] << 24) + (size_t)(ptrMemoryBlockBegin[3] << 16) + (size_t)(ptrMemoryBlockBegin[4] << 8) + ptrMemoryBlockBegin[5];
				if (memoryRegisteredSize != _ptrMemoryBlockInformation[indexEntryMemoryBlock].memorySize)
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryCheckMemoryAllocated() => real memory @ 0x%08lx : Inconsistent memory size", ptrMemoryBlockBegin);
					isblockErrorDetected = true;
				}
			}

			if (!isblockErrorDetected)
			{
				// Check Padding padding
				if (ptrMemoryBlockBegin[10] != 0x00 || ptrMemoryBlockBegin[11] != 0x00)
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryCheckMemoryAllocated() => real memory @ 0x%08lx : Inconsistent memory padding", ptrMemoryBlockBegin);
					isblockErrorDetected = true;
				}
			}

			if (!isblockErrorDetected)
			{
				// Tag : middle
				if (ptrMemoryBlockBegin[14] != 0x16 || ptrMemoryBlockBegin[15] != 0x64)
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryCheckMemoryAllocated() => real memory @ 0x%08lx : Invalid Middle Tag", ptrMemoryBlockBegin);
					isblockErrorDetected = true;
				}
			}
			
			if (!isblockErrorDetected)
			{
				// Tag : end
				if (ptrMemoryBlockBegin[memorySize + 16] != 0xAA || ptrMemoryBlockBegin[memorySize + 17] != 0x55)
				{
					lpaCoreLogAppend(SDK_LOG_LEVEL_WARNING, "[LPASDKMEM] _lpaCoreMemoryCheckMemoryAllocated() => real memory @ 0x%08lx : Invalid End Tag", ptrMemoryBlockBegin);
					isblockErrorDetected = true;
				}
			}

			if (isblockErrorDetected)
			{
				lpaCoreLogFlush();
				_memoryMonitoringBreakpointDebugger();
			}
		}
	}
}

#endif // LPA_SDK__MEMORY_MONITORING

#endif // LPA_SDK__MEMORY

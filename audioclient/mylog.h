#pragma once
#ifndef _ZSUMMER_LOG4Z_H_
#define _ZSUMMER_LOG4Z_H_

#include "AudioDataUnit.h"

SPLAB_AUDIOCLIENT_BEGIN
//---------------------------------------------------------------------------------------------------
typedef void* voidPtr;
typedef voidPtr(*OpenLogFile_t)(const char* strPath, const char* strFileName, bool bLogByDay);
typedef int  (*WriteLog_t)(void* pLogFile, const char* fmt, ...);
typedef void  (*CloseLog_t)(void* pLogFile);
typedef voidPtr(*OpenDataQueue_t)(int64_t iTotalKB4QueueBuf, int iMaxBlockBytes, bool bForTCP);//bool bTCP = false
typedef bool (*GetDataQueueInfo_t)(void* pQueue, int64_t iSize[5]);
typedef int64_t(*EnDataQueue_t)(void* pQueue, void* data, int64_t iDataBytes, void* extradata1, int64_t  iExtraDataBytes1, void* extradata2, int64_t  iExtraDataBytes2);
//typedef int64_t(*EnDataQueue_t)(void* pQueue, void* e, int64_t iDataBytes, void* extradata1 = NULL, int64_t  iExtraDataBytes1 = 0, void* extradata2 = NULL, int64_t  iExtraDataBytes2 = 0);
typedef int64_t(*DeDataQueue_t)(void* pQueue, void** e);
typedef bool (*DataQueueIsFull_t)(void* pQueue);
typedef void (*CloseDataQueue_t)(void*& pQueue);
int loadDecoderModule(char* strModulePath);
//---------------------------------------------------------------------------------------------------
SPLAB_AUDIOCLIENT_END
#endif

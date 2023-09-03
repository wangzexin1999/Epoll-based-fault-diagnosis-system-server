#include "mylog.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <algorithm>
#include <iostream>
#include <dlfcn.h>

SPLAB_AUDIOCLIENT_BEGIN

OpenLogFile_t OpenLogFile = NULL;
WriteLog_t WriteLog = NULL;
CloseLog_t CloseLog = NULL;

OpenDataQueue_t  OpenDataQueue = NULL;
GetDataQueueInfo_t GetDataQueueInfo = NULL;
EnDataQueue_t EnDataQueue = NULL;
DeDataQueue_t DeDataQueue = NULL;
DataQueueIsFull_t DataQueueIsFull = NULL;
CloseDataQueue_t CloseDataQueue = NULL;

void* g_pMoudleHandle = NULL;

	int loadDecoderModule(char* strModulePath)
	{
		char* _tmpError;

		g_pMoudleHandle = dlopen(strModulePath, RTLD_LAZY);// RTLD_NOW);
		if (NULL == g_pMoudleHandle) {
			_tmpError = dlerror();
			fprintf(stderr, "***Load %s  failed with error %s. \n", strModulePath, _tmpError);
			return -1;
		}

		//---------------------  log functions ----------------------------------------------------------
		OpenLogFile = (OpenLogFile_t)dlsym(g_pMoudleHandle, "OpenLogFile");
		if ((_tmpError = dlerror()) != NULL) {
			fprintf(stderr, "***Load  'OpenLogFile' from %s failed with error %s.\n", strModulePath, _tmpError);
			return -3;
		}

		WriteLog = (WriteLog_t)dlsym(g_pMoudleHandle, "WriteLog");
		if ((_tmpError = dlerror()) != NULL) {
			fprintf(stderr, "***Load  'WriteLog' from %s failed with error %s.\n", strModulePath, _tmpError);
			return -5;
		}

		CloseLog = (CloseLog_t)dlsym(g_pMoudleHandle, "CloseLog");
		if ((_tmpError = dlerror()) != NULL) {
			fprintf(stderr, "***Load  'CloseLog' from %s failed with error %s.\n", strModulePath, _tmpError);
			return -7;
		}

		OpenDataQueue = (OpenDataQueue_t)dlsym(g_pMoudleHandle, "OpenDataQueue");
		if ((_tmpError = dlerror()) != NULL) {
			fprintf(stderr, "***Load  'OpenDataQueue' from %s failed with error %s.\n", strModulePath, _tmpError);
			return -10;
		}

		GetDataQueueInfo = (GetDataQueueInfo_t)dlsym(g_pMoudleHandle, "GetDataQueueInfo");
		if ((_tmpError = dlerror()) != NULL) {
			fprintf(stderr, "***Load  'GetDataQueueInfo' from %s failed with error %s.\n", strModulePath, _tmpError);
			return -11;
		}

		EnDataQueue = (EnDataQueue_t)dlsym(g_pMoudleHandle, "EnDataQueue");
		if ((_tmpError = dlerror()) != NULL) {
			fprintf(stderr, "***Load  'EnDataQueue' from %s failed with error %s.\n", strModulePath, _tmpError);
			return -12;
		}

		DeDataQueue = (DeDataQueue_t)dlsym(g_pMoudleHandle, "DeDataQueue");
		if ((_tmpError = dlerror()) != NULL) {
			fprintf(stderr, "***Load  'DeDataQueue' from %s failed with error %s.\n", strModulePath, _tmpError);
			return -13;
		}

		DataQueueIsFull = (DataQueueIsFull_t)dlsym(g_pMoudleHandle, "DataQueueIsFull");
		if ((_tmpError = dlerror()) != NULL) {
			fprintf(stderr, "***Load  'DataQueueIsFull' from %s failed with error %s.\n", strModulePath, _tmpError);
			return -14;
		}

		CloseDataQueue = (CloseDataQueue_t)dlsym(g_pMoudleHandle, "CloseDataQueue");
		if ((_tmpError = dlerror()) != NULL) {
			fprintf(stderr, "***Load  'CloseDataQueue' from %s failed with error %s.\n", strModulePath, _tmpError);
			return -15;
		}

	}
	SPLAB_AUDIOCLIENT_END
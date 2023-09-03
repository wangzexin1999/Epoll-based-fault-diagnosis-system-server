#ifndef AUDIOANALYSESERVER_GLOBAL_H
#define AUDIOANALYSESERVER_GLOBAL_H
#include <vector>
#include "AudioDataUnit.h"

int loadDecoderModule(char* strModulePath);

extern bool g_isExit;
extern int    g_isTest;

// config
extern int32_t g_iAudioServerPort;
extern float g_coordinationRate;
extern bool g_coordinationThreadOn;
extern int32_t g_iStatusLogInterval;
// module

//extern uint32_t *g_iModuleID;
// log path
extern char  g_strAnalyseLogPath[500];
extern char g_strLangClassifyLogPath[500];
extern char g_strMergedFilePath[500];

extern char     g_strCurTime[50];
extern time_t  g_StartTime, g_CurrentTime;//
extern unsigned  int  g_iTimerTimes;
extern unsigned  int  g_iSecondPast;

int32_t systemInitialize();

int32_t readConfig(char ini_file[]);

int32_t already_running();
#endif //AUDIOANALYSESERVER_GLOBAL_H

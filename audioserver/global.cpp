//
// Created by 997289110 on 4/8/20.
//

#include <unistd.h>
#include <string>
#include <csignal>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include "global.h"
#include "AudioServer.h"
#include "CCfgFileParser.h"

//----------------- data queue  begin ----------------------
extern OpenDataQueue_t  OpenDataQueue;
extern GetDataQueueInfo_t GetDataQueueInfo;
extern EnDataQueue_t EnDataQueue;
extern DeDataQueue_t DeDataQueue;
extern DataQueueIsFull_t DataQueueIsFull;
extern CloseDataQueue_t CloseDataQueue;
extern char g_strQueueModulePath[300];
extern void* g_pDataQueueHandle;
extern void** g_pRcvDataQueue;
extern int64_t g_iQueryQueueMB;//MB
extern int64_t g_iResultQueueMB;//MB
extern int g_iProcessThreadCnt;
extern bool g_bNotConcurrentQuery;
//----------------- data queue  end ----------------------

bool g_isExit = false;
int g_isTest = 0;
// config
int32_t g_iAudioServerPort = 6500;
float g_coordinationRate = 0;
bool g_coordinationThreadOn = false;
int32_t g_iStatusLogInterval = 10;

//uint32_t *g_iModuleID = nullptr;
// log path
char g_strSystemLogPath[500] ="./logs";

char  g_strRcvingFilePath[500] = "./file_rcving";

char     g_strCurTime[50], g_strStartTime[50];
time_t  g_StartTime, g_CurrentTime;//
unsigned  int  g_iTimerTimes = 0;
unsigned  int  g_iSecondPast = 0;
struct tm g_tCurLocalTime;//local time

SPLAB_AUDIOSERVER_BEGIN
extern uint32_t g_iMaxNetMsgByte;
}
using namespace hitsplab_audioserver;

int32_t lockFile(int32_t fd) {
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;
    return fcntl(fd, F_SETLK, &fl);
}

int32_t already_running() {
    int32_t lock_fd;
    char buf[512];
    char* pLockFilePath;
    pLockFilePath = "AudioServer.pid";
    lock_fd = open(pLockFilePath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (lock_fd < 0) {
        printf("cannot open server lock file: %s  err: %s.\n", pLockFilePath, strerror(errno));
        exit(1);
    }
    int32_t iRet = lockFile(lock_fd);
    if (iRet < 0) {
        if (errno == EACCES || errno == EAGAIN) {
            fprintf(stderr,"***Error: Server is already running. lockFile failed! %d %d %s\n", iRet, errno,strerror(errno));
            close(lock_fd);
            return 1;
        }
        printf("cannot lock server lock file: %s err:%s.\n", pLockFilePath, strerror(errno));
        exit(1);
    }
    ftruncate(lock_fd, 0);
    sprintf(buf, "%ld", (int64_t)getpid());
    write(lock_fd, buf, strlen(buf) + 1);
    return 0;
}

int myMakeDir(const char* sPathName)
{
    char   DirName[256];
    strcpy(DirName, sPathName);
    int   i, len = strlen(DirName);
    if (DirName[len - 1] != '/')
        strcat(DirName, "/");

    len = strlen(DirName);

    for (i = 1; i < len; i++)
    {
        if (DirName[i] == '/')
        {
            DirName[i] = 0;
            if (access(DirName, 0) != 0)
            {
                if (mkdir(DirName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
                {
                   fprintf(stderr,"***mkdir \'%s\' fail: %s\n", DirName,strerror(errno) );
                    return   -1;
                }
            }
            DirName[i] = '/';
        }
    }
    return 0;
}

void  ReadPath(CCfgFileParser   _Parser, char*strGroup, char* strSettingItem, char* Path, bool bReportErr)//2014.02.01
{
    int i, j;
    bool  ret = true;
    const char* tempPath = NULL;
    tempPath = _Parser.getValue(strGroup, strSettingItem);
    int iLen = strlen(tempPath);
    if (iLen > 1)
    {
        if(tempPath[iLen-1]=='/')
            sprintf(Path, "%s", tempPath);
        else
            sprintf(Path, "%s/", tempPath);

        myMakeDir(tempPath);
        printf("\n path item:%s::%s, value:%s\n", strGroup, strSettingItem, Path);
    }
    else 
    {
        ret = false;
        if (bReportErr)
        {
            fprintf(stderr, "\n* **Read config %s::% s failed! % s   will exit...\n", strGroup, strSettingItem, g_strCurTime);
            exit(__LINE__);
        }
    }
}

bool  ReadString(CCfgFileParser& myParser, char* SettingClass, char* SettingItem, char* Path)
{
    int i, j, ret = 1;
    const char* tempPath;
    int 	iLen;

    tempPath = myParser.getValue(SettingClass, SettingItem);
    iLen = strlen(tempPath);

    if (iLen > 1)
    {
        strcpy(Path, tempPath);
        //printf("path item:%s, value:%s\n", SettingItem, Path);
        return true;
    }
    return false;
}

int32_t readConfig(char* configFilePath)
{
    char temp_buf[100] = { 0 };
    int32_t n;
    if (access(configFilePath, 0))
    {
        fprintf(stderr, "***Error: cannot access file \'%s\'  err:%s %s\n\n", configFilePath, strerror(errno), g_strCurTime);
        return -1;
    }

    CCfgFileParser parser{};
    if (parser.parseFile(configFilePath) < 0) {
        fprintf(stderr, "Read Config %sFailed!", configFilePath);
        return -1;
    }
    g_iQueryQueueMB = parser.getIntValue("Settings", "QueryQueueMB"); //1024;//MB
    if (g_iQueryQueueMB < 512)
        g_iQueryQueueMB = 512;

    g_bNotConcurrentQuery = parser.getIntValue("Settings", "NotConcurentQueryAudioDLL"); //1024;//MB
    
    ReadPath(parser, "Settings", "SystemLogPath", g_strSystemLogPath, true);

    g_iStatusLogInterval = parser.getDoubleValue("Settings", "StatusLogInterval");
    if (g_iStatusLogInterval < 1)
        g_iStatusLogInterval = 20;//default
                                  
    //netsettings
    g_isTest = parser.getIntValue("Settings", "AudioServerTest");
    if (g_isTest > 0) g_isTest++;

       g_iProcessThreadCnt = parser.getIntValue("Settings", "ProcessThreadCnt"); //1024;//MB
        if (g_iProcessThreadCnt < 1)
            g_iProcessThreadCnt = 1;

        g_iAudioServerPort = parser.getIntValue("NetSettings", "AudioServerPort");
        if (g_iAudioServerPort < 1024)
        {
            fprintf(stderr, "***Error: fail to read NetSettings:AnalyseServerPort from %s\n\n", configFilePath);
            fprintf(stderr, "***Error: fail to read NetSettings:AnalyseServerPort from %s\n\n", configFilePath);
            throw(__LINE__);
        }

        n= parser.getIntValue("NetSettings", "MaxNetMsgByte");
        if(n>0)
            g_iMaxNetMsgByte = n*1024;
        else
            g_iMaxNetMsgByte = 1024 * 1024;
              
     g_coordinationRate = parser.getDoubleValue("NetSettings", "AudioServerCoordinationRate");
    g_coordinationThreadOn = parser.getDoubleValue("NetSettings", "AudioServerCoordinationThreadOn");
    
    hitsplab_audioserver::g_bPrintResultLog = (bool)parser.getIntValue("Settings", "AudioServerWriteLog");
    hitsplab_audioserver::g_outputBaseNum = parser.getIntValue("Settings", "AudioServerOutputBaseNum");

    printf("Load config file successfully!\n");
    return 0;
}

void exit_handler(int32_t signo)
{
    g_isExit = true;
    fprintf(stderr, "Waiting for quit!...");
}

__inline void updateTimeInfo()
{
    g_iTimerTimes++;
    g_iSecondPast++;
    time(&g_CurrentTime);
    g_tCurLocalTime = *localtime(&g_CurrentTime);
    strftime(g_strCurTime, sizeof(g_strCurTime), "%Y.%m.%d %H:%M:%S", &g_tCurLocalTime);//generate string with format of  "YYYY-MM-DD hh:mm:ss"

}

int32_t systemInitialize()
{
    int32_t err_code;
    updateTimeInfo();
    strftime(g_strStartTime, sizeof(g_strStartTime), "%Y.%m.%d %H:%M:%S", &g_tCurLocalTime);//generate string with format of  "YYYY-MM-DD hh:mm:ss"

    // err_code = readConfig((char*)"./AudioServerFunction.ini");
    // if (err_code < 0)
    // {
    //     fprintf(stderr, "***Error: read config fail!  ...exit\n");
    //     fprintf(stderr, "***Error: read config fail!  ...exit\n");
    //     fprintf(stderr, "***Error: read config fail!  ...exit\n");
    //     exit(-1);
    // }

    if (0 != loadDecoderModule("./libmmdecoder.so"))//lib for data queue
    {
        fprintf(stderr, "***Error load libmmdecoder.so fail!  exit...\n");
        fprintf(stderr, "***Error load libmmdecoder.so fail!  exit...\n");
        fprintf(stderr, "***Error load libmmdecoder.so fail!  exit...\n");
        exit(-2);
    }

        g_pStatusLog = OpenLogFile(g_strSystemLogPath, "audioserver-status.log", 0);
        g_pProcessLog = OpenLogFile(g_strSystemLogPath, "audioserver-process.log", 0);
        g_pResultLog = OpenLogFile(g_strSystemLogPath, "audioserver-result.log", 0);
        printf("....AudioServer for Analyse  Start.");
        WriteLog(g_pStatusLog, "....AudioServer Start. %s\n", g_strCurTime);
   
    struct sigaction sa, osa;
    sa.sa_handler = exit_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, &osa) < 0)
    {
        fprintf(stderr, "set singal ctrl + c failure! %s\n", g_strCurTime);
        return 0;
    }

    struct sigaction sa_pip;
    sa_pip.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa_pip, NULL);

    g_pRcvDataQueue = new void* [g_iProcessThreadCnt+1];
    for(int i=0;i<g_iProcessThreadCnt;i++)
        g_pRcvDataQueue[i] = OpenDataQueue(g_iQueryQueueMB * 1024/ g_iProcessThreadCnt, 1024 * 1024 * 5, true);

    hitsplab_audioserver::AudioServer myServer;
    myServer.StartServer();
    struct tm* t;
    while (!g_isExit)
    {
        sleep(1);
        updateTimeInfo();
    }

    return 0;
}

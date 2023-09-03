#include <stdio.h>
#include <iostream>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#include "AudioClient.h"
#include "Wave.h"
#define USE_LOG 1

SPLAB_AUDIOCLIENT_BEGIN
extern OpenLogFile_t OpenLogFile;
extern WriteLog_t WriteLog;
extern CloseLog_t CloseLog;

extern OpenDataQueue_t  OpenDataQueue;
extern GetDataQueueInfo_t GetDataQueueInfo;
extern EnDataQueue_t EnDataQueue;
extern DeDataQueue_t DeDataQueue;
extern DataQueueIsFull_t DataQueueIsFull;
extern CloseDataQueue_t CloseDataQueue;

extern uint64_t g_iLocalRcvBufMB;
extern uint64_t g_iNetRcvBufMB;

extern void*g_pProcLog;
extern char g_strClientLogPath[500];
extern char g_strCurClientTime[50], g_strStartTime[50];//g_strCurClientTime

extern bool g_bSavePCMQueryData;
extern uint64_t g_iWrongBlockCnt, g_iDiscardCnt, g_iQueueFull;
extern uint64_t g_iLocalRcvBufFull, g_iNetRcvBufFull, g_iFreeBufFull;

FILE* g_pDataLog = nullptr;
char g_strPcmPath[100] = "./pcm";
char temp_buf[50] = { 0 };
static bool g_bSysKeepRun = true;
extern uint64_t g_iStatusLogInterval;
static uint64_t g_iWorkTime = 0;
static pthread_t g_pTimerHandle;

extern uint32_t max_send_buffer_size;
extern uint64_t g_iTotalDataBytes;

//------------------------------------------------------------------------
static inline char* GetCurTimeStr(char* strBuf, int iAvailableBufSize)// g_strCurClientTime
{
    time_t tt;
    tt = time(NULL);
    //strftime(strBuf, iAvailableBufSize, "%H:%M:%S", localtime(&tt));//������hh:mm:ss����ʽ���ַ�����
    strftime(strBuf, iAvailableBufSize, "%Y.%m.%d %H:%M:%S", localtime(&tt));//������hh:mm:ss����ʽ���ַ�����
    //strftime(g_strCurDateTime, sizeof(g_strCurDateTime), "%Y.%m.%d %H:%M:%S", localtime(&tt));//������YYYY-MM-DD hh:mm:ss����ʽ���ַ�����

    return strBuf;
}

//------------------------------------------------------------------------
void* SystemTimer(void* param)
{
    char buff[1024 * 4], name[50], unit[20];
    unsigned long iTotalMem, iFreeMem, iCachedMem = 0;
    char strInfo[2048];
    GetCurTimeStr(g_strStartTime, sizeof(g_strStartTime));

    while (g_bSysKeepRun)
    {
        sleep(1);
        GetCurTimeStr(g_strCurClientTime, sizeof(g_strCurClientTime));
        g_iWorkTime += 1;

        /* if ((g_iWorkTime % g_iStatusLogInterval) == 0)
         {
             //PrintSysStatus(buff, sizeof(buff));
            // g_pStatusLog->append_one_item(0, "\n%s", buff);
             fprintf(stderr, "\n@%s\n", buff);
             fflush(stderr);
         }*/

    }//while;
}

//------------------------------------------------------------------------
int WriteWav2File(void* data, uint32_t size, string name) {
    //static MyWave wav_head(sizeof(MyWave));
    DIR* dir = opendir(g_strPcmPath);
    if (!dir) {
        int err_code = mkdir(g_strPcmPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (err_code < 0) {
            if (g_writeLog) WriteLog(g_pProcLog, "***Error: Create dir %s failed: %s %s\n", g_strPcmPath, strerror(errno), g_strCurClientTime);
            fprintf(stderr, "***Error: Create dir %s failed: %s %s\n", g_strPcmPath, strerror(errno), g_strCurClientTime);
            return 0;
        }
    }
    else {
        closedir(dir);
    }
    FILE* t_file = fopen(name.c_str(), "ab");
    if (t_file) {
        //fwrite(&wav_head, sizeof(wav_head), 1, t_file);
        fwrite(data, size, 1, t_file);
        fclose(t_file);
    }
    return 0;
}

SPLAB_AUDIOCLIENT_END

using namespace hitsplab_audioclient;
ClientManagement *clientManagement = NULL;

int32_t init_audio_client(hitsplab_audioclient::RcvAudioResult_t funcReportResult) {
    GetCurTimeStr(g_strCurClientTime, sizeof(g_strCurClientTime));

    pthread_create(&g_pTimerHandle, NULL, SystemTimer, NULL);

    printf("Call InitAnalyseDLL!\n");

    if (0 != loadDecoderModule("../libmmdecoder.so"))//lib for data queue
    {
        fprintf(stderr, "***Error load libmmdecoder.so fail!  exit...\n");
        fprintf(stderr, "***Error load libmmdecoder.so fail!  exit...\n");
        throw(-101);
    }

    // if (clientManagement->readFunctionConfig((char*)("./AudioFunction.ini")) != 0) {
    //     fprintf(stderr, "***Error: Read config ./AudioFunction.ini failed!@%s line %d\n", __FILE__, __LINE__);
    //     throw(-2);
    // }

    //g_pStatusLog = OpenLogFile(g_strClientLogPath, "client-client.log", 0);
      g_pProcLog = OpenLogFile(g_strClientLogPath, "client.log", 0);
    
    printf("--- init_audio_client successfully!\n");

    clientManagement = new ClientManagement();
    clientManagement->OpenDataQueues();
    clientManagement->initCallBackFunction((void*)funcReportResult);
    clientManagement->initThreadFunc();

    return 0;
}

int64_t send_pcm(uint8_t iDataType, void * pData, uint32_t ibytes, void* myHeader, uint32_t myBytes)
{//succ: retrun 0;
  //queue is full: retrun -20 //�ײ�Ķ����������뷵��-20
   //if iDataType == DT_WAV, pData  include the data description information, such as channel num, sample rate, sample bits

    int64_t iRet, iBytes;
    if ( !pData  )
    {
        if (g_writeLog) WriteLog(g_pProcLog, "***Error: data %p type %d %s\n", pData, iDataType,  g_strCurClientTime);
        fprintf(stderr, "***Error: data %p type %d %s\n", pData, iDataType, g_strCurClientTime);        
        g_iWrongBlockCnt++;
        return -1;
    }

    if (iDataType == DT_WAV)
    {
        //Write some info into the datablock?
        g_iPcmBlockCnt++;
    }    

     //EnDataQueue����ֵ��ʾ����е��ֽ�����<0��ʾ����
    NetMsgHeader myMsgHeader;
    myMsgHeader.m_iConfirmNum = CONFIRM_NUM;
    myMsgHeader.m_iMsgType = iDataType;
    myMsgHeader.m_iMsgLen = ibytes;//not include NetMsgHeader


    //myMsgHeader.m_iUnUsed[0] = myMsgHeader.m_iUnUsed[1] = myMsgHeader.m_iUnUsed[2] = 0; &myMsgHeader, sizeof(NetMsgHeader)
    iRet = EnDataQueue(clientManagement->m_pQueueLocalRcv,nullptr,0 ,pData, ibytes,myHeader,myBytes);
    if (iRet < iBytes)
    {
        g_iLocalRcvBufFull++;//   , g_iNetRcvBufFull , g_iFreeBufFull ;
        g_iQueueAddErrCnt++;
    }

    return 0;
}


int32_t close_audio_client(void) {
    g_isExit = true;
    delete clientManagement;
    if (g_pDataLog) 
        fclose(g_pDataLog);

    if (g_writeLog)
    {
        WriteLog(g_pProcLog, "The client has been closed!! %s\n", g_strCurClientTime);
        CloseLog(g_pProcLog);
    }
    fprintf(stderr, "The client has been closed!! %s\n", g_strCurClientTime);

    return 0;
}

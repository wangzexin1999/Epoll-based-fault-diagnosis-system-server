// Created by 997289110 on 4/1/20.

#ifndef AUDIOSERVER_MANAGEMENT_H
#define AUDIOSERVER_MANAGEMENT_H

#include <queue>
#include "EpollServer.h"
#include "AudioDataUnit.h"

SPLAB_AUDIOSERVER_BEGIN
    extern int32_t log_counter;
    // Network transmission variables
    using namespace std;

    class AudioServer {
    public:
        AudioServer();
        ~AudioServer();
        void StartServer();
        int32_t readHeaderFunc(EpollMateData *emd);
        int32_t readFunc(epoll_event* ev);
        int32_t writeFunc(epoll_event *ev);
        //! 数据处理
        //! thread function
        void requestListenFunc();
        void workerFunc(int idx);
        void outputStateFunc();
        static AudioServer *getPointer();
    private:
        EpollServer m_EpollServer{};
        static AudioServer *m_AudioServer;
        bool m_bStarted;
        //! counter
        uint64_t g_iQryCnt;
        uint64_t g_iRcvPcmBlockCnt, g_iRcvQueryErrCnt, g_iMovBufCnt;
        uint64_t g_iRcvDbAddCnt;
        uint64_t g_iSrcDataCnt;
        uint64_t g_iValidResultCnt, g_iDiscardResultCnt, g_iRcvResultCnt;
        
        uint64_t m_iSentCnt, m_iSendFailCnt, m_iReSendCnt1, m_iReSendCnt[3];
        uint64_t m_iWait2SendCnt;
        uint64_t  m_iHeaderErrCnt;
        uint64_t  m_iHeaderErrBytes;

        bool        m_bHeadErr;

        //! thread
        pthread_t m_ListenThread;
        pthread_t *m_pWorkerThread = nullptr;
        pthread_t m_OutputStateThread;
        pthread_t m_ThreadScanThread;
    };

    void* requestListenThread(void *argv);
    void* workerThread(void *argv);
    void* outputStateThread(void *argv);
    void* threadScanThread(void *argv);

//! call back functions
    int32_t read_call_back_func(epoll_event *ev);
    int32_t write_call_back_func(epoll_event *ev);

//! memory usage
    size_t get_mem_usage();
    inline uint32_t MBytes(uint32_t len);

    time_t convertStr2Seconds(char * str_time);
    void checkSystemMaxAge();

SPLAB_AUDIOSERVER_END
#endif //AUDIOSERVER_MANAGEMENT_H

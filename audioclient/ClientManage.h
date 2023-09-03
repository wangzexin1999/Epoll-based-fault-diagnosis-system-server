//
// Created by 997289110 on 12/11/19.
//

#ifndef AUDIOCLIENT_ANALYSEMANAGEMENT_H
#define AUDIOCLIENT_ANALYSEMANAGEMENT_H

#include "AudioDataUnit.h"
#include "TcpClient.h"
#include "CCfgFileParser.h"

#define CHECKNUM 128

SPLAB_AUDIOCLIENT_BEGIN
#define getCodeInfo() getCodeLine(__FILE__, __LINE__)
    using namespace std;
    typedef int (*RcvAudioResult_t)(uint32_t iModuleID, void *pResult);

    class ClientManagement {
    public:
        ClientManagement();
        ~ClientManagement();

        void initThreadFunc();
        int32_t readFunctionConfig(char* configPath);
        void initCallBackFunction(void *rcv_result);

        void netWorkFuncOld();
        void netWorkFunc();

        int readHeaderFunc(unsigned char *unitHeader, uint32_t &h_read_len);
        void ProcRcvQueueData();
        void OutputStatusFunc();
        void OpenDataQueues();
        static ClientManagement *get_pointer();
        void* m_pQueueLocalRcv;
        void* m_pQueueNetRcv;
    protected:
        bool connectToServer();
        //! call back function
        RcvAudioResult_t receiveAnalyseResult;

    private:
        //! network
        TcpClient m_TcpClient;

        //! thread
        pthread_t output_status_thread_;
        pthread_t free_memory_thread_;
        pthread_t net_work_thread_;
        pthread_t msg_deal_thread_;

        static ClientManagement *search_management_;
    };

    void *resultDealThread(void *param);
    void *outputStatusThread(void *param);
    void *networkThread(void *param);
//! utility
uint32_t queryInfoCheck(void *queryInfo);
/*
 * get such format: sample.cpp:line
 * @param file _FILE_
 * @param line _LINE_
 * @return sample.cpp:line
 * */
    char *getCodeLine(const char *file, int32_t line);

/*
 * get such format: year-month-day hh:mm:ss
 * */
    char *getCurTime();

    uint32_t MBytes(uint32_t len);

    SPLAB_AUDIOCLIENT_END

#endif //AUDIOCLIENT_ANALYSEMANAGEMENT_H

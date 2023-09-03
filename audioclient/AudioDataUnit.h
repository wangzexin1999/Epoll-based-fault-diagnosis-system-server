//
// Created by 997289110 on 12/11/19.
//

#ifndef AUDIOCLIENT_AUDIODATAUNIT_H
#define AUDIOCLIENT_AUDIODATAUNIT_H

#define CONFIRM_NUM 0x20230601
#ifndef SPLAB_AUDIOCLIENT_BEGIN
#define SPLAB_AUDIOCLIENT_BEGIN namespace hitsplab_audioclient {
#endif
#ifndef SPLAB_AUDIOCLIENT_END
#define SPLAB_AUDIOCLIENT_END }
#endif
#define MAX_SRC_DATA_BLOCK_SIZE (1024*1024*10) //Bytes
#define NET_MSG_HEADER_LEN sizeof(NetMsgHeader)


#include <map>
#include <queue>
#include "mylog.h"
#include <cstdint>
//#include "MyFile.h"


SPLAB_AUDIOCLIENT_BEGIN
    using namespace std;
// Internal variables
    extern bool g_bSavePCMQueryData;
    extern bool g_networkError;
    extern bool g_isExit;

    extern uint64_t g_iDBAddCnt;//ProgAdd操作的数量
    extern uint64_t g_iDBQueryCnt ;//ProgQuery操作的数量
    extern uint64_t g_iPcmBlockCnt;//AnalyseQuery操作的数量
    extern uint64_t g_iQueueAddErrCnt;
    
// config
    extern uint64_t g_iStatusLogInterval;
    extern bool g_writeLog;
    extern char g_strAudioServerIP[20];
    extern char g_strSrcServerIP[20];

    extern uint16_t g_iAudioServerPort;
    extern uint16_t g_iSrcServerPort;
    extern uint32_t g_iSendMaxFlux;

    extern uint64_t g_iSendBytes;

    enum DataType { // 数据类型
        DT_WAV = 0,
        DT_LOG,// 1
        DT_EPOLLALTER,// 2
        DT_EPOLLDEL,// 3
        DT_SERVER_MSG,// 4
    };

    typedef struct _NetMsgHeader {
        _NetMsgHeader(uint8_t _msg_type = 0, uint32_t _msg_len = 0) {
            this->m_iConfirmNum = CONFIRM_NUM;
            this->m_iMsgType = _msg_type;
            this->m_iMsgLen = _msg_len;
        };
        uint32_t m_iConfirmNum; // 有效标识
        uint32_t m_iMsgLen;      // 长度
        uint8_t   m_iMsgType;     // 类型
        uint8_t   m_iUnUsed[3];   // 保留字段
    }__attribute__((packed)) NetMsgHeader;
    //all net message format: [ NetMsgHeader | msg-data]

    typedef struct _MyHeader {
    _MyHeader(uint8_t _id = 0, uint32_t _channelNum = 0, double _sampleRate = 0) {
        this->m_id = _id;
        this->m_channelNum = _channelNum;
        this->m_sampleRate = _sampleRate;
    };
    uint32_t m_id; // 麦克风编号
    uint32_t m_channelNum;      // 通道数量
    double   m_sampleRate;     // 采样率
    uint8_t   m_iUnUsed[3];   // 保留字段
    }__attribute__((packed)) MyHeader;
    
SPLAB_AUDIOCLIENT_END
#endif //AUDIOCLIENT_AUDIODATAUNIT_H

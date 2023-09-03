#ifndef AUDIOSERVER_AUDIODATAUNIT_H
#define AUDIOSERVER_AUDIODATAUNIT_H

#include <queue>
#include <set>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <cstring>
#include <netinet/in.h>
#include <pthread.h>
#include <atomic>
#include <map>
#include <errno.h>

#define MAX_RECV_BUF_SIZE (1024*1024*64)
#define CONFIRM_NUM 0x20230601
#define MAX_QUEUE_SIZE 20000
#define INVALID_CLIENT_FD -1

#define NET_MSG_HEADER_LEN sizeof(NetMsgHeader)

#define USE_MAX_AGE 1
#ifndef SPLAB_AUDIOSERVER_BEGIN
#define SPLAB_AUDIOSERVER_BEGIN namespace hitsplab_audioserver {
#endif
#ifndef SPLAB_AUDIOSERVER_END
#define SPLAB_AUDIOSERVER_END }
#endif


#define CHECKNUM 128
using namespace std;

//==========================================================
typedef void* voidPtr;
typedef voidPtr(*OpenDataQueue_t)(int64_t iTotalKB4QueueBuf, int iMaxBlockBytes, bool bConcurrent);
typedef bool (*GetDataQueueInfo_t)(void* pQueue, int64_t iSize[5]);
typedef int64_t(*EnDataQueue_t)(void* pQueue, void* data, int64_t iDataBytes, void* extradata1, int64_t  iExtraDataBytes1, void* extradata2, int64_t  iExtraDataBytes2);
typedef int64_t(*DeDataQueue_t)(void* pQueue, void** e);
typedef bool (*DataQueueIsFull_t)(void* pQueue);
typedef void (*CloseDataQueue_t)(void*& pQueue);
//---------------------------------------------------------------------------------------------------
typedef voidPtr(*OpenLogFile_t)(const char* strPath, const char* strFileName, bool bLogByDay);
typedef int  (*WriteLog_t)(void* pLogFile, const char* fmt, ...);
typedef void  (*CloseLog_t)(void* pLogFile);
//---------------------------------------------------------------------------------------------------
extern OpenDataQueue_t  OpenDataQueue;
extern GetDataQueueInfo_t GetDataQueueInfo;
extern EnDataQueue_t EnDataQueue;
extern DeDataQueue_t DeDataQueue;
extern DataQueueIsFull_t DataQueueIsFull;
extern CloseDataQueue_t CloseDataQueue;

extern OpenLogFile_t OpenLogFile;
extern WriteLog_t WriteLog;
extern CloseLog_t CloseLog;
extern void* g_pStatusLog, * g_pProcessLog, * g_pResultLog;

//==========================================================

extern int32_t g_iStatusLogInterval;
extern int32_t g_iSystemMaxAge;
SPLAB_AUDIOSERVER_BEGIN

#define getCodeInfo() getCodeLine(__FILE__, __LINE__)
// config
extern bool g_bPrintResultLog;
extern int32_t g_outputBaseNum;

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

    class ServerMsgHeader {
    public:
        ServerMsgHeader() {
            memset(this, 0, sizeof(ServerMsgHeader));
        };
        ServerMsgHeader(uint8_t _type, int32_t _infd, struct in_addr _ip, uint32_t _len, unsigned char* _data) {
            this->m_iType = _type;
            this->m_iFd = _infd;
            this->m_Ip = _ip;
            this->m_iLen =_len;
            this->m_pData = _data;
        };
        ~ServerMsgHeader() {
            //if (query_data) delete[] query_data;
        }
        uint8_t m_iType;
        uint32_t m_iLen;
        int32_t m_iFd;
        struct in_addr m_Ip;
        unsigned char* m_pData;
    };// __attribute__((packed));

    template<typename I, typename T>
    class TaskMap{
        public:
            TaskMap();
            ~TaskMap();
            int addNewNode(I id, T node);
            T getNodeByID(I id, int& status);
            bool isExist(I id);

            pthread_mutex_t m_Mutex;
            map<I, T> m_DataMap;
    };

    template<typename I, typename T>
    TaskMap<I, T>::TaskMap() {
        pthread_mutex_init(&m_Mutex, NULL);
    }

    template<typename I, typename T>
    TaskMap<I, T>::~TaskMap() {
        pthread_mutex_destroy(&m_Mutex);
    }

    template<typename I, typename T>
    int TaskMap<I, T>::addNewNode(I id, T node) {
        pthread_mutex_lock(&m_Mutex);
        m_DataMap[id] = node;
        pthread_mutex_unlock(&m_Mutex);
        return 0;
    }

    template<typename I, typename T>
    bool TaskMap<I, T>::isExist(I id) {
        //if (id < 0) return false;
        bool isEmpty = false;
        pthread_mutex_lock(&m_Mutex);
        if (m_DataMap.find(id) != m_DataMap.end()) isEmpty = true;
        pthread_mutex_unlock(&m_Mutex);
        return isEmpty;
    }

    template<typename I, typename T>
    T TaskMap<I, T>::getNodeByID(I id, int& status) {
        status = -1;
        // if (id < 0) return nullptr;
        T node = nullptr;
        pthread_mutex_lock(&m_Mutex);
        if (m_DataMap.find(id) != m_DataMap.end()) {
            node = m_DataMap[id];
            status = 0;
        }
        pthread_mutex_unlock(&m_Mutex);
        return node;
    }

class EpollMateData {
public:
    EpollMateData() {
        m_iFd = INVALID_CLIENT_FD;
        //[NetMsgHeader | data]
        m_pRSQueWait4Send = nullptr;
        MallocRcvBuf(MAX_RECV_BUF_SIZE);
        initEpollMateData();
    };
    void MallocRcvBuf(uint64_t iSize)
    {
        m_iRcvBufSize = iSize;
        m_pRcvBuf = new unsigned char[m_iRcvBufSize];
        m_pNetMsgHeader =(NetMsgHeader*) m_pRcvBuf;
        m_iRcvStartPos = 0;
    }
    void initEpollMateData() {
        m_iReadLen = 0;
        m_iSentLen = 0;
        m_iCloseType = 0;
        m_pNetMsgHeader = (NetMsgHeader*)m_pRcvBuf;;
        m_pSndBuf = nullptr;
        m_iWantSndLen = m_iOriginalLen = 0;
        m_iWantReadLen = 0;
        m_iRcvStartPos = 0;
     };

    ~EpollMateData() {
        delete[] m_pRcvBuf;
        if (m_pRSQueWait4Send) {
            void* pData = nullptr;

            int64_t iBytes = DeDataQueue(m_pRSQueWait4Send, (void**)(&pData));
            while (iBytes > 0) { // empty or erro
                delete[](unsigned char*)(*(void**)pData);
                iBytes = DeDataQueue(m_pRSQueWait4Send, (void**)(&pData));
            };
            CloseDataQueue(m_pRSQueWait4Send);
        }
    }
public:
        //! network
        int32_t m_iFd;
        struct in_addr m_Ip;
        int32_t m_iPort;
        //! receive data
        int64_t m_iReadLen;
        int64_t m_iRcvBufSize, m_iRcvStartPos;
        unsigned char *m_pRcvBuf;
        NetMsgHeader*m_pNetMsgHeader;
        int64_t m_iWantReadLen;
        //! send data
        //modify------2021.09.10,此队列用于保存第三方返回的结果
        void *m_pRSQueWait4Send;//result queue 2 send
        unsigned char *m_pSndBuf;
        int64_t m_iWantSndLen, m_iOriginalLen;
        int64_t m_iSentLen;
        char m_pHostName[NI_MAXHOST];//zgb
        int m_iCloseType;
    };

    typedef TaskMap<int32_t, EpollMateData *> TransMap;

SPLAB_AUDIOSERVER_END
#endif //AUDIOSERVER_AUDIODATAUNIT_H

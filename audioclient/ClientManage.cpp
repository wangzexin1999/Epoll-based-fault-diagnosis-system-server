#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <atomic>
#include <sys/stat.h>
#include "ClientManage.h"

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


bool g_bSavePCMQueryData;
uint64_t  g_iWrongBlockCnt = 0, g_iDiscardCnt=0, g_iQueueFull=0;
int64_t g_iRcvMsgCnt = 0;

uint64_t g_iLocalRcvBufMB = 512;
uint64_t g_iNetRcvBufMB = 256;

// Internal variables
   char g_strCurClientTime[50];
   char g_strStartTime[50];

   char g_strClientLogPath[500]="./logs";
   void * g_pProcLog = NULL;

    bool g_networkError = true;
    bool g_isExit = false;
    uint64_t g_iDBAddCnt = 0;//ProgAdd操作的数量
    uint64_t g_iDBQueryCnt = 0;//ProgQuery操作的数量
    uint64_t g_iPcmBlockCnt = 0;//AnalyseQuery操作的数量
    uint64_t g_iQueueAddErrCnt=0;


// config
    uint64_t g_iStatusLogInterval = 20;
    bool g_writeLog = false;
    char g_strAudioServerIP[20] = { 0 };
    char g_strSrcServerIP[20] = { 0 };
    uint16_t g_iAudioServerPort;
    uint16_t g_iSrcServerPort;
    uint32_t g_iSendMaxFlux = 300;//Mbps


    uint32_t max_send_buffer_size = 50 * 1024 * 1024;
    uint64_t g_iSendBytes=0;
    uint64_t g_iTotalDataBytes =0;
    uint64_t g_iLocalRcvBufFull = 0, g_iNetRcvBufFull = 0, g_iFreeBufFull = 0;

 //////////////////////////////////////////////////////////////////////////
//! 构造函数&&析构函数
//////////////////////////////////////////////////////////////////////////
    ClientManagement::ClientManagement() {
        if (ClientManagement::search_management_ != nullptr) {
            return;
        }
        //! network
        srand(time(NULL));
        ClientManagement::search_management_ = this;
        m_pQueueLocalRcv=NULL;
        m_pQueueNetRcv = NULL;
    }

    ClientManagement::~ClientManagement() {
        if (!pthread_join(net_work_thread_, nullptr)) {
            if (g_writeLog)  WriteLog(g_pProcLog,"network thread has shutdown %s\n", g_strCurClientTime);
        }
        if (!pthread_join(msg_deal_thread_, nullptr)) {
            if (g_writeLog) WriteLog(g_pProcLog,"result deal thread has shutdown %s\n", g_strCurClientTime);
        }
        if (!pthread_join(output_status_thread_, nullptr)) {
            if (g_writeLog)  WriteLog(g_pProcLog,"output status thread has shutdown %s\n", g_strCurClientTime);
        }
        if (!pthread_join(free_memory_thread_, nullptr)) {
            if (g_writeLog)  WriteLog(g_pProcLog,"free memery thread has shutdown %s\n", g_strCurClientTime);
        }
    }

    ClientManagement *ClientManagement::search_management_ = nullptr;

    ClientManagement *ClientManagement::get_pointer() {
        return ClientManagement::search_management_;
    }
//////////////////////////////////////////////////////////////////////////
//! 初始化函数
//////////////////////////////////////////////////////////////////////////
    void ClientManagement::OpenDataQueues()
    {
            m_pQueueLocalRcv = OpenDataQueue(g_iLocalRcvBufMB * 1024, 4*1024 * 1024, 1);
            m_pQueueNetRcv = OpenDataQueue(   g_iNetRcvBufMB * 1024, 4*1024 * 1024, 1);
            if (m_pQueueLocalRcv == NULL ||  m_pQueueNetRcv == NULL)
            {
                fprintf(stderr, "***Error: QueueLocalRcv %p  QueueNetRcv %p  %s %d\n",
                    m_pQueueLocalRcv, m_pQueueNetRcv,
                    __FILE__, __LINE__);
                throw(__LINE__);
            }        
    }


    void ClientManagement::initThreadFunc() {
        int32_t error_code = pthread_create(&net_work_thread_, nullptr, networkThread, nullptr);
        if (error_code != 0) {
            if (g_writeLog) WriteLog(g_pProcLog, "***Error: Can't create network thread! %s\n", g_strCurClientTime);
        }
        //! output status
        error_code = pthread_create(&output_status_thread_, nullptr, outputStatusThread, nullptr);
        if (error_code != 0) {
            if (g_writeLog) WriteLog(g_pProcLog, "***Error: Can't create output status thread! %s\n", g_strCurClientTime);
        }

        //! resultDealThread
        error_code = pthread_create(&msg_deal_thread_, nullptr, resultDealThread, nullptr);
        if (error_code != 0) {
            if (g_writeLog) WriteLog(g_pProcLog, "***Error: Can't create result deal thread! %s\n", g_strCurClientTime);
        }
    }

    void ClientManagement::initCallBackFunction(void *rcv_result) {
            receiveAnalyseResult = (RcvAudioResult_t)rcv_result;
    }
//////////////////////////////////////////////////////////////////////////
//! 服务器配置
//////////////////////////////////////////////////////////////////////////
      //-----------------------------------------------------------------------------------------
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
                        perror("tba mkdir   error");
                        return   -1;
                    }
                }
                DirName[i] = '/';
            }
        }
        return 0;
    }
    
    //-----------------------------------------------------------------------------------------
    bool  ReadString(CCfgFileParser& myParser, char* SettingClass, char* SettingItem, char* dst)
    {
        int i, j, ret = 1;
        const char* tempPath;
        int 	ibytes;

        tempPath = myParser.getValue(SettingClass, SettingItem);
        ibytes = strlen(tempPath);

        if (ibytes > 1)
        {
            strcpy(dst, tempPath);
            //printf("path item:%s, value:%s\n", SettingItem, Path);
            return true;
        }
        return false;
    }
 
    //-----------------------------------------------------------------------------------------
    int  ReadPath(CCfgFileParser   _Parser, char* SettingClass, char* SettingItem, char* Path, bool bReportErr)//2014.02.01
    {
        int i, j, ret = 1;
        const char* tempPath = NULL;
        tempPath = _Parser.getValue(SettingClass, SettingItem);

        if (strlen(tempPath) > 1)
        {
            sprintf(Path, "%s", tempPath);
            myMakeDir(tempPath);
            printf("\n path item:%s, value:%s\n", SettingItem, Path);
        }
        else if (bReportErr)
        {
           fprintf(stderr, "*** %s is not appointed!\n", SettingItem);
            ret = -5;
        }
        return ret;
    }
    //-----------------------------------------------------------------------------------------
    int32_t ClientManagement::readFunctionConfig(char* configPath) {
        CCfgFileParser parser;
        if (parser.parseFile(configPath)) {
            return -1;
        }
        if (!ReadString(parser, "Settings", "AudioServerIP", g_strAudioServerIP))
        {
            fprintf(stderr, "****Error: fail to read Settings::AnalyseServerIP(%s) from %s\n\n", g_strAudioServerIP, configPath);
            throw(__LINE__);
        }

        g_iAudioServerPort = (uint16_t)parser.getIntValue("Settings", "AudioServerPort");

        if (g_iAudioServerPort < 1)
        {
            fprintf(stderr, "****Error: fail to read Settings::AnalyseServerPort(%d) from %s\n\n", g_iAudioServerPort, configPath);
            throw(__LINE__);
        }
        g_iSendMaxFlux = (uint16_t)parser.getIntValue("Settings", "MaxSendMbps");
        ReadPath(parser, "Settings", "LogPath", g_strClientLogPath, true);
        int   ibytes = strlen(g_strClientLogPath);
        if (g_strClientLogPath[ibytes - 1] != '/')
        {
            g_strClientLogPath[ibytes] = '/';
            g_strClientLogPath[ibytes + 1] = 0;
        }

        g_iStatusLogInterval = parser.getIntValue("Settings", "StatusLogInterval");
        if (g_iStatusLogInterval < 1)
            g_iStatusLogInterval = 20;
        if (g_iStatusLogInterval > 600)
            g_iStatusLogInterval = 600;

        g_writeLog = (bool)parser.getIntValue("Settings", "AudioClientWriteLog");

        uint64_t n;
        n = parser.getIntValue("Settings", "CLIENT_LOCAL_RCV_BUF_MB");
        g_iLocalRcvBufMB = n>32 ?n: g_iLocalRcvBufMB;

        n = parser.getIntValue("Settings", "CLIENT_NET_RCV_BUF_MB");
        g_iNetRcvBufMB = n > 32 ? n : g_iNetRcvBufMB;

        n = parser.getIntValue("Settings", "CLIENT_SRC_SEND_BUF_MB");

        return 0;
    }

 //////////////////////////////////////////////////////////////////////////
//!network
//////////////////////////////////////////////////////////////////////////

    bool ClientManagement::connectToServer() {
        if (g_networkError) {
            int32_t connect_code = 0;
            if (g_writeLog) WriteLog(g_pProcLog,"---trying to connect to server %s:%hu %s\n", g_strAudioServerIP, g_iAudioServerPort, g_strCurClientTime);
            connect_code = m_TcpClient.connectServer("10.211.55.4", 6500, true, g_iSendMaxFlux);
            if (connect_code) {
                g_networkError = true;
                int32_t try_gap = 10 + rand() % 11;
                if (g_writeLog)  WriteLog(g_pProcLog,"---connect to server error, will retry after %d seconds %s\n", try_gap, g_strCurClientTime);
                sleep(try_gap);
            } else {
                if (g_writeLog) WriteLog(g_pProcLog,"successfully connect to server %s:%hu %s\n", g_strAudioServerIP, g_iAudioServerPort, g_strCurClientTime);
                m_TcpClient.makeSocketNonBlocking();
                m_TcpClient.setTcpNoDelay();
                //m_TcpClient.setSockBufferSize(BUFFER_SIZE);
                g_networkError = false;
            }
        }
        return !g_networkError;
    }

    int64_t g_iHeaderErrCnt = 0;
    int64_t g_iHeaderErrBytes = 0;
    int64_t g_iTotalRcvBytes = 0;
    int32_t g_iRcvBufSizeMB = 16;
    int ClientManagement::readHeaderFunc(unsigned char *unitHeader, uint32_t &h_read_len) {
        int32_t x = 0x0fffffff;
        ssize_t t_bytes = 0;
        NetMsgHeader *unit_header = (NetMsgHeader *)unitHeader;
        while (true) {
            if (g_isExit) {
                h_read_len = 0;
                return -2;
            }
            //! receive message header
            if (h_read_len < sizeof(NetMsgHeader)) { // 接收了部分header
                t_bytes = m_TcpClient.receiveData(unitHeader + h_read_len, sizeof(NetMsgHeader) - h_read_len);
            } else {
                h_read_len = 0;
                return -1;
            }
            //! judge receive bytes
            if (t_bytes > 0) { // 接收temp_bytes字节header
                h_read_len += t_bytes;
            } else if (0 == t_bytes) { // 对端关闭socket
                if (g_writeLog) WriteLog(g_pProcLog,"connection closed by peer %s\n", g_strCurClientTime);
                g_networkError = true;
                h_read_len = 0;
                return -2;
            } else { // 接收失败
                if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                    return -1;
                } else {
                    if (g_writeLog) WriteLog(g_pProcLog,"receive unit header from server error %s\n", g_strCurClientTime);
                    g_networkError = true;
                    h_read_len = 0;
                    return -2;
                }
            }
            //! judge header
            if (sizeof(NetMsgHeader) == h_read_len) { // 成功接收header
                h_read_len = 0;
                if (CONFIRM_NUM == unit_header->m_iConfirmNum) {
                    // right unit header
                    return 0;
                } else { // wrong unit header m_iErrorHeaderCnt
                    g_iHeaderErrCnt++;
                    size_t i;
                    bool bFoud = false;
                    for (i = 1; i < sizeof(NetMsgHeader) - 3; ++i) {
                        if (*((uint32_t*)(unitHeader + i)) == CONFIRM_NUM) {
                            bFoud = true;
                            break;
                        }
                    }
                    h_read_len = sizeof(NetMsgHeader) - i;
                    for (uint32_t j = i; j < sizeof(NetMsgHeader); j++) unitHeader[j - i] = unitHeader[j];
                }// wrong unit header
            }
        }
    }

    //--------------------------------------------------------------------------------------
    char* GetMsgTypeName(int iType)
    {
        char* pStrTypeName;
        switch (iType)
        {
        case 	DT_EPOLLALTER:
            pStrTypeName = "epollalter-result"; break;
        case DT_EPOLLDEL:
            pStrTypeName = "epolldel-result"; break;
        case DT_SERVER_MSG:
            pStrTypeName = "server-msg"; break;
        default:
            pStrTypeName = "unknow-msg";
        }
        return pStrTypeName;
    }
    
    //--------------------------------------------------------------------------------------
	void ClientManagement::netWorkFunc()
	{//new version
		ssize_t iRcvBytes = 0, iSendBytes = 0;
		unsigned char* pRcvBuf = nullptr;
		uint32_t iRcvStartPos;
		unsigned char* pBuf;// QUERY_HEADER_SIZE;
		uint64_t iAvailableBufSize;// QUERY_HEADER_SIZE;
		int64_t iRcvBufSize, iNeedBufSize;
		NetMsgHeader* pNetMsgHeader = NULL;

		uint32_t iRcvMsgBytes;
		uint32_t iReadLen;
		void* pSendBuf = NULL;// = new unsigned char[max_send_buffer_size];
		uint32_t iWantSendLen = 0;
		uint32_t iSentBytes = 0;
		int64_t iRet, iBytes;
		void* pData;
		uint32_t iSleepTimes = 0;
		char strBuf[1024];
		int i, iFindTimes;
		bool bHeadErr = false;

		iRcvBufSize = g_iRcvBufSizeMB * 1024 * 1024;
		pRcvBuf = new unsigned char[iRcvBufSize];

		//-----init the rcv buf and variables
		iRcvStartPos = 0;
		pBuf = pRcvBuf + iRcvStartPos;
		iAvailableBufSize = iRcvBufSize - iRcvStartPos;
		pNetMsgHeader = (NetMsgHeader*)pBuf;
		iRcvMsgBytes = 0;
		iReadLen = 0;


		while (!g_isExit) {
			//! network
			while (!connectToServer() && !g_isExit);
			//! handle message body
			//! receive message body
			iFindTimes = 0;
			iRcvBytes = recv(m_TcpClient.sock_fd, pBuf + iReadLen, iRcvBufSize - iReadLen, 0);
			if (iRcvBytes < 1)//   if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
			{
				if( (iRcvBytes==0)||(!(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)))
				{
					g_networkError = true;
					iReadLen = iRcvMsgBytes = iRcvStartPos = 0;
					pBuf = pRcvBuf + iRcvStartPos;// QUERY_HEADER_SIZE;
					iAvailableBufSize = iRcvBufSize - iRcvStartPos;// QUERY_HEADER_SIZE;
					pNetMsgHeader = (NetMsgHeader*)pBuf;
					continue;
				}
				goto SEND;
			}
			g_iTotalRcvBytes += iRcvBytes;
			iReadLen += iRcvBytes;
			//! judge header
		FindHeader:
			if (!iRcvMsgBytes)//!bHeaderOK)
			{
				iFindTimes++;
				if (CONFIRM_NUM == pNetMsgHeader->m_iConfirmNum)
				{// right  header
					iRcvMsgBytes = pNetMsgHeader->m_iMsgLen + NET_MSG_HEADER_LEN;
					bHeadErr = false;
				}
				if (!iRcvMsgBytes)
				{  // wrong  header
					if (!bHeadErr)
						g_iHeaderErrCnt++;
					bHeadErr = true;

					for (i = 1; i < iReadLen - sizeof(NetMsgHeader); ++i)
					{
						if (CONFIRM_NUM == ((NetMsgHeader*)(pBuf + i))->m_iConfirmNum)
						{
							iRcvMsgBytes = ((NetMsgHeader*)(pBuf + i))->m_iMsgLen + NET_MSG_HEADER_LEN;
							//bHeaderOK = true;
							bHeadErr = false;
							break;
						}
					}
					iReadLen -= i;
					iRcvStartPos += i;
					pBuf += i;
					pNetMsgHeader = (NetMsgHeader*)(pBuf);
					iAvailableBufSize = iRcvBufSize - iRcvStartPos;
					g_iHeaderErrBytes += i;
				}//wrong  header

				if (iRcvMsgBytes)// (bHeaderOK)//header just becames ready
				{
					//buf is enough?
					iNeedBufSize = pNetMsgHeader->m_iMsgLen + NET_MSG_HEADER_LEN;
					if (iNeedBufSize > iRcvBufSize)
					{  //buf is not long enough to save query
						if (iNeedBufSize > 512 * 1024 * 1024)
						{
							WriteLog(g_pProcLog, "***Error: too large msg, type %d len %u %s\n", pNetMsgHeader->m_iMsgType, pNetMsgHeader->m_iMsgLen, g_strCurClientTime);
							g_iHeaderErrBytes += iReadLen;
							iReadLen = iRcvMsgBytes = iRcvStartPos = 0;
							pBuf = pRcvBuf + iRcvStartPos;// QUERY_HEADER_SIZE;
							iAvailableBufSize = iRcvBufSize - iRcvStartPos;// QUERY_HEADER_SIZE;
							pNetMsgHeader = (NetMsgHeader*)pBuf;
						}
						else
						{
							unsigned char* pOldBuf = pRcvBuf;
							iRcvBufSize = ((iNeedBufSize >> 20) + 1) << 20;

							pRcvBuf = (unsigned char*)malloc(iRcvBufSize);
							memcpy(pRcvBuf, pBuf, iReadLen);
							delete[] pOldBuf;
                            if(g_iRcvBufSizeMB < (iRcvBufSize >> 20))
                                g_iRcvBufSizeMB = (iRcvBufSize >> 20);

							iRcvStartPos = 0;
							pBuf = pRcvBuf;
							iAvailableBufSize = iRcvBufSize;
							pNetMsgHeader = (NetMsgHeader*)(pBuf);
						}
					}
				}//if (!bHeaderOK &&(NET_MSG_HEADER_LEN <= iReadLen) )
			}// if (!iRcvMsgBytes)//!bHeaderOK)

	//need to mov forward?
			iNeedBufSize = iRcvMsgBytes ? iRcvMsgBytes : iRcvBufSize / 2;// -md->m_iReadLen;

			if (iAvailableBufSize < iNeedBufSize)
			{
				memcpy(pRcvBuf, pRcvBuf + iRcvStartPos, iReadLen);
				iRcvStartPos = 0;
				pBuf = pRcvBuf;
				iAvailableBufSize = iRcvBufSize;
				pNetMsgHeader = (NetMsgHeader*)(pBuf);
			}

			//! judge message body
			if (iRcvMsgBytes && (iReadLen >= iRcvMsgBytes))//NET_MSG_HEADER_LEN + md->m_pQryHeader->m_iQueryLen) )
			{//rcv a new one!

				iRet = EnDataQueue(m_pQueueNetRcv, pNetMsgHeader, iRcvMsgBytes, NULL, 0, NULL, 0);

                char* pStrTypeName = GetMsgTypeName(pNetMsgHeader->m_iMsgType);
				WriteLog(g_pProcLog, "rcv %s from %s %s %s\n", pStrTypeName, 	g_strAudioServerIP,
					iRet < iBytes ? "***Discard for queue full!" : "Saved!",
					g_strCurClientTime);

				iRcvStartPos += iRcvMsgBytes;
				pBuf = pRcvBuf + iRcvStartPos;
				pNetMsgHeader = (NetMsgHeader*)(pBuf);
				iAvailableBufSize = iRcvBufSize - iRcvStartPos;
				iReadLen -= iRcvMsgBytes;
				iRcvMsgBytes = 0;
				if (iReadLen < 0)
				{
					iReadLen = iRcvStartPos = 0;
					throw(__LINE__);
				}

				if (iReadLen >= NET_MSG_HEADER_LEN)
					goto FindHeader;//find next header
			}

			//---------------------------------------------------------------------------
		SEND:
			//! prepare message
			if (0 == iWantSendLen) { //无正在发送的message

                if (DeDataQueue(m_pQueueLocalRcv, (void**)(&pData)) <= 0)//full queue, get nothing
				{
					//usleep(2000);
					iSleepTimes++;
					if (iSleepTimes > 5) //弱控制
						m_TcpClient.m_iSendBytes = iSleepTimes = 0;
					continue;
				}
				iSleepTimes = 0;


                char* charArray = (char*)pData;
                printf("CharArray: %s\n", charArray);

				pSendBuf = pData;
				iWantSendLen = ((NetMsgHeader*)pSendBuf)->m_iMsgLen + sizeof(NetMsgHeader);
				iSentBytes = 0;
				g_iTotalDataBytes += iWantSendLen;
			}

			// !send message
			if (iWantSendLen > 0) { //有正在发送的message
				//! send
				iSendBytes = m_TcpClient.sendData(pSendBuf + iSentBytes, iWantSendLen,  MSG_NOSIGNAL);
				//! judge iSendBytes
				if (iSendBytes > 0) { // 发送iSendBytes字节message
					//! 更新计数器
					iWantSendLen -= iSendBytes;
					iSentBytes += iSendBytes;
					g_iSendBytes += iSendBytes;

					if (0 == iWantSendLen) {
						iSentBytes = 0;
					}
				}
				else { // 发送失败
					if ((iSendBytes!=0)||(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
                        usleep(2000);
						continue;
					}
					else {
                        m_TcpClient.closeClient();
						g_networkError = true;
						iWantSendLen = 0;
						iSentBytes = 0;

					}
				}
			}
		}
	}

 //////////////////////////////////////////////////////////////////////////
//! 统计处理的业务类型
//////////////////////////////////////////////////////////////////////////

    void ClientManagement::ProcRcvQueueData() {
        NetMsgHeader*pMsgHeader = nullptr;
        unsigned char *pMsgData;
        char strInfo[1000];
        void* pData;
        int64_t iBytes;
        int32_t iModuleID = 0;//??
        int iRet;
        while (!g_isExit) {
            if(0 >= DeDataQueue(m_pQueueNetRcv,(void**)(&pData) ) )
            {
                usleep(100);
                continue;
            }
            g_iRcvMsgCnt++;
           pMsgHeader = (NetMsgHeader*)pData;
           pMsgData = (unsigned char*)(pData + sizeof(NetMsgHeader));
           if (DT_SERVER_MSG == pMsgHeader->m_iMsgType)
           {
               //
           }
            // free result unit
            pMsgHeader = nullptr;
        }
    }

//////////////////////////////////////////////////////////////////////////
//! 线程
//////////////////////////////////////////////////////////////////////////

    void *outputStatusThread(void *param) {
        if (g_writeLog) WriteLog(g_pProcLog, "---output status thread start! %s\n", g_strCurClientTime);
        ClientManagement::get_pointer()->OutputStatusFunc();
        return nullptr;
    }

    void *resultDealThread(void *param) {
        if (g_writeLog) WriteLog(g_pProcLog, "---result deal thread start! %s\n", g_strCurClientTime);
        ClientManagement::get_pointer()->ProcRcvQueueData();
        return nullptr;
    }

    void *networkThread(void *param) {
        if (g_writeLog)  WriteLog(g_pProcLog,"---network thread start! %s\n", g_strCurClientTime);
        ClientManagement::get_pointer()->netWorkFunc();
        return nullptr;
    }

//////////////////////////////////////////////////////////////////////////
//! utility
//////////////////////////////////////////////////////////////////////////

    uint32_t queryInfoCheck(void *queryInfo) {
        char *info = (char *)queryInfo;
        uint32_t result = 0;
        for (int i = 0; i < CHECKNUM; i += 2) {
            uint16_t cur = *(uint16_t *)(info + i);
            result += cur;
        }
        return result;
    }

    char *getCodeLine(const char *file, int32_t line) {
        static char str[100] = {0};
        const char *pNameBegin = file + strlen(file);
        do {
            if (*pNameBegin == '\\' || *pNameBegin == '/') {
                pNameBegin++;
                break;
            }
            if (pNameBegin == file) { break; }
            pNameBegin--;
        } while (true);
        memcpy(str, pNameBegin, strlen(pNameBegin));
        sprintf(str + strlen(pNameBegin), ":%d", line);
        return str;
    }

    char *getCurTime() {
        static char str[100] = {0}; // data
        timeval tmv;
        gettimeofday(&tmv, nullptr);
        tm tt = {0};
        localtime_r(&tmv.tv_sec, &tt);
        tt.tm_hour = 0;
        tt.tm_min = 0;
        tt.tm_sec = 0;
        time_t sec = tmv.tv_sec - mktime(&tt);
        sprintf(str, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.3d",
                tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday,
                (int32_t) sec / 3600, (int32_t) (sec % 3600) / 60, (int32_t) sec % 60, (int32_t) tmv.tv_usec / 1000);
        return str;
    }

    uint32_t MBytes(uint32_t len) {
        return ((uint32_t)(len & 0xfff00000) + (uint32_t)((len & 0xfffff) ? 0x100000 : 0));
    }

    void ClientManagement::OutputStatusFunc() 
    {
        sleep(1);
        uint64_t iTotalOptCnt;
        uint64_t net_send;
        uint64_t net_recv;
        uint64_t local_send;
        uint64_t preBytes = 0;
        char strInfo[500];
        int  ibytes, iStrLen;
        char* pBuf;
        const char* mode = "Audio-Client" ;
        int64_t iSize[5];
        int64_t iOldRcvBytes = 0; 
        float fSpeed;
        /*GetDataQueueInfo返回值：是否成功获取信息
        GetDataQueueInfo传回参数iSize是关于队列的如下信息
            iSize[0] Size in byte
            iSize[1] free bytes
            iSize[2] total block cnt  received
            iSize[3] pop block cnt
            iSize[4] remain block cnt
*/
        while (!g_isExit) {
            GetDataQueueInfo(m_pQueueLocalRcv, iSize);
            iTotalOptCnt = iSize[2];
            net_send = iSize[3];
            GetDataQueueInfo(m_pQueueNetRcv, iSize);
            net_recv = iSize[2]; 
            local_send = iSize[3];

            pBuf = strInfo;
            ibytes = sizeof(strInfo);

            snprintf(pBuf, ibytes, "\n------------%s Ver 1.0  Built @%s %s  interval:%ds\n%s\n%s NetLink:%d buf: %d\n", 
                mode, __DATE__, __TIME__,  g_iStatusLogInterval, 
                g_strStartTime, 
                g_strCurClientTime,
                !g_networkError,
                g_iRcvBufSizeMB);
            iStrLen = strlen(pBuf);	pBuf += iStrLen; 	ibytes -= iStrLen;
            
            fSpeed = 8.0 / 1024 / 1024 * (g_iTotalRcvBytes - iOldRcvBytes) / (uint64_t)g_iStatusLogInterval;
            snprintf(pBuf, ibytes, "rcv %.4fG %.3fMbps discard: %lu  header: %lu %.3f local-full %lu net-full %lu add-err %lu\n",
                1.0 / 1024 / 1024 / 1024 * g_iTotalRcvBytes,
                fSpeed,
                g_iDiscardCnt,
                g_iHeaderErrCnt, 1.0 / 1024 / 1024 / 1024 * g_iHeaderErrBytes,
                g_iLocalRcvBufFull, g_iNetRcvBufFull, g_iQueueAddErrCnt);
            iStrLen = strlen(pBuf);	pBuf += iStrLen; 	ibytes -= iStrLen;

            snprintf(pBuf, ibytes, "local-rcv: %lu  net-sent: %lu query: %lu\n",
                    iTotalOptCnt, net_send, g_iPcmBlockCnt, g_iQueueAddErrCnt);
       
            iStrLen = strlen(pBuf);	pBuf += iStrLen; 	ibytes -= iStrLen;
                
            fSpeed = 8.0 / 1024 / 1024 * (g_iSendBytes - preBytes) / (uint64_t)g_iStatusLogInterval;
            snprintf(pBuf, ibytes, "send %.3f Mbps  total %f GB\n", 
                fSpeed, 1.0/1024/1024/1024*g_iTotalDataBytes);
            iStrLen = strlen(pBuf);	pBuf += iStrLen; 	ibytes -= iStrLen;   
            
            snprintf(pBuf, ibytes, "net-rcv: %ld local-sent: %ld ", net_recv, local_send);
            iStrLen = strlen(pBuf);	pBuf += iStrLen; 	ibytes -= iStrLen;

            snprintf(pBuf, ibytes, "rcv msg:%ld   \n",g_iRcvMsgCnt);
            iStrLen = strlen(pBuf);	pBuf += iStrLen; 	ibytes -= iStrLen;

            if (g_writeLog) WriteLog(g_pProcLog, strInfo);
            fprintf(stderr, strInfo);
            preBytes = g_iSendBytes;
            iOldRcvBytes  = g_iTotalRcvBytes;
            //g_iSendBytes -= preBytes;
            sleep(g_iStatusLogInterval);
        }
    }

    SPLAB_AUDIOCLIENT_END
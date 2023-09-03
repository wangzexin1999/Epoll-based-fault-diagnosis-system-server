//
// Created by 997289110 on 7/1/20.
//

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <assert.h>
#include <dlfcn.h>
#include "AudioServer.h"
#include "global.h"
#include "MyFile.h"
#include "Wave.h"

using namespace hitsplab_audioserver;

static bool g_bOverSystemMaxAge = false;
#define TIMELIMITED          1 //定义使用期限限制（含该年份），超时后，LongQuery减少20%
#define DEADYEAR               2022
#define DEADMONTH            9 
#define NORMAL_TIME_SECONDS  (int)(3600*4.6)//seconds  15

OpenDataQueue_t  OpenDataQueue = NULL;
GetDataQueueInfo_t GetDataQueueInfo = NULL;
EnDataQueue_t EnDataQueue = NULL;
DeDataQueue_t DeDataQueue = NULL;
DataQueueIsFull_t DataQueueIsFull = NULL;
CloseDataQueue_t CloseDataQueue = NULL;
OpenLogFile_t OpenLogFile = NULL;
WriteLog_t WriteLog = NULL;
CloseLog_t CloseLog = NULL;
void* g_pStatusLog = NULL, * g_pProcessLog=NULL, * g_pResultLog = NULL;

char g_strQueueModulePath[300];
void* g_pDataQueueHandle = NULL;
void** g_pRcvDataQueue = NULL;
void* g_pResultQueue = NULL;
int64_t g_iQueryQueueMB = 1024;//MB
int64_t g_iResultQueueMB = 512;//MB
int g_iProcessThreadCnt = 10;//
bool g_bNotConcurrentQuery = false;
int32_t g_iSystemMaxAge;

extern uint32_t g_iRcvBufMB;

uint64_t g_iCheckErrCnt = 0;


int loadDecoderModule(char* strModulePath)
{
	strcpy(g_strQueueModulePath, strModulePath);
	char* _tmpError;

	g_pDataQueueHandle = dlopen(strModulePath, RTLD_LAZY);// RTLD_NOW);
	if (NULL == g_pDataQueueHandle) {
		_tmpError = dlerror();
		fprintf(stderr, "***Load %s  failed with error: %s. \n", strModulePath, _tmpError);
		return -1;
	}

   //---------------------  log functions ----------------------------------------------------------
	OpenLogFile = (OpenLogFile_t)dlsym(g_pDataQueueHandle, "OpenLogFile");
	if ((_tmpError = dlerror()) != NULL) {
		fprintf(stderr, "***Load  'OpenLogFile' from %s failed with error: %s.\n", strModulePath, _tmpError);
		return -3;
	}

	WriteLog = (WriteLog_t)dlsym(g_pDataQueueHandle, "WriteLog");
	if ((_tmpError = dlerror()) != NULL) {
		fprintf(stderr, "***Load  'WriteLog' from %s failed with error: %s.\n", strModulePath, _tmpError);
		return -5;
	}

	CloseLog = (CloseLog_t)dlsym(g_pDataQueueHandle, "CloseLog");
	if ((_tmpError = dlerror()) != NULL) {
		fprintf(stderr, "***Load  'CloseLog' from %s failed with error: %s.\n", strModulePath, _tmpError);
		return -7;
	}

  //------------------------ data queue functions --------------------------------------------------------
	OpenDataQueue = (OpenDataQueue_t)dlsym(g_pDataQueueHandle, "OpenDataQueue");
	if ((_tmpError = dlerror()) != NULL) {
		fprintf(stderr, "***Load  'OpenDataQueue' from %s failed with error: %s.\n", strModulePath, _tmpError);
		return -10;
	}

	GetDataQueueInfo = (GetDataQueueInfo_t)dlsym(g_pDataQueueHandle, "GetDataQueueInfo");
	if ((_tmpError = dlerror()) != NULL) {
		fprintf(stderr, "***Load  'GetDataQueueInfo' from %s failed with error: %s.\n", strModulePath, _tmpError);
		return -11;
	}

	EnDataQueue = (EnDataQueue_t)dlsym(g_pDataQueueHandle, "EnDataQueue");
	if ((_tmpError = dlerror()) != NULL) {
		fprintf(stderr, "***Load  'EnDataQueue' from %s failed with error: %s.\n", strModulePath, _tmpError);
		return -12;
	}

	DeDataQueue = (DeDataQueue_t)dlsym(g_pDataQueueHandle, "DeDataQueue");
	if ((_tmpError = dlerror()) != NULL) {
		fprintf(stderr, "***Load  'DeDataQueue' from %s failed with error: %s.\n", strModulePath, _tmpError);
		return -13;
	}

	DataQueueIsFull = (DataQueueIsFull_t)dlsym(g_pDataQueueHandle, "DataQueueIsFull");
	if ((_tmpError = dlerror()) != NULL) {
		fprintf(stderr, "***Load  'DataQueueIsFull' from %s failed with error: %s.\n", strModulePath, _tmpError);
		return -14;
	}

	CloseDataQueue = (CloseDataQueue_t)dlsym(g_pDataQueueHandle, "CloseDataQueue");
	if ((_tmpError = dlerror()) != NULL) {
		fprintf(stderr, "***Load  'CloseDataQueue' from %s failed with error: %s.\n", strModulePath, _tmpError);
		return -15;
	}


	return 0;
}

extern int32_t g_iStatusLogInterval;

SPLAB_AUDIOSERVER_BEGIN
// config
bool g_bPrintResultLog = false;
int32_t g_outputBaseNum = 20;
// Instrumental variables
static uint32_t MaxSendLen = 50 * 1024 * 1024;
// Network transmission variables
TransMap g_TransMap, g_OldTransMap;

uint64_t g_iTotalRcvBytes = 0;
uint64_t g_iTotalMsgCnt = 0;

atomic<uint64_t> g_iDiscardCnt = { 0 };
atomic<uint64_t> g_iSendQueryMicroSeconds = { 0 };
atomic<uint64_t> g_iSendQueryBytes = { 0 };
atomic<uint64_t> g_iDiscardBytes = { 0 };

atomic<uint64_t> g_iSendQueryCnt = { 0 };
atomic<uint64_t> g_iSendClassifyCnt = { 0 }; 
atomic<uint64_t> g_iSendLangCnt = { 0 };

AudioServer* AudioServer::m_AudioServer = nullptr;
//////////////////////////////////////////////////////////////////////////
//! 构造函数&&析构函数
//////////////////////////////////////////////////////////////////////////
AudioServer::AudioServer() {
	m_bStarted = false;
};

void	AudioServer::StartServer() {
	if (m_bStarted) {
		return;
	}
	m_bStarted = true;
	AudioServer::m_AudioServer = this;
	int32_t error_code = 0;
	//! counter
	g_iQryCnt = m_iHeaderErrCnt = m_iHeaderErrBytes = 0;
	g_iRcvPcmBlockCnt = g_iRcvQueryErrCnt = g_iMovBufCnt=0;
	g_iRcvDbAddCnt = 0;
	g_iSrcDataCnt = 0;
	g_iValidResultCnt = g_iRcvResultCnt = g_iSendClassifyCnt = g_iSendLangCnt = g_iDiscardResultCnt = 0;
	m_iSentCnt = m_iSendFailCnt = 0;
	m_iReSendCnt[0] = m_iReSendCnt[1] = m_iReSendCnt[2] = 0;
	m_iWait2SendCnt = 0;
	srand(time(NULL));

	m_bHeadErr = false;

	error_code = pthread_create(&m_ListenThread, nullptr, requestListenThread, nullptr);
	if (error_code != 0) {
		WriteLog(g_pProcessLog, "%s", "Can't create request listen thread!\n");
	}

	//! worker thread
	m_pWorkerThread = new pthread_t[g_iProcessThreadCnt];
	for (int i = 0; i < g_iProcessThreadCnt; i++) {
		error_code = pthread_create(&m_pWorkerThread[i], nullptr, workerThread, (void*)(i));
		if (error_code != 0) {
			WriteLog(g_pProcessLog, "%s", "Can't create worker thread!\n");
		}
	}
	//! output state thread
	error_code = pthread_create(&m_OutputStateThread, nullptr, outputStateThread, nullptr);
	if (error_code != 0) {
		WriteLog(g_pProcessLog, "%s", "Can't create output state thread!\n");
	}
	//! thread scan thread
	if (g_coordinationRate > 0) {
		error_code = pthread_create(&m_ThreadScanThread, nullptr, threadScanThread, nullptr);
		if (error_code != 0) {
			WriteLog(g_pProcessLog, "%s", "Can't create scan thread!\n");
		}
	}
}


AudioServer::~AudioServer() {
	if (!m_bStarted)
		return;
	if (!pthread_join(m_ListenThread, nullptr))
		fprintf(stderr, "request listen thread  shut down %s\n", g_strCurTime);

	for (int i = 0; i < g_iProcessThreadCnt; i++) {
		if (!pthread_join(m_pWorkerThread[i], nullptr))
			fprintf(stderr, "%dth worker thread  shut down %s\n", i, g_strCurTime);
	}
	delete[] m_pWorkerThread;

	if (!pthread_join(m_OutputStateThread, nullptr))
		fprintf(stderr, "output state thread shut down %s\n", g_strCurTime);
	if (g_coordinationRate > 0 && !pthread_join(m_ThreadScanThread, nullptr))
		fprintf(stderr, "scan thread shut down %s\n", g_strCurTime);
}

AudioServer* AudioServer::getPointer() {
	return AudioServer::m_AudioServer;
}
//////////////////////////////////////////////////////////////////////////
//! network
const int TCP_MTU = 1500;
ssize_t TcpSendData(int sock_fd, void* data0, size_t length,int iSendType) {
	//>=0:sent-bytes
	//<0: error
	ssize_t send_len = 0, iBytes2Send = length;
	void* p = data0;
	//int iSendLen = 0;
	while (iBytes2Send > 0)
	{
		//iSendLen = iBytes2Send > TCP_MTU ? TCP_MTU : iBytes2Send;
		//send_len = send(sock_fd, p, iSendLen, iSendType);
		send_len = send(sock_fd, p, iBytes2Send, iSendType);
		if (send_len < 0)
		{
			if ( (errno != EAGAIN) && (errno != EWOULDBLOCK) )//EWOULDBLOCK
			{//error
				fprintf(stderr, "***TCP_Error: tcp send data error %d, %s, send_len: %zd %s\n",errno, strerror(errno), send_len, g_strCurTime);
				WriteLog(g_pProcessLog, "***TCP_Error: tcp send data error %d, %s, send_len: %zd %s\n", errno, strerror(errno), send_len, g_strCurTime);
				return send_len;
			}
			return length - iBytes2Send;
		}
		else
		{
			iBytes2Send -= send_len;
			p += send_len;
		}
	}
	return length - iBytes2Send;
}

uint32_t g_iRcvBufMB = (MAX_RECV_BUF_SIZE>>20);

#if 0
#define OUTPUT_BYTES0 WriteLog(g_pProcessLog, \
"line %d:iReadLen=%ld used=%ld iWantReadLen=%ld iFindTimes=%ld %ld\n", __LINE__,\
md->m_iReadLen, \
iTotalValidBytes,\
md->m_iWantReadLen,iFindTimes,iReadTimes);
#else
#define OUTPUT_BYTES0 ;
#endif

#if 0
#define OUTPUT_BYTES1 printf("line %d:%dth iRcvBytes=%ld used=%ld iReadLen=%ld iWantReadLen=%ld iFindTimes=%ld %ld\n",\
 __LINE__,\
iReadTimes, iRcvBytes,\
iTotalValidBytes,md->m_iReadLen, \
md->m_iWantReadLen,iFindTimes,iFindMsgCnt);fflush(stdout);
#else
#define OUTPUT_BYTES1 ;
#endif

int64_t g_iMaxBytePerRead = 0, g_iMaxMsgByte=0;
uint32_t g_iMaxNetMsgByte = 64 * 1024 * 1024;

inline bool IsValidNetMsgHeader(NetMsgHeader* pNetMsgHeader)
{
	return (CONFIRM_NUM == pNetMsgHeader->m_iConfirmNum)
		&&(pNetMsgHeader->m_iMsgLen< g_iMaxNetMsgByte);
}

//epoll读事件
int32_t AudioServer::readFunc(epoll_event* ev)
{
	EpollMateData* md = (EpollMateData*)ev->data.ptr;
	//bool bHeaderOK = (md->m_iWantReadLen > 0);
	int64_t & iReadLen = md->m_iReadLen;
	unsigned char* pBuf = md->m_pRcvBuf + md->m_iRcvStartPos;
	int64_t iBufSize = md->m_iRcvBufSize - md->m_iRcvStartPos;
	NetMsgHeader* &pNetMsgHeader = md->m_pNetMsgHeader; 
	pNetMsgHeader = (NetMsgHeader*)pBuf;
	if (IsValidNetMsgHeader(pNetMsgHeader)) {
		printf("Valid!!, Len %d Type %d\n", pNetMsgHeader->m_iMsgLen, pNetMsgHeader->m_iMsgType);
	}

	int64_t iRcvBytes = 0;
	int64_t iNeedBufSize;
	size_t i;
	int64_t idx = 0;
	bool bNeedEnQueue = false;
	int iFindTimes = 0, iFindMsgCnt=0;
	int64_t iTotalValidBytes = 0;
	int64_t iReadTimes=0;
	iRcvBytes = 1;

	OUTPUT_BYTES0 OUTPUT_BYTES1

#if EPOLL_FAST_READ_MODE//fast mode
	while (!g_isExit)//EPOLLET mode
#endif
	{	
		
		iRcvBytes = recv(md->m_iFd, pBuf, iBufSize, MSG_DONTWAIT);
		
		if (iRcvBytes > 0) {
			// 输出接收到的数据
			// printf("Received data: %s\n",pBuf);
			fprintf(stdout, "Received: %.*s\n", (int)(iRcvBytes - iReadLen), pNetMsgHeader);
		}
		
		iReadTimes++;
		if (iRcvBytes < 1)
		{
#if EPOLL_FAST_READ_MODE//fast mode
			if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
				return 0;
			}
			else
				return -1;// <0, will close the connection
#else
			if (iRcvBytes == 0)
				return -2;
			else if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
				return 0;
			else
				return -3;
#endif
		}
		g_iMaxBytePerRead = iRcvBytes > g_iMaxBytePerRead ? iRcvBytes : g_iMaxBytePerRead;
		g_iTotalRcvBytes += iRcvBytes;
		iReadLen += iRcvBytes;

		//! judge header
	FindHeader:
		if (!md->m_iWantReadLen)//!bHeaderOK)
		{
			iFindTimes++;
			if (NET_MSG_HEADER_LEN <= iReadLen)
			{//just rcv enough data for pNetMsgHeader
				if (IsValidNetMsgHeader( pNetMsgHeader) )
				{// right  header
					md->m_iWantReadLen = pNetMsgHeader->m_iMsgLen + NET_MSG_HEADER_LEN;
					assert(md->m_iWantReadLen > 0);
					OUTPUT_BYTES0 OUTPUT_BYTES1
					//bHeaderOK = true;
					m_bHeadErr = false;
				}
				if (!md->m_iWantReadLen)
				{  // wrong  header
					if (!m_bHeadErr)
						m_iHeaderErrCnt++;
					m_bHeadErr = true;
					for (i = 1; i < iReadLen - sizeof(NetMsgHeader); ++i)
					{
						if ( IsValidNetMsgHeader((NetMsgHeader*)(pBuf + i)) )
						{
							md->m_iWantReadLen = ((NetMsgHeader*)(pBuf + i))->m_iMsgLen + NET_MSG_HEADER_LEN;
							assert(md->m_iWantReadLen > 0);
							OUTPUT_BYTES0 OUTPUT_BYTES1
							//bHeaderOK = true;
							m_bHeadErr = false;
							break;
						}
					}
					printf("---Head error,i=%d\n", i); fflush(stdout);
					iReadLen -= i;
					md->m_iRcvStartPos += i;
					pBuf += i;
					pNetMsgHeader = (NetMsgHeader*)(pBuf);
					iBufSize = md->m_iRcvBufSize - md->m_iRcvStartPos;
					m_iHeaderErrBytes += i;
				}//wrong  header

				if (md->m_iWantReadLen)// (bHeaderOK)//header just becames ready
				{
					//buf is enough?
					g_iMaxMsgByte = md->m_iWantReadLen > g_iMaxMsgByte ? md->m_iWantReadLen: g_iMaxMsgByte;
					assert(md->m_iWantReadLen > 0);

					OUTPUT_BYTES0 OUTPUT_BYTES1

					iNeedBufSize = pNetMsgHeader->m_iMsgLen  + NET_MSG_HEADER_LEN;
					if (iNeedBufSize > md->m_iRcvBufSize)
					{  //buf is not long enough to save query
						if (iNeedBufSize > 512 * 1024 * 1024)
						{
							WriteLog(g_pProcessLog, "***Error: too large msg, type %d len %u %s\n", pNetMsgHeader->m_iMsgType, pNetMsgHeader->m_iMsgLen, g_strCurTime);
							m_iHeaderErrBytes += md->m_iReadLen;
							md->m_iReadLen = md->m_iWantReadLen = md->m_iRcvStartPos = 0;
						}
						else
						{
							unsigned char* pOldBuf = md->m_pRcvBuf;
							iNeedBufSize = ((iNeedBufSize >> 20) + 1) << 20;
							md->MallocRcvBuf(iNeedBufSize);
							memcpy(md->m_pRcvBuf + md->m_iRcvStartPos, pBuf, iReadLen);
							delete[] pOldBuf;
							if (g_iRcvBufMB < (iNeedBufSize >> 20))
								g_iRcvBufMB = (iNeedBufSize >> 20);

							pBuf = md->m_pRcvBuf + md->m_iRcvStartPos;
							iBufSize = md->m_iRcvBufSize - md->m_iRcvStartPos;
							pNetMsgHeader = (NetMsgHeader*)(pBuf);
						}
					}

					md->m_pNetMsgHeader = pNetMsgHeader;
				}//if (!bHeaderOK &&(NET_MSG_HEADER_LEN <= iReadLen) )
			}
		}//if (!md->m_iWantReadLen)//!bHeaderOK)

		//need to mov forward?
		iNeedBufSize = md->m_iWantReadLen ? md->m_iWantReadLen : md->m_iRcvBufSize / 2;// -md->m_iReadLen;

		if (iBufSize < iNeedBufSize)
		{
			memcpy(md->m_pRcvBuf ,	md->m_pRcvBuf + md->m_iRcvStartPos, iReadLen);
			md->m_iRcvStartPos = 0;
			pBuf = md->m_pRcvBuf ;
			pNetMsgHeader = (NetMsgHeader*)(pBuf);
			iBufSize = md->m_iRcvBufSize - md->m_iRcvStartPos;
			if(md->m_iWantReadLen)
				g_iMovBufCnt++;
		}

		//! judge message body
		if (md->m_iWantReadLen && (md->m_iReadLen >= md->m_iWantReadLen))
		{//rcv a new one!
			iFindMsgCnt++;
			iTotalValidBytes += md->m_iWantReadLen;
			idx = 0;
			bNeedEnQueue = false;
			switch (md->m_pNetMsgHeader->m_iMsgType)
			{
			case DT_WAV:
				g_iRcvPcmBlockCnt++;
				bNeedEnQueue = true;
				break;
			
			};

			if (bNeedEnQueue) {
				idx = (idx + 1) % g_iProcessThreadCnt;
				ServerMsgHeader msgHeader;
				msgHeader.m_iType = md->m_pNetMsgHeader->m_iMsgType;
				msgHeader.m_iFd = md->m_iFd;
				msgHeader.m_Ip = md->m_Ip;
				msgHeader.m_iLen = md->m_pNetMsgHeader->m_iMsgLen;
				msgHeader.m_pData = (unsigned char*)(md->m_pNetMsgHeader) + sizeof(NetMsgHeader);

				int64_t iRet = EnDataQueue(g_pRcvDataQueue[idx], &msgHeader, sizeof(ServerMsgHeader),
					md->m_pNetMsgHeader,  md->m_iWantReadLen,	NULL, 0);

				if (iRet <= 0) {//队列满，入队失败
					g_iDiscardBytes += (md->m_iWantReadLen);
					g_iDiscardCnt++;
				}
			}//if (bNeedEnQueue)

			OUTPUT_BYTES0 OUTPUT_BYTES1
			md->m_iRcvStartPos += md->m_iWantReadLen;
			pBuf = md->m_pRcvBuf + md->m_iRcvStartPos;
			pNetMsgHeader = (NetMsgHeader*)(pBuf);
			iBufSize = md->m_iRcvBufSize - md->m_iRcvStartPos;
			md->m_iReadLen -= md->m_iWantReadLen;
			md->m_iWantReadLen = 0;
			if (md->m_iReadLen < 0)
			{
				md->m_iReadLen = md->m_iRcvStartPos = 0;
				throw(__LINE__);
			}

			if (md->m_iReadLen >= NET_MSG_HEADER_LEN)
				goto FindHeader;//find next header

		}//read a finish one!
	}

	return 0;
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
int32_t AudioServer::writeFunc(epoll_event* ev)
{
	EpollMateData* md = (EpollMateData*)ev->data.ptr;
	int64_t iBytes;
	char strBuf[1000];
	ssize_t iWriteBytes;

	if (md->m_iWantSndLen == 0) { // 无正在发送的数据
		//modify------2021.09.10
		void* pData = nullptr;
		ServerMsgHeader* pServerMsgHeader = nullptr;
		NetMsgHeader* pNetMsgHeader = nullptr;
		iBytes = DeDataQueue(md->m_pRSQueWait4Send, (void**)(&pData));
		if (iBytes <= 0)// empty or erro
			return 0;

		/*pServerMsgHeader = (ServerMsgHeader*)(*(void**)pData);
		md->m_pNetMsgHeader = pServerMsgHeader;
		md->m_iWantSndLen = md->m_iOriginalLen = pServerMsgHeader->m_iResultLen + sizeof(NetMsgHeader);
		md->m_pSndBuf = (unsigned char*)pServerMsgHeader + sizeof(ServerMsgHeader);
		pNetMsgHeader = (NetMsgHeader*)((unsigned char*)pServerMsgHeader + sizeof(ServerMsgHeader));
		pNetMsgHeader->m_iMsgType = pServerMsgHeader->m_ResultType;
		pNetMsgHeader->m_iMsgLen = pServerMsgHeader->m_iResultLen;
		pNetMsgHeader->m_iConfirmNum = CONFIRM_NUM;
		md->m_iSentLen = 0;
		pServerMsgHeader = nullptr;
		pNetMsgHeader = nullptr;*/
	}

	//! send message
	if (md->m_iWantSndLen > 0) { //there is message in sending
		iWriteBytes = TcpSendData(md->m_iFd, md->m_pSndBuf + md->m_iSentLen, md->m_iWantSndLen, MSG_NOSIGNAL);//MSG_NOSIGNAL will not generate SIGPIPE
		//! judge temp_bytes
		if (iWriteBytes > 0)
		{ // 发送iWriteBytes字节result unit,更新计数器
			//发送iWriteBytes字节result unit,更新计数器
			md->m_iWantSndLen -= iWriteBytes;
			md->m_iSentLen += iWriteBytes;

			if (md->m_iWantSndLen == 0)
			{ // 成功发送
				m_iSentCnt++;
 			 /*WriteLog(g_pResultLog, "%04d send  No. %lu,  bytes %u %s %s\n", m_iSentCnt, info->m_iResultID, md->m_iSentLen, strBuf, g_strCurTime);
					fprintf(stderr, "%04d send analyse-result No. %lu,  bytes %u %s %s\n", m_iSentCnt, info->m_iResultID, md->m_iSentLen, strBuf, g_strCurTime);
				}
				else
				{
					char* pStrTypeName = GetMsgTypeName(md->m_pNetMsgHeader->m_ResultType);
					WriteLog(g_pResultLog, "%04d send %s,  bytes %u  %s\n", m_iSentCnt, pStrTypeName, md->m_iSentLen, g_strCurTime);
					fprintf(stderr, "%04d send %s,  bytes %u  %s\n", m_iSentCnt, pStrTypeName, md->m_iSentLen, g_strCurTime);
				}
				delete[](unsigned char*)(md->m_pNetMsgHeader);
				*/
				md->m_pNetMsgHeader = nullptr;
				md->m_iSentLen = 0;
				return 1;
			}
		}
		else
		{
			if ( (iWriteBytes!=0)&&(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
			{ //if (iWriteBytes == 0)
				if (md->m_iWantSndLen == md->m_iOriginalLen)
				{
					m_iReSendCnt[0]++;
					fprintf(stderr, "*** will retry0....%lu\n", m_iReSendCnt[0]);
				}
				else
				{
					m_iReSendCnt[1]++;
					fprintf(stderr, "*** will retry1....%lu\n", m_iReSendCnt[1]);
				}
				return 0;
			}
			else
			{ // 发送失败
				m_iSendFailCnt++;
				sprintf(strBuf, "***Error: %dth send fail, errno=%d %s link=%d  need_length=%u, iWriteBytes=%lld will retry %s:%2d. %s\n",
					m_iSendFailCnt, errno, strerror(errno), md->m_iFd, md->m_iWantSndLen, iWriteBytes, md->m_pHostName, md->m_iPort, g_strCurTime);

				fprintf(stderr, strBuf);
				WriteLog(g_pProcessLog, strBuf);
			}
		}//if (iWriteBytes > 0) 
	}//while((!g_isExit) && (md->m_iWantSndLen > 0) )
	return 0;
}

uint64_t getResultID(){
	static uint64_t iOldMonthDay = 0;
	static uint64_t iSeqInDay = 1;

	time_t  tCurrentTime;
	struct tm tCurLocalTime;//local time
	time(&tCurrentTime);
	tCurLocalTime = *localtime(&tCurrentTime);
	uint64_t iMonthDay;//3位10进制
	//iMonthDay = (((tCurLocalTime.tm_mon + 1) % 10) * 100 + tCurLocalTime.tm_mday);
	iMonthDay = (tCurLocalTime.tm_mon + 1) * 10000 + tCurLocalTime.tm_mday*100 + tCurLocalTime.tm_hour;
	if (iMonthDay != iOldMonthDay)
	{
		iOldMonthDay = iMonthDay;
		iSeqInDay = 1;//start from 1
	}
	else
		iSeqInDay++;

	return iMonthDay * 1000000000 + iSeqInDay;
}

//////////////////////////////////////////////////////////////////////////
//! 数据处理
//////////////////////////////////////////////////////////////////////////
	/*
	 * 完成数据处理后的结果回调函数
	 * @param iKind
	 * @param data 处理完成的数据
	 * @return 成功返回0，否则返回-1,-2,-3,-4,-5
	 * */


void  AudioServer::requestListenFunc() {
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
		WriteLog(g_pProcessLog,"request listen thread bind CPU failed! %s\n", g_strCurTime);
	}
	m_EpollServer.Bind((uint16_t)g_iAudioServerPort);
	m_EpollServer.Listen();
	m_EpollServer.Create();
	m_EpollServer.epollRun(read_call_back_func, write_call_back_func);
}

/* 将接收的数据发送到处理模块的线程函数
 * @param argv 线程函数需要用到的参数
 */
void AudioServer::workerFunc(int idx) {
	timeval tmv;
	uint64_t last_time = 0;
	uint64_t begin = 0;
	//modify------2021.09.10
	void* pRcvDataQueue = g_pRcvDataQueue[idx];
	void* pData;
	int64_t iBytes;
	char strBuf[300];
	const int iMaxLabelCnt = 100;
	static pthread_mutex_t tmpWorkMutex = PTHREAD_MUTEX_INITIALIZER;
	int err_code;
	NetMsgHeader* pNetMsgHeader;
	ServerMsgHeader* pServerMsgHeader;
	while (!g_isExit) {//

        gettimeofday(&tmv, nullptr);
        begin = tmv.tv_sec * 1000000 + tmv.tv_usec;

		iBytes = DeDataQueue(pRcvDataQueue, &pData);////modify------2021.09.10
		if (iBytes <= 0) {
			usleep(1000);
			continue;
		}

		//pData;
		pServerMsgHeader = (ServerMsgHeader*)pData;//pServerMsgHeader has ip 
		pNetMsgHeader = (NetMsgHeader*)(pData + sizeof(ServerMsgHeader));
		if (DT_WAV == pNetMsgHeader->m_iMsgType)
		{
//...      do something
		}
		else 
		{
			//...
		}

	}//while
}

/*
 * 进行线程扫描的线程函数
 * @param argv 线程函数需要用到的参数
 * */
void* threadScanThread(void* argv) {
	char result_buf[1024], command[1024];
	FILE* fp;
	memset(command, 0, sizeof(command));
	sprintf(command, "pgrep Server");
	while (!g_isExit) {
		memset(result_buf, 0, sizeof(result_buf));
		fp = popen(command, "r");
		if (!fp) {
			g_coordinationThreadOn = false;
		}
		else {
			fread(result_buf, 1, 1024, fp);
			if (result_buf[0]) {
				g_coordinationThreadOn = true;
			}
			else {
				g_coordinationThreadOn = false;
			}
			pclose(fp);
		}
		sleep(3);
	}
	return nullptr;
}

void* requestListenThread(void* argv) {
	fprintf(stderr, "request listen thread start! %s\n", g_strCurTime);
    AudioServer::getPointer()->requestListenFunc();
	return nullptr;
}

void* workerThread(void* argv) {
	fprintf(stderr, "Worker Thread Start! %s\n", g_strCurTime);
	int idx = (int64_t)argv;
    AudioServer::getPointer()->workerFunc(idx);
	fprintf(stderr, "Worker Thread finished! %s\n", g_strCurTime);
	return nullptr;
}

void* outputStateThread(void* argv) {
	fprintf(stderr, "Output State Thread Start! %s\n", g_strCurTime);
    AudioServer::getPointer()->outputStateFunc();
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//! call back function
//////////////////////////////////////////////////////////////////////////
int32_t read_call_back_func(epoll_event* ev)
{
	return AudioServer::getPointer()->readFunc(ev);
};

int32_t write_call_back_func(epoll_event* ev) 
{ 
	return AudioServer::getPointer()->writeFunc(ev);
};

//////////////////////////////////////////////////////////////////////////
//! utility
//////////////////////////////////////////////////////////////////////////
	/*
	 * 获取内存使用情况
	 * @return 返回内存使用大小
	 * */
size_t get_mem_usage()
{
	static const char* status_path = "/proc/self/status";
	const int32_t buf_size = 256;
	static char buf[buf_size];
	int32_t n_kilo;
	size_t bytes_size = 0;

	FILE* f = fopen(status_path, "r");
	if (f == nullptr) {
		fprintf(stderr,"open [%s] failed. %s\n", status_path, g_strCurTime);
		return 0;
	}
	while (fgets(buf, buf_size, f)) {
		if (strcasestr(buf, "VmRSS:")) {
			sscanf(buf, "VmRSS:%dkB", &n_kilo);
			/* from kilobyte to byte */
			bytes_size = (size_t)n_kilo * 1024;
		}
	}
	fclose(f);
	return bytes_size;
}

uint32_t MBytes(uint32_t len) {
	return ((uint32_t)(len & 0xfff00000) + (uint32_t)((len & 0xfffff) ? 0x100000 : 0));
}

uint32_t queryInfoCheck(void* queryInfo) {
	char* info = (char*)queryInfo;
	uint32_t result = 0;
	for (int i = 0; i < CHECKNUM; i += 2) {
		uint16_t cur = *(uint16_t*)(info + i);
		result += cur;
	}
	return result;
}


/*
 * 处理状态输出线程函数
 * @param argv 线程函数需要用到的参数
 * */
void AudioServer::outputStateFunc() {
	char     strVersion[20] = "1.0";
	uint64_t iLastRcvQueryCnt = 0;
	uint64_t iTotalRcvCnt = 0;
	uint64_t iTotalFinishCnt = 0;
	uint64_t iLastTotalRcvBytes = g_iTotalRcvBytes;
	uint64_t preSentNum = 0;
	uint64_t preSentTime = 0;
	uint64_t timeInterval = 0;
	uint64_t sent_cnt = 0;
	int64_t iSize[5];

	char strInfo[1024];
	char* pBuf = strInfo;
	int iStrLen, iLen = sizeof(strInfo);
	char strStartTime[50] = { 0 };
	char strCurrentTime[50] = { 0 };
	strncpy(strStartTime, getCurTime(), 19);

#if TIMELIMITED
	time_t clock;
	time(&clock);
	struct tm  tStartTime = *localtime(&clock);

	int64_t iSeconds, iTotalSeconds2Work;
	int64_t iDays = (DEADYEAR * 365 + DEADMONTH * 31 + rand() % 3)
		- ((tStartTime.tm_year + 1900) * 365 + tStartTime.tm_mon * 30 + tStartTime.tm_mday);//day
	iTotalSeconds2Work = iDays > 0 ? iDays * 24 * 3600 : NORMAL_TIME_SECONDS;
#endif
	char* strModeName="AudioServer";

	while (!g_isExit) {
		strncpy(strCurrentTime, getCurTime(), 19);
#if TIMELIMITED
		if (iTotalSeconds2Work > g_iStatusLogInterval)
			iTotalSeconds2Work -= g_iStatusLogInterval;
		else
			g_bOverSystemMaxAge = true;
#endif
		timeInterval = g_iSendQueryMicroSeconds - preSentTime;
		sent_cnt = g_iSendQueryCnt - preSentNum;
		preSentNum = g_iSendQueryCnt;
		preSentTime = g_iSendQueryMicroSeconds;

		//modify------2021.09.10,需要统计队列累计push和pop数量
		iTotalRcvCnt = 0; iTotalFinishCnt = 0;
		iLen = sizeof(strInfo);
		pBuf = strInfo;

		for (int i = 0; i < g_iProcessThreadCnt; i++) {
			GetDataQueueInfo(g_pRcvDataQueue[i], iSize);
			iTotalRcvCnt += iSize[2];
			iTotalFinishCnt += iSize[3];
		}
		snprintf(pBuf, iLen, "----------%s ver %s built@%s %s interval: %2ds\n%s,  port: %d links: %lu   Mem: %.2fGB buf: %d\n%s, ",
			strModeName, strVersion, __DATE__, __TIME__, g_iStatusLogInterval, strStartTime, g_iAudioServerPort, 
			m_EpollServer.getClientNumber(), 1.0 / 1024 / 1024 / 1024 * get_mem_usage(), g_iRcvBufMB,strCurrentTime);
		iStrLen = strlen(pBuf);	pBuf += iStrLen; 	iLen -= iStrLen;

		snprintf(pBuf, iLen, " rcv: %.4fG  flux: %.4f Mbps\n RcvQuery: %lu err %lu inc %lu/s  header: %lu %.3f %lu maxrcv %ld maxmsg %lu\n",
			1.0 / 1024 / 1024 /1024 * g_iTotalRcvBytes,
			1.0 / 1024 / 1024 * 8 * (g_iTotalRcvBytes - iLastTotalRcvBytes) / (uint64_t)g_iStatusLogInterval,
			g_iRcvPcmBlockCnt, 
			g_iRcvQueryErrCnt,
			(g_iRcvPcmBlockCnt - iLastRcvQueryCnt) / (g_iStatusLogInterval),
			AudioServer::getPointer()->m_iHeaderErrCnt,
			1.0/1024/1024/1024*m_iHeaderErrBytes, 
			g_iMovBufCnt, 
			g_iMaxBytePerRead, 
			g_iMaxMsgByte
		);
		g_iMaxBytePerRead = g_iMaxMsgByte=0;
		iStrLen = strlen(pBuf);	pBuf += iStrLen; 	iLen -= iStrLen;

		snprintf(pBuf, iLen, " Queue-OP: push %lu  pop %lu   discard %lu (%.4f MB)", iTotalRcvCnt, iTotalFinishCnt, g_iDiscardCnt.load(), g_iDiscardBytes * 1.0 / 1048576);
		iStrLen = strlen(pBuf);	pBuf += iStrLen; 	iLen -= iStrLen;

		WriteLog(g_pStatusLog, strInfo);
		fprintf(stderr, strInfo);

		iLastRcvQueryCnt = g_iRcvPcmBlockCnt;
		iLastTotalRcvBytes = g_iTotalRcvBytes;
		sleep(g_iStatusLogInterval);
	}
}

SPLAB_AUDIOSERVER_END

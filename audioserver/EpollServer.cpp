//
// Created by 997289110 on 4/1/20.
//

#include <netdb.h>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <malloc.h>
#include "EpollServer.h"
#include "global.h"
#include "AudioDataUnit.h"

//----------------- data queue  begin ----------------------
extern OpenDataQueue_t  OpenDataQueue;
extern GetDataQueueInfo_t GetDataQueueInfo;
extern EnDataQueue_t EnDataQueue;
extern DeDataQueue_t DeDataQueue;
extern DataQueueIsFull_t DataQueueIsFull;
extern CloseDataQueue_t CloseDataQueue;
extern char g_strQueueModulePath[300];
extern void* g_pDataQueueHandle;
extern void* g_pRcvDataQueue;
extern void* g_pResultQueue;
extern int64_t g_iQueryQueueMB;//MB
extern int64_t g_iResultQueueMB;//MB
//----------------- data queue  end ----------------------

SPLAB_AUDIOSERVER_BEGIN

const int32_t MAXEVENTS = 1024;
bool is_exit = false;
extern TransMap g_TransMap, g_OldTransMap;

__inline bool getOldEpollMateData(TransMap &TheTransMap, uint32_t ip, EpollMateData*& md)
{ 
    int status;
    md = TheTransMap.getNodeByID(ip, status);
	md =nullptr;
    return md != nullptr;
};

EpollServer::EpollServer() {
    m_iServerFd = -1;
    m_iEpollFd = -1;
    m_iClientCnt = 0;
    m_pEvents = (epoll_event *) calloc(MAXEVENTS, sizeof(m_Event));
}

EpollServer::~EpollServer() {
    free(m_pEvents);
    close(m_iServerFd);
    is_exit = true;
}

int32_t EpollServer::Bind(uint16_t port) {
    if (m_iServerFd != -1) {
        close(m_iServerFd);
        m_iServerFd = -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

	int lfd = socket(AF_INET, SOCK_STREAM, 0);

    if ((m_iServerFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        WriteLog(g_pProcessLog,"socket error: %s %s\n", strerror(errno), g_strCurTime);
        return -1;
    }

    int32_t flag = 1;
    setsockopt(m_iServerFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    if (bind(m_iServerFd, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        WriteLog(g_pProcessLog,"bind error: %s %s\n", strerror(errno), g_strCurTime);
        return -1;
    }
    MakeSocketNonBlocking(m_iServerFd);
    setTcpNoDelay(m_iServerFd);

    return 0;
}

int32_t EpollServer::Listen()
{
    if (listen(m_iServerFd, SOMAXCONN) < 0) {
        WriteLog(g_pProcessLog,"listen error: %s %s\n", strerror(errno), g_strCurTime);
        return -1;
    }
    return 0;
}

int32_t EpollServer::Create() {
    m_iEpollFd = epoll_create(MAX_EVENTS+1);
    if (m_iEpollFd <= 0) {
        WriteLog(g_pProcessLog,"epoll_create in  %s err %s %s\n", __func__, strerror(errno), g_strCurTime);
        throw(__LINE__) ;
    }
    m_Event.data.fd = m_iServerFd;
    m_Event.events = EPOLLIN;
    int32_t err_code = epoll_ctl(m_iEpollFd, EPOLL_CTL_ADD, m_iServerFd, &m_Event);
    if (err_code == -1) {
        WriteLog(g_pProcessLog,"epoll ctl error. %s\n", g_strCurTime);
        return -1;
    }

    return 0;
}

void EpollServer::epollRun(int32_t (*read_event_call_back_func)(epoll_event*), int32_t (*write_event_call_back_func)(epoll_event*)) {
    int32_t ready_number;
    bool bNewMateData = false;
    int infd;
    struct sockaddr in_addr;
    socklen_t in_len = sizeof(in_addr);
    char host_buf[NI_MAXHOST], port_buf[NI_MAXSERV];
    int32_t i;
    int32_t error_code = 0;
    uint32_t ip;
	epoll_event* ev;
	EpollMateData* md;
	int32_t code;

    while (!g_isExit) {
        ready_number = epoll_wait(m_iEpollFd, m_pEvents, MAXEVENTS, 2);
		for (i = 0; i < ready_number; ++i)
		{
			if (m_pEvents[i].data.fd == m_iServerFd)
			{

				
				while (true)
				{
					infd = accept(m_iServerFd, &in_addr, &in_len);
					if (infd == -1)
					{
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
						{
							break;
						}
						else
						{
							WriteLog(g_pProcessLog, "***accept error %s %s\n", strerror(errno), g_strCurTime);
							break;
						}
					}

					error_code = 0;
					getnameinfo(&in_addr, in_len, host_buf, sizeof(host_buf),
						port_buf, sizeof(port_buf), NI_NUMERICHOST | NI_NUMERICSERV);

					ip = inet_addr(host_buf);
					// struct in_addr m_Ip;
					// m_Ip.s_addr = inet_addr(host_buf);

					bNewMateData = false;

					if (getOldEpollMateData(g_TransMap, ip, md))
					{//aready has, may be alive or closed
						if (md->m_iFd != INVALID_CLIENT_FD) {//alive, forbid duplicate connection !!!!
							fprintf(stderr, "***already has connection from %s:%2d,  new port: %s type:%d %s\n", host_buf, md->m_iPort, port_buf, md->m_iCloseType, g_strCurTime);
							WriteLog(g_pProcessLog, "***already has connection from %s:%2d,  new port: %s type:%d %s\n", host_buf, md->m_iPort, port_buf, md->m_iCloseType, g_strCurTime);
							for (int j = 0; j < MAXEVENTS; j++)
							{
								if (md == m_pEvents[j].data.ptr)
								{
									WriteLog(g_pProcessLog, "***Close connection fd %d code %d , %s:%2d type: %d. %s\n",
										md->m_iFd, code, md->m_pHostName, md->m_iPort, md->m_iCloseType, g_strCurTime);
									m_pEvents[j].data.ptr = nullptr;
									epoll_ctl(m_iEpollFd, EPOLL_CTL_DEL, md->m_iFd, &m_pEvents[j]);
									break;
								}

							}
							close(md->m_iFd);
							// continue;
						}
						else
							m_iClientCnt++;

						//closed, keep and reuse the result queue!
						fprintf(stderr, "---reuse connection from %s:%2d,  new port: %s type:%d %s\n", host_buf, md->m_iPort, port_buf, md->m_iCloseType, g_strCurTime);
						WriteLog(g_pProcessLog, "---reuse connection from %s:%2d,  new port: %s type:%d %s\n", host_buf, md->m_iPort, port_buf, md->m_iCloseType, g_strCurTime);
						if (md->m_pNetMsgHeader)//2021.11.18
							EnDataQueue(md->m_pRSQueWait4Send, &(md->m_pNetMsgHeader), sizeof(unsigned char*), NULL, 0, NULL, 0);
						md->initEpollMateData();

					}
					else
					{
						m_iClientCnt++;
						bNewMateData = true;
						md = new EpollMateData{};
					}

					error_code = MakeSocketNonBlocking(infd);
					if (error_code == -1) {
						WriteLog(g_pProcessLog, "***Error: set socket non blocking error. %s\n", g_strCurTime);
					}
					setTcpNoDelay(infd);

					md->m_iFd = infd;
					md->m_Ip.s_addr = ip;// inet_addr(host_buf);
					md->m_iPort = (uint16_t)atoi(port_buf);
					memcpy(md->m_pHostName, host_buf, NI_MAXHOST);
					if (bNewMateData) {
						md->m_pRSQueWait4Send = OpenDataQueue(8 * 1024, 128, true);//each connection has private result queue used for results sending
						//g_TransMap.addNewNode(md->m_Ip.s_addr, md);//us ip as id, not fd   //md->m_iFd, md);
						g_TransMap.addNewNode(ip, md);//us ip as id, not fd   //md->m_iFd, md);
					}
					else {
						//use the old data-queue
					}

					m_Event.data.ptr = md;
					m_Event.events = EPOLLOUT | EPOLLIN | EPOLLERR | EPOLL_ET_MODE;
					error_code = epoll_ctl(m_iEpollFd, EPOLL_CTL_ADD, md->m_iFd, &m_Event);
					if (error_code == -1) {
						WriteLog(g_pProcessLog, "add new socket error. %s\n", g_strCurTime);
					}
					else {
						WriteLog(g_pProcessLog, "Add connection fd %d (%s:%s) %s\n", md->m_iFd, host_buf, port_buf, g_strCurTime);
					}
				}
			}
			else if (m_pEvents[i].events & EPOLLIN)
			{
				ev = &m_pEvents[i];
				md = (EpollMateData*)ev->data.ptr;
				if (md == nullptr) continue;
				code = read_event_call_back_func(ev);
				if (code >=0) {
					ev->events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLL_ET_MODE;
					epoll_ctl(m_iEpollFd, EPOLL_CTL_MOD, md->m_iFd, ev);
				}else{//if (code < 0)  Close connection
					WriteLog(g_pProcessLog, "***Close connection fd %d ret %d , %s:%2d type: %d. %s\n",
						md->m_iFd, code, md->m_pHostName, md->m_iPort, md->m_iCloseType, g_strCurTime);
					ev->data.ptr = nullptr;
					epoll_ctl(m_iEpollFd, EPOLL_CTL_DEL, md->m_iFd, ev);
					m_iClientCnt--;
					close(md->m_iFd);
					md->m_iCloseType = md->m_iCloseType | 256;
					md->m_iFd = INVALID_CLIENT_FD;
				}

			}
			else if (m_pEvents[i].events & EPOLLOUT)
			{
				ev = &m_pEvents[i];
				md = (EpollMateData*)ev->data.ptr;
				if (md == nullptr) continue;
				code = write_event_call_back_func(ev);
				if (code >= 0)   {
					ev->events = EPOLLOUT | EPOLLIN | EPOLLERR | EPOLL_ET_MODE;
					epoll_ctl(m_iEpollFd, EPOLL_CTL_MOD, md->m_iFd, ev);
				}else{//if (code <0 ) Close connection
					WriteLog(g_pProcessLog, "***Close connection fd %d ret %d , %s:%2d type: %d. %s\n",
						md->m_iFd, code, md->m_pHostName, md->m_iPort, md->m_iCloseType, g_strCurTime);
					ev->data.ptr = nullptr;
					epoll_ctl(m_iEpollFd, EPOLL_CTL_DEL, md->m_iFd, ev);
					m_iClientCnt--;
					close(md->m_iFd);
					md->m_iCloseType = md->m_iCloseType | 512;
					md->m_iFd = INVALID_CLIENT_FD;//
				}
			}
			else if ((m_pEvents[i].events & EPOLLERR) || (m_pEvents[i].events & EPOLLHUP))
			{
				epoll_event* ev = &m_pEvents[i];
				EpollMateData* md = (EpollMateData*)ev->data.ptr;
				if (md == nullptr) continue;
				WriteLog(g_pProcessLog, "***epoll error or network error on fd %d, %s:%2d type: %d. %s\n",
					md->m_iFd, md->m_pHostName, md->m_iPort, md->m_iCloseType, g_strCurTime);
				ev->data.ptr = nullptr;
				epoll_ctl(m_iEpollFd, EPOLL_CTL_DEL, md->m_iFd, ev);
				m_iClientCnt--;
				close(md->m_iFd);
				md->m_iCloseType = md->m_iCloseType | 1024;
				md->m_iFd = INVALID_CLIENT_FD;
			}
		}
    }
}

int32_t EpollServer::MakeSocketNonBlocking(int32_t sfd) {
    int32_t flags, s;
    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        WriteLog(g_pProcessLog,"get socket flags error. %s\n", g_strCurTime);
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        WriteLog(g_pProcessLog,"set socket flags error. %s\n", g_strCurTime);
        return -1;
    }
    return 0;
}

int32_t EpollServer::setTcpNoDelay(int32_t sfd) {
    int32_t flag = 1;
    return setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY,(char*)&flag, sizeof(flag));
}

int32_t EpollServer::setSockBufferSize(const uint32_t buffer_size, int32_t sfd) {
    int32_t error_code = setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (const char *)&buffer_size, sizeof(uint32_t));
    error_code = setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, (const char *)&buffer_size, sizeof(uint32_t));
    return error_code;
}

int32_t EpollServer::alterEpollFd(int32_t type, int32_t ifd) {
    PipeData data{};
    data.data_type = type;
    data.ifd = ifd;
    if (write(pipe_fd_[1], &data, sizeof(PipeData)) == sizeof(PipeData)) return 0;
    else return -1;
}

SPLAB_AUDIOSERVER_END

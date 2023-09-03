//
// Created by 997289110 on 4/1/20.
//

#ifndef AUDIOSERVER_EPOLLSERVER_H
#define AUDIOSERVER_EPOLLSERVER_H

#include <sys/epoll.h>
#include <netdb.h>
#include "AudioDataUnit.h"
#include <errno.h>
#include <stdlib.h>

#define MAX_EVENTS 1024

//fast mode: epoll has EPOLLLT and EPOLLET two mode, LT is default  ET is fast mode(data must be all read in each poll)
#define EPOLL_FAST_READ_MODE 1
#if EPOLL_FAST_READ_MODE
#define EPOLL_ET_MODE   EPOLLET  //  EPOLLET = 0x80000000
#else
#define EPOLL_ET_MODE   0 //fast mode: epoll has EPOLLLT and EPOLLET two mode, LT is default  ET is fast mode(data must be all read in each poll)
#endif

SPLAB_AUDIOSERVER_BEGIN
    typedef struct _pipeData {
        int32_t data_type;
        int32_t ifd;
    } PipeData;

    class EpollServer {
    public:
        EpollServer();

        ~EpollServer();

        /*
         * 绑定端口，且设置为无阻塞，无时延
         * @param port 端口号
         * @return 成功返回0，否则返回-1
         * */
        int32_t Bind(uint16_t port);

        /*
         * 设置监听
         * @return 成功返回0，否则返回-1
         * */
        int32_t Listen();

        /*
         * 创建epoll例程，并将socket文件描述符注册到该例程中，触发事件设置为需要读取数据的情况
         * @return 成功返回0，否则返回-1
         * */
        int32_t Create();

        /*
         * 监视epoll例程内注册的文件描述符，当发生某I/O事件，执行相应事件函数
         * @param read_event_call_back_func 读事件函数
         * @param write_event_call_back_func 写事件函数
         * */
        void epollRun(int32_t (*read_event_call_back_func)(epoll_event *ev),
                 int32_t (*write_event_call_back_func)(epoll_event *ev));

        /*
         * 设置为无阻塞，无时延
         * @param sfd 文件描述符
         * @return 成功返回0，否则返回-1
         * */
        int32_t MakeSocketNonBlocking(int32_t sfd);

        /*
         * 禁止Nagle算法
         * @param sfd 文件描述符
         * @return 成功返回0，否则返回-1
         * */
        int32_t setTcpNoDelay(int32_t sfd);

        /*
         * 获取客户端数量
         * @return 返回客户端数量
         * */
        size_t getClientNumber() { return m_iClientCnt; };

        /*
         * 设置socket缓冲区大小
         * @param buffer_size 缓冲区大小
         * @return 成功返回0，否则返回-1
         * */
        int32_t setSockBufferSize(const uint32_t buffer_size, int32_t sfd);

        /*
         * 利用管道唤醒epoll_wait，并且对指定的文件描述符进行修改操作
         * @param infd 指定的文件描述符
         * @param type 修改操作
         * @return 成功返回0，否则返回-1
         * */
        int32_t alterEpollFd(int32_t type, int32_t ifd);

        /*
         * clear the receive buf
         * */
        int32_t clearRecvBuffer(int32_t fd);

    private:
        int32_t m_iServerFd;
        int32_t m_iEpollFd;
        int32_t pipe_fd_[2];
        struct epoll_event m_Event;
        struct epoll_event *m_pEvents;
        size_t m_iClientCnt;
    };

    SPLAB_AUDIOSERVER_END

#endif //AUDIOSERVER_EPOLLSERVER_H

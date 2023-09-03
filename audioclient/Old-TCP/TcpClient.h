//
// Created by 997289110 on 2019/12/17.
//

#ifndef AUDIOCLIENT_TCPCLIENT_H
#define AUDIOCLIENT_TCPCLIENT_H

#include <netinet/in.h>
#include "AudioDataUnit.h"
SPLAB_AUDIOCLIENT_BEGIN
    class TcpClient {
    public:
        TcpClient();
        ~TcpClient();
        /*
         * 连接服务器
         * @param host_ip 服务器ip
         * @param host_port 服务器端口号
         * @return 成功返回0，否则返回-1
         * */
        int32_t connectServer(const char *host_ip, uint16_t host_port);

        int32_t connectServer(uint32_t host_ip, uint16_t host_port);
        /*
         * 接收服务器发来的数据
         * @param data 数据缓冲区
         * @param length 接收数据长度
         * @return 成功返回接收数据长度，否则返回-1
         * */
        ssize_t receiveData(void *data, uint32_t length);
        /*
         * 发送数据到服务器
         * @param data 数据缓冲区
         * @param length 发送数据长度
         * @return 成功返回发送数据长度，否则返回错误
         * */
        ssize_t sendData(void *data, uint32_t length);
        /*
         * 关闭客户端
         * @return 成功返回0，否则返回-1
         * */
        int32_t closeClient();
        /*
         * 设置成非阻塞模式
         * @return 成功返回0，否则返回-1
         * */
        int32_t makeSocketNonBlocking();
        /*
         * 禁止Nagle算法
         * @return 成功返回0，否则返回-1
         * */
        int32_t setTcpNoDelay();
        /*
         * 设置socket缓冲区大小
         * @param buffer_size 缓冲区大小
         * @return 成功返回0，否则返回-1
         * */
        int32_t setSockBufferSize(uint32_t buffer_size);

    private:
        int32_t socket_fd_;
        struct sockaddr_in dest_addr_;
        struct sockaddr_in local_addr_;
    };
}

#endif //AUDIOCLIENT_TCPCLIENT_H

//
// Created by 997289110 on 2019/12/17.
//

#include "TcpClient.h"
#include <unistd.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
SPLAB_AUDIOCLIENT_BEGIN

    TcpClient::TcpClient() {
        socket_fd_ = -1;
        memset(&dest_addr_, 0, sizeof(dest_addr_));
        memset(&local_addr_, 0, sizeof(local_addr_));
    }

    TcpClient::~TcpClient() {
        closeClient();
    }

    int32_t TcpClient::connectServer(const char *host_ip, uint16_t host_port) {
        uint32_t ip = inet_addr(host_ip);
        return connectServer(ip, host_port);
    }

    int32_t TcpClient::connectServer(uint32_t host_ip, uint16_t host_port) {
        dest_addr_.sin_addr.s_addr = host_ip;
        dest_addr_.sin_family = AF_INET;
        dest_addr_.sin_port = htons(host_port);
        closeClient();
        if ((socket_fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            if (g_writeLog) LOGFMTE("TcpClient error: %s.", strerror(errno));
            return -1;
        }
        if ((connect(socket_fd_, (struct sockaddr *) &dest_addr_, sizeof(struct sockaddr))) < 0) {
            if (g_writeLog) LOGFMTE("TcpClient error: %s.", strerror(errno));
            return -1;
        }
        return 0;
    }

    ssize_t TcpClient::receiveData(void *data, uint32_t length) {
        ssize_t receive_len = 0;
        if ((receive_len = recv(socket_fd_, data, length, 0)) < 0) {
            if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                return -1;
            }
            if (g_writeLog) LOGFMTW("TcpClient error: %s.", strerror(errno));
            return -1;
        } else {
            return receive_len;
        }
    }

    ssize_t TcpClient::sendData(void *data, uint32_t length) {
        ssize_t send_len = 0;
        if ((send_len = send(socket_fd_, data, length, 0)) < 0) {
            if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                return -1;
            }
            if (g_writeLog) LOGFMTW("TcpClient error: %s.", strerror(errno));
            return -1;
        } else {
            return send_len;
        }
    }

    int32_t TcpClient::closeClient() {
        if (socket_fd_ != -1) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        return 0;
    }

    int32_t TcpClient::makeSocketNonBlocking() {
        int32_t flags, s;
        flags = fcntl(socket_fd_, F_GETFL, 0);
        if (flags == -1) {
            if (g_writeLog) LOGFMTE("get socket flags error.");
            return -1;
        }

        flags |= O_NONBLOCK;
        s = fcntl(socket_fd_, F_SETFL, flags);
        if (s == -1) {
            if (g_writeLog) LOGFMTE("set socket flags error.");
            return -1;
        }
        return 0;
    }

    int32_t TcpClient::setTcpNoDelay() {
        int32_t flag = 1;
        return setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(flag));
    }

    int32_t TcpClient::setSockBufferSize(uint32_t buffer_size) {
        int32_t error_code =
                setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, (const char *) &buffer_size, sizeof(uint32_t))
                + setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, (const char *) &buffer_size, sizeof(uint32_t));
        return !error_code ? 0 : -1;
    }
}

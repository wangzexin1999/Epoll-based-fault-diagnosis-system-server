#ifndef AUDIO_TCPCLIENT_H
#define AUDIO_TCPCLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "AudioDataUnit.h"
SPLAB_AUDIOCLIENT_BEGIN


class TcpClient {
public:
	TcpClient();
	~TcpClient();
	int connectServer(const char* hostIP, unsigned short hostPort, bool bKeepAlive= true, uint32_t iMaxSendMbps=200);
	int connectServer(unsigned int hostIP, unsigned short hostPort, bool bKeepAlive = true, uint32_t iMaxSendMbps=200);
    ssize_t sendData(void *data, size_t length, int iFlag);
    ssize_t receiveData(void *data, size_t length);
    int closeClient();
    int initSock(int socked_fd);
    int makeSocketNonBlocking();
    int setTcpNoDelay();
	bool keepAlive(int interval);
	bool getLocalIP(char *buf, int iAvailableBufSize);
	bool canConnect(unsigned int ip){ 
		return  (m_bNetOK && (unsigned int)(dest_addr.sin_addr.s_addr ) == ip) || !m_bNetOK;
	};
	bool connectionIsReady(){
		return m_bNetOK;
	};
	void setConnectionState(bool bReady){ m_bNetOK = bReady;};
	uint32_t m_iSendBytes;
    int sock_fd;
private:
    struct sockaddr_in local_addr;
    struct sockaddr_in dest_addr;
	bool m_bNetOK;
	uint32_t m_iSendMaxFlux;
};
}

#endif //AUDIOSEARCHDIS_TCPCLIENT_H

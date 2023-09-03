#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdio.h>
#include "errno.h"
#include "TcpClient.h"

SPLAB_AUDIOCLIENT_BEGIN
//------------------------------------------------------------------------
extern char g_strCurClientTime[50];

TcpClient::TcpClient() {
    memset(&local_addr, 0, sizeof(local_addr));
	memset(&dest_addr, 0, sizeof(dest_addr));
	m_bNetOK = false;
    sock_fd = -1;
}

TcpClient::~TcpClient() {
	closeClient();
}

int TcpClient::connectServer(const char *hostIP, unsigned short hostPort, bool bKeepLive, uint32_t iMaxSendMbps)
{
    unsigned int ip = inet_addr(hostIP);
	return connectServer(ip, hostPort, bKeepLive, iMaxSendMbps);
}
//---------------------------------------------------------------------------

//typedef uint32_t in_addr_t;
int TcpClient::connectServer(unsigned int hostIP, unsigned short hostPort, bool bKeepLive, uint32_t iSendMaxFlux) {
	char strErr[300];
	int iRet;
	m_iSendMaxFlux = iSendMaxFlux;
	m_iSendBytes = 0;
	dest_addr.sin_family = AF_INET;		// AF_INET(又称 PF_INET)是 IPv4 网络协议的套接字类型
	//AF_INET6 则是 IPv6 的;而 AF_UNIX 则是 Unix 系统本地通信。

    dest_addr.sin_addr.s_addr = hostIP;//in_addr_t
    dest_addr.sin_port = htons(hostPort);
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		sprintf(strErr, "***tcp client socket fail:%d,  error %d, %s, ip:%d.%d.%d.%d:%d %s\n",
			sock_fd,
			errno,
			strerror(errno),
			(hostIP & 0x000000ff),
			(hostIP & 0x0000ff00) >> 8,
			(hostIP & 0x00ff0000) >> 16,
			(hostIP & 0xff000000) >> 24,
			hostPort,
			g_strCurClientTime
			);
		//g_pResultLog->append_one_item(0, strErr);
		fprintf(stderr, strErr);
		return sock_fd;
    }
	iRet = connect(sock_fd, (struct sockaddr *) &dest_addr, sizeof(struct sockaddr));
	if (iRet < 0) {
		sprintf(strErr, "***tcp client connect to %d.%d.%d.%d:%d fail: ret=%d, error %d, %s %s\n",
			(hostIP & 0x000000ff),
			(hostIP & 0x0000ff00) >> 8,
			(hostIP & 0x00ff0000) >> 16,
			(hostIP & 0xff000000) >> 24,
			hostPort,
			iRet,
			errno,
			strerror(errno),
			g_strCurClientTime
			);
		fprintf(stderr, strErr);
		//g_pResultLog->append_one_item(0, strErr);
		closeClient();
		return iRet;
    }
	if (bKeepLive)
	{
		if (!keepAlive(6))
		{
			closeClient();
			sprintf(strErr, "***tcp %d.%d.%d.%d:5d keep-alive fail: error %d, %s %s\n",
				(hostIP & 0x000000ff),
				(hostIP & 0x0000ff00) >> 8,
				(hostIP & 0x00ff0000) >> 16,
				(hostIP & 0xff000000) >> 24,
				hostPort,
				errno,
				strerror(errno),
				g_strCurClientTime
				);
			fprintf(stderr, strErr);
			//g_pResultLog->append_one_item(0, strErr);
			return -3;
		}
	}
	m_bNetOK = true;
	return 0;
}

 ssize_t TcpClient::sendData(void *data, size_t length, int iFlag) {
    ssize_t send_len =  send(sock_fd, data, length, iFlag);
    return send_len;
}

//
//ssize_t TcpClient::sendData(void* data, size_t length) {
//	ssize_t send_len = 0, iBytes2Send = length;
//	void* p = data;
//
//	while (iBytes2Send > 0)
//	{
//		send_len = send(sock_fd, p, iBytes2Send, 0);
//		if (send_len <= 0)
//		{//error
//			if (errno != EAGAIN) {
//				//g_pResultLog->append_one_item(0, "***tcp send data error %d, %s, send_len: %zd %s\n",errno, strerror(errno), send_len, g_strCurClientTime);
//				fprintf(stderr, "***tcp send data error %d, %s, send_len: %zd %s\n",
//					errno, strerror(errno), send_len, g_strCurClientTime);
//			}
//			return send_len;
//		}
//		else
//		{
//			m_iSendBytes += send_len;
//
//			iBytes2Send -= send_len;
//			p += send_len;
//		}
//	}
//	if( (!iBytes2Send)&&(m_iSendBytes > (1 << 17)) )// 1Mb
//	{
//		uint32_t iTime = uint32_t(0.7 * 8 * m_iSendBytes / m_iSendMaxFlux);
//		usleep(iTime);//按7折来计算sleep时间
//		m_iSendBytes = 0;
//	}
//	return length - iBytes2Send;
//}

ssize_t TcpClient::receiveData(void *data, size_t length) {
    ssize_t receive_len = 0;
    if ((receive_len = recv(sock_fd, data, length, 0)) < 0) {
        return -1;
    } else {
        return receive_len;
    }
}

int TcpClient::closeClient() {
    if (sock_fd != -1)
    {
        close(sock_fd);
    }
    sock_fd = -1;
	memset(&local_addr, 0, sizeof(local_addr));
	memset(&dest_addr, 0, sizeof(dest_addr));
	m_bNetOK = false;

    return 1;
}

int TcpClient::initSock(int socked_fd) {
    sock_fd = socked_fd;
    return 0;
}

int TcpClient::makeSocketNonBlocking() {
    int flags, ret;
	char strErr[300];
    flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags == -1)
    {
		sprintf(strErr, "***get socket flags error, ret= %d, error %d, %s %s\n",
			flags, errno, strerror(errno), g_strCurClientTime);
		//g_pResultLog->append_one_item(0, strErr);
		fprintf(stderr, strErr);
		return -1;
    }

    flags |= O_NONBLOCK;
	ret = fcntl(sock_fd, F_SETFL, flags);
	if (ret == -1)
    {
		sprintf(strErr, "***set socket flags error, ret= %d, error %d, %s %s\n",
			ret, errno, strerror(errno), g_strCurClientTime);
		//g_pResultLog->append_one_item(0, strErr);
		fprintf(stderr, strErr);
		return -1;
    }
    return 0;
}
//-----------------------------------------------------------------------------------------------------------
int TcpClient::setTcpNoDelay() {
    int flag = 1;
    return setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY,(char*)&flag, sizeof(flag));
}
//-----------------------------------------------------------------------------------------------------------
/* Set TCP keep alive option to detect dead peers. The interval option
* is only used for Linux as we are using Linux-specific APIs to set
* the probe send time, interval, and count. */
bool TcpClient::keepAlive(int interval)
{
	int ret = 0;
	int val = 1;
	char strErr[300];
	//开启keepalive机制  
	ret = setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&val, sizeof(val));
	if (ret < 0)
	{
		sprintf(strErr, "***setsockopt SO_KEEPALIVE fail, ret= %d, error %d %s %s\n",
			ret, errno, strerror(errno), g_strCurClientTime);
		//g_pResultLog->append_one_item(0, strErr);
		fprintf(stderr, strErr);
		return false;
	}

	/* Default settings are more or less garbage, with the keepalive time
	* set to 7200 by default on Linux. Modify settings to make the feature
	* actually useful. */

	/* Send first probe after interval. */
	val = interval;
	ret = setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val));
	if (ret < 0) {
		//anetSetError(err, "setsockopt TCP_KEEPIDLE: %s\n", strerror(errno));  
		sprintf(strErr, "***setsockopt TCP_KEEPIDLE fail, ret= %d, error %d, %s %s\n",
			ret, errno, strerror(errno), g_strCurClientTime);
		//g_pResultLog->append_one_item(0, strErr);
		fprintf(stderr, strErr);
		return false;
	}

	/* Send next probes after the specified interval. Note that we set the
	* delay as interval / 3, as we send three probes before detecting
	* an error (see the next setsockopt call). */
	val = interval / 3;
	if (val == 0) val = 1;
	ret = setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val));
	if (ret < 0) {
		sprintf(strErr, "***setsockopt TCP_KEEPINTVL fail, ret= %d, error %d, %s %s\n",
			ret, errno, strerror(errno), g_strCurClientTime);
		//g_pResultLog->append_one_item(0, strErr);
		fprintf(stderr, strErr);
		return false;
	}

	/* Consider the socket in error state after three we send three ACK
	* probes without getting a reply. */
	val = 9;
	ret = setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val));
	if (ret < 0) {
		sprintf(strErr, "***setsockopt TCP_KEEPCNT fail, ret= %d, error %d, %s %s\n",
			ret, errno, strerror(errno), g_strCurClientTime);
		//g_pResultLog->append_one_item(0, strErr);
		fprintf(stderr, strErr);
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------------------------------------
bool TcpClient::getLocalIP(char *ipstr, int iSize)
{
	int32_t ret;
	socklen_t len = sizeof(local_addr);
	char strErr[300];

	if (sock_fd != -1)
	{
		ret = getsockname(sock_fd, (struct sockaddr*)&local_addr, &len); //获取本地信息
		if (ret == 0)
		{
			const char *ptr = inet_ntop(AF_INET, &local_addr.sin_addr, ipstr, iSize);
			if (NULL != ptr)
			{
				//strncpy(ipstr, ptr, iSize);
				return true;
			}
			else
			{
				sprintf(strErr, "*** getsockname failed, error %d, %s %s\n",
					               errno, strerror(errno), g_strCurClientTime);
				//g_pResultLog->append_one_item(0, strErr);
				fprintf(stderr, strErr);
				return false;
			}
		}
		else
			return false;
	}
	else
	{
		return false;
	}
}
}//SPLAB_AUDIOCLIENT_BEGIN


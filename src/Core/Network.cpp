//
// Created by hyj on 2019-12-10.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>
#include "Network.h"
#include <memory>

int tpc::Core::Network::SetNonBlock(int iSock) {
    int iFlags;

    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= O_NONBLOCK;
    iFlags |= O_NDELAY;
    int ret = fcntl(iSock, F_SETFL, iFlags);
    return ret;

}

int tpc::Core::Network::CreateTcpSocket(const unsigned short shPort, const char *pszIP, bool bReuse) {
    int fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
    if( fd >= 0 )
    {
        if(shPort != 0)
        {
            if(bReuse)
            {
                int nReuseAddr = 1;
                setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&nReuseAddr,sizeof(nReuseAddr));
            }
            struct sockaddr_in addr ;
            SetAddr(pszIP,shPort,addr);
            int ret = bind(fd,(struct sockaddr*)&addr,sizeof(addr));
            if( ret != 0)
            {
                close(fd);
                return -1;
            }
        }
    }
    return fd;
}

void tpc::Core::Network::SetAddr(const char *pszIP, const unsigned short shPort, struct sockaddr_in &addr) {
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(shPort);
    int nIP = 0;
    if( !pszIP || '\0' == *pszIP
        || 0 == strcmp(pszIP,"0") || 0 == strcmp(pszIP,"0.0.0.0")
        || 0 == strcmp(pszIP,"*")
            )
    {
        nIP = htonl(INADDR_ANY);
    }
    else
    {
        nIP = inet_addr(pszIP);
    }
    addr.sin_addr.s_addr = nIP;

}

string tpc::Core::Network::ReadBuff(int fd, int len) {
    char *buff = new char[len+1];
    string retStr;
    while (len > 0) {
        long n = read(fd, buff, len);
        if (n <= 0) {
//            if (errno == EAGAIN)
//                continue;
            delete[](buff);
            return retStr;
        }
        retStr += string(buff, n);
        len -= n;
    }
    delete[](buff);
    return retStr;
}

int tpc::Core::Network::SendBuff(int fd, char *buff, size_t len) {
    int pos = 0;
    while (len > 0) {
        long n = write(fd, buff+pos, len);
        if (n < 0) {
//            if (errno == EAGAIN)
//                continue;
            return n;
        }
        len -= n;
        pos += n;
    }
    return 0;
}

int tpc::Core::Network::Connect(string host, int port) {
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    SetAddr(host.c_str(), port, addr);
    int ret = connect(fd,(struct sockaddr*)&addr,sizeof(addr));
    if (ret != 0) {
        return ret;
    }
    return fd;
}

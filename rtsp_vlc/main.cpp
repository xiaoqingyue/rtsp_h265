#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include "rtp.h"
#include "avc.h"
#include "rtsp.h"
#include "network.h"
#include "rtpenc.h"

#define SERVER_PORT      8555
#define SERVER_RTP_PORT  55532

int main()
{

    int servertcpSockfd;
    int serverudpSockfd;
    // 创建tcp服务器
    servertcpSockfd = createTcpSocket();
    if(servertcpSockfd < 0)
    {
        printf("failed to create tcp socket\n");
        return -1;
    }
    if(serverSocketAddr(servertcpSockfd) < 0)
    {
        printf("failed to bind addr1\n");
        return -1;
    }
    //开始监听
    if(listen(servertcpSockfd, 10) < 0)
    {
        printf("failed to listen\n");
        return -1;
    }
    //为rtp和rtcp创建udp套接字
    serverudpSockfd = createUdpSocket();

    if(serverudpSockfd < 0)
    {
        printf("failed to create udp socket\n");
        return -1;
    }

    printf("rtsp:/127.0.0.1:%d\n", SERVER_PORT);

    while(1)
    {
        int clienttcpSockfd;
        char clientIp[40];
        int clientPort;
       //受理客户端连接请求 成功时返回创建的套接字文件描述符，失败时返回-1  建立链接
        clienttcpSockfd = acceptClient(servertcpSockfd, clientIp, &clientPort);

        if(clienttcpSockfd < 0)
        {
            printf("failed to accept client\n");
            return -1;
        }

        printf("accept client;client ip:%s,client port:%d\n", clientIp, clientPort);

        doClient(clienttcpSockfd ,clientIp, serverudpSockfd);


    return 0;
}
}

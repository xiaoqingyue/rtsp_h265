#include "rtpenc.h"
#include "rtp.h"
#include "avc.h"
#include "rtsp.h"
#include "network.h"

#define H264_FILE_NAME   "stream-0.h265"
#define SERVER_PORT      8554
#define SERVER_RTP_PORT  55532
#define BUF_MAX_SIZE     (1024*1024)

int rtpSendH264Frame(int socket, const char* ip, int16_t port,
                            struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize)
{
    int naluType; // nalu第一个字节
    int sendBytes = 0;
    int ret;

    naluType = (frame[0]>>1) & 0x3F;

    if (frameSize <= RTP_MAX_PKT_SIZE) // nalu长度小于最大包场：单一NALU单元模式
    {


        /*
         *  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |F|    Type   |  LayerId  |TID|a single NAL unit ...
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */

        memcpy(rtpPacket->payload, frame, frameSize);
        ret = rtpSendPacket(socket, ip, port, rtpPacket, frameSize);
        if(ret < 0)
            return -1;

        rtpPacket->rtpHeader.seq++;
        sendBytes += ret;

        if (((naluType & 0x7F)>>1) == 32 || ((naluType & 0x7F)>>1) == 33) // 如果是SPS、PPS就不需要加时间戳
            goto out;
    }
    else // nalu长度小于最大包场：分片模式
    {   /*
          create the HEVC payload header and transmit the buffer as fragmentation units (FU)

                 0                   1
                 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 |F|   Type    |  LayerId  | TID |
                 +-------------+-----------------+

                 F       = 0
                 Type    = 49 (fragmentation unit (FU))
                 LayerId = 0
                 TID     = 1
       */


        /*
         *      FU Header
         *    0 1 2 3 4 5 6 7
         *   +-+-+-+-+-+-+-+-+
         *   |S|E|R|  Type   |
         *   +---------------+
         */

        int pktNum = frameSize / RTP_MAX_PKT_SIZE;       // 有几个完整的包
        int remainPktSize = frameSize % RTP_MAX_PKT_SIZE; // 剩余不完整包的大小
        int i, pos = 2;

        /* 发送完整的包 */
        for (i = 0; i < pktNum; i++)
        {
            rtpPacket->payload[0] = 49<<1;
            rtpPacket->payload[1] = 1;
            rtpPacket->payload[2] = naluType;

            //rtpPacket->payload[2] |= 1<<7;

            if (i == 0) //第一包数据
                rtpPacket->payload[2] |= 1<<7;// start
            else if (remainPktSize == 0 && i == pktNum - 1) //最后一包数据
                rtpPacket->payload[2] &= ~(1 << 7); // end

            memcpy(rtpPacket->payload+3, frame+pos, RTP_MAX_PKT_SIZE);
            ret = rtpSendPacket(socket, ip, port, rtpPacket, RTP_MAX_PKT_SIZE+3);
            if(ret < 0)
                return -1;

            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
            pos += RTP_MAX_PKT_SIZE;
        }

        /* 发送剩余的数据 */
        if (remainPktSize > 0)
        {
            rtpPacket->payload[0] = 49<<1;
            rtpPacket->payload[1] = 1;
            rtpPacket->payload[2] = naluType;
            rtpPacket->payload[2] |= 1<<6; //end

            memcpy(rtpPacket->payload+3, frame+pos, remainPktSize+3);
            ret = rtpSendPacket(socket, ip, port, rtpPacket, remainPktSize+3);
            if(ret < 0)
                return -1;

            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
        }
    }

out:

    return sendBytes;
}

void doClient(int clientSockfd, const char* clientIP, int serverRtpSockfd)
{
    char method[40];
    char url[100];
    char version[40];
    int cseq;
    int clientRtpPort,clientRtcpPort;
    char *bufPtr;
    char* rBuf = (char*)malloc(BUF_MAX_SIZE);
    char* sBuf = (char*)malloc(BUF_MAX_SIZE);
    char line[400];

    while(1)
    {
        int recvLen;
        // 接受客户端数据
        recvLen = recv(clientSockfd, rBuf, BUF_MAX_SIZE, 0);
        if(recvLen <= 0)
            goto out;

        rBuf[recvLen] = '\0';
        printf("---------------C->S--------------\n");
        printf("%s", rBuf);

        /* 解析方法 */
        bufPtr = getLineFromBuf(rBuf, line); //从buf中读取一行
        if(sscanf(line, "%s %s %s\r\n", method, url, version) != 3)
        {
            printf("parse err\n");
            goto out;
        }

        /* 解析序列号 */
//          bufPtr = getLineFromBuf(bufPtr, line);
//        if(sscanf(line, "CSeq: %d\r\n", &cseq) != 1)
//        {
//            printf("parse err\n");
//            goto out;
//        }

        /* 如果是SETUP，那么就再解析client_port */
        if(!strcmp(method, "SETUP"))
        {
                bufPtr = getLineFromBuf(bufPtr, line);
                if(!strncmp(line, "Transport:", strlen("Transport:")))
                {
                    sscanf(line, "Transport: RTP/AVP/UDP;unicast;client_port=%d\r\n", &clientRtpPort);
                    printf("clientport:%d\n", clientRtpPort);

                }
       }

        if(!strcmp(method, "OPTIONS"))
        {
            if(handleCmd_OPTIONS(sBuf, cseq))
            {
                printf("failed to handle options\n");
                goto out;
            }
        }
        else if(!strcmp(method, "DESCRIBE"))
        {
            if(handleCmd_DESCRIBE(sBuf, cseq, url))
            {
                printf("failed to handle describe\n");
                goto out;
            }
        }
        else if(!strcmp(method, "SETUP"))
        {
            if(handleCmd_SETUP(sBuf, cseq, clientRtpPort))
            {
                printf("failed to handle setup\n");
                goto out;
            }
        }
        else if(!strcmp(method, "PLAY"))
        {
            if(handleCmd_PLAY(sBuf, cseq))
            {
                printf("failed to handle play\n");
                goto out;
            }
        }
        else
        {
            goto out;
        }

        printf("---------------S->C--------------\n");
        printf("%s", sBuf);
        send(clientSockfd, sBuf, strlen(sBuf), 0);

        /* 开始播放，发送RTP包 */
        if(!strcmp(method, "PLAY"))
        {

            int frameSize, startCode;
            char* frame = (char*)malloc(200000000);
            struct RtpPacket* rtpPacket = (struct RtpPacket*)malloc(200000000);
            rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_H265, 0,
                            0, 0, 0x88923423);
            while (1)
            {

            int fd = open("/home/zhy/Documents/camera_code/rtsp/rtsp_vlc/image.h265", O_RDONLY);
            assert(fd > 0);            

             printf("start play\n");
             printf("client ip:%s\n", clientIP);
             printf("client port:%d\n", clientRtpPort);

            while (1)
            {
                frameSize = getFrameFromH264File(fd, frame, 200000000);//获取一帧
               //printf("start play:%d %d %d %d %d\n", frame[0],frame[1],frame[2],frame[3],frame[4]);
                if(frameSize < 0)
                {
                    break;
                }

                if(startCode3(frame))
                    startCode = 3;
                else
                    startCode = 4;

                frameSize -= startCode;
                rtpSendH264Frame(serverRtpSockfd, clientIP, clientRtpPort,
                                    rtpPacket, (uint8_t *)(frame+startCode), frameSize);//rtp打包发送
                rtpPacket->rtpHeader.timestamp += 90000/25;

                usleep(1000*1000/25);
            }

        }
            free(frame);
            free(rtpPacket);
            goto out;
}
    }
out:
    printf("finish\n");
    close(clientSockfd);
    free(rBuf);
    free(sBuf);
}



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"
#include "rtpenc.h"
#include "network.h"

int main() {

    int len = 0;
    int res;
    uint8_t *stream = NULL;
    const char *fileName = "/home/zhy/Documents/camera_code/rtsp/rtpserver_h265/image.h265";

    RTPMuxContext rtpMuxContext;
    UDPContext udpContext = {
        .dstIp = "127.0.0.1",   // destination ip
        .dstPort = 1234         // destination port
    };

    res = readFile(&stream, &len, fileName);
    if (res){
        printf("readFile error.\n");
        return -1;
    }

    // create udp socket
    res = udpInit(&udpContext);
    if (res){
        printf("udpInit error.\n");
        return -1;
    }

    initRTPMuxContext(&rtpMuxContext);
    rtpSendH264HEVC(&rtpMuxContext, &udpContext, stream, len);

    free(stream);

    return 0;
}

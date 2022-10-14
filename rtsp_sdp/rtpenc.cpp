#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "rtpenc.h"
#include "utils.h"
#include "avc.h"
#include "network.h"

#define RTP_VERSION 2
#define RTP_H264    96

static UDPContext *gUdpContext;


int initRTPMuxContext(RTPMuxContext *ctx){
    ctx->seq = 0;
    ctx->timestamp = 0;
    ctx->ssrc = 0x12345678; // random number
    ctx->aggregation = 1;   // use Aggregation Unit
    ctx->buf_ptr = ctx->buf;
    ctx->payload_type = 0;  // 0, H.264/AVC; 1, HEVC/H.265
    return 0;
}

// enc RTP packet
void rtpSendData(RTPMuxContext *ctx, const uint8_t *buf, int len, int mark)
{
    int res = 0;

    /* build the RTP header */
    /*
     *
     *    0                   1                   2                   3
     *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                           timestamp                           |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |           synchronization source (SSRC) identifier            |
     *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
     *   |            contributing source (CSRC) identifiers             |
     *   :                             ....                              :
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *
     **/

    uint8_t *pos = ctx->cache;
    pos[0] = (RTP_VERSION << 6) & 0xff;      // V P X CC
    pos[1] = (uint8_t)((RTP_H264 & 0x7f) | ((mark & 0x01) << 7)); // M PayloadType
    Load16(&pos[2], (uint16_t)ctx->seq);    // Sequence number
    Load32(&pos[4], ctx->timestamp);
    Load32(&pos[8], ctx->ssrc);

    /* copy av data */
    memcpy(&pos[12], buf, len);

    res = udpSend(gUdpContext, ctx->cache, (uint32_t)(len + 12));
    printf("\nrtpSendData cache [%d]: ", res);
    for (int i = 0; i < 20; ++i) {
        printf("%.2X ", ctx->cache[i]);
    }
    printf("\n");

    memset(ctx->cache, 0, RTP_PAYLOAD_MAX+10);

    ctx->buf_ptr = ctx->buf;  // restore buf_ptr

    ctx->seq = (ctx->seq + 1) & 0xffff;
}

// 拼接NAL头部 在 ctx->buff, 然后ff_rtp_send_data
static void rtpSendNAL(RTPMuxContext *ctx, const uint8_t *buf, int size, int last)
{
    //RTPMuxContext *rtp_ctx = ctx->priv_data;
    int rtp_payload_size   = RTP_PAYLOAD_MAX - 3;
    int nal_type           = (buf[0] >> 1) & 0x3F;

    /* send it as one single NAL unit? */
    if (size <= RTP_PAYLOAD_MAX) {
     /* use the original NAL unit buffer and transmit it as RTP payload */
     rtpSendData(ctx, buf, size, last);
    } else {
      /*
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
        ctx->buf[0] = 49 << 1;
        ctx->buf[1] = 1;

       /*
             create the FU header

                  0 1 2 3 4 5 6 7
                  +-+-+-+-+-+-+-+-+
                  |S|E|  FuType   |
                  +---------------+

                  S       = variable
                  E       = variable
                  FuType  = NAL unit type
            */
         ctx->buf[2]  = nal_type;
             /* set the S bit: mark as start fragment */
         ctx->buf[2] |= 1 << 7;

            /* pass the original NAL header */
          buf += 2;
          size -= 2;

         while (size+3 > rtp_payload_size) {
             /* complete and send current RTP packet */
            memcpy(&ctx->buf[3], buf, rtp_payload_size);
            rtpSendData(ctx, ctx->buf, RTP_PAYLOAD_MAX, 0);

            buf += rtp_payload_size;
            size -= rtp_payload_size;

                /* reset the S bit */
             ctx->buf[2] &= ~(1 << 7);
           }

            /* set the E bit: mark as last fragment */
             ctx->buf[2] |= 1 << 6;

             /* complete and send last RTP packet */
           memcpy(&ctx->buf[3], buf, size);
           rtpSendData(ctx, ctx->buf, size + 3, last);
       }
}
// 从一段H265流中，查询完整的NAL发送，直到发送完此流中的所有NAL
void rtpSendH264HEVC(RTPMuxContext *ctx, UDPContext *udp, const uint8_t *buf, int size){
    const uint8_t *r;
    const uint8_t *end = buf + size;
    gUdpContext = udp;

    printf("\nrtpSendH264HEVC start\n");

    if (NULL == ctx || NULL == udp || NULL == buf ||  size <= 0){
        printf("rtpSendH264HEVC param error.\n");
        return;
    }

    r = ff_avc_find_startcode(buf, end);
    while (r < end){
        const uint8_t *r1;
        while (!*(r++));  // skip current startcode

        r1 = ff_avc_find_startcode(r, end);  // find next startcode

        // send a NALU (except NALU startcode), r1==end indicates this is the last NALU
        rtpSendNAL(ctx, r, (int)(r1-r), r1==end);

        // control transmission speed
        usleep(1000000/25);
        // suppose the frame rate is 25 fps
        ctx->timestamp += (90000.0/25);
        r = r1;
    }
}

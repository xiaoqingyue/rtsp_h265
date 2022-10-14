#ifndef AVC_H
#define AVC_H

#include <stdint.h>

/* copy from FFmpeg libavformat/acv.c */
const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end);

#endif // AVC_H

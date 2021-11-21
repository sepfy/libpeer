#ifndef H264_DEPACKETIZER_H_
#define H264_DEPACKETIZER_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <rtp-packet.h>
#include <rtp-payload.h>

typedef struct H264Depacketizer H264Depacketizer;

H264Depacketizer* h264_depacketizer_create();

void h264_depacketizer_destroy(H264Depacketizer *h264_depacketizer);

void h264_depacketizer_recv(H264Depacketizer *h264_depacketizer, uint8_t *buf, size_t size);

#endif // H264_PACKETIZER_H_

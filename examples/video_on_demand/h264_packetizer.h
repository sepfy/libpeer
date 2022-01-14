#ifndef H264_PACKETIZER_H_
#define H264_PACKETIZER_H_

#include <stdint.h>

#include <rtp-packet.h>
#include <rtp-payload.h>

#include "peer_connection.h"

typedef struct H264Packetizer H264Packetizer;

H264Packetizer* h264_packetizer_create(PeerConnection *pc);

void h264_packetizer_destroy(H264Packetizer *h264_packetizer);

void h264_packetizer_send(H264Packetizer *h264_packetizer, uint8_t *buf, size_t size, unsigned long timestamp);

#endif // H264_PACKETIZER_H_

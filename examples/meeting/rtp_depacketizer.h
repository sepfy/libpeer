#ifndef RTP_DEPACKETIZER_H_
#define RTP_DEPACKETIZER_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <rtp-packet.h>
#include <rtp-payload.h>

typedef struct RtpDepacketizer RtpDepacketizer;

RtpDepacketizer* rtp_depacketizer_create(const char *codec);

void rtp_depacketizer_destroy(RtpDepacketizer *rtp_depacketizer);

void rtp_depacketizer_recv(RtpDepacketizer *rtp_depacketizer, uint8_t *buf, size_t size);

#endif // RTP_PACKETIZER_H_

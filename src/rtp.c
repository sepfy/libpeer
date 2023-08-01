#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "peer_connection.h"
#include "rtp.h"
#include "utils.h"

typedef enum RtpH264Type {

  NALU = 23,
  FU_A = 28,

} RtpH264Type;

typedef struct NaluHeader {

  uint8_t type:5;
  uint8_t nri:2;
  uint8_t f:1;
} NaluHeader;

typedef struct FuHeader {

  uint8_t type:5;
  uint8_t r:1;
  uint8_t e:1;
  uint8_t s:1;
} FuHeader;

#define RTP_PAYLOAD_SIZE (CONFIG_MTU - sizeof(RtpHeader))
#define FU_PAYLOAD_SIZE (CONFIG_MTU - sizeof(RtpHeader) - sizeof(FuHeader) - sizeof(NaluHeader))

int rtp_packet_validate(uint8_t *packet, size_t size) {

  if(size < 12)
    return 0;

  RtpHeader *rtp_header = (RtpHeader*)packet;
  return ((rtp_header->type < 64) || (rtp_header->type >= 96));
}

static int rtp_packetizer_encode_h264_single(RtpPacketizer *rtp_packetizer, uint8_t *buf, size_t size) {
    RtpPacket *rtp_packet = (RtpPacket*)rtp_packetizer->buf;

    rtp_packet->header.version = 2;
    rtp_packet->header.padding = 0;
    rtp_packet->header.extension = 0;
    rtp_packet->header.csrccount = 0;
    rtp_packet->header.markerbit = 0;
    rtp_packet->header.type = rtp_packetizer->type;
    rtp_packet->header.seq_number = htons(rtp_packetizer->seq_number++);
    rtp_packet->header.timestamp = htonl(rtp_packetizer->timestamp);
    rtp_packet->header.ssrc = htonl(rtp_packetizer->ssrc);

    memcpy(rtp_packet->payload, buf, size);
    rtp_packetizer->on_packet(rtp_packetizer->buf, size + sizeof(RtpHeader), rtp_packetizer->user_data);
}

static int rtp_packetizer_encode_h264_fu_a(RtpPacketizer *rtp_packetizer, uint8_t *buf, size_t size) {

    RtpPacket *rtp_packet = (RtpPacket*)rtp_packetizer->buf;

    rtp_packet->header.version = 2;
    rtp_packet->header.padding = 0;
    rtp_packet->header.extension = 0;
    rtp_packet->header.csrccount = 0;
    rtp_packet->header.markerbit = 0;
    rtp_packet->header.type = rtp_packetizer->type;
    rtp_packet->header.seq_number = htons(rtp_packetizer->seq_number++);
    rtp_packet->header.timestamp = htonl(rtp_packetizer->timestamp);
    rtp_packet->header.ssrc = htonl(rtp_packetizer->ssrc);
    rtp_packetizer->timestamp += 90000/25; // 25 FPS.

    uint8_t type = buf[0] & 0x1f;
    uint8_t nri = (buf[0] & 0x60) >> 5;
    buf = buf + 1;
    size = size - 1;

    NaluHeader *fu_indicator = (NaluHeader*)rtp_packet->payload;
    FuHeader *fu_header = (FuHeader*)rtp_packet->payload + sizeof(NaluHeader);
    fu_header->s = 1;

    while (size > 0) {

      fu_indicator->type = FU_A;
      fu_indicator->nri = nri;
      fu_indicator->f = 0;
      fu_header->type = type;
      fu_header->r = 0;

      if (size <= FU_PAYLOAD_SIZE) {

        fu_header->e = 1;
        rtp_packet->header.markerbit = 1;
        memcpy(rtp_packet->payload + sizeof(NaluHeader) + sizeof(FuHeader), buf, size);
	rtp_packetizer->on_packet(rtp_packetizer->buf, size + sizeof(RtpHeader) + sizeof(NaluHeader) + sizeof(FuHeader), rtp_packetizer->user_data);
        break;
      }

      fu_header->e = 0;

      memcpy(rtp_packet->payload + sizeof(NaluHeader) + sizeof(FuHeader), buf, FU_PAYLOAD_SIZE);
      rtp_packetizer->on_packet(rtp_packetizer->buf, CONFIG_MTU, rtp_packetizer->user_data);
      size -= FU_PAYLOAD_SIZE;
      buf += FU_PAYLOAD_SIZE;

      fu_header->s = 0;
      rtp_packet->header.seq_number = htons(rtp_packetizer->seq_number++);

    }
}

static uint8_t* h264_find_nalu(uint8_t *buf_start, uint8_t *buf_end) {

  uint8_t *p = buf_start + 2;

  while (p < buf_end) {

    if (*(p - 2) == 0x00 && *(p - 1) == 0x00 && *p == 0x01)
      return p + 1;
    p++;
  }

  return buf_end;
}

static int rtp_packetizer_encode_h264(RtpPacketizer *rtp_packetizer, uint8_t *buf, size_t size) {

  uint8_t *buf_end = buf + size;
  uint8_t *pstart, *pend;
  size_t nalu_size;

  for (pstart = h264_find_nalu(buf, buf_end); pstart < buf_end; pstart = pend) {

    pend = h264_find_nalu(pstart, buf_end);
    nalu_size = pend - pstart;

    if (pend != buf_end)
      nalu_size--;

    while (pstart[nalu_size - 1] == 0x00)
      nalu_size--;

    if (nalu_size <= RTP_PAYLOAD_SIZE) {

      rtp_packetizer_encode_h264_single(rtp_packetizer, pstart, nalu_size);

    } else {

      rtp_packetizer_encode_h264_fu_a(rtp_packetizer, pstart, nalu_size);
    }

  }
  
}

static int rtp_packetizer_encode_generic(RtpPacketizer *rtp_packetizer, uint8_t *buf, size_t size) {

  RtpHeader *rtp_header = (RtpHeader*)rtp_packetizer->buf;
  rtp_header->version = 2;
  rtp_header->padding = 0;
  rtp_header->extension = 0;
  rtp_header->csrccount = 0;
  rtp_header->markerbit = 0;
  rtp_header->type = rtp_packetizer->type;
  rtp_header->seq_number = htons(rtp_packetizer->seq_number++);
  rtp_packetizer->timestamp += size; // 8000 HZ. 
  rtp_header->timestamp = htonl(rtp_packetizer->timestamp);
  rtp_header->ssrc = htonl(rtp_packetizer->ssrc);
  memcpy(rtp_packetizer->buf + sizeof(RtpHeader), buf, size);

  rtp_packetizer->on_packet(rtp_packetizer->buf, size + sizeof(RtpHeader), rtp_packetizer->user_data);
  
  return 0;
}

void rtp_packetizer_init(RtpPacketizer *rtp_packetizer, MediaCodec codec, void (*on_packet)(uint8_t *packet, size_t bytes, void *user_data), void *user_data) {

  rtp_packetizer->on_packet = on_packet;
  rtp_packetizer->user_data = user_data;
  rtp_packetizer->timestamp = 0;
  rtp_packetizer->seq_number = 0;

  switch (codec) {

    case CODEC_H264:
      rtp_packetizer->type = PT_H264;
      rtp_packetizer->ssrc = SSRC_H264;
      rtp_packetizer->encode_func = rtp_packetizer_encode_h264;
      break;
    case CODEC_PCMA:
      rtp_packetizer->type = PT_PCMA;
      rtp_packetizer->ssrc = SSRC_PCMA;
      rtp_packetizer->encode_func = rtp_packetizer_encode_generic;
      break;
    case CODEC_PCMU:
      rtp_packetizer->type = PT_PCMU;
      rtp_packetizer->ssrc = SSRC_PCMU;
      rtp_packetizer->encode_func = rtp_packetizer_encode_generic;
    default:
      break;
  }
}

int rtp_packetizer_encode(RtpPacketizer *rtp_packetizer, uint8_t *buf, size_t size) {

  return rtp_packetizer->encode_func(rtp_packetizer, buf, size);
}


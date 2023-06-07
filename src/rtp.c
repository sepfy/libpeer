#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "rtp.h"
#include "utils.h"

int rtp_packet_validate(char *packet, size_t size) {

  if(size < 12)
    return 0;

  RtpHeader *rtp_header = (RtpHeader*)packet;
  return ((rtp_header->type < 64) || (rtp_header->type >= 96));
}

static int rtp_packetizer_encode_generic(RtpPacketizer *rtp_packetizer, uint8_t *buf, size_t size) {

  RtpHeader *rtp_header = (RtpHeader*)rtp_packetizer->buf;
  rtp_header->version = 2;
  rtp_header->padding = 0;
  rtp_header->extension = 0;
  rtp_header->csrccount = 0;
  rtp_header->markerbit = 0;
  rtp_header->type = rtp_packetizer->type;
  rtp_header->seq_number = htons(1);
  rtp_packetizer->timestamp += size; // 8000 HZ. 
  rtp_header->timestamp = htonl(rtp_packetizer->timestamp);
  rtp_header->ssrc = htonl(rtp_packetizer->ssrc);
  memcpy(rtp_packetizer->buf + sizeof(RtpHeader), buf, size);
  rtp_packetizer->on_packet(rtp_packetizer->buf, size + sizeof(RtpHeader), rtp_packetizer->user_data);
  
  return 0;
}

void rtp_packetizer_init(RtpPacketizer *rtp_packetizer, RtpPayloadType type, uint32_t ssrc,
 void (*on_packet)(const uint8_t *packet, size_t bytes, void *user_data), void *user_data) {

  rtp_packetizer->type = type;
  rtp_packetizer->on_packet = on_packet;
  rtp_packetizer->user_data = user_data;
  rtp_packetizer->ssrc = ssrc;
  rtp_packetizer->timestamp = 0;

  switch (rtp_packetizer->type) {

    case PT_H264:
      LOGD("Not implemented yes");
      break;
    case PT_PCMU:
    case PT_PCMA:
    default:
      rtp_packetizer->encode_func = rtp_packetizer_encode_generic;
    break;
  }
}

int rtp_packetizer_encode(RtpPacketizer *rtp_packetizer, uint8_t *buf, size_t size) {

  return rtp_packetizer->encode_func(rtp_packetizer, buf, size);
}


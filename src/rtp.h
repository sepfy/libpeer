#ifndef RTP_H_
#define RTP_H_

#include <stdint.h>
#include <endian.h>

#include "config.h"

typedef enum RtpPayloadType {

  PT_PCMU = 0,
  PT_PCMA = 8,
  PT_G722 = 9,
  PT_H264 = 96,
  PT_OPUS = 111

} RtpPayloadType;

typedef struct RtpHeader {
#if __BYTE_ORDER == __BIG_ENDIAN
  uint16_t version:2;
  uint16_t padding:1;
  uint16_t extension:1;
  uint16_t csrccount:4;
  uint16_t markerbit:1;
  uint16_t type:7;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
  uint16_t csrccount:4;
  uint16_t extension:1;
  uint16_t padding:1;
  uint16_t version:2;
  uint16_t type:7;
  uint16_t markerbit:1;
#endif
  uint16_t seq_number;
  uint32_t timestamp;
  uint32_t ssrc;
  uint32_t csrc[0];

} RtpHeader;

typedef struct RtpPacket {

  RtpHeader header;
  uint8_t *payload;

} RtpPacket;

typedef struct RtpMap {

  int pt_h264;
  int pt_opus;
  int pt_pcma;

} RtpMap;

typedef struct RtpPacketizer RtpPacketizer;

struct RtpPacketizer {

  RtpPayloadType type;
  void (*on_packet)(const uint8_t *packet, size_t bytes, void *user_data);
  int (*encode_func)(RtpPacketizer *rtp_packetizer, uint8_t *buf, size_t size);
  void *user_data;
  uint32_t ssrc;
  uint8_t buf[CONFIG_MTU];
  uint32_t timestamp;
};

int rtp_packet_validate(uint8_t *packet, size_t size);

void rtp_packetizer_init(RtpPacketizer *rtp_packetizer, RtpPayloadType type, uint32_t ssrc,
 void (*on_packet)(const uint8_t *packet, size_t bytes, void *user_data), void *user_data);

int rtp_packetizer_encode(RtpPacketizer *rtp_packetizer, uint8_t *buf, size_t size);


#endif // RTP_H_

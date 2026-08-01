#ifndef RTP_H_
#define RTP_H_

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "peer_connection.h"

typedef enum RtpPayloadType {

  PT_PCMU = 0,
  PT_PCMA = 8,
  PT_G722 = 9,
  PT_H264 = 96,
  PT_OPUS = 111

} RtpPayloadType;

typedef enum RtpSsrc {

  SSRC_H264 = 1,
  SSRC_PCMA = 4,
  SSRC_PCMU = 5,
  SSRC_OPUS = 6,

} RtpSsrc;

#define RTP_HEADER_VERSION 2u
#define RTP_HEADER_VERSION_MASK 0xC0u
#define RTP_HEADER_VERSION_SHIFT 6u
#define RTP_HEADER_MARKER_BIT 0x80u
#define RTP_HEADER_PAYLOAD_TYPE_MASK 0x7Fu

typedef struct RtpHeader {
  /* RFC 3550 fixed header bytes. Do not use C bitfields for wire data. */
  uint8_t version_padding_extension_csrc;
  uint8_t marker_payload_type;
  uint16_t seq_number;
  uint32_t timestamp;
  uint32_t ssrc;
  uint32_t csrc[0];

} RtpHeader;

typedef struct RtpPacket {
  RtpHeader header;
  uint8_t payload[0];

} RtpPacket;

typedef struct RtpMap {
  int pt_h264;
  int pt_opus;
  int pt_pcma;

} RtpMap;

typedef struct RtpEncoder RtpEncoder;
typedef struct RtpDecoder RtpDecoder;
typedef void (*RtpOnPacket)(uint8_t* packet, size_t bytes, void* user_data);

struct RtpDecoder {
  RtpPayloadType type;
  RtpOnPacket on_packet;
  int (*decode_func)(RtpDecoder* rtp_decoder, uint8_t* data, size_t size);
  void* user_data;
};

struct RtpEncoder {
  RtpPayloadType type;
  RtpOnPacket on_packet;
  int (*encode_func)(RtpEncoder* rtp_encoder, uint8_t* data, size_t size);
  void* user_data;
  uint16_t seq_number;
  uint32_t ssrc;
  uint32_t timestamp;
  uint32_t timestamp_increment;
  uint8_t buf[CONFIG_MTU + 128];
};

int rtp_packet_validate(uint8_t* packet, size_t size);

void rtp_encoder_init(RtpEncoder* rtp_encoder, MediaCodec codec, RtpOnPacket on_packet, void* user_data);

int rtp_encoder_encode(RtpEncoder* rtp_encoder, const uint8_t* data, size_t size);

void rtp_decoder_init(RtpDecoder* rtp_decoder, MediaCodec codec, RtpOnPacket on_packet, void* user_data);

int rtp_decoder_decode(RtpDecoder* rtp_decoder, const uint8_t* data, size_t size);

uint32_t rtp_get_ssrc(uint8_t* packet);

#endif  // RTP_H_

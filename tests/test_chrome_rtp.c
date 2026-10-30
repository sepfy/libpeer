#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rtp.h"

static uint8_t received[4096];
static size_t received_size;
static int packets;
static RtpDecoder roundtrip_decoder;
static void on_packet(uint8_t* data, size_t size, void* unused) {
  (void)unused;
  assert(size <= sizeof(received));
  memcpy(received, data, size);
  received_size = size;
  packets++;
}
static void decode_packet(uint8_t* data, size_t size, void* unused) {
  (void)unused;
  assert(rtp_decoder_decode(&roundtrip_decoder, data, size) == 0);
}

int main(void) {
  RtpDecoder decoder = {0};
  RtpEncoder encoder = {0};
  assert(sizeof(encoder.buf) == RTP_PACKET_BUFFER_SIZE);
  uint8_t packet[64] = {0x91, 0}; /* V2, X, one CSRC */
  packet[16] = 0xbe; packet[17] = 0xde;
  packet[19] = 1; /* One extension word */
  packet[24] = 0xff; packet[25] = 0x7f;
  rtp_decoder_init(&decoder, CODEC_PCMU, on_packet, NULL);
  assert(rtp_decoder_decode(&decoder, packet, 26) >= 0);
  assert(received_size == 2 && received[0] == 0xff && received[1] == 0x7f);
  packet[0] |= 0x20;
  packet[26] = 0; packet[27] = 2;
  assert(rtp_decoder_decode(&decoder, packet, 28) >= 0 && received_size == 2);
  packet[27] = 0;
  assert(rtp_decoder_decode(&decoder, packet, 28) < 0);
  packet[0] &= ~0x20;
  packet[19] = 255;
  assert(rtp_decoder_decode(&decoder, packet, 26) < 0);
  packet[19] = 1;
  assert(rtp_decoder_decode(&decoder, packet, 24) < 0);
  assert(rtp_decoder_decode(&decoder, packet, 17) < 0);
  packet[0] = 0x8f;
  assert(rtp_decoder_decode(&decoder, packet, 26) < 0);
  packet[0] = 0x91;
  rtp_decoder_init(&decoder, CODEC_H264, on_packet, NULL);
  packet[24] = 0x65; packet[25] = 0xab;
  assert(rtp_decoder_decode(&decoder, packet, 26) == 0);
  assert(received_size == 6 && received[3] == 1 && received[4] == 0x65 && received[5] == 0xab);
  uint8_t stap[17] = {0x80, PT_H264};
  stap[12] = 24;
  stap[14] = 1;
  stap[15] = 0x65;
  assert(rtp_decoder_decode(&decoder, stap, 16) == 0);
  stap[16] = 0xff;
  assert(rtp_decoder_decode(&decoder, stap, sizeof(stap)) < 0);
  packet[24] = 28;
  assert(rtp_decoder_decode(&decoder, packet, 25) < 0);
  packet[25] = 5; /* continuation without start */
  assert(rtp_decoder_decode(&decoder, packet, 26) < 0);
  rtp_encoder_init(&encoder, CODEC_PCMU, on_packet, NULL);
  uint8_t large[CONFIG_MTU] = {0};
  assert(rtp_encoder_encode(&encoder, large, sizeof(large)) < 0);
  rtp_encoder_init(&encoder, CODEC_H264, on_packet, NULL);
  encoder.type = 125;
  const uint8_t nal[] = {0, 0, 0, 1, 0x65, 0xab};
  assert(rtp_encoder_encode(&encoder, nal, sizeof(nal)) == 0);
  assert((received[1] & 0x7f) == 125);
  uint8_t fragmented[1469];
  memset(fragmented, 0xab, sizeof(fragmented));
  fragmented[0] = fragmented[1] = fragmented[2] = 0;
  fragmented[3] = 1;
  fragmented[4] = 0x65;
  received_size = 0;
  rtp_decoder_init(&roundtrip_decoder, CODEC_H264, on_packet, NULL);
  rtp_encoder_init(&encoder, CODEC_H264, decode_packet, NULL);
  assert(rtp_encoder_encode(&encoder, fragmented, sizeof(fragmented)) == 0);
  assert(received_size == sizeof(fragmented));
  assert(memcmp(received, fragmented, sizeof(fragmented)) == 0);
  assert(rtp_encoder_encode(&encoder, NULL, 0) < 0);
  const uint8_t empty_nal[] = {0, 0, 1, 0, 0, 0};
  int before = packets;
  assert(rtp_encoder_encode(&encoder, empty_nal, sizeof(empty_nal)) == 0 && before == packets);
  puts("Chrome RTP extension/bounds regression: PASS");
  return 0;
}

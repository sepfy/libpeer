#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "address.h"
#include "rtp.h"

#define MAX_CAPTURED_PACKETS 16

typedef struct PacketCapture {
  int count;
  uint16_t sequence[MAX_CAPTURED_PACKETS];
  uint32_t timestamp[MAX_CAPTURED_PACKETS];
  uint8_t marker[MAX_CAPTURED_PACKETS];
  uint8_t payload_type[MAX_CAPTURED_PACKETS];
  uint8_t payload_first[MAX_CAPTURED_PACKETS];
  uint8_t payload_second[MAX_CAPTURED_PACKETS];
} PacketCapture;

static int failures = 0;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
              #condition);                                                 \
      failures++;                                                          \
    }                                                                      \
  } while (0)

static void capture_packet(uint8_t* packet, size_t bytes, void* user_data) {
  PacketCapture* capture = (PacketCapture*)user_data;

  if (capture->count < MAX_CAPTURED_PACKETS &&
      bytes >= sizeof(RtpHeader)) {
    int index = capture->count;
    RtpPacket* rtp_packet = (RtpPacket*)packet;
    size_t payload_size = bytes - sizeof(RtpHeader);

    capture->sequence[index] = ntohs(rtp_packet->header.seq_number);
    capture->timestamp[index] = ntohl(rtp_packet->header.timestamp);
    capture->marker[index] = rtp_packet->header.markerbit;
    capture->payload_type[index] = rtp_packet->header.type;
    capture->payload_first[index] =
        payload_size > 0 ? rtp_packet->payload[0] : 0;
    capture->payload_second[index] =
        payload_size > 1 ? rtp_packet->payload[1] : 0;
    capture->count++;
  }
}

static void test_access_unit_uses_one_timestamp_and_final_marker(void) {
  static const uint8_t access_unit[] = {
      0x00, 0x00, 0x00, 0x01, 0x09, 0x10,
      0x00, 0x00, 0x00, 0x01, 0x41, 0x11, 0x22,
      0x00, 0x00, 0x00, 0x01, 0x41, 0x33, 0x44};
  PacketCapture capture;
  RtpEncoder encoder;

  memset(&capture, 0, sizeof(capture));
  memset(&encoder, 0, sizeof(encoder));
  rtp_encoder_init(&encoder, CODEC_H264, capture_packet, &capture);
  encoder.timestamp = 9000;

  CHECK(rtp_encoder_encode(&encoder, access_unit, sizeof(access_unit)) == 0);
  CHECK(capture.count == 3);
  CHECK(capture.timestamp[0] == 9000);
  CHECK(capture.timestamp[1] == 9000);
  CHECK(capture.timestamp[2] == 9000);
  CHECK(capture.marker[0] == 0);
  CHECK(capture.marker[1] == 0);
  CHECK(capture.marker[2] == 1);
  CHECK(capture.sequence[0] == 0);
  CHECK(capture.sequence[1] == 1);
  CHECK(capture.sequence[2] == 2);
  CHECK(encoder.timestamp == 12000);
}

static void test_final_fu_a_fragment_carries_marker(void) {
  uint8_t access_unit[CONFIG_MTU + 800];
  PacketCapture capture;
  RtpEncoder encoder;
  size_t offset = 0;
  size_t index;

  memset(&capture, 0, sizeof(capture));
  memset(&encoder, 0, sizeof(encoder));
  memset(access_unit, 0x55, sizeof(access_unit));

  access_unit[offset++] = 0;
  access_unit[offset++] = 0;
  access_unit[offset++] = 0;
  access_unit[offset++] = 1;
  access_unit[offset++] = 0x09;
  access_unit[offset++] = 0x10;
  access_unit[offset++] = 0;
  access_unit[offset++] = 0;
  access_unit[offset++] = 0;
  access_unit[offset++] = 1;
  access_unit[offset++] = 0x65;
  for (index = offset; index < sizeof(access_unit); index++) {
    access_unit[index] = (uint8_t)index;
  }

  rtp_encoder_init(&encoder, CODEC_H264, capture_packet, &capture);
  encoder.timestamp = 123456;

  CHECK(rtp_encoder_encode(&encoder, access_unit, sizeof(access_unit)) == 0);
  CHECK(capture.count == 3);
  CHECK(capture.marker[0] == 0);
  CHECK(capture.marker[1] == 0);
  CHECK(capture.marker[2] == 1);
  CHECK((capture.payload_first[1] & 0x1f) == 28);
  CHECK((capture.payload_second[1] & 0x80) != 0);
  CHECK((capture.payload_second[1] & 0x40) == 0);
  CHECK((capture.payload_first[2] & 0x1f) == 28);
  CHECK((capture.payload_second[2] & 0x80) == 0);
  CHECK((capture.payload_second[2] & 0x40) != 0);
  CHECK(capture.timestamp[0] == 123456);
  CHECK(capture.timestamp[1] == 123456);
  CHECK(capture.timestamp[2] == 123456);
  CHECK(encoder.timestamp == 126456);
}

static void test_invalid_access_unit_does_not_advance_timestamp(void) {
  static const uint8_t invalid_access_unit[] = {0x41, 0x01, 0x02};
  PacketCapture capture;
  RtpEncoder encoder;

  memset(&capture, 0, sizeof(capture));
  memset(&encoder, 0, sizeof(encoder));
  rtp_encoder_init(&encoder, CODEC_H264, capture_packet, &capture);
  encoder.timestamp = 777;

  CHECK(rtp_encoder_encode(&encoder, NULL, 0) < 0);
  CHECK(rtp_encoder_encode(&encoder, invalid_access_unit,
                           sizeof(invalid_access_unit)) < 0);
  CHECK(capture.count == 0);
  CHECK(encoder.timestamp == 777);
}

int main(void) {
  test_access_unit_uses_one_timestamp_and_final_marker();
  test_final_fu_a_fragment_carries_marker();
  test_invalid_access_unit_does_not_advance_timestamp();

  if (failures != 0) {
    fprintf(stderr, "%d test checks failed\n", failures);
  }
  return failures == 0 ? 0 : 1;
}

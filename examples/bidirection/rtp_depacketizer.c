#include <stdint.h>
#include <string.h>

#include "rtp_depacketizer.h"

struct RtpDepacketizer {

  struct rtp_payload_t handler;
  void* decoder;
  char codec[32];

};

static void* rtp_depacketizer_alloc(void* param, int bytes) {

  static uint8_t buffer[8 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };
  if((sizeof(buffer) - 4) <= bytes) {
    printf("rtp package to large\n");
    exit(1);
  }

  return buffer + 4;
}

static void rtp_depacketizer_free(void* param, void *packet) {
}

static int rtp_depacketizer_packet_audio(void* param, const void *packet, int bytes, uint32_t timestamp, int flags) {

  RtpDepacketizer *rtp_depacketizer = (RtpDepacketizer*)param;
  uint8_t *data = (uint8_t*)packet;

  static FILE *fp = NULL;
  static int count = 0;
  static int index = 0;

  char filename[64];
  memset(filename, 0, sizeof(filename));
  snprintf(filename, sizeof(filename), "audio_%d.pcma", index);

  if(fp == NULL) {
    printf("Record browser audio to %s\n", filename);
    fp = fopen(filename, "wb");
  }

  if(fp != NULL) {
    fwrite(data, bytes, 1, fp);
  }

  count++;
  if(count % 500 == 0 && fp != NULL) {
    printf("Stop recording audio %s\n", filename);
    fclose(fp);
    fp = NULL;
    index++;
  }

  return 0;
}

static int rtp_depacketizer_packet_video(void* param, const void *packet, int bytes, uint32_t timestamp, int flags) {

  RtpDepacketizer *rtp_depacketizer = (RtpDepacketizer*)param;

  uint8_t *data = (uint8_t*)packet;

  static uint8_t nalu_header[] = {0x00, 0x00, 0x00, 0x01};
  static FILE *fp = NULL;
  static int count = 0;
  static int index = 0;

  char filename[64];
  memset(filename, 0, sizeof(filename));
  snprintf(filename, sizeof(filename), "video_%d.h264", index);

  if(fp == NULL) {
    printf("Record browser video to %s\n", filename);
    fp = fopen(filename, "wb");
  }

  if(fp != NULL) {
    fwrite(nalu_header, sizeof(nalu_header), 1, fp);
    fwrite(data, bytes, 1, fp);
  }

  count++;
  if(count % 200 == 0 && fp != NULL) {
    printf("Close recording video %s\n", filename);
    fclose(fp);
    fp = NULL;
    index++;
  }

  return 0;
}

RtpDepacketizer* rtp_depacketizer_create(const char *codec) {

  RtpDepacketizer *rtp_depacketizer = NULL;

  rtp_depacketizer = (RtpDepacketizer*)malloc(sizeof(struct RtpDepacketizer));

  rtp_depacketizer->handler.alloc = rtp_depacketizer_alloc;
  rtp_depacketizer->handler.free = rtp_depacketizer_free;

  if(strcmp(codec, "H264") == 0) {
    rtp_depacketizer->handler.packet = rtp_depacketizer_packet_video;
    rtp_depacketizer->decoder = rtp_payload_decode_create(102, "H264", &rtp_depacketizer->handler, rtp_depacketizer);
    snprintf(rtp_depacketizer->codec, sizeof(rtp_depacketizer->codec), "h264");
  }
  else if(strcmp(codec, "PCMA") == 0) {
    rtp_depacketizer->handler.packet = rtp_depacketizer_packet_audio;
    rtp_depacketizer->decoder = rtp_payload_decode_create(8, "PCMA", &rtp_depacketizer->handler, rtp_depacketizer);
    snprintf(rtp_depacketizer->codec, sizeof(rtp_depacketizer->codec), "pcma");
  }

  return rtp_depacketizer;
}

void rtp_depacketizer_destroy(RtpDepacketizer *rtp_depacketizer) {

  if(rtp_depacketizer) {
    if(rtp_depacketizer->decoder)
      rtp_payload_decode_destroy(rtp_depacketizer->decoder);
    free(rtp_depacketizer);
    rtp_depacketizer = NULL;
  }
}


void rtp_depacketizer_recv(RtpDepacketizer *rtp_depacketizer, uint8_t *buf, size_t size) {

  rtp_payload_decode_input(rtp_depacketizer->decoder, buf, size);
}


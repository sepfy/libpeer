#include <stdint.h>

#include "rtp_depacketizer.h"

static void* rtp_decode_alloc(void* param, int bytes) {

  static uint8_t buffer[8 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };
  if((sizeof(buffer) - 4) <= bytes) {
    printf("rtp package to large\n");
    exit(1);
  }

  return buffer + 4;
}

static void rtp_decode_free(void* param, void *packet) {
}

static int rtp_decode_audio_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags) {

  rtp_decode_context_t *rtp_decode_context = (struct rtp_decode_context_t*)param;
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

static int rtp_decode_video_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags) {

  rtp_decode_context_t *rtp_decode_context = (struct rtp_decode_context_t*)param;

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

struct rtp_decode_context_t* create_rtp_decode_context(const char *codec) {

  struct rtp_decode_context_t *rtp_decode_context = NULL;

  rtp_decode_context = (rtp_decode_context_t*)malloc(sizeof(struct rtp_decode_context_t));

  rtp_decode_context->handler.alloc = rtp_decode_alloc;
  rtp_decode_context->handler.free = rtp_decode_free;

  if(strcmp(codec, "H264") == 0) {
    rtp_decode_context->handler.packet = rtp_decode_video_packet;
    rtp_decode_context->decoder = rtp_payload_decode_create(102, "H264", &rtp_decode_context->handler, rtp_decode_context);
    snprintf(rtp_decode_context->codec, sizeof(rtp_decode_context->codec), "h264");
  }
  else if(strcmp(codec, "PCMA") == 0) {
    rtp_decode_context->handler.packet = rtp_decode_audio_packet;
    rtp_decode_context->decoder = rtp_payload_decode_create(8, "PCMA", &rtp_decode_context->handler, rtp_decode_context);
    snprintf(rtp_decode_context->codec, sizeof(rtp_decode_context->codec), "pcma");
  }


  return rtp_decode_context;
}

void rtp_decode_frame(struct rtp_decode_context_t *rtp_decode_context, uint8_t *buf, size_t size) {
  rtp_payload_decode_input(rtp_decode_context->decoder, buf, size);
}


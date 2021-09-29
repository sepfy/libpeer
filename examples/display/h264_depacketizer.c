#include <stdint.h>

#include "h264_depacketizer.h"

static void* rtp_decode_alloc(void* param, int bytes) {
  static uint8_t buffer[2 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };
  if((sizeof(buffer) - 4) <= bytes) {
    printf("rtp package to large\n");
    exit(1);
  }
  return buffer + 4;
}

static void rtp_decode_free(void* param, void *packet) {
}

static int rtp_decode_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags) {

  rtp_decode_context_t *rtp_decode_context = (struct rtp_decode_context_t*)param;

  uint8_t *data = (uint8_t*)packet;
#if 0
if(bytes > 4) {
  int i = 0;
  for(i = 0; i < 4; i++) {
    printf("%.2X ", data[i]);
  }
  printf("\n");
}
#endif

  static FILE *fp = NULL;
  if(fp == NULL) {
    fp = fopen("out.h264", "wb");
  }

  static uint8_t nalu_header[] = {0x00, 0x00, 0x00, 0x01};
  if(fp != NULL) {
    fwrite(nalu_header, sizeof(nalu_header), 1, fp);
    fwrite(data, bytes, 1, fp);
  }

  static int count = 0;
  count++;
  if(count %300 == 0 && fp != NULL) {
    fclose(fp);
    exit(0);
  }

  return 0;
}

struct rtp_decode_context_t* create_rtp_decode_context() {

  struct rtp_decode_context_t *rtp_decode_context = NULL;
  rtp_decode_context = (rtp_decode_context_t*)malloc(sizeof(struct rtp_decode_context_t));
  rtp_decode_context->handler.alloc = rtp_decode_alloc;
  rtp_decode_context->handler.free = rtp_decode_free;
  rtp_decode_context->handler.packet = rtp_decode_packet;
  rtp_decode_context->decoder = rtp_payload_decode_create(102, "H264", &rtp_decode_context->handler, rtp_decode_context);

  return rtp_decode_context;
}

void rtp_decode_frame(struct rtp_decode_context_t *rtp_decode_context, uint8_t *buf, size_t size) {
  rtp_payload_decode_input(rtp_decode_context->decoder, buf, size);
}


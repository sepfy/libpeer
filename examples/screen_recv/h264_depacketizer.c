#include <stdint.h>

#include "h264_depacketizer.h"

struct H264Depacketizer {
  
  struct rtp_payload_t handler;
  void* decoder;

};

static void* h264_depacketizer_alloc(void* param, int bytes) {

  static uint8_t buffer[2 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };
  if((sizeof(buffer) - 4) <= bytes) {
    printf("rtp package to large\n");
    exit(1);
  }

  return buffer + 4;
}

static void h264_depacketizer_free(void* param, void *packet) {
}

static int h264_depacketizer_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags) {

  H264Depacketizer *h264_depacketizer = (H264Depacketizer*)param;

  uint8_t *data = (uint8_t*)packet;

  static uint8_t nalu_header[] = {0x00, 0x00, 0x00, 0x01};
  static FILE *fp = NULL;
  static int count = 0;

  if(fp == NULL) {
    printf("Record media to record.h264\n");
    fp = fopen("record.h264", "wb");
  }

  if(fp != NULL) {
    fwrite(nalu_header, sizeof(nalu_header), 1, fp);
    fwrite(data, bytes, 1, fp);
  }

  count++;
  if(count % 300 == 0 && fp != NULL) {
    printf("Stop recording. Exit!\n");
    fclose(fp);
    exit(0);
  }

  return 0;
}

H264Depacketizer* h264_depacketizer_create() {

  H264Depacketizer *h264_depacketizer = NULL;
  h264_depacketizer = (H264Depacketizer*)malloc(sizeof(H264Depacketizer));
  h264_depacketizer->handler.alloc = h264_depacketizer_alloc;
  h264_depacketizer->handler.free = h264_depacketizer_free;
  h264_depacketizer->handler.packet = h264_depacketizer_packet;
  h264_depacketizer->decoder = rtp_payload_decode_create(102, "H264", &h264_depacketizer->handler, h264_depacketizer);

  return h264_depacketizer;
}

void h264_depacketizer_destroy(H264Depacketizer *h264_depacketizer) {

  if(h264_depacketizer) {
    if(h264_depacketizer->decoder)
      rtp_payload_decode_destroy(h264_depacketizer->decoder);
    free(h264_depacketizer);
    h264_depacketizer = NULL;
  }
}


void h264_depacketizer_recv(H264Depacketizer *h264_depacketizer, uint8_t *buf, size_t size) {

  rtp_payload_decode_input(h264_depacketizer->decoder, buf, size);
}


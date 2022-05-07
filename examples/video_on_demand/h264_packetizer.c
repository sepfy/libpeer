#include <stdint.h>

#include "h264_packetizer.h"

struct H264Packetizer {
  
  struct rtp_payload_t handler;
  void* encoder;
  PeerConnection *pc;

};

static void* h264_packetizer_alloc(void* param, int bytes) {

  static uint8_t buffer[2 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };
  if((sizeof(buffer) - 4) <= bytes) {
    printf("rtp package to large\n");
    exit(1);
  }
  return buffer + 4;
}

static void h264_packetizer_free(void* param, void *packet) {
}

static int h264_packetizer_packet(void *param, const void *packet, int bytes, uint32_t timestamp, int flags) {

  H264Packetizer *h264_packetizer = (H264Packetizer*)param;
  peer_connection_send_rtp_packet(h264_packetizer->pc, (uint8_t*)packet, bytes);
  return 0;
}

H264Packetizer* h264_packetizer_create(PeerConnection *pc) {

  H264Packetizer *h264_packetizer = NULL;
  h264_packetizer = (H264Packetizer*)malloc(sizeof(struct H264Packetizer));
  h264_packetizer->handler.alloc = h264_packetizer_alloc;
  h264_packetizer->handler.free = h264_packetizer_free;
  h264_packetizer->handler.packet = h264_packetizer_packet;
  h264_packetizer->encoder = rtp_payload_encode_create(102, "H264", 0, 1, &h264_packetizer->handler, h264_packetizer);
  h264_packetizer->pc = pc;

  return h264_packetizer;
}

void h264_packetizer_destroy(H264Packetizer *h264_packetizer) {

  if(h264_packetizer) {
    if(h264_packetizer->encoder)
      rtp_payload_encode_destroy(h264_packetizer->encoder);
    free(h264_packetizer);
    h264_packetizer = NULL;
  }
}

void h264_packetizer_send(H264Packetizer *h264_packetizer, uint8_t *buf, size_t size, unsigned long timestamp) {

  rtp_payload_encode_input(h264_packetizer->encoder, buf, size, timestamp);
}


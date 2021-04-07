#ifndef H264_PACKETIZER_H_
#define H264_PACKETIZER_H_

#include <stdint.h>

#include <rtp-packet.h>
#include <rtp-payload.h>

#include "pear.h"

typedef struct rtp_encode_context_t {
  struct rtp_payload_t handler;
  void* encoder;
  peer_connection_t *peer_connection;
} rtp_encode_context_t;

static void* rtp_encode_alloc(void* param, int bytes);

static void rtp_encode_free(void* param, void *packet);

static int rtp_encode_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);

struct rtp_encode_context_t* create_rtp_encode_context(peer_connection_t *peer_connection);

void rtp_encode_frame(struct rtp_encode_context_t *rtp_encode_context, uint8_t *buf, size_t size, unsigned long timestamp);

#endif // H264_PACKETIZER_H_

#ifndef RTP_DEPACKETIZER_H_
#define RTP_DEPACKETIZER_H_

#include <stdint.h>

#include <rtp-packet.h>
#include <rtp-payload.h>

#include "peer_connection.h"

typedef struct rtp_decode_context_t {

  struct rtp_payload_t handler;
  void* decoder;
  char codec[32];

} rtp_decode_context_t;

static void* rtp_decode_alloc(void* param, int bytes);

static void rtp_decode_free(void* param, void *packet);

static int rtp_decode_video_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);

static int rtp_decode_audio_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);

struct rtp_decode_context_t* create_rtp_decode_context(const char *codec);

void rtp_decode_frame(struct rtp_decode_context_t *rtp_decode_context, uint8_t *buf, size_t size);

#endif // RTP_PACKETIZER_H_

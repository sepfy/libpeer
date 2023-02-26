#ifndef MEDIA_STREAM_H_
#define MEDIA_STREAM_H_

#include <stdlib.h>
#include <stdint.h>

typedef enum MediaCodec {

  /* Video */
  CODEC_H264,
  CODEC_VP8,
  /* Audio */
  CODEC_OPUS,
  CODEC_PCMA,
  CODEC_PCMU,
  CODEC_NONE,

} MediaCodec;

typedef struct MediaStream MediaStream;

MediaStream* media_stream_create(MediaCodec codec,
 const char *outgoing_pipeline_text,
 const char *incoming_pipeline_text,
 void (*on_rtp_data)(const char *rtp_packet, size_t bytes, void *userdata), void *userdata);

int media_stream_set_payloadtype(MediaStream *media_stream, int pt);

void media_stream_set_ssrc(MediaStream *media_stream, uint64_t ssrc);

void media_stream_play(MediaStream *media_stream);
  
void media_stream_pause(MediaStream *media_stream);

void media_stream_destroy(MediaStream *media_stream);

MediaCodec media_stream_get_codec(MediaStream *media_stream);

void media_stream_ontrack(MediaStream *media_stream, uint8_t *data, size_t bytes);

#endif // MEDIA_STREAM_H_


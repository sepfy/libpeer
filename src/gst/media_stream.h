#ifndef MEDIA_STREAM_H_
#define MEDIA_STREAM_H_

#include <stdlib.h>
#include <stdint.h>
#include <gst/gst.h>

#ifdef SPEEX_AEC
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#endif

#include "buffer.h"
#include "utils.h"
#include "codec.h"

#define PIPELINE_TEXT_MAX_LENGTH 1024

typedef struct MediaStream {

  MediaCodec codec;

  char outgoing_pipeline_text[1024];
  char incoming_pipeline_text[1024];

  GstElement *outgoing_pipeline;
  GstElement *incoming_pipeline;
  GstElement *sink;
  GstElement *src;
  GstElement *rtp;

  Buffer **outgoing_rb;
  Buffer **incoming_rb;

#ifdef SPEEX_AEC
  GstElement *aec_far;
  GstElement *aec_cap;

  SpeexEchoState *echo_state;
  SpeexPreprocessState *preprocess_state;
#endif

} MediaStream;

MediaStream* media_stream_create(MediaCodec codec,
 const char *outgoing_pipeline_text,
 const char *incoming_pipeline_text);

void media_stream_set_payloadtype(MediaStream *media_stream, int pt);

void media_stream_set_ssrc(MediaStream *media_stream, uint64_t ssrc);

void media_stream_play(MediaStream *media_stream);
  
void media_stream_pause(MediaStream *media_stream);

void media_stream_destroy(MediaStream *media_stream);

MediaCodec media_stream_get_codec(MediaStream *media_stream);

void media_stream_ontrack(MediaStream *media_stream, uint8_t *data, size_t bytes);

#endif // MEDIA_STREAM_H_


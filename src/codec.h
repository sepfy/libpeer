#ifndef CODEC_H_
#define CODEC_H_

#include <stdlib.h>
#include <stdint.h>

typedef enum MediaCodec {

  CODEC_NONE = 0,

  /* Video */
  CODEC_H264,
  CODEC_VP8,
  CODEC_MJPEG,

  /* Audio */
  CODEC_OPUS,
  CODEC_PCMA,
  CODEC_PCMU,

} MediaCodec;

#endif // CODEC_H_


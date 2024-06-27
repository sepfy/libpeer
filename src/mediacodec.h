#ifndef MEDIA_CODEC
#define MEDIA_CODEC

typedef enum MediaCodec {

  CODEC_NONE = 0,

  /* Video */
  CODEC_H264,
  CODEC_VP8, // not implemented yet 
  CODEC_MJPEG, // not implemented yet

  /* Audio */
  CODEC_OPUS, // not implemented yet
  CODEC_PCMA,
  CODEC_PCMU,

} MediaCodec;

#endif
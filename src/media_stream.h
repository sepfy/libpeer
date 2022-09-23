/**
 * @file media_stream.h
 * @brief MediaStream includes audio and video 
 */

#ifndef MEDIA_STREAM_H_
#define MEDIA_STREAM_H_

typedef enum MediaCodec {

  /* Video */
  CODEC_H264,
  /* Audio */
  CODEC_OPUS,
  CODEC_PCMA,
  CODEC_NONE,

} MediaCodec;

typedef struct MediaStream MediaStream;

#endif // MEDIA_STREAM_H_

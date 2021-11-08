/**
 * @file media_stream.h
 * @brief MediaStream includes audio and video 
 */

#ifndef MEDIA_STREAM_H_
#define MEDIA_STREAM_H_

typedef enum TransceiverDirection {

  SENDRECV,
  RECVONLY,
  SENDONLY,

} TransceiverDirection;


typedef struct Transceiver {

  TransceiverDirection audio;
  TransceiverDirection video;

} Transceiver;


typedef enum MediaCodec {

  /* Video */
  CODEC_H264,
  /* Audio */
  CODEC_OPUS,
  CODEC_PCMA,
  CODEC_NONE,

} MediaCodec;


typedef struct {

  MediaCodec audio_codec;
  MediaCodec video_codec;
  int tracks_num;
  
} MediaStream;

/**
 * @brief New a MediaStream.
 * @return MediaStream
 */
MediaStream* media_stream_new();

/**
 * @brief Set codec of track to stream.
 * @param MediaStream
 * @param MediaCodec
 */
void media_stream_add_track(MediaStream* media_stream, MediaCodec codec);

/**
 * @brief Free MediaStream.
 * @param MediaStream
 */
void media_stream_free(MediaStream* media_stream);


#endif // MEDIA_STREAM_H_

#include <stdlib.h>
#include "media_stream.h"

MediaStream* media_stream_new() {

  MediaStream *media_stream = (MediaStream*)malloc(sizeof(MediaStream)); 

  if(media_stream == NULL) {
    return NULL;
  }

  media_stream->video_codec = CODEC_NONE;
  media_stream->audio_codec = CODEC_NONE;
  media_stream->tracks_num = 0;
  return media_stream;
}

void media_stream_add_track(MediaStream* media_stream, MediaCodec codec) {

  switch(codec) {
    case CODEC_H264:
      media_stream->video_codec = CODEC_H264;
      media_stream->tracks_num++;
      break;
    case CODEC_OPUS:
      media_stream->audio_codec = CODEC_OPUS;
      media_stream->tracks_num++;
      break;
    default:
      break;
  }

}


void media_stream_free(MediaStream* media_stream) {

  if(media_stream)
    free(media_stream);

  media_stream = NULL;

}


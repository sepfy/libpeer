#include <stdlib.h>
#include <stdint.h>
#include <gst/gst.h>

#include "utils.h"
#include "media_stream.h"

struct MediaStream {

  MediaCodec codec;
  char outgoing_pipeline_text[1024];
  char incoming_pipeline_text[1024];

  uint8_t rtp_packet[1400];

  GstElement *outgoing_pipeline;
  GstElement *incoming_pipeline;
  GstElement *sink;
  GstElement *src;
  GstElement *rtp;

  void *userdata;
  void (*on_rtp_data)(const char *rtp_packet, size_t bytes, void *userdata);
};

void media_stream_need_data(GstElement *src, guint size, void *data) {

}


int media_stream_new_sample(GstElement *sink, void *userdata) {

  int bytes;

  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;
  MediaStream *media_stream = (MediaStream*)userdata;

  g_signal_emit_by_name(sink, "pull-sample", &sample);

  if(sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);

    memset(media_stream->rtp_packet, 0, sizeof(media_stream->rtp_packet));
    memcpy(media_stream->rtp_packet, info.data, info.size);
    bytes = info.size;
    if(media_stream->on_rtp_data) {

      media_stream->on_rtp_data(media_stream->rtp_packet, bytes, media_stream->userdata);
    }
    gst_buffer_unmap(buffer, &info);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
  }
  return GST_FLOW_ERROR;
}


MediaStream* media_stream_create(MediaCodec codec,
 const char *outgoing_pipeline_text,
 const char *incoming_pipeline_text,
 void (*on_rtp_data)(const char *rtp_packet, size_t bytes, void *userdata), void *userdata) {

  MediaStream *media_stream = (MediaStream*)calloc(1, sizeof(MediaStream));

  if(!media_stream) {

    return NULL;
  }

  media_stream->codec = codec;
  media_stream->on_rtp_data = on_rtp_data;
  media_stream->userdata = userdata;


  if(outgoing_pipeline_text != NULL) {

    LOG_INFO("outgoing pipeline source: %s", outgoing_pipeline_text);

    switch(codec) {

      case CODEC_H264:
        snprintf(media_stream->outgoing_pipeline_text, sizeof(media_stream->outgoing_pipeline_text),
         "%s ! video/x-h264, stream-format=(string)byte-stream, level=(string)4, alighnment=(string)au ! rtph264pay name=rtp config-interval=-1 ssrc=2 ! appsink name=sink",
         outgoing_pipeline_text);
        break;
      case CODEC_OPUS:
        snprintf(media_stream->outgoing_pipeline_text, sizeof(media_stream->outgoing_pipeline_text),
         "%s ! rtpopuspay name=rtp ssrc=2 ! appsink name=sink",
         outgoing_pipeline_text);
        break;
      case CODEC_PCMA:
        snprintf(media_stream->outgoing_pipeline_text, sizeof(media_stream->outgoing_pipeline_text),
         "%s ! rtppcmapay name=rtp ssrc=3 ! appsink name=sink",
         outgoing_pipeline_text);
        break;
      case CODEC_PCMU:
        snprintf(media_stream->outgoing_pipeline_text, sizeof(media_stream->outgoing_pipeline_text),
         "%s ! rtppcmupay name=rtp ssrc=4 ! appsink name=sink",
         outgoing_pipeline_text);
        break;
      default:
        break;
    }


    LOG_DEBUG("outgoing pipeline: %s", media_stream->outgoing_pipeline_text);
    media_stream->outgoing_pipeline = gst_parse_launch(media_stream->outgoing_pipeline_text, NULL);
    media_stream->sink = gst_bin_get_by_name(GST_BIN(media_stream->outgoing_pipeline), "sink");
    media_stream->rtp = gst_bin_get_by_name(GST_BIN(media_stream->outgoing_pipeline), "rtp");

    g_signal_connect(media_stream->sink, "new-sample", G_CALLBACK(media_stream_new_sample), media_stream);

    g_object_set(media_stream->sink, "emit-signals", TRUE, NULL);
  }

  if(incoming_pipeline_text != NULL) {

    LOG_INFO("incoming pipeline source: %s", incoming_pipeline_text);

    switch(codec) {

      case CODEC_H264:
        break;
      case CODEC_OPUS:
        break;
      case CODEC_PCMA:
        snprintf(media_stream->incoming_pipeline_text, sizeof(media_stream->incoming_pipeline_text),
         "appsrc name=src format=time ! application/x-rtp,clock-rate=8000,encoding-name=PCMA ! rtppcmadepay ! %s",
         incoming_pipeline_text);
        break;
      case CODEC_PCMU:
        snprintf(media_stream->incoming_pipeline_text, sizeof(media_stream->incoming_pipeline_text),
         "appsrc name=src format=time ! application/x-rtp,clock-rate=8000,encoding-name=PCMU ! rtppcmudepay ! %s",
         incoming_pipeline_text);
        break;
      default:
        break;
    }

    LOG_DEBUG("incoming pipeline: %s", media_stream->incoming_pipeline_text);

    media_stream->incoming_pipeline = gst_parse_launch(media_stream->incoming_pipeline_text, NULL);
    media_stream->src = gst_bin_get_by_name(GST_BIN(media_stream->incoming_pipeline), "src");
    g_signal_connect(media_stream->src, "need-data", G_CALLBACK(media_stream_need_data), media_stream);
    g_object_set(media_stream->src, "emit-signals", TRUE, NULL);
  }

  return media_stream;
}

int media_stream_set_payloadtype(MediaStream *media_stream, int pt) {

  if(media_stream) 
    g_object_set(media_stream->rtp, "pt", pt, NULL);

  return 0;
}

void media_stream_set_ssrc(MediaStream *media_stream, uint64_t ssrc) {

  if(media_stream) 
    g_object_set(media_stream->rtp, "ssrc", ssrc, NULL);
}

void media_stream_play(MediaStream *media_stream) {

  if(media_stream->outgoing_pipeline)
    gst_element_set_state(media_stream->outgoing_pipeline, GST_STATE_PLAYING);

  if(media_stream->incoming_pipeline)
    gst_element_set_state(media_stream->incoming_pipeline, GST_STATE_PLAYING);
}

void media_stream_pause(MediaStream *media_stream) {

  if(media_stream->outgoing_pipeline)
    gst_element_set_state(media_stream->outgoing_pipeline, GST_STATE_NULL);

  if(media_stream->incoming_pipeline)
    gst_element_set_state(media_stream->incoming_pipeline, GST_STATE_NULL);
}

void media_stream_destroy(MediaStream *media_stream) {

  if(media_stream) {

    media_stream_pause(media_stream);

    gst_object_unref(media_stream->sink);
    gst_object_unref(media_stream->src);
    gst_object_unref(media_stream->incoming_pipeline);
    gst_object_unref(media_stream->outgoing_pipeline);

    free(media_stream);
    media_stream = NULL;
  }
}

MediaCodec media_stream_get_codec(MediaStream *media_stream) {

  return media_stream->codec;
}

void media_stream_ontrack(MediaStream *media_stream, uint8_t *data, size_t bytes) {

  GstBuffer *buf;
  GstMapInfo info;
  GstFlowReturn ret;

  if(!media_stream->incoming_pipeline) {

    return;
  }

  buf = gst_buffer_new_and_alloc(bytes);

  gst_buffer_map(buf, &info, GST_MAP_WRITE);

  memcpy(info.data, data, bytes);

  info.size = bytes;

  g_signal_emit_by_name(media_stream->src, "push-buffer", buf, &ret);

  gst_buffer_unmap(buf, &info);
  gst_buffer_unref(buf);

}


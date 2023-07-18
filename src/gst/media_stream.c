#include <stdlib.h>
#include <stdint.h>

#include "media_stream.h"

#ifdef SPEEX_AEC
#define FRAME_SIZE 48000*20/1000 

typedef struct FarData {

  uint8_t *buf;
  int size;
  uint64_t ts;

} FarData;

GQueue* queue;

unsigned long long getms() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec*1.0e+3 + tv.tv_usec/1000;
}

#endif

int media_stream_new_sample(GstElement *sink, void *userdata) {

  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;
  MediaStream *ms = (MediaStream*)userdata;

  uint16_t size = 0;
  g_signal_emit_by_name(sink, "pull-sample", &sample);

  if (sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);
    size = info.size;

    if (utils_buffer_push(ms->outgoing_rb[1], info.data, size) == size) {

      utils_buffer_push(ms->outgoing_rb[0], (uint8_t*)&size, sizeof(size));
    }

    gst_buffer_unmap(buffer, &info);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
  }
  return GST_FLOW_ERROR;
}

void media_stream_on_aec_far(GstElement *identity, GstBuffer *buf, gpointer udata) {

  MediaStream *ms = (MediaStream*)udata;

  GstMapInfo info;

  gst_buffer_map(buf, &info, GST_MAP_READWRITE);


#ifdef SPEEX_AEC

  LOGI("%lld: aec_far: %d", getms(), info.size);

  FarData *far_data = (FarData*)calloc(1, sizeof(FarData));

  far_data->buf = (uint8_t*)calloc(1, info.size);

  memcpy(far_data->buf, info.data, info.size);

  far_data->size = info.size;

  far_data->ts = getms();

  g_queue_push_tail(queue, far_data);

#endif

#ifdef AEC_DUMPS

  static FILE *fp = NULL;
  if (fp == NULL) {
    fp = fopen("far.raw", "wb");
  }

  if(fp) {

    fwrite(info.data, info.size, 1, fp);
  }
#endif

  gst_buffer_unmap(buf, &info);

}

void media_stream_on_aec_cap(GstElement *identity, GstBuffer *buf, gpointer udata) {

  MediaStream *ms = (MediaStream*)udata;

  GstMapInfo info;

  gst_buffer_map(buf, &info, GST_MAP_READWRITE);

#ifdef SPEEX_AEC

  uint8_t output_frame[FRAME_SIZE*4];

  memset(output_frame, 0, sizeof(output_frame));

  LOGI("%lld: aec_cap: %d", getms(), info.size);

  FarData *far_data = NULL;

  uint64_t curr_ts = getms();

  while ((far_data = g_queue_peek_head(queue)) != NULL) {

    if ((curr_ts - far_data->ts) < 300) {
      // pass
      LOGI("pass");
      break;
 
    } else if ((curr_ts - far_data->ts) > 330) {
      // drop
      LOGI("drop");
      g_queue_pop_head(queue);
    }
    else {


      LOGI("echo-> playback time: %lld, current time: %lld, timediff: %lld", far_data->ts, curr_ts, curr_ts - far_data->ts);

      speex_echo_cancellation(ms->echo_state, (spx_int16_t*)info.data, (spx_int16_t*)far_data->buf, (spx_int16_t*)output_frame);

      speex_preprocess_run(ms->preprocess_state, (spx_int16_t*)output_frame);

      memcpy(info.data, output_frame, info.size);

      g_queue_pop_head(queue);

      break;
    }

  }
#endif

#ifdef AEC_DUMPS
  static FILE *fp = NULL;
  if (fp == NULL) {
    fp = fopen("rec.raw", "wb");
  }

  if(fp) {

    fwrite(info.data, info.size, 1, fp);
  }
#endif

  gst_buffer_unmap(buf, &info);
}

MediaStream* media_stream_create(MediaCodec codec,
 const char *outgoing_pipeline_text,
 const char *incoming_pipeline_text) {

  MediaStream *media_stream = (MediaStream*)calloc(1, sizeof(MediaStream));

  if (!media_stream) {

    return NULL;
  }

  media_stream->codec = codec;

#ifdef SPEEX_AEC

  int rate = 48000;

  media_stream->echo_state = speex_echo_state_init_mc(FRAME_SIZE, FRAME_SIZE*5, 1, 1);

  media_stream->preprocess_state = speex_preprocess_state_init(FRAME_SIZE, rate);

  speex_echo_ctl(media_stream->echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &rate);

  speex_preprocess_ctl(media_stream->preprocess_state, SPEEX_PREPROCESS_SET_ECHO_STATE, media_stream->echo_state);

  queue = g_queue_new();
#endif

  if (outgoing_pipeline_text != NULL) {

    LOGI("outgoing pipeline source: %s", outgoing_pipeline_text);

    switch(codec) {

      case CODEC_H264:
        snprintf(media_stream->outgoing_pipeline_text, sizeof(media_stream->outgoing_pipeline_text),
         "%s ! video/x-h264, stream-format=(string)byte-stream, alighnment=(string)au ! rtph264pay name=rtp config-interval=-1 ssrc=1 ! appsink name=sink",
         outgoing_pipeline_text);
        break;
      case CODEC_VP8:
        snprintf(media_stream->outgoing_pipeline_text, sizeof(media_stream->outgoing_pipeline_text),
         "%s ! video/x-vp8 ! rtpvp8pay name=rtp ssrc=2 ! appsink name=sink",
         outgoing_pipeline_text);
        break;
      case CODEC_OPUS:
        snprintf(media_stream->outgoing_pipeline_text, sizeof(media_stream->outgoing_pipeline_text),
         "%s ! rtpopuspay name=rtp ssrc=3 ! appsink name=sink",
         outgoing_pipeline_text);
        break;
      case CODEC_PCMA:
        snprintf(media_stream->outgoing_pipeline_text, sizeof(media_stream->outgoing_pipeline_text),
         "%s ! rtppcmapay name=rtp ssrc=4 ! appsink name=sink",
         outgoing_pipeline_text);
        break;
      case CODEC_PCMU:
        snprintf(media_stream->outgoing_pipeline_text, sizeof(media_stream->outgoing_pipeline_text),
         "%s ! rtppcmupay name=rtp ssrc=5 ! appsink name=sink",
         outgoing_pipeline_text);
        break;
      default:
        break;
    }

    LOGD("outgoing pipeline: %s", media_stream->outgoing_pipeline_text);
    media_stream->outgoing_pipeline = gst_parse_launch(media_stream->outgoing_pipeline_text, NULL);
    media_stream->sink = gst_bin_get_by_name(GST_BIN(media_stream->outgoing_pipeline), "sink");
    media_stream->rtp = gst_bin_get_by_name(GST_BIN(media_stream->outgoing_pipeline), "rtp");

    g_signal_connect(media_stream->sink, "new-sample", G_CALLBACK(media_stream_new_sample), media_stream);

    g_object_set(media_stream->sink, "emit-signals", TRUE, NULL);

#ifdef SPEEX_AEC
    media_stream->aec_cap = gst_bin_get_by_name(GST_BIN(media_stream->outgoing_pipeline), "aec-cap");
    if (media_stream->aec_cap) {

      g_signal_connect(media_stream->aec_cap, "handoff",
       G_CALLBACK(media_stream_on_aec_cap), media_stream);
    }
#endif
  }

  if (incoming_pipeline_text != NULL) {

    LOGI("incoming pipeline source: %s", incoming_pipeline_text);

    switch(codec) {

      case CODEC_H264:
        snprintf(media_stream->incoming_pipeline_text, sizeof(media_stream->incoming_pipeline_text),
         "appsrc name=src format=time ! application/x-rtp,clock-rate=90000,encoding-name=H264 ! rtph264depay ! %s",
         incoming_pipeline_text);
        break;
      case CODEC_VP8:
        snprintf(media_stream->incoming_pipeline_text, sizeof(media_stream->incoming_pipeline_text),
         "appsrc name=src format=time ! application/x-rtp,clock-rate=90000,encoding-name=VP8 ! rtpvp8depay ! %s",
         incoming_pipeline_text);
        break;
      case CODEC_OPUS:
        snprintf(media_stream->incoming_pipeline_text, sizeof(media_stream->incoming_pipeline_text),
         "appsrc name=src format=time ! application/x-rtp,clock-rate=48000,encoding-name=OPUS,payload=111 ! rtpopusdepay ! %s",
         incoming_pipeline_text);
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

    LOGD("incoming pipeline: %s", media_stream->incoming_pipeline_text);

    media_stream->incoming_pipeline = gst_parse_launch(media_stream->incoming_pipeline_text, NULL);
    media_stream->src = gst_bin_get_by_name(GST_BIN(media_stream->incoming_pipeline), "src");
    //g_signal_connect(media_stream->src, "need-data", G_CALLBACK(media_stream_need_data), media_stream);
    g_object_set(media_stream->src, "emit-signals", TRUE, NULL);

#ifdef SPEEX_AEC
    media_stream->aec_far = gst_bin_get_by_name(GST_BIN(media_stream->incoming_pipeline), "aec-far");
    if (media_stream->aec_far) {

      g_signal_connect(media_stream->aec_far, "handoff",
       G_CALLBACK(media_stream_on_aec_far), media_stream);
    }
#endif

  }

  return media_stream;
}

void media_stream_set_payloadtype(MediaStream *media_stream, int pt) {

  if (media_stream)
    g_object_set(media_stream->rtp, "pt", pt, NULL);
}

void media_stream_set_ssrc(MediaStream *media_stream, uint64_t ssrc) {

  if (media_stream)
    g_object_set(media_stream->rtp, "ssrc", ssrc, NULL);
}

void media_stream_play(MediaStream *media_stream) {

  if (media_stream->outgoing_pipeline)
    gst_element_set_state(media_stream->outgoing_pipeline, GST_STATE_PLAYING);

  if (media_stream->incoming_pipeline)
    gst_element_set_state(media_stream->incoming_pipeline, GST_STATE_PLAYING);
}

void media_stream_pause(MediaStream *media_stream) {

  if (media_stream->outgoing_pipeline)
    gst_element_set_state(media_stream->outgoing_pipeline, GST_STATE_NULL);

  if (media_stream->incoming_pipeline)
    gst_element_set_state(media_stream->incoming_pipeline, GST_STATE_NULL);
}

void media_stream_destroy(MediaStream *media_stream) {

  if(media_stream) {

    media_stream_pause(media_stream);


    if(media_stream->incoming_pipeline) {

      gst_object_unref(media_stream->src);
      gst_object_unref(media_stream->incoming_pipeline);
    }

    if(media_stream->outgoing_pipeline) {

      gst_object_unref(media_stream->outgoing_pipeline);
      gst_object_unref(media_stream->sink);
    }

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
  GstFlowReturn *ret;

#ifdef SPEEX_AEC
  static uint64_t last_ts = 0;
  uint64_t curr_ts = getms();

  if ((curr_ts - last_ts) < 18) {

    return;
  }
#endif

  if (media_stream->incoming_pipeline) {

    buf = gst_buffer_new_and_alloc(bytes);

    gst_buffer_map(buf, &info, GST_MAP_WRITE);

    memcpy(info.data, data, bytes);

    info.size = bytes;

    g_signal_emit_by_name(media_stream->src, "push-buffer", buf, &ret);

    gst_buffer_unmap(buf, &info);

    gst_buffer_unref(buf);
  }
}


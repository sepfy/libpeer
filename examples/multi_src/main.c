#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gst/gst.h>

#include "signal_service.h"
#include "utils.h"
#include "rtp_depacketizer.h"
#include "peer_connection.h"

#define MTU 1400

GstElement *audio_element;
GstElement *video_element;

GAsyncQueue *g_async_queue = NULL;

char *g_sdp = NULL;
static GCond g_cond;
static GMutex g_mutex;

PeerConnection *g_peer_connection = NULL;

typedef struct RtpDepacketizer {

  rtp_decode_context_t *rtp_decode_context_audio;
  rtp_decode_context_t *rtp_decode_context_video;

} RtpDepacketizer;

RtpDepacketizer g_rtp_depacketizer;

const char VIDEO_PIPELINE[] = "v4l2src device=/dev/video0 ! videorate ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! queue ! x264enc bitrate=6000 speed-preset=ultrafast tune=zerolatency key-int-max=15 ! video/x-h264,profile=constrained-baseline ! queue ! h264parse ! queue ! rtph264pay config-interval=-1 pt=102 seqnum-offset=0 timestamp-offset=0 mtu=1400 ! appsink name=video-app-sink";

//const char AUDIO_PIPELINE[] = "alsasrc device=hw:1 ! opusenc ! rtpopuspay pt=111 ssrc=12 ! appsink name=audio-app-sink";
const char AUDIO_PIPELINE[] = "alsasrc device=hw:1 ! audioconvert ! audioresample ! alawenc ! rtppcmapay pt=8 ssrc=12 ! appsink name=audio-app-sink";

typedef struct GstRtpPacket {

  uint8_t data[MTU];
  size_t size;
 
} GstRtpPacket;

static void on_iceconnectionstatechange(IceConnectionState state, void *data) {
  if(state == FAILED) {
    LOG_INFO("Disconnect with browser... Stop streaming");
    gst_element_set_state(audio_element, GST_STATE_PAUSED);
    gst_element_set_state(video_element, GST_STATE_PAUSED);
  }
}

static void on_icecandidate(char *sdp, void *data) {

  if(g_sdp)
    g_free(g_sdp);

  g_sdp = g_base64_encode((const char *)sdp, strlen(sdp));
  g_cond_signal(&g_cond);
}

static GstFlowReturn new_sample(GstElement *sink, void *data) {

  //if(strcmp(gst_element_get_name(sink), "audio-app-sink") == 0)
  //  return GST_FLOW_OK;

  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;

  g_signal_emit_by_name (sink, "pull-sample", &sample);

  if(sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);

    GstRtpPacket *gst_rtp_packet = g_new(GstRtpPacket, 1);
    gst_rtp_packet->size = info.size;

    memset(gst_rtp_packet->data, 0, MTU);
    memcpy(gst_rtp_packet->data, info.data, info.size);

    g_async_queue_lock(g_async_queue);
    g_async_queue_push_unlocked(g_async_queue, gst_rtp_packet); 
    g_async_queue_unlock(g_async_queue);

    gst_sample_unref(sample);
    return GST_FLOW_OK;
  }
  return GST_FLOW_ERROR;
}

void* srtp_transport_thread(void *data) {

  while(1) {

    g_async_queue_lock(g_async_queue);
    GstRtpPacket *gst_rtp_packet = g_async_queue_pop_unlocked(g_async_queue);
    g_async_queue_unlock(g_async_queue);

    peer_connection_send_rtp_packet(g_peer_connection, gst_rtp_packet->data, gst_rtp_packet->size);
    if(gst_rtp_packet) {
      g_free(gst_rtp_packet);
      gst_rtp_packet = NULL;
    }

  }

  pthread_exit(NULL);

}

static void on_transport_ready(void *data) {

  gst_element_set_state(audio_element, GST_STATE_PLAYING);
  gst_element_set_state(video_element, GST_STATE_PLAYING);
}

void on_track(uint8_t *packet, size_t bytes, void *data) {

  RtpDepacketizer *rtp_depacketizer = (RtpDepacketizer*)data;

  uint32_t ssrc = (packet[8] << 24) | (packet[9] << 16) | (packet[10] << 8) | (packet[11]);

  if(ssrc == peer_connection_get_ssrc(g_peer_connection, "audio")) {
    rtp_decode_frame(rtp_depacketizer->rtp_decode_context_audio, packet, bytes); 
  }
  else if(ssrc == peer_connection_get_ssrc(g_peer_connection, "video")) {

    static int video_count = 0;
    video_count++;
    if(video_count % 100 == 0) {
      uint32_t rtcp_ssrc;
      memcpy(&rtcp_ssrc, packet + 8, 4);
      peer_connection_send_rtcp_pil(g_peer_connection, rtcp_ssrc);
    }
    rtp_decode_frame(rtp_depacketizer->rtp_decode_context_video, packet, bytes); 
  }

}

char* on_offer_get_cb(char *offer, void *data) {

  gst_element_set_state(audio_element, GST_STATE_PAUSED);
  gst_element_set_state(video_element, GST_STATE_PAUSED);

  g_mutex_lock(&g_mutex);
  peer_connection_destroy(g_peer_connection);
  g_peer_connection = peer_connection_create();

  MediaStream *media_stream = media_stream_new();
  media_stream_add_track(media_stream, CODEC_H264);
  media_stream_add_track(media_stream, CODEC_PCMA);
  peer_connection_add_stream(g_peer_connection, media_stream);

  Transceiver transceiver = {.video = SENDRECV, .audio = SENDRECV};
  peer_connection_add_transceiver(g_peer_connection, transceiver);

  peer_connection_ontrack(g_peer_connection, on_track, &g_rtp_depacketizer);
  peer_connection_onicecandidate(g_peer_connection, on_icecandidate, NULL);
  peer_connection_set_on_transport_ready(g_peer_connection, &on_transport_ready, NULL);
  peer_connection_oniceconnectionstatechange(g_peer_connection, &on_iceconnectionstatechange, NULL);
  peer_connection_create_answer(g_peer_connection);

  g_cond_wait(&g_cond, &g_mutex);
  peer_connection_set_remote_description(g_peer_connection, offer);
  g_mutex_unlock(&g_mutex);

  return g_sdp;
}

static void print_usage(const char *prog) {

  printf("Usage: %s \n"
   " -p      - port (default: 8080)\n"
   " -H      - address to bind (default: 0.0.0.0)\n"
   " -r      - document root\n"
   " -h      - print help\n", prog);

}

void parse_argv(int argc, char **argv, options_t *options) {

  int opt;

  while((opt = getopt(argc, argv, "p:H:r:h")) != -1) {
    switch(opt) {
      case 'p':
        options->port = atoi(optarg);
        break;
      case 'H':
        options->host = optarg;
        break;
      case 'r':
        options->root = optarg;
        break;
      case 'h':
        print_usage(argv[0]);
        exit(1);
        break;
      default :
        printf("Unknown option %c\n", opt);
        break;
    }
  }

}

int main(int argc, char **argv) {

  signal_service_t signal_service;
  options_t options = {8000, "0.0.0.0", "root"};
  parse_argv(argc, argv, &options);

  GstElement *audio_sink, *video_sink;

  gst_init(&argc, &argv);

  audio_element = gst_parse_launch(AUDIO_PIPELINE, NULL);
  video_element = gst_parse_launch(VIDEO_PIPELINE, NULL);

  audio_sink = gst_bin_get_by_name(GST_BIN(audio_element), "audio-app-sink");
  video_sink = gst_bin_get_by_name(GST_BIN(video_element), "video-app-sink");

  g_signal_connect(audio_sink, "new-sample", G_CALLBACK(new_sample), NULL);
  g_signal_connect(video_sink, "new-sample", G_CALLBACK(new_sample), NULL);

  g_object_set(audio_sink, "emit-signals", TRUE, NULL);
  g_object_set(video_sink, "emit-signals", TRUE, NULL);


  g_async_queue = g_async_queue_new();
  pthread_t tid;
  pthread_create(&tid, NULL, srtp_transport_thread, NULL);

  g_rtp_depacketizer.rtp_decode_context_audio = create_rtp_decode_context("PCMA");
  g_rtp_depacketizer.rtp_decode_context_video = create_rtp_decode_context("H264");

  if(signal_service_create(&signal_service, options)) {
    exit(1);
  }

  signal_service_on_offer_get(&signal_service, &on_offer_get_cb, NULL);
  signal_service_dispatch(&signal_service);

  gst_element_set_state(audio_element, GST_STATE_NULL);
  gst_element_set_state(video_element, GST_STATE_NULL);

  gst_object_unref(audio_element);
  gst_object_unref(video_element);

  return 0;
}

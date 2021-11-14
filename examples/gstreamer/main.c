#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gst/gst.h>

#include "index_html.h"
#include "utils.h"
#include "peer_connection.h"
#include "signaling.h"

#define MTU 1400

GstElement *gst_element;
char *g_sdp = NULL;
static GCond g_cond;
static GMutex g_mutex;
PeerConnection *g_peer_connection = NULL;

const char PIPE_LINE[] = "v4l2src ! videorate ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! queue ! x264enc bitrate=6000 speed-preset=ultrafast tune=zerolatency key-int-max=15 ! video/x-h264,profile=constrained-baseline ! queue ! h264parse ! queue ! rtph264pay config-interval=-1 pt=102 seqnum-offset=0 timestamp-offset=0 mtu=1400 ! appsink name=pear-sink";

static void on_iceconnectionstatechange(IceConnectionState state, void *data) {
  if(state == FAILED) {
    LOG_INFO("Disconnect with browser... Stop streaming");
    gst_element_set_state(gst_element, GST_STATE_PAUSED);
  }
}

static void on_icecandidate(char *sdp, void *data) {

  if(g_sdp)
    g_free(g_sdp);

  g_sdp = g_base64_encode((const char *)sdp, strlen(sdp));
  g_cond_signal(&g_cond);
}

static void on_transport_ready(void *data) {

  gst_element_set_state(gst_element, GST_STATE_PLAYING);
}

void on_offer_get_cb(SignalingEvent event, char *msg, void *data) {

#if 0
  gst_element_set_state(gst_element, GST_STATE_PAUSED);

  g_mutex_lock(&g_mutex);
  peer_connection_destroy(g_peer_connection);
  g_peer_connection = peer_connection_create();

  MediaStream *media_stream = media_stream_new();
  media_stream_add_track(media_stream, CODEC_H264);

  peer_connection_add_stream(g_peer_connection, media_stream);

  peer_connection_onicecandidate(g_peer_connection, on_icecandidate, NULL);
  peer_connection_oniceconnectionstatechange(g_peer_connection, &on_iceconnectionstatechange, NULL);
  peer_connection_set_on_transport_ready(g_peer_connection, &on_transport_ready, NULL);
  peer_connection_create_answer(g_peer_connection);

  g_cond_wait(&g_cond, &g_mutex);
  peer_connection_set_remote_description(g_peer_connection, offer);
  g_mutex_unlock(&g_mutex);
  return g_sdp;
#endif
}

static GstFlowReturn new_sample(GstElement *sink, void *data) {

  static uint8_t rtp_packet[MTU] = {0};
  int bytes;

  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;

  g_signal_emit_by_name (sink, "pull-sample", &sample);
  if(sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);

    memset(rtp_packet, 0, sizeof(rtp_packet));
    memcpy(rtp_packet, info.data, info.size);
    bytes = info.size;

    peer_connection_send_rtp_packet(g_peer_connection, rtp_packet, bytes);

    gst_sample_unref(sample);
    return GST_FLOW_OK;
  }
  return GST_FLOW_ERROR;
}

int main(int argc, char **argv) {

  SignalingOption signaling_option = {SIGNALING_PROTOCOL_HTTP, "0.0.0.0", "demo", 8000, index_html};
  Signaling *signaling = signaling_create_service(signaling_option);

  if(!signaling) {
    printf("Create signaling service failed\n");
    return 0;
  }

  signaling_on_channel_event(signaling, &on_offer_get_cb, NULL);

  GstElement *pear_sink;

  gst_init(&argc, &argv);

  gst_element = gst_parse_launch(PIPE_LINE, NULL);
  pear_sink = gst_bin_get_by_name(GST_BIN(gst_element), "pear-sink");
  g_signal_connect(pear_sink, "new-sample", G_CALLBACK(new_sample), NULL);
  g_object_set(pear_sink, "emit-signals", TRUE, NULL);

  signaling_dispatch(signaling);


  signaling_destroy(signaling);

  gst_element_set_state(gst_element, GST_STATE_NULL);
  gst_object_unref(gst_element);

  return 0;
}

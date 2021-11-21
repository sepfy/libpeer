#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gst/gst.h>

#include "index_html.h"
#include "peer_connection.h"
#include "signaling.h"

#define MTU 1400

const char PIPE_LINE[] = "v4l2src ! videorate ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! queue ! x264enc bitrate=6000 speed-preset=ultrafast tune=zerolatency key-int-max=15 ! video/x-h264,profile=constrained-baseline ! queue ! h264parse ! queue ! rtph264pay config-interval=-1 pt=102 seqnum-offset=0 timestamp-offset=0 mtu=1400 ! appsink name=peer-connection-sink";

typedef struct CameraSend {

  GstElement *pipeline;
  GstElement *sink;

  GCond cond;
  GMutex mutex;

  PeerConnection *pc;
  Signaling *signaling;

} CameraSend;

CameraSend g_camera_send = {0};


static void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  if(state == FAILED) {
    printf("Disconnect with browser... Stop streaming");
    gst_element_set_state(g_camera_send.pipeline, GST_STATE_PAUSED);
  }
}

static void on_icecandidate(char *sdp, void *data) {

  signaling_send_answer_to_channel(g_camera_send.signaling, sdp);
  g_cond_signal(&g_camera_send.cond);
}

static void on_transport_ready(void *data) {

  gst_element_set_state(g_camera_send.pipeline, GST_STATE_PLAYING);
}

void on_channel_event(SignalingEvent signaling_event, char *msg, void *data) {

  if(signaling_event == SIGNALING_EVENT_GET_OFFER) {

    printf("Get offer from singaling\n");
    g_mutex_lock(&g_camera_send.mutex);
    gst_element_set_state(g_camera_send.pipeline, GST_STATE_PAUSED);

    peer_connection_destroy(g_camera_send.pc);

    g_camera_send.pc = peer_connection_create();

    MediaStream *media_stream = media_stream_new();
    media_stream_add_track(media_stream, CODEC_H264);

    peer_connection_add_stream(g_camera_send.pc, media_stream);

    peer_connection_onicecandidate(g_camera_send.pc, on_icecandidate, NULL);
    peer_connection_oniceconnectionstatechange(g_camera_send.pc, &on_iceconnectionstatechange, NULL);
    peer_connection_set_on_transport_ready(g_camera_send.pc, &on_transport_ready, NULL);
    peer_connection_create_answer(g_camera_send.pc);

    g_cond_wait(&g_camera_send.cond, &g_camera_send.mutex);
    peer_connection_set_remote_description(g_camera_send.pc, msg);
    g_mutex_unlock(&g_camera_send.mutex);
  }
}

static GstFlowReturn new_sample(GstElement *sink, void *data) {

  static uint8_t rtp_packet[MTU] = {0};
  int bytes;

  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;

  g_signal_emit_by_name(sink, "pull-sample", &sample);

  if(sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);

    memset(rtp_packet, 0, sizeof(rtp_packet));
    memcpy(rtp_packet, info.data, info.size);
    bytes = info.size;

    peer_connection_send_rtp_packet(g_camera_send.pc, rtp_packet, bytes);

    gst_buffer_unmap(buffer, &info);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
  }
  return GST_FLOW_ERROR;
}

void signal_handler(int signal) {

  gst_element_set_state(g_camera_send.pipeline, GST_STATE_NULL);
  gst_object_unref(g_camera_send.sink);
  gst_object_unref(g_camera_send.pipeline);
 
  if(g_camera_send.signaling)
    signaling_destroy(g_camera_send.signaling);

  if(g_camera_send.pc)
    peer_connection_destroy(g_camera_send.pc);

  exit(0);
}

int main(int argc, char *argv[]) {

  gst_init(&argc, &argv);
  signal(SIGINT, signal_handler);

  SignalingOption signaling_option = {SIGNALING_PROTOCOL_HTTP, "0.0.0.0", "demo", 8000, index_html};

  g_camera_send.signaling = signaling_create(signaling_option);
  if(!g_camera_send.signaling) {
    printf("Create signaling service failed\n");
    return 0;
  }

  signaling_on_channel_event(g_camera_send.signaling, &on_channel_event, NULL);

  g_camera_send.pipeline = gst_parse_launch(PIPE_LINE, NULL);
  g_camera_send.sink = gst_bin_get_by_name(GST_BIN(g_camera_send.pipeline), "peer-connection-sink");
  g_signal_connect(g_camera_send.sink, "new-sample", G_CALLBACK(new_sample), NULL);
  g_object_set(g_camera_send.sink, "emit-signals", TRUE, NULL);

  signaling_dispatch(g_camera_send.signaling);

  return 0;
}

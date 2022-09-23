#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gst/gst.h>

#include "index_html.h"
#include "signaling.h"
#include "peer_connection.h"

const char VIDEO_SRC_PIPELINE[] = "appsrc name=video-app-src format=time ! application/x-rtp,clock-rate=90000,encoding-name=H264 ! rtph264depay ! h264parse config-interval=-1 ! v4l2h264dec ! kmssink";

typedef struct Screencast {

  GstElement *video_src_pipeline;

  GstElement *video_src;

  GCond cond;
  GMutex mutex;

  PeerConnection *pc;
  Signaling *signaling;

} Screencast;

Screencast g_screencast = {0};

static void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  if(state == FAILED) {
    gst_element_set_state(g_screencast.video_src_pipeline, GST_STATE_PAUSED);
    printf("Disconnect with browser.");
  }
}

static void on_icecandidate(char *sdp, void *data) {

  signaling_send_answer_to_call(g_screencast.signaling, sdp);
  g_cond_signal(&g_screencast.cond);
}

static void on_connected(void *data) {

  gst_element_set_state(g_screencast.video_src_pipeline, GST_STATE_PLAYING);
}

static void need_data(GstElement *src, guint size, void *data) {
 // printf("need data\n");
}

void on_track(uint8_t *packet, size_t bytes, void *data) {

  GstBuffer *buf;
  GstMapInfo info;

  GstFlowReturn *ret;
  buf = gst_buffer_new_and_alloc(bytes);

  gst_buffer_map(buf, &info, GST_MAP_WRITE);

  memcpy(info.data, packet, bytes);
  info.size = bytes;

  g_signal_emit_by_name(g_screencast.video_src, "push-buffer", buf, &ret);

  gst_buffer_unmap(buf, &info);
  gst_buffer_unref(buf);

}

void on_call_event(SignalingEvent signaling_event, char *msg, void *data) {

  if(signaling_event == SIGNALING_EVENT_GET_OFFER) {

    printf("Get offer from singaling\n");
    g_mutex_lock(&g_screencast.mutex);

    if(g_screencast.pc)
      peer_connection_destroy(g_screencast.pc);

    g_screencast.pc = peer_connection_create(NULL);

    peer_connection_ontrack(g_screencast.pc, on_track);
    peer_connection_onicecandidate(g_screencast.pc, on_icecandidate);
    peer_connection_oniceconnectionstatechange(g_screencast.pc, on_iceconnectionstatechange);
    peer_connection_on_connected(g_screencast.pc, on_connected);
    peer_connection_set_remote_description(g_screencast.pc, msg);
    peer_connection_create_answer(g_screencast.pc);

    g_cond_wait(&g_screencast.cond, &g_screencast.mutex);
    g_mutex_unlock(&g_screencast.mutex);
  }

}


void signal_handler(int signal) {

  if(g_screencast.signaling)
    signaling_destroy(g_screencast.signaling);

  if(g_screencast.pc)
    peer_connection_destroy(g_screencast.pc);

  exit(0);
}

int main(int argc, char *argv[]) {

  signal(SIGINT, signal_handler);

  gst_init(&argc, &argv);

  g_screencast.video_src_pipeline = gst_parse_launch(VIDEO_SRC_PIPELINE, NULL);

  g_screencast.video_src = gst_bin_get_by_name(GST_BIN(g_screencast.video_src_pipeline), "video-app-src");

  g_signal_connect(g_screencast.video_src, "need-data", G_CALLBACK(need_data), NULL);

  g_object_set(g_screencast.video_src, "emit-signals", TRUE, NULL);

  SignalingOption signaling_option = {SIGNALING_PROTOCOL_HTTP, "0.0.0.0", "demo", 8000, index_html};

  g_screencast.signaling = signaling_create(signaling_option);
  if(!g_screencast.signaling) {
    printf("Create signaling service failed\n");
    return 0;
  }

  signaling_on_call_event(g_screencast.signaling, &on_call_event, NULL);
  signaling_dispatch(g_screencast.signaling);

  return 0;
}


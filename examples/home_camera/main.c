#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gst/gst.h>

#include "index_html.h"
#include "peer_connection.h"
#include "signaling.h"

const char VIDEO_SINK_PIPELINE[] = "libcamerasrc ! video/x-raw, format=NV12, width=1280, height=960, framerate=30/1, interlace-mode=progressive, colorimetry=bt709 ! v4l2h264enc capture-io-mode=4 output-io-mode=4";

const char AUDIO_SINK_PIPELINE[] = "audiotestsrc ! alawenc";

const char AUDIO_SRC_PIPELINE[] = "alawdec ! audioresample ! audioconvert ! alsasink device=hdmi:vc4hdmi0,0";

typedef struct HomeCamera {

  GCond cond;

  GMutex mutex;

  Signaling *signaling;

  PeerConnection *pc;

} HomeCamera;

HomeCamera g_home_camera;

static void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  if(state == FAILED) {

    LOG_INFO("Disconnect with browser.");
  }
}

static void on_icecandidate(char *sdp, void *data) {

  signaling_send_answer_to_call(g_home_camera.signaling, sdp);
  g_cond_signal(&g_home_camera.cond);
}

void on_call_event(SignalingEvent signaling_event, char *msg, void *data) {

  if(signaling_event == SIGNALING_EVENT_GET_OFFER) {

    g_mutex_lock(&g_home_camera.mutex);

    if(g_home_camera.pc)
      peer_connection_destroy(g_home_camera.pc);

    g_home_camera.pc = peer_connection_create(NULL);

    peer_connection_add_stream(g_home_camera.pc, CODEC_H264, VIDEO_SINK_PIPELINE, NULL);

    peer_connection_add_stream(g_home_camera.pc, CODEC_PCMA, AUDIO_SINK_PIPELINE, AUDIO_SRC_PIPELINE);

    peer_connection_onicecandidate(g_home_camera.pc, on_icecandidate);

    peer_connection_oniceconnectionstatechange(g_home_camera.pc, on_iceconnectionstatechange);

    peer_connection_set_remote_description(g_home_camera.pc, msg);

    peer_connection_create_answer(g_home_camera.pc);

    g_cond_wait(&g_home_camera.cond, &g_home_camera.mutex);

    g_mutex_unlock(&g_home_camera.mutex);
  }
}


void signal_handler(int signal) {

  if(g_home_camera.signaling)
    signaling_destroy(g_home_camera.signaling);

  if(g_home_camera.pc)
    peer_connection_destroy(g_home_camera.pc);

  exit(0);
}


int main(int argc, char **argv) {

  signal(SIGINT, signal_handler);

  SignalingOption signaling_option = {SIGNALING_PROTOCOL_HTTP, "0.0.0.0", "demo", 8000, index_html};

  g_home_camera.signaling = signaling_create(signaling_option);

  if(!g_home_camera.signaling) {

    printf("Create signaling service failed\n");
    return 0;
  }

  signaling_on_call_event(g_home_camera.signaling, &on_call_event, NULL);

  signaling_dispatch(g_home_camera.signaling);

  return 0;
}

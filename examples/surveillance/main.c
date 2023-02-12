#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "index_html.h"
#include "peer_connection.h"
#include "signaling.h"

const char PIPE_LINE[] = "libcamerasrc ! video/x-raw, format=NV12, width=800, height=600, framerate=20/1, interlace-mode=progressive, colorimetry=bt709 ! v4l2h264enc capture-io-mode=4 output-io-mode=4";

typedef struct Surveillance {

  GCond cond;

  GMutex mutex;

  PeerConnection *pc;

  Signaling *signaling;

} Surveillance;

Surveillance g_surveillance = {0};

static void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  if(state == FAILED) {

    LOG_INFO("disconnect with browser...");
  }
}

static void on_icecandidate(char *sdp, void *data) {

  signaling_send_answer_to_call(g_surveillance.signaling, sdp);

  g_cond_signal(&g_surveillance.cond);
}

static void on_connected(void *data) {

  LOG_INFO("on connected");
}

void on_call_event(SignalingEvent signaling_event, char *msg, void *data) {

  if(signaling_event == SIGNALING_EVENT_GET_OFFER) {

    printf("get offer from singaling\n");

    g_mutex_lock(&g_surveillance.mutex);

    peer_connection_destroy(g_surveillance.pc);

    g_surveillance.pc = peer_connection_create(NULL);

    peer_connection_add_stream(g_surveillance.pc, CODEC_H264, PIPE_LINE);

    peer_connection_onicecandidate(g_surveillance.pc, on_icecandidate);

    peer_connection_oniceconnectionstatechange(g_surveillance.pc, on_iceconnectionstatechange);

    peer_connection_on_connected(g_surveillance.pc, on_connected);

    peer_connection_set_remote_description(g_surveillance.pc, msg);

    peer_connection_create_answer(g_surveillance.pc);

    g_cond_wait(&g_surveillance.cond, &g_surveillance.mutex);

    g_mutex_unlock(&g_surveillance.mutex);
  }
}

void signal_handler(int signal) {

  if(g_surveillance.signaling)
    signaling_destroy(g_surveillance.signaling);

  if(g_surveillance.pc)
    peer_connection_destroy(g_surveillance.pc);

  exit(0);
}

int main(int argc, char *argv[]) {

  signal(SIGINT, signal_handler);

  SignalingOption signaling_option = {SIGNALING_PROTOCOL_HTTP, "0.0.0.0", "demo", 8000, index_html};

  g_surveillance.signaling = signaling_create(signaling_option);

  if(!g_surveillance.signaling) {

    printf("create signaling service failed\n");
    return 0;
  }

  signaling_on_call_event(g_surveillance.signaling, &on_call_event, NULL);

  signaling_dispatch(g_surveillance.signaling);

  return 0;
}

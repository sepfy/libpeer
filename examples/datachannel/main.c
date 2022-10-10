#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "index_html.h"
#include "peer_connection.h"
#include "signaling.h"

typedef struct Datachannel {

  GCond cond;
  GMutex mutex;

  PeerConnection *pc;
  Signaling *signaling;

} Datachannel;

Datachannel g_datachannel = {0};

static void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  if(state == FAILED) {
    printf("Disconnect with browser... Stop streaming");
  }
}

static void on_icecandidate(char *sdp, void *data) {

  signaling_send_answer_to_call(g_datachannel.signaling, sdp);
  g_cond_signal(&g_datachannel.cond);
}

static void on_open(void *userdata) {

  char msg[] = "open";
  peer_connection_datachannel_send(g_datachannel.pc, msg, strlen(msg));
}

static void on_message(char *buf, size_t len, void *userdata) {
  LOG_INFO("Got message %s", buf);
  peer_connection_datachannel_send(g_datachannel.pc, buf, len);
}

void on_call_event(SignalingEvent signaling_event, char *msg, void *data) {

  if(signaling_event == SIGNALING_EVENT_GET_OFFER) {

    printf("Get offer from singaling\n");
    g_mutex_lock(&g_datachannel.mutex);

    peer_connection_destroy(g_datachannel.pc);

    g_datachannel.pc = peer_connection_create(NULL);

    peer_connection_onicecandidate(g_datachannel.pc, on_icecandidate);
    peer_connection_oniceconnectionstatechange(g_datachannel.pc, on_iceconnectionstatechange);
    peer_connection_ondatachannel(g_datachannel.pc, on_message, on_open, NULL);

    peer_connection_set_remote_description(g_datachannel.pc, msg);
    peer_connection_create_answer(g_datachannel.pc);

    g_cond_wait(&g_datachannel.cond, &g_datachannel.mutex);
    g_mutex_unlock(&g_datachannel.mutex);
  }
}

void signal_handler(int signal) {

  if(g_datachannel.signaling)
    signaling_destroy(g_datachannel.signaling);

  if(g_datachannel.pc)
    peer_connection_destroy(g_datachannel.pc);

  exit(0);
}

int main(int argc, char *argv[]) {

  signal(SIGINT, signal_handler);

  SignalingOption signaling_option = {SIGNALING_PROTOCOL_HTTP, "0.0.0.0", "demo", 8000, index_html};

  g_datachannel.signaling = signaling_create(signaling_option);
  if(!g_datachannel.signaling) {
    printf("Create signaling service failed\n");
    return 0;
  }

  signaling_on_call_event(g_datachannel.signaling, &on_call_event, NULL);
  signaling_dispatch(g_datachannel.signaling);

  return 0;
}

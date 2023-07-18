#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "peer_connection.h"
#include "signaling.h"

static PeerConnection g_pc;
static int g_datachannel_opened = 0;

void* peer_connection_thread(void *data) {

  while (1) {

    peer_connection_loop(&g_pc);
    usleep(1*1000); 
  }

  pthread_exit(NULL); 
}

static void on_icecandidate(char *sdp_text, void *data) {

  printf("on_icecandidate");
  signaling_set_local_description(sdp_text);
}

static void on_iceconnectionstatechange(PeerConnectionState state, void *data) {

  printf("state is changed: %d\n", state);
}

static void onmessasge(char *msg, size_t len, void *userdata) {

  printf("on message: %s", msg);

  if (strncmp(msg, "ping", 4) == 0) {
    printf(", send pong\n");
    peer_connection_datachannel_send(&g_pc, "pong", 4);
  }
}

void onopen(void *userdata) {

  printf("on open\n");
  g_datachannel_opened = 1;
}

void onclose(void *userdata) {
  
}


void on_signaling_event(SignalingEvent event, const char *buf, size_t len, void *user_data) {

  switch (event) {

    case SIGNALING_EVENT_REQUEST_OFFER:

      peer_connection_create_offer(&g_pc);

      break;

    case SIGNALING_EVENT_RESPONSE_ANSWER:

      peer_connection_set_remote_description(&g_pc, buf);

      break;

    default:
      break;
  }

}

void signal_handler(int signal) {

  exit(0);
}

int main(int argc, char *argv[]) {

  char device_id[128] = {0,};

  pthread_t thread;

  PeerOptions options = { .datachannel = 1 };

  signal(SIGINT, signal_handler);

  peer_connection_configure(&g_pc, &options);
  peer_connection_init(&g_pc);
  peer_connection_onicecandidate(&g_pc, on_icecandidate);
  peer_connection_oniceconnectionstatechange(&g_pc, on_iceconnectionstatechange);
  peer_connection_ondatachannel(&g_pc, onmessasge, onopen, onclose);

  pthread_create(&thread, NULL, peer_connection_thread, NULL);

  snprintf(device_id, sizeof(device_id), "test_%d", getpid());

  printf("open https://sepfy.github.io/webrtc?deviceId=%s\n", device_id);

  signaling_dispatch(device_id, on_signaling_event, NULL);

  return 0;
}

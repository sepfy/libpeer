#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "signaling.h"
#include "peer_connection.h"

const char PIPE_LINE[] = "filesrc location=test.mp4 ! qtdemux ! h264parse ! queue min-threshold-time=10000000000";

static PeerConnection g_pc;

void* peer_connection_thread(void *data) {

  while (1) {
    peer_connection_loop(&g_pc);
    usleep(10*1000);
  }

  pthread_exit(NULL);
}

void on_icecandidate(char *sdp_text, void *data) {

  printf("on_icecandidate : \n%s\n", sdp_text);
  signaling_set_local_description(sdp_text);
}

void on_iceconnectionstatechange(PeerConnectionState state, void *data) {

  printf("state is changed: %d\n", state);
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

  printf("catch interrupt. exit app\n");
  exit(0);
}

int main(int argc, char *argv[]) {

  char device_id[128] = {0};

  static pthread_t thread;

  PeerOptions options = { .video_codec = CODEC_H264, .video_outgoing_pipeline = PIPE_LINE };

  peer_connection_configure(&g_pc, &options);
  peer_connection_init(&g_pc);
  peer_connection_onicecandidate(&g_pc, on_icecandidate);
  peer_connection_oniceconnectionstatechange(&g_pc, on_iceconnectionstatechange);

  pthread_create(&thread, NULL, peer_connection_thread, NULL);

  snprintf(device_id, sizeof(device_id), "test_%d", getpid());

  printf("open https://sepfy.github.io/webrtc/?deviceId=%s\n", device_id);

  signal(SIGINT, signal_handler);

  signaling_dispatch(device_id, on_signaling_event, NULL);

  return 0;
}

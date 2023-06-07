#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "signaling.h"
#include "peer_connection.h"

const char VIDEO_OUTGOING_PIPELINE[] = "v4l2src ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! queue ! x264enc bitrate=6000 speed-preset=ultrafast tune=zerolatency key-int-max=15";

const char AUDIO_OUTGOING_PIPELINE[] = "alsasrc device=plughw:U0x46d0x81b,0 ! audio/x-raw,channels=1,rate=8000 ! alawenc";

static PeerConnection g_pc;

void on_iceconnectionstatechange(IceCandidateState state, void *data) {

  printf("state is changed: %d\n", state);
}

void on_icecandidate(char *answersdp, void *data) {

  printf("on ice candidate\n");
}

void on_connected(void *data) {

  printf("on connected\n");
}

void on_signaling_event(SignalingEvent event, const char *buf, size_t len, void *user_data) {

  const char *local_description = NULL;

  PeerOptions options = {0,};

  switch (event) {
  
    case SIGNALING_EVENT_REQUEST_OFFER:

      options.video_codec = CODEC_H264;
      options.audio_codec = CODEC_PCMA;
      options.video_outgoing_pipeline = VIDEO_OUTGOING_PIPELINE;
      options.audio_outgoing_pipeline = AUDIO_OUTGOING_PIPELINE;

      peer_connection_configure(&g_pc, &options);
      peer_connection_init(&g_pc);
      peer_connection_on_connected(&g_pc, on_connected);
      peer_connection_onicecandidate(&g_pc, on_icecandidate);
      peer_connection_oniceconnectionstatechange(&g_pc, on_iceconnectionstatechange);

      local_description = peer_connection_create_offer(&g_pc);

      signaling_set_local_description(local_description);

      break;

    case SIGNALING_EVENT_RESPONSE_ANSWER:

      peer_connection_set_remote_description(&g_pc, buf);

      break;

    default:
      break;
  }

}

void* peer_connection_thread(void *data) {

  while (1) {
    peer_connection_loop(&g_pc);
    usleep(1000);
  }

  pthread_exit(NULL);
} 

void signal_handler(int signal) {

  printf("catch interrupt. exit app\n");
  exit(0);
}

int main(int argc, char *argv[]) {

  char device_id[128] = {0,};

  snprintf(device_id, sizeof(device_id), "test_666");//%d", getpid());

  printf("open http://127.0.0.1?deviceId=%s\n", device_id);

  signal(SIGINT, signal_handler);

  pthread_t thread;
  pthread_create(&thread, NULL, peer_connection_thread, NULL);

  signaling_dispatch(device_id, on_signaling_event, NULL);

  return 0;
}

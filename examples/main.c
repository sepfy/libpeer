#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include "reader.h"
#include "peer_connection.h"
#include "signaling.h"

int g_quit = 0;
static PeerConnection g_pc;
static int g_datachannel_opened = 0;

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

  g_quit = 1;
}

void* sip_task(void *data) {

  char device_id[128] = {0,};

  snprintf(device_id, sizeof(device_id), "test_%d", 12);

  printf("open https://sepfy.github.io/webrtc?deviceId=%s\n", device_id);

  signaling_dispatch(device_id, on_signaling_event, NULL);

  pthread_exit(NULL);
}

void* pc_task(void *data) {

  PeerOptions options = { .datachannel = DATA_CHANNEL_STRING, .video_codec = CODEC_H264, .audio_codec = CODEC_PCMA };

  peer_connection_configure(&g_pc, &options);
  peer_connection_init(&g_pc);
  peer_connection_onicecandidate(&g_pc, on_icecandidate);
  peer_connection_oniceconnectionstatechange(&g_pc, on_iceconnectionstatechange);
  peer_connection_ondatachannel(&g_pc, onmessasge, onopen, onclose);

  while (1) {

    peer_connection_loop(&g_pc);
    usleep(1*1000); 
  }

  pthread_exit(NULL); 
}

uint64_t get_timestamp() {

  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int main(int argc, char *argv[]) {

  uint64_t curr_time, video_time, audio_time;
  uint8_t buf[102400];
  int size;
  pthread_t pc_thread;
  pthread_t sip_thread;

  signal(SIGINT, signal_handler);

  pthread_create(&pc_thread, NULL, pc_task, NULL);
  pthread_create(&sip_thread, NULL, sip_task, NULL);

  reader_init("./media/");


  while (!g_quit) {

    if (g_datachannel_opened) {

      curr_time = get_timestamp();

      // FPS 25
      if (curr_time - video_time > 40) {
        video_time = curr_time;
        if (reader_get_video_frame(buf, &size) == 0) {
          peer_connection_send_video(&g_pc, buf, size);
        }
      }

      if (curr_time - audio_time > 20) {
        if (reader_get_audio_frame(buf, &size) == 0) {
          peer_connection_send_audio(&g_pc, buf, size);
        }
        audio_time = curr_time;
      }

      usleep(1000);
    }
  }

  reader_deinit();

  return 0;
}

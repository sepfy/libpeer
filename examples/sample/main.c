#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>

#include "reader.h"
#include "peer.h"

int g_interrupted = 0;
PeerConnection *g_pc = NULL;
PeerConnectionState g_state;

static void onconnectionstatechange(PeerConnectionState state, void *data) {

  printf("state is changed: %s\n", peer_connection_state_to_string(state));
  g_state = state;
}

static void onopen(void *user_data) {

}

static void onclose(void *user_data) {

}

static void onmessage(char *msg, size_t len, void *user_data) {

  printf("on message: %s", msg);

  if (strncmp(msg, "ping", 4) == 0) {
    printf(", send pong\n");
    peer_connection_datachannel_send(g_pc, "pong", 4);
  }
}

static void signal_handler(int signal) {

  g_interrupted = 1;
}

static void* peer_singaling_task(void *data) {

  while (!g_interrupted) {

    peer_signaling_loop();
    usleep(1000);
  }

  return NULL;
}

static void* peer_connection_task(void *data) {

  while (!g_interrupted) {

    peer_connection_loop(g_pc);
    usleep(1000);
  }

  return NULL;
}

static uint64_t get_timestamp() {

  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int main(int argc, char *argv[]) {

  uint64_t curr_time, video_time, audio_time;
  uint8_t buf[102400];
  int size;

  pthread_t peer_singaling_thread;
  pthread_t peer_connection_thread;

  signal(SIGINT, signal_handler);

  PeerConfiguration config = {
   .ice_servers = {
    { .urls = "stun:stun.l.google.com:19302" },
   },
   .datachannel = DATA_CHANNEL_STRING,
   .video_codec = CODEC_H264,
   .audio_codec = CODEC_PCMA
  };

  snprintf((char*)buf, sizeof(buf), "test_%d", getpid());
  printf("open https://sepfy.github.io/webrtc?deviceId=%s\n", buf);

  peer_init();
  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, onconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);

  peer_signaling_join_channel((const char*)buf, g_pc);

  pthread_create(&peer_connection_thread, NULL, peer_connection_task, NULL);
  pthread_create(&peer_singaling_thread, NULL, peer_singaling_task, NULL);

  reader_init(/*"./media/"*/);

  while (!g_interrupted) {

    if (g_state == PEER_CONNECTION_COMPLETED) {

      curr_time = get_timestamp();

      // FPS 25
      if (curr_time - video_time > 40) {
        video_time = curr_time;
        if (reader_get_video_frame(buf, &size) == 0) {
          peer_connection_send_video(g_pc, buf, size);
        }
      }

      if (curr_time - audio_time > 20) {
        if (reader_get_audio_frame(buf, &size) == 0) {
          peer_connection_send_audio(g_pc, buf, size);
        }
        audio_time = curr_time;
      }

      usleep(1000);
    }
  }

  pthread_join(peer_singaling_thread, NULL);
  pthread_join(peer_connection_thread, NULL);

  reader_deinit();

  peer_signaling_leave_channel();
  peer_connection_destroy(g_pc);
  peer_deinit();

  return 0;
}

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "peer.h"

#define MAX_CONNECTION_ATTEMPTS 25

typedef struct {
  PeerConnection *offer_peer_connection, *answer_peer_connection;
} TestUserData;

static void onconnectionstatechange_offerer_peer_connection(PeerConnectionState state, void* user_data) {
  printf("offer state is changed: %s\n", peer_connection_state_to_string(state));
}

static void onconnectionstatechange_answerer_peer_connection(PeerConnectionState state, void* user_data) {
  printf("answerer state is changed: %s\n", peer_connection_state_to_string(state));
}

static void onicecandidate_offerer_peer_connection(char* description, void* user_data) {
  TestUserData* test_user_data = (TestUserData*)user_data;
  peer_connection_set_remote_description(test_user_data->answer_peer_connection, description);
}

static void onicecandidate_answerer_peer_connection(char* description, void* user_data) {
  TestUserData* test_user_data = (TestUserData*)user_data;
  peer_connection_set_remote_description(test_user_data->offer_peer_connection, description);
}

static void* peer_connection_task(void* user_data) {
  PeerConnection* peer_connection = (PeerConnection*)user_data;

  while (1) {
    if (peer_connection_get_state(peer_connection) == PEER_CONNECTION_COMPLETED) {
      break;
    }

    peer_connection_loop(peer_connection);
    usleep(1000);
  }

  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char* argv[]) {
  pthread_t offer_thread, answer_thread;

  TestUserData test_user_data = {
      .offer_peer_connection = NULL,
      .answer_peer_connection = NULL,
  };

  PeerConfiguration config = {
      .ice_servers = {
          {.urls = "stun:stun.l.google.com:19302"},
      },
      .datachannel = DATA_CHANNEL_STRING,
      .video_codec = CODEC_H264,
      .audio_codec = CODEC_OPUS,
      .user_data = &test_user_data,
  };

  peer_init();

  test_user_data.offer_peer_connection = peer_connection_create(&config);
  test_user_data.answer_peer_connection = peer_connection_create(&config);

  peer_connection_oniceconnectionstatechange(test_user_data.offer_peer_connection, onconnectionstatechange_offerer_peer_connection);
  peer_connection_oniceconnectionstatechange(test_user_data.answer_peer_connection, onconnectionstatechange_answerer_peer_connection);

  peer_connection_onicecandidate(test_user_data.offer_peer_connection, onicecandidate_offerer_peer_connection);
  peer_connection_onicecandidate(test_user_data.answer_peer_connection, onicecandidate_answerer_peer_connection);

  peer_connection_create_offer(test_user_data.offer_peer_connection);

  pthread_create(&offer_thread, NULL, peer_connection_task, test_user_data.offer_peer_connection);
  pthread_create(&answer_thread, NULL, peer_connection_task, test_user_data.answer_peer_connection);

  int attempts = 0;
  while (1) {
    if (peer_connection_get_state(test_user_data.offer_peer_connection) == PEER_CONNECTION_COMPLETED && peer_connection_get_state(test_user_data.answer_peer_connection) == PEER_CONNECTION_COMPLETED) {
      break;
    } else if (attempts == MAX_CONNECTION_ATTEMPTS) {
      break;
    }

    attempts++;
    usleep(250000);
  }

  peer_connection_destroy(test_user_data.offer_peer_connection);
  peer_connection_destroy(test_user_data.answer_peer_connection);

  peer_deinit();
  return attempts == MAX_CONNECTION_ATTEMPTS ? 1 : 0;
}

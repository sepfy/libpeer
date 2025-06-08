#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "peer.h"

#define MAX_CONNECTION_ATTEMPTS 25
#define OFFER_DATACHANNEL_MESSAGE "Hello World"
#define ANSWER_DATACHANNEL_MESSAGE "Foobar"
#define DATACHANNEL_NAME "libpeer-datachannel"

int test_complete = 0;

typedef struct {
  PeerConnection *offer_peer_connection, *answer_peer_connection;
  int onmessage_offer_called, onmessage_answer_called, test_complete;
} TestUserData;

static void onconnectionstatechange_offerer_peer_connection(PeerConnectionState state, void* user_data) {
  printf("offer state is changed: %s\n", peer_connection_state_to_string(state));
}

static void onconnectionstatechange_answerer_peer_connection(PeerConnectionState state, void* user_data) {
  printf("answerer state is changed: %s\n", peer_connection_state_to_string(state));
}

static void onicecandidate_offerer_peer_connection(char* description, void* user_data) {
}

static void onicecandidate_answerer_peer_connection(char* description, void* user_data) {
}

static void ondatachannel_onmessage_offerer_peer_connection(char* msg, size_t len, void* userdata, uint16_t sid) {
  TestUserData* test_user_data = (TestUserData*)userdata;

  if (strcmp(msg, ANSWER_DATACHANNEL_MESSAGE) == 0) {
    test_user_data->onmessage_offer_called = 1;
  }
}

static void ondatachannel_onmessage_answerer_peer_connection(char* msg, size_t len, void* userdata, uint16_t sid) {
  TestUserData* test_user_data = (TestUserData*)userdata;

  if (strcmp(msg, OFFER_DATACHANNEL_MESSAGE) == 0) {
    test_user_data->onmessage_answer_called = 1;
  }
}

static void* peer_connection_task(void* user_data) {
  PeerConnection* peer_connection = (PeerConnection*)user_data;

  while (!test_complete) {
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

  peer_connection_ondatachannel(test_user_data.offer_peer_connection, ondatachannel_onmessage_offerer_peer_connection, NULL, NULL);
  peer_connection_ondatachannel(test_user_data.answer_peer_connection, ondatachannel_onmessage_answerer_peer_connection, NULL, NULL);

  pthread_create(&offer_thread, NULL, peer_connection_task, test_user_data.offer_peer_connection);
  pthread_create(&answer_thread, NULL, peer_connection_task, test_user_data.answer_peer_connection);

  const char* offer = peer_connection_create_offer(test_user_data.offer_peer_connection);
  peer_connection_set_remote_description(test_user_data.answer_peer_connection, offer, SDP_TYPE_OFFER);
  const char* answer = peer_connection_create_answer(test_user_data.answer_peer_connection);
  peer_connection_set_remote_description(test_user_data.offer_peer_connection, answer, SDP_TYPE_ANSWER);

  int attempts = 0, datachannel_created = 0;
  while (attempts < MAX_CONNECTION_ATTEMPTS) {
    if (!datachannel_created && peer_connection_get_state(test_user_data.offer_peer_connection) == PEER_CONNECTION_COMPLETED) {
      if (peer_connection_create_datachannel(test_user_data.offer_peer_connection, DATA_CHANNEL_RELIABLE, 0, 0, DATACHANNEL_NAME, "bar") == 18) {
        datachannel_created = 1;
      }
    }

    if (peer_connection_get_state(test_user_data.offer_peer_connection) == PEER_CONNECTION_COMPLETED &&
        peer_connection_get_state(test_user_data.answer_peer_connection) == PEER_CONNECTION_COMPLETED &&
        test_user_data.onmessage_offer_called == 1 &&
        test_user_data.onmessage_answer_called == 1) {
      break;
    }

    peer_connection_datachannel_send(test_user_data.offer_peer_connection, OFFER_DATACHANNEL_MESSAGE, sizeof(OFFER_DATACHANNEL_MESSAGE));
    peer_connection_datachannel_send(test_user_data.answer_peer_connection, ANSWER_DATACHANNEL_MESSAGE, sizeof(ANSWER_DATACHANNEL_MESSAGE));

    attempts++;
    usleep(250000);
  }

  if (strcmp(DATACHANNEL_NAME, peer_connection_lookup_sid_label(test_user_data.answer_peer_connection, 0)) != 0) {
    return 1;
  }

  test_complete = 1;
  pthread_join(offer_thread, NULL);
  pthread_join(answer_thread, NULL);
  peer_connection_destroy(test_user_data.offer_peer_connection);
  peer_connection_destroy(test_user_data.answer_peer_connection);

  peer_deinit();
  return attempts == MAX_CONNECTION_ATTEMPTS ? 1 : 0;
}

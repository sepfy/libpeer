#ifndef PEER_SIGNALING_H_
#define PEER_SIGNALING_H_

#include "peer_connection.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DISABLE_PEER_SIGNALING

typedef struct ServiceConfiguration {
  const char* mqtt_url;
  int mqtt_port;
  const char* client_id;
  const char* http_url;
  int http_port;
  const char* username;
  const char* password;
  PeerConnection* pc;
} ServiceConfiguration;

#define SERVICE_CONFIG_DEFAULT()  \
  {                               \
    .mqtt_url = "broker.emqx.io", \
    .mqtt_port = 8883,            \
    .client_id = "peer",          \
    .http_url = "",               \
    .http_port = 443,             \
    .username = "",               \
    .password = "",               \
    .pc = NULL                    \
  }

void peer_signaling_set_config(ServiceConfiguration* config);

int peer_signaling_whip_connect();

void peer_signaling_whip_disconnect();

int peer_signaling_join_channel();

void peer_signaling_leave_channel();

int peer_signaling_loop();

#endif  // DISABLE_PEER_SIGNALING

#ifdef __cplusplus
}
#endif

#endif  // PEER_SIGNALING_H_

#ifndef PEER_CONNECTION_H_
#define PEER_CONNECTION_H_

#include "dtls_transport.h"
#include "ice_agent.h"

typedef struct peer_connection_t {

  ice_agent_t ice_agent;
  dtls_transport_t dtls_transport;

} peer_connection_t;

int peer_connection_init(peer_connection_t *peer_connection);

int peer_connection_create_answer(peer_connection_t *peer_connection);

void peer_connection_set_remote_description(peer_connection_t *peer_connection, char *sdp);

int peer_connection_send_rtp_packet(peer_connection_t *peer_connection, uint8_t *packet, int bytes);

void peer_connection_set_on_icecandidate(peer_connection_t *peer_connection,
 void (*on_icecandidate), void  *data);

void peer_connection_set_on_transport_ready(peer_connection_t *peer_connection,
 void (*on_transport_ready), void *data);

#endif // PEER_CONNECTION_H_

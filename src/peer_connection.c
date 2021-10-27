#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ice_agent_bio.h"
#include "ice_agent.h"
#include "utils.h"
#include "peer_connection.h"

PeerConnection* peer_connection_create(void) {

  PeerConnection *peer_connection = NULL;
  peer_connection = (PeerConnection*)malloc(sizeof(PeerConnection));
  if(peer_connection != NULL) {
    //peer_connection_init(peer_connection);
    ice_agent_init(&peer_connection->ice_agent, &peer_connection->dtls_transport);
  }
  return peer_connection;
}

void peer_connection_destroy(PeerConnection *peer_connection) {

  if(peer_connection != NULL) {

    dtls_transport_destroy(&peer_connection->dtls_transport);
    free(peer_connection);
  }
}

char* peer_connection_get_local_sdp(PeerConnection *peer_connection) {

  char *sdp = sdp_attribute_get_answer(peer_connection->ice_agent.sdp_attribute);
  return g_base64_encode((const char *)sdp, strlen(sdp));
}

void peer_connection_add_stream(PeerConnection *peer_connection, MediaStream *media_stream) {

  ice_agent_add_stream(&peer_connection->ice_agent, media_stream);

}

int peer_connection_create_answer(PeerConnection *pc) {

  if(!nice_agent_gather_candidates(pc->ice_agent.nice_agent, pc->ice_agent.stream_id)) {
    LOG_ERROR("Failed to start candidate gathering");
    return -1;
  }
  return 0;
}

void peer_connection_set_remote_description(PeerConnection *pc, char *sdp) {

  ice_agent_set_remote_sdp(&pc->ice_agent, sdp);
}


int peer_connection_send_rtp_packet(PeerConnection *peer_connection, uint8_t *packet, int bytes) {

  ice_agent_t *ice_agent = (ice_agent_t*)&peer_connection->ice_agent;
  return ice_agent_send_rtp_packet(ice_agent, packet, &bytes);
}

void peer_connection_set_on_transport_ready(PeerConnection *peer_connection,
 void (*on_transport_ready), void *data) {

  ice_agent_t *ice_agent = (ice_agent_t*)&peer_connection->ice_agent;
  ice_agent->on_transport_ready = on_transport_ready;
  ice_agent->on_transport_ready_data = data;
}

void peer_connection_onicecandidate(PeerConnection *peer_connection,
 void (*on_icecandidate), void  *data) {

  ice_agent_t *ice_agent = (ice_agent_t*)&peer_connection->ice_agent;
  ice_agent->on_icecandidate = on_icecandidate;
  ice_agent->on_icecandidate_data = data;
}

void peer_connection_oniceconnectionstatechange(PeerConnection *peer_connection,
  void (*on_iceconnectionstatechange), void *data) {

  ice_agent_t *ice_agent = (ice_agent_t*)&peer_connection->ice_agent;
  ice_agent->on_iceconnectionstatechange = on_iceconnectionstatechange;
  ice_agent->on_iceconnectionstatechange_data = data;
}


void peer_connection_ontrack(PeerConnection *peer_connection,
 void (*on_track), void *data) {

  ice_agent_t *ice_agent = (ice_agent_t*)&peer_connection->ice_agent;
  ice_agent->on_track = on_track;
  ice_agent->on_track_data = data;
}

int peer_connection_add_transceiver(PeerConnection *pc, transceiver_t transceiver) {
  pc->transceiver = transceiver;
  pc->ice_agent.direction = transceiver.video;
}

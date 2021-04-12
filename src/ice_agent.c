#include <gio/gnetworking.h>

#include "utils.h"
#include "ice_agent.h"
#include "ice_agent_bio.h"
#include "sdp_attribute.h"
#include "rtcp_receiver.h"

static const gchar *STATE_NAME[] = {"disconnected", "gathering", "connecting",
 "connected", "ready", "failed"};
static const gchar *CANDIDATE_TYPE_NAME[] = {"host", "srflx", "prflx", "relay"};
static const gchar *STUN_ADDR = "18.191.223.12";
static const guint STUN_PORT = 3478;

void* ice_agent_gather_thread(void *data) {

  ice_agent_t *ice_agent;
  ice_agent = (ice_agent_t*)data;

  g_main_loop_run(ice_agent->gloop);
  g_main_loop_quit(ice_agent->gloop);

  return NULL;
}

int ice_agent_send_rtp_packet(ice_agent_t *ice_agent, uint8_t *packet, int *bytes) {

    dtls_transport_encrypt_rtp_packet(ice_agent->dtls_transport, packet, bytes);
    int sent = nice_agent_send(ice_agent->nice_agent, ice_agent->stream_id,
     ice_agent->component_id, *bytes, (gchar*)packet);
    if(sent < *bytes) {
      LOG_ERROR("only sent %d bytes? (was %d)\n", sent, *bytes);
    }
    return sent;
}

static void cb_ice_recv(NiceAgent *agent, guint stream_id, guint component_id,
 guint len, gchar *buf, gpointer data) {

  ice_agent_t *ice_agent = (ice_agent_t*)data;
  if(rtcp_receiver_is_rtcp(buf)) {
    if(ice_agent->on_transport_ready != NULL) {
      ice_agent->on_transport_ready((void*)ice_agent->on_transport_ready_data);
    }
  }
  else if(dtls_transport_is_dtls(buf)) {
    dtls_transport_incomming_msg(ice_agent->dtls_transport, buf, len);
  }
}


static void cb_new_selected_pair_full(NiceAgent* agent, guint stream_id,
 guint component_id, NiceCandidate *lcandidate,
 NiceCandidate* rcandidate, gpointer data) {

  ice_agent_t *ice_agent = (ice_agent_t*)data;
  dtls_transport_do_handshake(ice_agent->dtls_transport);
}

static void* cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
 gpointer data) {

  ice_agent_t *ice_agent;
  ice_agent = (ice_agent_t*)data;

  GRand *rand;
  gchar *sdp;
  gchar *answer_base64;
  gchar *local_ufrag = NULL;
  gchar *local_password = NULL;
  gchar ipaddr[INET6_ADDRSTRLEN];
  GSList *nice_candidates = NULL;
  gint ssrc;
  ssrc = 12345678;
  rand = g_rand_new();
  //ssrc = g_rand_int_range(rand, 1, 10000000);

  int i = 0;
  NiceCandidate *nice_candidate;
  char nice_candidate_addr[INET6_ADDRSTRLEN];

  sdp_attribute_t *sdp_attribute = ice_agent->sdp_attribute;

  if(!nice_agent_get_local_credentials(ice_agent->nice_agent,
   ice_agent->stream_id, &local_ufrag, &local_password)) {
    LOG_ERROR("get local credentials failed");
    return NULL;
  }

  sdp_attribute_append(sdp_attribute, "v=0");
  sdp_attribute_append(sdp_attribute, "o=- 1495799811084970 1495799811084970 IN IP4 0.0.0.0");
  sdp_attribute_append(sdp_attribute, "s=Streaming Test");
  sdp_attribute_append(sdp_attribute, "t=0 0");
  sdp_attribute_append(sdp_attribute, "a=group:BUNDLE 0");
  sdp_attribute_append(sdp_attribute, "a=msid-semantic: pear");
  sdp_attribute_append(sdp_attribute, "m=video 9 UDP/TLS/RTP/SAVPF 102");
  sdp_attribute_append(sdp_attribute, "c=IN IP4 0.0.0.0");
  sdp_attribute_append(sdp_attribute, "a=sendonly");
  sdp_attribute_append(sdp_attribute, "a=mid:0");
  sdp_attribute_append(sdp_attribute, "a=rtcp-mux");
  sdp_attribute_append(sdp_attribute, "a=ice-ufrag:%s", local_ufrag);
  sdp_attribute_append(sdp_attribute, "a=ice-pwd:%s", local_password);
  sdp_attribute_append(sdp_attribute, "a=ice-options:trickle");
  sdp_attribute_append(sdp_attribute, "a=fingerprint:sha-256 %s",
   ice_agent->dtls_transport->fingerprint);
  sdp_attribute_append(sdp_attribute, "a=setup:passive");
  sdp_attribute_append(sdp_attribute, "a=rtpmap:102 H264/90000");
  sdp_attribute_append(sdp_attribute, "a=fmtp:102 packetization-mode=1");
  sdp_attribute_append(sdp_attribute, "a=rtcp-fb:102 nack");
  sdp_attribute_append(sdp_attribute, "a=rtcp-fb:102 nack pli");
  sdp_attribute_append(sdp_attribute, "a=rtcp-fb:102 goog-remb");

  nice_candidates = nice_agent_get_local_candidates(ice_agent->nice_agent,
   ice_agent->stream_id, ice_agent->component_id);

  for(i = 0; i < g_slist_length(nice_candidates); ++i) {

    nice_candidate = (NiceCandidate *)g_slist_nth(nice_candidates, i)->data;
    nice_address_to_string(&nice_candidate->addr, nice_candidate_addr);
    if(utils_is_valid_ip_address(nice_candidate_addr) > 0) {
      continue;
    }

    sdp_attribute_append(sdp_attribute, "a=candidate:%s 1 udp %u %s %d typ %s",
     nice_candidate->foundation,
     nice_candidate->priority,
     nice_candidate_addr,
     nice_address_get_port(&nice_candidate->addr),
     CANDIDATE_TYPE_NAME[nice_candidate->type]);

  }

  if(ice_agent->on_icecandidate != NULL) {
    ice_agent->on_icecandidate(sdp_attribute_get_answer(sdp_attribute),
     ice_agent->on_icecandidate_data);
  }

}

static void* cb_component_state_chanaged(NiceAgent *agent,
 guint stream_id, guint component_id, guint state, gpointer data) {

  LOG_INFO("SIGNAL: state changed %d %d %s[%d]",
   stream_id, component_id, STATE_NAME[state], state);
}


int ice_agent_init(ice_agent_t *ice_agent, dtls_transport_t *dtls_transport) {

  ice_agent->on_transport_ready = NULL;
  ice_agent->on_transport_ready_data = NULL;
  ice_agent->on_icecandidate = NULL;
  ice_agent->on_icecandidate_data = NULL;

  dtls_transport_init(dtls_transport, ice_agent_bio_new(ice_agent));

  ice_agent->dtls_transport = dtls_transport;
  ice_agent->controlling = FALSE;

  ice_agent->gloop = g_main_loop_new(NULL, FALSE);

  GIOChannel* io_stdin;
  io_stdin = g_io_channel_unix_new(fileno(stdin));
  g_io_channel_set_flags(io_stdin, G_IO_FLAG_NONBLOCK, NULL);

  ice_agent->nice_agent = nice_agent_new(g_main_loop_get_context(ice_agent->gloop),
   NICE_COMPATIBILITY_RFC5245);

  if(ice_agent->nice_agent == NULL) {
    LOG_ERROR("Failed to create agent");
    return -1;
  }

  g_object_set(ice_agent->nice_agent, "stun-server", STUN_ADDR, NULL);
  g_object_set(ice_agent->nice_agent, "stun-server-port", STUN_PORT, NULL);
  g_object_set(ice_agent->nice_agent, "controlling-mode",
   ice_agent->controlling, NULL);

  g_signal_connect(ice_agent->nice_agent, "candidate-gathering-done", G_CALLBACK(cb_candidate_gathering_done), ice_agent);
  g_signal_connect(ice_agent->nice_agent, "component-state-changed", G_CALLBACK(cb_component_state_chanaged), ice_agent);
  g_signal_connect(ice_agent->nice_agent, "new-selected-pair-full", G_CALLBACK(cb_new_selected_pair_full), ice_agent);

  ice_agent->component_id = 1;
  ice_agent->stream_id = nice_agent_add_stream(ice_agent->nice_agent,
	 ice_agent->component_id);

  if(ice_agent->stream_id == 0) {
    LOG_ERROR("Failed to add stream");
    return -1;
  }

  nice_agent_set_stream_name(ice_agent->nice_agent,
	 ice_agent->stream_id, "video");

  nice_agent_attach_recv(ice_agent->nice_agent,
	 ice_agent->stream_id, ice_agent->component_id,
	 g_main_loop_get_context(ice_agent->gloop), cb_ice_recv, ice_agent);

  ice_agent->gthread = g_thread_new("ice gather thread",
   &ice_agent_gather_thread, ice_agent);

  ice_agent->sdp_attribute = sdp_attribute_create();

  return 0;
}

void ice_agent_set_remote_sdp(ice_agent_t *ice_agent, char *remote_sdp_base64) {

  guchar *remote_sdp;
  gsize len;
  gchar* ufrag = NULL;
  gchar* pwd = NULL;
  GSList *plist;

  remote_sdp = g_base64_decode(remote_sdp_base64, &len);

  plist = nice_agent_parse_remote_stream_sdp(ice_agent->nice_agent,
   ice_agent->component_id, (gchar*)remote_sdp, &ufrag, &pwd);

  if(ufrag && pwd && g_slist_length(plist) > 0) {
    ufrag[strlen(ufrag) - 1] = '\0';
    pwd[strlen(pwd) - 1] = '\0';
    NiceCandidate* c = (NiceCandidate*)g_slist_nth(plist, 0)->data;
    if(!nice_agent_set_remote_credentials(ice_agent->nice_agent, 1, ufrag, pwd))
    {
      LOG_WARNING("failed to set remote credentials");
    }
    if(nice_agent_set_remote_candidates(ice_agent->nice_agent, ice_agent->stream_id,
     ice_agent->component_id, plist) < 1) {
      LOG_WARNING("failed to set remote candidates");
    }
    g_free(ufrag);
    g_free(pwd);
    g_slist_free_full(plist, (GDestroyNotify)&nice_candidate_free);
  }

}

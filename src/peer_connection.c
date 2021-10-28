#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gio/gnetworking.h>

#include "dtls_transport.h"
#include "nice_agent_bio.h"
#include "rtp_packet.h"
#include "rtcp_packet.h"
#include "utils.h"
#include "peer_connection.h"

static const gchar *STATE_NAME[] = {"disconnected", "gathering", "connecting",
 "connected", "ready", "failed"};
static const gchar *CANDIDATE_TYPE_NAME[] = {"host", "srflx", "prflx", "relay"};
static const gchar *STUN_ADDR = "18.191.223.12";
static const guint STUN_PORT = 3478;

struct PeerConnection {

  NiceAgent *nice_agent;
  gboolean controlling;
  guint stream_id;
  guint component_id;
  GMainLoop *gloop;
  GThread *gthread;

  DtlsTransport *dtls_transport;
  SessionDescription *sdp;
  transceiver_direction_t direction;
  int h264_gop;

  MediaStream *media_stream;

  void (*onicecandidate)(char *sdp, void *userdata);
  void (*oniceconnectionstatechange)(IceConnectionState state, void *userdata);
  void (*ontrack)(uint8_t *packet, size_t bytes, void *userdata);

  void *onicecandidate_userdata;
  void *oniceconnectionstatechange_userdata;
  void *ontrack_userdata;

  void (*on_transport_ready)(void *userdata);
  void *on_transport_ready_userdata;
};


void* peer_connection_gather_thread(void *data) {

  PeerConnection *pc = (PeerConnection*)data;

  g_main_loop_run(pc->gloop);
  g_main_loop_quit(pc->gloop);

  return NULL;

}


static void peer_connection_new_selected_pair_full_cb(NiceAgent* agent, guint stream_id,
 guint component_id, NiceCandidate *lcandidate, NiceCandidate* rcandidate, gpointer data) {

  PeerConnection *pc = (PeerConnection*)data;
  dtls_transport_do_handshake(pc->dtls_transport);
}

static void* peer_connection_component_state_chanaged_cb(NiceAgent *agent,
 guint stream_id, guint component_id, guint state, gpointer data) {

  PeerConnection *pc = (PeerConnection*)data;
  //LOG_INFO("SIGNAL: state changed %d %d %s[%d]",
  // stream_id, component_id, STATE_NAME[state], state);
  if(pc->oniceconnectionstatechange != NULL) {
    pc->oniceconnectionstatechange(state, pc->oniceconnectionstatechange_userdata);
  }

}

static void* peer_connection_candidate_gathering_done_cb(NiceAgent *agent, guint stream_id,
 gpointer data) {

  PeerConnection *pc = (PeerConnection*)data;

  gchar *local_ufrag = NULL;
  gchar *local_password = NULL;
  gchar ipaddr[INET6_ADDRSTRLEN];
  GSList *nice_candidates = NULL;

  int i = 0;
  NiceCandidate *nice_candidate;
  char nice_candidate_addr[INET6_ADDRSTRLEN];

  SessionDescription *sdp = pc->sdp;

  if(!nice_agent_get_local_credentials(pc->nice_agent,
   pc->stream_id, &local_ufrag, &local_password)) {
    LOG_ERROR("get local credentials failed");
    return NULL;
  }

  session_description_append(sdp, "v=0");
  session_description_append(sdp, "o=- 1495799811084970 1495799811084970 IN IP4 0.0.0.0");
  session_description_append(sdp, "s=-");
  session_description_append(sdp, "t=0 0");
  session_description_append(sdp, "a=msid-semantic: WMS");

  if(pc->media_stream->tracks_num > 1)
    session_description_append(sdp, "a=group:BUNDLE 0 1");
  else
    session_description_append(sdp, "a=group:BUNDLE 0");


  session_description_add_codec(sdp, pc->media_stream->video_codec, pc->direction, local_ufrag, local_password, dtls_transport_get_fingerprint(pc->dtls_transport));

  if(local_ufrag)
    free(local_ufrag);

  if(local_password)
    free(local_password);


  nice_candidates = nice_agent_get_local_candidates(pc->nice_agent,
   pc->stream_id, pc->component_id);

  for(i = 0; i < g_slist_length(nice_candidates); ++i) {

    nice_candidate = (NiceCandidate *)g_slist_nth(nice_candidates, i)->data;
    nice_address_to_string(&nice_candidate->addr, nice_candidate_addr);
    if(utils_is_valid_ip_address(nice_candidate_addr) > 0) {
      nice_candidate_free(nice_candidate);
      continue;
    }

    session_description_append(sdp, "a=candidate:%s 1 udp %u %s %d typ %s",
     nice_candidate->foundation,
     nice_candidate->priority,
     nice_candidate_addr,
     nice_address_get_port(&nice_candidate->addr),
     CANDIDATE_TYPE_NAME[nice_candidate->type]);

    nice_candidate_free(nice_candidate);
  }

  if(pc->onicecandidate != NULL) {

    char *answer = NULL;
    const char *sdp_content = session_description_get_content(pc->sdp);
    answer = (char*)malloc(strlen(sdp_content) + 30);
    sprintf(answer, "{\"type\": \"answer\", \"sdp\": \"%s\"}", sdp_content);

    pc->onicecandidate(answer, pc->onicecandidate_userdata);

    if(answer)
      free(answer);
  }

  if(nice_candidates)
    g_slist_free(nice_candidates);

}

int peer_connection_send_rtcp_pil(PeerConnection *pc, uint32_t ssrc) {

  int ret = -1;
  guint size = 12;
  uint8_t plibuf[128];
  rtcp_packet_get_pli(plibuf, 12, ssrc);

  dtls_transport_encrypt_rctp_packet(pc->dtls_transport, plibuf, &size);
  ret = nice_agent_send(pc->nice_agent, pc->stream_id, pc->component_id, size, (gchar*)plibuf);

  return ret;
}


static void peer_connection_ice_recv_cb(NiceAgent *agent, guint stream_id, guint component_id,
 guint len, gchar *buf, gpointer data) {

  PeerConnection *pc = (PeerConnection*)data;

  if(rtcp_packet_validate(buf, len)) {

     if(pc->on_transport_ready != NULL) {
      pc->on_transport_ready((void*)pc->on_transport_ready_userdata);
     }
  }
  else if(dtls_transport_validate(buf)) {

    dtls_transport_incomming_msg(pc->dtls_transport, buf, len);

  }
  else if(rtp_packet_validate(buf, len)) {

    dtls_transport_decrypt_rtp_packet(pc->dtls_transport, buf, &len);

    static int frame_number = 0;
    frame_number++;
    if(frame_number % pc->h264_gop == 0) {
      uint32_t ssrc = *(uint32_t*)(buf + 8);
      peer_connection_send_rtcp_pil(pc, ssrc);
    }

    if(pc->ontrack != NULL) {
      pc->ontrack(buf, len, pc->ontrack_userdata);
    }

  }


}

gboolean peer_connection_nice_agent_setup(PeerConnection *pc) {

  pc->gloop = g_main_loop_new(NULL, FALSE);

  pc->nice_agent = nice_agent_new(g_main_loop_get_context(pc->gloop),
   NICE_COMPATIBILITY_RFC5245);

  if(pc->nice_agent == NULL) {
    LOG_ERROR("Failed to create agent");
    return FALSE;
  }

  g_object_set(pc->nice_agent, "stun-server", STUN_ADDR, NULL);
  g_object_set(pc->nice_agent, "stun-server-port", STUN_PORT, NULL);
  g_object_set(pc->nice_agent, "controlling-mode", FALSE, NULL);
  g_object_set(pc->nice_agent, "keepalive-conncheck", TRUE, NULL);

  g_signal_connect(pc->nice_agent, "candidate-gathering-done",
   G_CALLBACK(peer_connection_candidate_gathering_done_cb), pc);

  g_signal_connect(pc->nice_agent, "component-state-changed",
   G_CALLBACK(peer_connection_component_state_chanaged_cb), pc);

  g_signal_connect(pc->nice_agent, "new-selected-pair-full",
   G_CALLBACK(peer_connection_new_selected_pair_full_cb), pc);

  pc->component_id = 1;
  pc->stream_id = nice_agent_add_stream(pc->nice_agent, pc->component_id);

  if(pc->stream_id == 0) {
    LOG_ERROR("Failed to add stream");
    return FALSE;
  }

  nice_agent_set_stream_name(pc->nice_agent, pc->stream_id, "video");

  nice_agent_attach_recv(pc->nice_agent, pc->stream_id, pc->component_id,
   g_main_loop_get_context(pc->gloop), peer_connection_ice_recv_cb, pc);

  pc->gthread = g_thread_new("ice gather thread", &peer_connection_gather_thread, pc);

  return TRUE;
}

PeerConnection* peer_connection_create(void) {

  PeerConnection *pc = NULL;
  pc = (PeerConnection*)malloc(sizeof(PeerConnection));

  if(pc == NULL)
    return pc;


  pc->onicecandidate = NULL;
  pc->onicecandidate_userdata = NULL;

  pc->oniceconnectionstatechange = NULL;
  pc->oniceconnectionstatechange_userdata = NULL;

  pc->on_transport_ready = NULL;
  pc->on_transport_ready_userdata = NULL;
  pc->direction = SENDONLY;

  if(peer_connection_nice_agent_setup(pc) == FALSE) {
    peer_connection_destroy(pc);
    return NULL;
  }

  pc->dtls_transport = dtls_transport_create(nice_agent_bio_new(pc->nice_agent, pc->stream_id, pc->component_id));

  pc->sdp = session_description_create();

  return pc;
}

void peer_connection_destroy(PeerConnection *pc) {

  if(pc != NULL) {

    dtls_transport_destroy(pc->dtls_transport);
    free(pc);
  }
}

void peer_connection_add_stream(PeerConnection *pc, MediaStream *media_stream) {

  pc->media_stream = media_stream;

}

int peer_connection_create_answer(PeerConnection *pc) {

  if(!nice_agent_gather_candidates(pc->nice_agent, pc->stream_id)) {
    LOG_ERROR("Failed to start candidate gathering");
    return -1;
  }
  return 0;
}

void peer_connection_set_remote_description(PeerConnection *pc, char *remote_sdp_base64) {

  guchar *remote_sdp = NULL;
  gsize len;
  gchar* ufrag = NULL;
  gchar* pwd = NULL;
  GSList *plist;

  remote_sdp = g_base64_decode(remote_sdp_base64, &len);

  // Remove mDNS
  SessionDescription *sdp = NULL;
  if(strstr(remote_sdp, "local") != NULL) {
    sdp = session_description_create();
    char *token;
    token = strtok(remote_sdp, "\r\n");
    while(token != NULL) {

      if(strstr(token, "candidate") != NULL && strstr(token, "local") != NULL) {
        char buf[256] = {0};
        if(session_description_update_mdns_of_candidate(token, buf, sizeof(buf)) != -1) {
          session_description_append_newline(sdp, buf);
        }
      }
      else {
        session_description_append_newline(sdp, token);
      }

      token = strtok(NULL, "\r\n");
    }

    remote_sdp = session_description_get_content(sdp);
  }


  plist = nice_agent_parse_remote_stream_sdp(pc->nice_agent,
   pc->component_id, (gchar*)remote_sdp, &ufrag, &pwd);

  if(ufrag && pwd && g_slist_length(plist) > 0) {
    ufrag[strlen(ufrag) - 1] = '\0';
    pwd[strlen(pwd) - 1] = '\0';
    NiceCandidate* c = (NiceCandidate*)g_slist_nth(plist, 0)->data;
    if(!nice_agent_set_remote_credentials(pc->nice_agent, 1, ufrag, pwd))
    {
      LOG_WARNING("failed to set remote credentials");
    }
    if(nice_agent_set_remote_candidates(pc->nice_agent, pc->stream_id,
     pc->component_id, plist) < 1) {
      LOG_WARNING("failed to set remote candidates");
    }
    g_free(ufrag);
    g_free(pwd);
    g_slist_free_full(plist, (GDestroyNotify)&nice_candidate_free);
  }

  if(sdp) {
    session_description_destroy(sdp);
  }
  else {
    free(remote_sdp);
  }

}


int peer_connection_send_rtp_packet(PeerConnection *pc, uint8_t *packet, int bytes) {

  dtls_transport_encrypt_rtp_packet(pc->dtls_transport, packet, &bytes);
  int sent = nice_agent_send(pc->nice_agent, pc->stream_id, pc->component_id, bytes, (gchar*)packet);
  if(sent < bytes) {
    LOG_ERROR("only sent %d bytes? (was %d)\n", sent, bytes);
  }
  return sent;

}

void peer_connection_set_on_transport_ready(PeerConnection *pc, void (*on_transport_ready), void *data) {

  pc->on_transport_ready = on_transport_ready;
  pc->on_transport_ready_userdata = data;

}

void peer_connection_onicecandidate(PeerConnection *pc, void (*onicecandidate), void  *userdata) {

  pc->onicecandidate = onicecandidate;
  pc->onicecandidate_userdata = userdata;

}

void peer_connection_oniceconnectionstatechange(PeerConnection *pc,
  void (*oniceconnectionstatechange), void *userdata) {

  pc->oniceconnectionstatechange = oniceconnectionstatechange;
  pc->oniceconnectionstatechange_userdata = userdata;

}

void peer_connection_ontrack(PeerConnection *pc, void (*ontrack), void *userdata) {

  pc->ontrack = ontrack;
  pc->ontrack_userdata = userdata;

}

int peer_connection_add_transceiver(PeerConnection *pc, transceiver_t transceiver) {
  //pc->transceiver = transceiver;
  pc->direction = transceiver.video;
}

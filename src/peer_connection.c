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

  gboolean mdns_enabled;

  uint32_t audio_ssrc, video_ssrc;

  DtlsTransport *dtls_transport;
  SessionDescription *sdp;
  Transceiver transceiver;
  MediaStream *media_stream;
  RtpMap rtp_map;

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

  if(pc->media_stream->tracks_num > 1) {
    session_description_append(sdp, "a=group:BUNDLE 0 1");

    session_description_add_codec(sdp, pc->media_stream->audio_codec, pc->transceiver.audio, local_ufrag, local_password, dtls_transport_get_fingerprint(pc->dtls_transport), 0);

    session_description_add_codec(sdp, pc->media_stream->video_codec, pc->transceiver.video, local_ufrag, local_password, dtls_transport_get_fingerprint(pc->dtls_transport), 1);

  }
  else {
    session_description_append(sdp, "a=group:BUNDLE 0");

    session_description_add_codec(sdp, pc->media_stream->audio_codec, pc->transceiver.audio, local_ufrag, local_password, dtls_transport_get_fingerprint(pc->dtls_transport), 0);

    session_description_add_codec(sdp, pc->media_stream->video_codec, pc->transceiver.video, local_ufrag, local_password, dtls_transport_get_fingerprint(pc->dtls_transport), 0);


  }

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

    char *sdp_content = session_description_get_content(pc->sdp);
    pc->onicecandidate(sdp_content, pc->onicecandidate_userdata);

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
  memset(pc, 0, sizeof(*pc));
  if(pc == NULL)
    return pc;

  pc->mdns_enabled = FALSE;

  pc->audio_ssrc = 0;
  pc->video_ssrc = 0;

  pc->onicecandidate = NULL;
  pc->onicecandidate_userdata = NULL;

  pc->oniceconnectionstatechange = NULL;
  pc->oniceconnectionstatechange_userdata = NULL;

  pc->on_transport_ready = NULL;
  pc->on_transport_ready_userdata = NULL;
  pc->transceiver.video = SENDONLY;
  pc->transceiver.audio = SENDONLY;

  if(peer_connection_nice_agent_setup(pc) == FALSE) {
    peer_connection_destroy(pc);
    return NULL;
  }

  pc->dtls_transport = dtls_transport_create(nice_agent_bio_new(pc->nice_agent, pc->stream_id, pc->component_id));

  pc->sdp = session_description_create();

  return pc;
}

void peer_connection_enable_mdns(PeerConnection *pc, gboolean b_enabled) {

  pc->mdns_enabled = b_enabled;
}

void peer_connection_destroy(PeerConnection *pc) {

  if(pc == NULL)
    return;

  g_main_loop_quit(pc->gloop);

  g_thread_join(pc->gthread);

  g_main_loop_unref(pc->gloop);

  if(pc->nice_agent)
    g_object_unref(pc->nice_agent);

  if(pc->dtls_transport)
    dtls_transport_destroy(pc->dtls_transport);

  if(pc->media_stream)
    media_stream_free(pc->media_stream);

  if(pc->sdp)
    session_description_destroy(pc->sdp);

  free(pc);
  pc = NULL;
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

void peer_connection_set_remote_description(PeerConnection *pc, char *remote_sdp) {

  gsize len;
  gchar* ufrag = NULL;
  gchar* pwd = NULL;
  GSList *plist;
  int i;

  if(!remote_sdp) return;

  pc->audio_ssrc = session_description_find_ssrc("audio", remote_sdp);
  pc->video_ssrc = session_description_find_ssrc("video", remote_sdp);

  // Remove mDNS
  SessionDescription *sdp = NULL;
  if(strstr(remote_sdp, "local") != NULL) {

    sdp = session_description_create();
    gchar **splits;

    splits = g_strsplit(remote_sdp, "\r\n", 256);
    for(i = 0; splits[i] != NULL; i++) {

      if(strstr(splits[i], "candidate") != NULL && strstr(splits[i], "local") != NULL) {

        if(pc->mdns_enabled) {
          char buf[256] = {0};
          if(session_description_update_mdns_of_candidate(splits[i], buf, sizeof(buf)) != -1) {
            session_description_append_newline(sdp, buf);
          }
        }
      }
      else {
        session_description_append_newline(sdp, splits[i]);
      }
    }

    remote_sdp = session_description_get_content(sdp);
  }

  pc->rtp_map = session_description_parse_rtpmap(remote_sdp);

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

  if(sdp)
    session_description_destroy(sdp);
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

int peer_connection_add_transceiver(PeerConnection *pc, Transceiver transceiver) {
  pc->transceiver = transceiver;

}

uint32_t peer_connection_get_ssrc(PeerConnection *pc, const char *type) {

  if(strcmp(type, "audio") == 0) {
    return pc->audio_ssrc;
  }
  else if(strcmp(type, "video") == 0) {
    return pc->video_ssrc;
  }

  return 0;
}

int peer_connection_get_rtpmap(PeerConnection *pc, MediaCodec codec) {

  switch(codec) {

    case CODEC_H264:
      return pc->rtp_map.pt_h264;
    case CODEC_OPUS:
      return pc->rtp_map.pt_opus;
    case CODEC_PCMA:
      return pc->rtp_map.pt_pcma;
    default:
     return -1;
  }

   return -1;
}

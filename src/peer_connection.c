#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gio/gnetworking.h>

#include <gst/gst.h>
#include "sctp.h"
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

typedef struct {

  uint8_t h264:1;
  uint8_t vp8:1;
  uint8_t opus:1;
  uint8_t pcma:1;
  uint8_t pcmu:1;

} CodecCapability;

struct PeerConnection {

  NiceAgent *nice_agent;
  gboolean controlling;
  guint stream_id;
  guint component_id;
  GMainLoop *gloop;
  GThread *gthread;

  CodecCapability codec_capability;
  int mdns_enabled;

  uint32_t audio_ssrc, video_ssrc;

  SessionDescription *remote_sdp;
  SessionDescription *local_sdp;

  Sctp *sctp;
  DtlsTransport *dtls_transport;
  RtpMap rtp_map;

  void (*onicecandidate)(char *sdp, void *userdata);
  void (*oniceconnectionstatechange)(IceConnectionState state, void *userdata);
  void (*ontrack)(uint8_t *packet, size_t bytes, void *userdata);
  void (*on_connected)(void *userdata);
  void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void *userdata);

  void *userdata;

  GMutex mutex;

  Buffer *outgoing_rb;

  MediaStream *audio_media_stream;
  MediaStream *video_media_stream;

  GCancellable *cancellable;
  GTask *task; 
};

static void peer_connection_task(GTask *task,
 gpointer source_object, gpointer userdata, GCancellable *cancellable) {

  size_t size;
  uint8_t data[1500];

  PeerConnection *pc = (PeerConnection*)userdata;

  while (!g_cancellable_is_cancelled(cancellable)) {

    if (pc->video_media_stream && (utils_buffer_pop(pc->video_media_stream->outgoing_rb, (uint8_t*)&size, sizeof(size_t)) == sizeof(size_t))) {

      if (size > 0 && utils_buffer_pop(pc->video_media_stream->outgoing_rb, data, size) == size) {

        peer_connection_send_rtp_packet(pc, data, size);
      }
    } else if (pc->audio_media_stream && (utils_buffer_pop(pc->audio_media_stream->outgoing_rb, (uint8_t*)&size, sizeof(size_t)) == sizeof(size_t))) {

      if (size > 0 && utils_buffer_pop(pc->audio_media_stream->outgoing_rb, data, size) == size) {
     
        peer_connection_send_rtp_packet(pc, data, size);
      }

    } else {

      usleep(10000);
    }
  }

  LOGD("peer_connection_task exit");
}

static void peer_connection_start_task(PeerConnection *pc) {

  LOGD("peer_connection_start_task");

  pc->cancellable = g_cancellable_new();

  pc->task = g_task_new(NULL, pc->cancellable, NULL, NULL);

  g_task_set_task_data(pc->task, pc, NULL);

  g_task_run_in_thread(pc->task, peer_connection_task);

}

static void peer_connection_stop_task(PeerConnection *pc) {

  LOGD("peer_connection_stop_task");

  g_cancellable_cancel(pc->cancellable);

  g_task_propagate_boolean(pc->task, NULL);

  g_object_unref(pc->cancellable);

  g_object_unref(pc->task);

}

void* peer_connection_gather_thread(void *userdata) {

  PeerConnection *pc = (PeerConnection*)userdata;

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
    pc->oniceconnectionstatechange(state, pc->userdata);
  }

}

static void peer_connection_candidates_to_sdp(PeerConnection *pc, SessionDescription *sdp) {

  GSList *nice_candidates = NULL;
  NiceCandidate *nice_candidate;
  char nice_candidate_addr[INET6_ADDRSTRLEN];
  int i = 0;

  nice_candidates = nice_agent_get_local_candidates(pc->nice_agent,
   pc->stream_id, pc->component_id);

  for(i = 0; i < g_slist_length(nice_candidates); ++i) {

    nice_candidate = (NiceCandidate *)g_slist_nth(nice_candidates, i)->data;
    nice_address_to_string(&nice_candidate->addr, nice_candidate_addr);
    if(utils_is_valid_ip_address(nice_candidate_addr) > 0) {
      nice_candidate_free(nice_candidate);
      continue;
    }

    session_description_append(sdp, "a=candidate:%s 1 udp %u %s %d typ host",
     nice_candidate->foundation,
     nice_candidate->priority,
     nice_candidate_addr,
     nice_address_get_port(&nice_candidate->addr));

    nice_candidate_free(nice_candidate);
  }

  if(nice_candidates)
    g_slist_free(nice_candidates);
}

static void peer_connection_video_to_sdp(PeerConnection *pc, SessionDescription *sdp, uint32_t ssrc) {

  session_description_append(sdp, "m=video 9 UDP/TLS/RTP/SAVPF 96 102");

  if(pc->codec_capability.h264) {

    session_description_append(sdp, "a=rtcp-fb:102 nack");
    session_description_append(sdp, "a=rtcp-fb:102 nack pli");
    session_description_append(sdp, "a=fmtp:96 profile-level-id=42e01f;level-asymmetry-allowed=1");
    session_description_append(sdp, "a=fmtp:102 profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1");
    session_description_append(sdp, "a=fmtp:102 x-google-max-bitrate=6000;x-google-min-bitrate=2000;x-google-start-bitrate=4000");
    session_description_append(sdp, "a=rtpmap:96 H264/90000");
    session_description_append(sdp, "a=rtpmap:102 H264/90000");
  }

  session_description_append(sdp, "a=ssrc:%d cname:pear", ssrc);
  session_description_append(sdp, "a=sendrecv");

}

static void peer_connection_audio_to_sdp(PeerConnection *pc, SessionDescription *sdp, uint32_t ssrc) {

  if(pc->codec_capability.opus) {

    session_description_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 111");
    session_description_append(sdp, "a=rtcp-fb:111 nack");
    session_description_append(sdp, "a=rtpmap:111 opus/48000/2");
  }
  else if(pc->codec_capability.pcma) {

    session_description_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 8");
    session_description_append(sdp, "a=rtpmap:8 PCMA/8000");
  }

  session_description_append(sdp, "a=ssrc:%d cname:pear", ssrc);
  session_description_append(sdp, "a=sendrecv");
}

static void peer_connection_datachannel_to_sdp(PeerConnection *pc, SessionDescription *sdp) {

  session_description_append(sdp, "m=application 50712 UDP/DTLS/SCTP webrtc-datachannel");
  session_description_append(sdp, "a=sctp-port:5000");
  session_description_append(sdp, "a=max-message-size:262144");
}

static void* peer_connection_candidate_gathering_done_cb(NiceAgent *agent, guint stream_id,
 gpointer data) {

  PeerConnection *pc = (PeerConnection*)data;

  MediaDescription *media_descriptions;

  gchar *local_ufrag = NULL;
  gchar *local_password = NULL;
  gchar ipaddr[INET6_ADDRSTRLEN];

  int i = 0;
  int num = 0;
  char attribute_text[128];
  char bundle_text[64];

  if(pc->local_sdp) {
    session_description_destroy(pc->local_sdp);
  }

  pc->local_sdp = session_description_create(NULL);
  SessionDescription *sdp = pc->local_sdp;

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
  media_descriptions = session_description_get_media_descriptions(pc->remote_sdp, &num);

  memset(attribute_text, 0, sizeof(attribute_text));
  for(i = 0; i < num; i++) {

    memset(bundle_text, 0, sizeof(bundle_text));
    sprintf(bundle_text, " %d", i);
    strcat(attribute_text, bundle_text);
  }

  session_description_append(sdp, "a=group:BUNDLE%s", attribute_text);

  for(i = 0; i < num; i++) {

    switch(media_descriptions[i]) {
      case MEDIA_VIDEO:
        peer_connection_video_to_sdp(pc, sdp, (i+1));
 
        media_stream_set_ssrc(pc->video_media_stream, i+1);
        break;
      case MEDIA_AUDIO:
        peer_connection_audio_to_sdp(pc, sdp, (i+1));

        media_stream_set_ssrc(pc->audio_media_stream, i+1);
        break;
      case MEDIA_DATACHANNEL:
        peer_connection_datachannel_to_sdp(pc, sdp);
        break;
      default:
        break;
    }

    session_description_append(sdp, "a=mid:%d", i);
    session_description_append(sdp, "c=IN IP4 0.0.0.0");
    session_description_append(sdp, "a=rtcp-mux");
    session_description_append(sdp, "a=ice-ufrag:%s", local_ufrag);
    session_description_append(sdp, "a=ice-pwd:%s", local_password);
    session_description_append(sdp, "a=ice-options:trickle");
    session_description_append(sdp, "a=fingerprint:sha-256 %s",
     dtls_transport_get_fingerprint(pc->dtls_transport));
    session_description_append(sdp, "a=setup:passive");
    peer_connection_candidates_to_sdp(pc, sdp);
  }

  if(local_ufrag)
    free(local_ufrag);

  if(local_password)
    free(local_password);

  if(pc->onicecandidate != NULL) {

    char *sdp_content = session_description_get_content(pc->local_sdp);
    pc->onicecandidate(sdp_content, pc->userdata);
  }

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

void peer_connection_incomming_rtcp(PeerConnection *pc, uint8_t *buf, size_t len) {

  RtcpHeader rtcp_header = {0};
  memcpy(&rtcp_header, buf, sizeof(rtcp_header));
  switch(rtcp_header.type) {
    case RTCP_RR:
      if(rtcp_header.rc > 0) {
        RtcpRr rtcp_rr = rtcp_packet_parse_rr(buf);
        uint32_t fraction = ntohl(rtcp_rr.report_block[0].flcnpl) >> 24;
        uint32_t total = ntohl(rtcp_rr.report_block[0].flcnpl) & 0x00FFFFFF;
        if(pc->on_receiver_packet_loss && fraction > 0) {
          pc->on_receiver_packet_loss((float)fraction/256.0, total, pc->userdata);
        }
      }
      break;
    default:
      break;
  }
}

void peer_connection_media_stream_playback(PeerConnection *pc) {

  LOGD("peer_connection_media_stream_playback");

  int pt;

  MediaCodec codec;
  MediaStream *media_stream;

  media_stream = pc->audio_media_stream;

  if(media_stream) {

    codec = media_stream_get_codec(media_stream);
    pt = peer_connection_get_rtpmap(pc, codec);
    media_stream_set_payloadtype(media_stream, pt);
    media_stream_play(media_stream);
  }

  media_stream = pc->video_media_stream;

  if(media_stream) {

    codec = media_stream_get_codec(media_stream);
    pt = peer_connection_get_rtpmap(pc, codec);
    media_stream_set_payloadtype(media_stream, pt);
    media_stream_play(media_stream);
  }

}

static void peer_connection_ice_recv_cb(NiceAgent *agent, guint stream_id, guint component_id,
 guint len, gchar *buf, gpointer data) {

  PeerConnection *pc = (PeerConnection*)data;
  int ret;
  char decrypted_data[3000];
  if(rtcp_packet_validate(buf, len)) {

    dtls_transport_decrypt_rtcp_packet(pc->dtls_transport, buf, &len);
    peer_connection_incomming_rtcp(pc, buf, len);
  }
  else if(dtls_transport_validate(buf)) {

    if(!dtls_transport_get_srtp_initialized(pc->dtls_transport)) {

      dtls_transport_incomming_msg(pc->dtls_transport, buf, len);

      if(dtls_transport_get_srtp_initialized(pc->dtls_transport)) {

        if(pc->on_connected)
          pc->on_connected(pc->userdata);

        peer_connection_media_stream_playback(pc);
        peer_connection_start_task(pc);
      }

      if (pc->remote_sdp->datachannel_enabled) {
        sctp_create_socket(pc->sctp);
      }

    } else {

      ret = dtls_transport_decrypt_data(pc->dtls_transport, buf, len, decrypted_data, sizeof(decrypted_data));
      sctp_incoming_data(pc->sctp, decrypted_data, ret);
    }

  } else if (rtp_packet_validate(buf, len)) {

    dtls_transport_decrypt_rtp_packet(pc->dtls_transport, buf, &len);

    if (pc->ontrack != NULL) {
      pc->ontrack(buf, len, pc->userdata);
    }

    if (pc->audio_media_stream) {
      media_stream_ontrack(pc->audio_media_stream, buf, len);
    }

    if (pc->video_media_stream) {
      media_stream_ontrack(pc->video_media_stream, buf, len);
    }

  }

}

gboolean callback(gpointer arg) {

  printf("test...\n");

}

gboolean peer_connection_nice_agent_setup(PeerConnection *pc) {

  pc->gloop = g_main_loop_new(NULL, FALSE);

//  g_idle_add(callback, NULL);

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

  pc->gthread = g_thread_new("ice gather thread", peer_connection_gather_thread, pc);

  return TRUE;
}

PeerConnection* peer_connection_create(void *userdata) {

  PeerConnection *pc = NULL;
  pc = (PeerConnection*)calloc(1, sizeof(PeerConnection));
  memset(pc, 0, sizeof(*pc));
  if(pc == NULL)
    return pc;

  gst_init(NULL, NULL);

  pc->audio_ssrc = 0;
  pc->video_ssrc = 0;

  if(peer_connection_nice_agent_setup(pc) == FALSE) {
    peer_connection_destroy(pc);
    return NULL;
  }

  pc->dtls_transport = dtls_transport_create(nice_agent_bio_new(pc->nice_agent, pc->stream_id, pc->component_id));

  pc->sctp = sctp_create(pc->dtls_transport);

  return pc;
}

void peer_connection_enable_mdns(PeerConnection *pc, int enabled) {

  if(pc->remote_sdp) {
    session_description_set_mdns_enabled(pc->remote_sdp, enabled);
  }
}

void peer_connection_destroy(PeerConnection *pc) {

  int i, n;

  if(pc == NULL)
    return;

  peer_connection_stop_task(pc);

  g_main_loop_quit(pc->gloop);

  g_thread_join(pc->gthread);

  g_main_loop_unref(pc->gloop);

  if(pc->nice_agent)
    g_object_unref(pc->nice_agent);

  if(pc->dtls_transport)
    dtls_transport_destroy(pc->dtls_transport);

  if(pc->local_sdp)
    session_description_destroy(pc->local_sdp);

  if(pc->remote_sdp)
    session_description_destroy(pc->remote_sdp);

  if(pc->audio_media_stream) {

    media_stream_destroy(pc->audio_media_stream);
  }

  if(pc->video_media_stream) {
    media_stream_destroy(pc->video_media_stream);
  }

  free(pc);
  pc = NULL;
}

void peer_connection_add_stream(PeerConnection *pc, MediaCodec codec,
 const char *outgoing_pipeline, const char *incoming_pipeline) {

  MediaStream **media_stream = NULL;

  switch(codec) {

    case CODEC_H264:
      pc->codec_capability.h264 = 1;
      media_stream = &pc->video_media_stream;
      break;
    case CODEC_VP8:
      pc->codec_capability.vp8 = 1;
      media_stream = &pc->video_media_stream;
      break;
    case CODEC_OPUS:
      pc->codec_capability.opus = 1;
      media_stream = &pc->audio_media_stream;
      break;
    case CODEC_PCMA:
      pc->codec_capability.pcma = 1;
      media_stream = &pc->audio_media_stream;
      break;
    case CODEC_PCMU:
      pc->codec_capability.pcmu = 1;
      media_stream = &pc->audio_media_stream;
      break;
    default:
      break;
  }

  if (*media_stream != NULL) {

    media_stream_destroy(*media_stream);
  }

  *media_stream = media_stream_create(codec, outgoing_pipeline, incoming_pipeline);
}

int peer_connection_create_answer(PeerConnection *pc) {

  if(!nice_agent_gather_candidates(pc->nice_agent, pc->stream_id)) {
    LOG_ERROR("Failed to start candidate gathering");
    return -1;
  }
  return 0;
}

void peer_connection_set_remote_description(PeerConnection *pc, const char *sdp_text) {

  gsize len;
  gchar* ufrag = NULL;
  gchar* pwd = NULL;
  GSList *plist;
  int i;

  if(!sdp_text) {

    LOG_WARN("Remote SDP is empty");
    return;
  }

  pc->audio_ssrc = session_description_find_ssrc("audio", sdp_text);
  pc->video_ssrc = session_description_find_ssrc("video", sdp_text);

  if(pc->remote_sdp) {
    session_description_destroy(pc->remote_sdp);
  }

  pc->remote_sdp = session_description_create(sdp_text);

  pc->rtp_map = session_description_get_rtpmap(pc->remote_sdp);

  sdp_text = session_description_get_content(pc->remote_sdp);

  plist = nice_agent_parse_remote_stream_sdp(pc->nice_agent, pc->component_id, sdp_text, &ufrag, &pwd);

  if(ufrag && pwd && g_slist_length(plist) > 0) {

    ufrag[strlen(ufrag) - 1] = '\0';
    pwd[strlen(pwd) - 1] = '\0';
    NiceCandidate* c = (NiceCandidate*)g_slist_nth(plist, 0)->data;

    if(!nice_agent_set_remote_credentials(pc->nice_agent, 1, ufrag, pwd)) {

      LOG_WARN("failed to set remote credentials");
    }

    if(nice_agent_set_remote_candidates(pc->nice_agent, pc->stream_id,
     pc->component_id, plist) < 1) {
 
     LOG_WARN("failed to set remote candidates");
    }

    g_free(ufrag);
    g_free(pwd);
    g_slist_free_full(plist, (GDestroyNotify)&nice_candidate_free);
  }

}

int peer_connection_datachannel_send(PeerConnection *pc, char *message, size_t len) {

  if(sctp_is_connected(pc->sctp))
    return sctp_outgoing_data(pc->sctp, message, len);

  return -1;
}

int peer_connection_send_rtp_packet(PeerConnection *pc, uint8_t *packet, int bytes) {

  dtls_transport_encrypt_rtp_packet(pc->dtls_transport, packet, &bytes);
  int sent = nice_agent_send(pc->nice_agent, pc->stream_id, pc->component_id, bytes, (gchar*)packet);
  if(sent < bytes) {
    LOG_ERROR("only sent %d bytes? (was %d)", sent, bytes);
  }
  return sent;

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

// callbacks
void peer_connection_on_connected(PeerConnection *pc, void (*on_connected)(void *userdata)) {

  pc->on_connected = on_connected;
}

void peer_connection_on_receiver_packet_loss(PeerConnection *pc,
 void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void *userdata)) {

  pc->on_receiver_packet_loss = on_receiver_packet_loss;
}

void peer_connection_onicecandidate(PeerConnection *pc, void (*onicecandidate)(char *sdp_text, void *userdata)) {

  pc->onicecandidate = onicecandidate;
}

void peer_connection_oniceconnectionstatechange(PeerConnection *pc,
 void (*oniceconnectionstatechange)(IceConnectionState state, void *userdata)) {

  pc->oniceconnectionstatechange = oniceconnectionstatechange;

}

void peer_connection_ontrack(PeerConnection *pc, void (*ontrack)(uint8_t *packet, size_t byte, void *userdata)) {

  pc->ontrack = ontrack;
}

void peer_connection_ondatachannel(PeerConnection *pc,
 void (*onmessasge)(char *msg, size_t len, void *userdata),
 void (*onopen)(void *userdata),
 void (*onclose)(void *userdata)) {

  if(pc) {

    sctp_onopen(pc->sctp, onopen);
    sctp_onclose(pc->sctp, onclose);
    sctp_onmessage(pc->sctp, onmessasge);
  }
}


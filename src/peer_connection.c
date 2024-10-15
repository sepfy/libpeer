#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "agent.h"
#include "buffer.h"
#include "config.h"
#include "dtls_srtp.h"
#include "peer_connection.h"
#include "ports.h"
#include "rtcp.h"
#include "rtp.h"
#include "sctp.h"
#include "sdp.h"

#define STATE_CHANGED(pc, curr_state)                                 \
  if (pc->oniceconnectionstatechange && pc->state != curr_state) {    \
    pc->oniceconnectionstatechange(curr_state, pc->config.user_data); \
    pc->state = curr_state;                                           \
  }

struct PeerConnection {
  PeerConfiguration config;
  PeerConnectionState state;
  Agent agent;
  DtlsSrtp dtls_srtp;
  Sctp sctp;

  Sdp local_sdp;
  Sdp remote_sdp;

  void (*onicecandidate)(char* sdp, void* user_data);
  void (*oniceconnectionstatechange)(PeerConnectionState state, void* user_data);
  void (*on_connected)(void* userdata);
  void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void* user_data);

  uint8_t temp_buf[CONFIG_MTU];
  uint8_t agent_buf[CONFIG_MTU];
  int agent_ret;
  int b_local_description_created;

  Buffer* audio_rb;
  Buffer* video_rb;
  Buffer* data_rb;

  RtpEncoder artp_encoder;
  RtpEncoder vrtp_encoder;
  RtpDecoder vrtp_decoder;
  RtpDecoder artp_decoder;

  uint32_t remote_assrc;
  uint32_t remote_vssrc;
};

static void peer_connection_outgoing_rtp_packet(uint8_t* data, size_t size, void* user_data) {
  PeerConnection* pc = (PeerConnection*)user_data;
  dtls_srtp_encrypt_rtp_packet(&pc->dtls_srtp, data, (int*)&size);
  agent_send(&pc->agent, data, size);
}

static int peer_connection_dtls_srtp_recv(void* ctx, unsigned char* buf, size_t len) {
  static const int MAX_RECV = 200;
  int recv_max = 0;
  int ret;
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  PeerConnection* pc = (PeerConnection*)dtls_srtp->user_data;

  if (pc->agent_ret > 0 && pc->agent_ret <= len) {
    memcpy(buf, pc->agent_buf, pc->agent_ret);
    return pc->agent_ret;
  }

  while (recv_max < MAX_RECV) {
    ret = agent_recv(&pc->agent, buf, len);

    if (ret > 0) {
      break;
    }

    recv_max++;
  }
  return ret;
}

static int peer_connection_dtls_srtp_send(void* ctx, const uint8_t* buf, size_t len) {
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  PeerConnection* pc = (PeerConnection*)dtls_srtp->user_data;

  // LOGD("send %.4x %.4x, %ld", *(uint16_t*)buf, *(uint16_t*)(buf + 2), len);
  return agent_send(&pc->agent, buf, len);
}

static void peer_connection_incoming_rtcp(PeerConnection* pc, uint8_t* buf, size_t len) {
  RtcpHeader* rtcp_header;
  size_t pos = 0;

  while (pos < len) {
    rtcp_header = (RtcpHeader*)(buf + pos);

    switch (rtcp_header->type) {
      case RTCP_RR:
        LOGD("RTCP_PR");
        if (rtcp_header->rc > 0) {
// TODO: REMB, GCC ...etc
#if 0
          RtcpRr rtcp_rr = rtcp_parse_rr(buf);
          uint32_t fraction = ntohl(rtcp_rr.report_block[0].flcnpl) >> 24;
          uint32_t total = ntohl(rtcp_rr.report_block[0].flcnpl) & 0x00FFFFFF;
          if(pc->on_receiver_packet_loss && fraction > 0) {

            pc->on_receiver_packet_loss((float)fraction/256.0, total, pc->config.user_data);
          }
#endif
        }
        break;
      case RTCP_PSFB: {
        int fmt = rtcp_header->rc;
        LOGD("RTCP_PSFB %d", fmt);
        // PLI and FIR
        if ((fmt == 1 || fmt == 4) && pc->config.on_request_keyframe) {
          pc->config.on_request_keyframe(pc->config.user_data);
        }
      }
      default:
        break;
    }

    pos += 4 * ntohs(rtcp_header->length) + 4;
  }
}

const char* peer_connection_state_to_string(PeerConnectionState state) {
  switch (state) {
    case PEER_CONNECTION_NEW:
      return "new";
    case PEER_CONNECTION_CHECKING:
      return "checking";
    case PEER_CONNECTION_CONNECTED:
      return "connected";
    case PEER_CONNECTION_COMPLETED:
      return "completed";
    case PEER_CONNECTION_FAILED:
      return "failed";
    case PEER_CONNECTION_CLOSED:
      return "closed";
    case PEER_CONNECTION_DISCONNECTED:
      return "disconnected";
    default:
      return "unknown";
  }
}

PeerConnectionState peer_connection_get_state(PeerConnection* pc) {
  return pc->state;
}

void* peer_connection_get_sctp(PeerConnection* pc) {
  return &pc->sctp;
}

PeerConnection* peer_connection_create(PeerConfiguration* config) {
  PeerConnection* pc = calloc(1, sizeof(PeerConnection));
  if (!pc) {
    return NULL;
  }

  memcpy(&pc->config, config, sizeof(PeerConfiguration));

  agent_create(&pc->agent);

  memset(&pc->sctp, 0, sizeof(pc->sctp));

  if (pc->config.datachannel) {
#if (CONFIG_DATA_BUFFER_SIZE) > 0
    LOGI("Datachannel allocates heap size: %d", CONFIG_DATA_BUFFER_SIZE);
    pc->data_rb = buffer_new(CONFIG_DATA_BUFFER_SIZE);
#endif
  }

  if (pc->config.audio_codec) {
#if (CONFIG_AUDIO_BUFFER_SIZE) > 0
    LOGI("Audio allocates heap size: %d", CONFIG_AUDIO_BUFFER_SIZE);
    pc->audio_rb = buffer_new(CONFIG_AUDIO_BUFFER_SIZE);
#endif

    rtp_encoder_init(&pc->artp_encoder, pc->config.audio_codec,
                     peer_connection_outgoing_rtp_packet, (void*)pc);

    rtp_decoder_init(&pc->artp_decoder, pc->config.audio_codec,
                     pc->config.onaudiotrack, pc->config.user_data);
  }

  if (pc->config.video_codec) {
#if (CONFIG_VIDEO_BUFFER_SIZE) > 0
    LOGI("Video allocates heap size: %d", CONFIG_VIDEO_BUFFER_SIZE);
    pc->video_rb = buffer_new(CONFIG_VIDEO_BUFFER_SIZE);
#endif
    rtp_encoder_init(&pc->vrtp_encoder, pc->config.video_codec,
                     peer_connection_outgoing_rtp_packet, (void*)pc);

    rtp_decoder_init(&pc->vrtp_decoder, pc->config.video_codec,
                     pc->config.onvideotrack, pc->config.user_data);
  }

  return pc;
}

void peer_connection_destroy(PeerConnection* pc) {
  if (pc) {
    agent_destroy(&pc->agent);
    buffer_free(pc->data_rb);
    buffer_free(pc->audio_rb);
    buffer_free(pc->video_rb);

    free(pc);
    pc = NULL;
  }
}

void peer_connection_close(PeerConnection* pc) {
  pc->state = PEER_CONNECTION_CLOSED;
}

int peer_connection_send_audio(PeerConnection* pc, const uint8_t* buf, size_t len) {
  if (pc->state != PEER_CONNECTION_COMPLETED) {
    // LOGE("dtls_srtp not connected");
    return -1;
  }
#if (CONFIG_AUDIO_BUFFER_SIZE) > 0
  return buffer_push_tail(pc->audio_rb, buf, len);
#else
  return rtp_encoder_encode(&pc->artp_encoder, buf, len);
#endif
}

int peer_connection_send_video(PeerConnection* pc, const uint8_t* buf, size_t len) {
  if (pc->state != PEER_CONNECTION_COMPLETED) {
    // LOGE("dtls_srtp not connected");
    return -1;
  }
#if (CONFIG_VIDEO_BUFFER_SIZE) > 0
  return buffer_push_tail(pc->video_rb, buf, len);
#else
  return rtp_encoder_encode(&pc->vrtp_encoder, buf, len);
#endif
}

int peer_connection_datachannel_send(PeerConnection* pc, char* message, size_t len) {
  return peer_connection_datachannel_send_sid(pc, message, len, 0);
}

int peer_connection_datachannel_send_sid(PeerConnection* pc, char* message, size_t len, uint16_t sid) {
  if (!sctp_is_connected(&pc->sctp)) {
    LOGE("sctp not connected");
    return -1;
  }

#if (CONFIG_DATA_BUFFER_SIZE) > 0
  return buffer_push_tail(pc->data_rb, (uint8_t*)message, len);
#else
  if (pc->config.datachannel == DATA_CHANNEL_STRING)
    return sctp_outgoing_data(&pc->sctp, message, len, PPID_STRING, sid);
  else
    return sctp_outgoing_data(&pc->sctp, message, len, PPID_BINARY, sid);
#endif
}

static char* peer_connection_dtls_role_setup_value(DtlsSrtpRole d) {
  return d == DTLS_SRTP_ROLE_SERVER ? "a=setup:passive" : "a=setup:active";
}

static void peer_connection_state_new(PeerConnection* pc, DtlsSrtpRole role, int isOfferer) {
  char* description = (char*)pc->temp_buf;

  memset(pc->temp_buf, 0, sizeof(pc->temp_buf));

  dtls_srtp_reset_session(&pc->dtls_srtp);
  dtls_srtp_init(&pc->dtls_srtp, role, pc);
  pc->dtls_srtp.udp_recv = peer_connection_dtls_srtp_recv;
  pc->dtls_srtp.udp_send = peer_connection_dtls_srtp_send;

  pc->sctp.connected = 0;

  if (isOfferer) {
    agent_clear_candidates(&pc->agent);
    pc->agent.mode = AGENT_MODE_CONTROLLING;
  } else {
    pc->agent.mode = AGENT_MODE_CONTROLLED;
  }

  agent_gather_candidate(&pc->agent, NULL, NULL, NULL);  // host address
  for (int i = 0; i < sizeof(pc->config.ice_servers) / sizeof(pc->config.ice_servers[0]); ++i) {
    if (pc->config.ice_servers[i].urls) {
      LOGI("ice server: %s", pc->config.ice_servers[i].urls);
      agent_gather_candidate(&pc->agent, pc->config.ice_servers[i].urls, pc->config.ice_servers[i].username, pc->config.ice_servers[i].credential);
    }
  }

  agent_get_local_description(&pc->agent, description, sizeof(pc->temp_buf));

  memset(&pc->local_sdp, 0, sizeof(pc->local_sdp));
  // TODO: check if we have video or audio codecs
  sdp_create(&pc->local_sdp,
             pc->config.video_codec != CODEC_NONE,
             pc->config.audio_codec != CODEC_NONE,
             pc->config.datachannel);

  if (pc->config.video_codec == CODEC_H264) {
    sdp_append_h264(&pc->local_sdp);
    sdp_append(&pc->local_sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
    sdp_append(&pc->local_sdp, peer_connection_dtls_role_setup_value(role));
    strcat(pc->local_sdp.content, description);
  }

  switch (pc->config.audio_codec) {
    case CODEC_PCMA:

      sdp_append_pcma(&pc->local_sdp);
      sdp_append(&pc->local_sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
      sdp_append(&pc->local_sdp, peer_connection_dtls_role_setup_value(role));
      strcat(pc->local_sdp.content, description);
      break;

    case CODEC_PCMU:

      sdp_append_pcmu(&pc->local_sdp);
      sdp_append(&pc->local_sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
      sdp_append(&pc->local_sdp, peer_connection_dtls_role_setup_value(role));
      strcat(pc->local_sdp.content, description);
      break;

    case CODEC_OPUS:
      sdp_append_opus(&pc->local_sdp);
      sdp_append(&pc->local_sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
      sdp_append(&pc->local_sdp, peer_connection_dtls_role_setup_value(role));
      strcat(pc->local_sdp.content, description);

    default:
      break;
  }

  if (pc->config.datachannel) {
    sdp_append_datachannel(&pc->local_sdp);
    sdp_append(&pc->local_sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
    sdp_append(&pc->local_sdp, peer_connection_dtls_role_setup_value(role));
    strcat(pc->local_sdp.content, description);
  }

  pc->b_local_description_created = 1;

  if (pc->onicecandidate) {
    pc->onicecandidate(pc->local_sdp.content, pc->config.user_data);
  }
}

int peer_connection_loop(PeerConnection* pc) {
  peer_connection_sender_loop(PEER_VIDEOSEND_TASK, pc);
  peer_connection_common_loop(pc);
  peer_connection_sender_loop(PEER_DATASEND_TASK, pc);
  peer_connection_sender_loop(PEER_AUDIOSEND_TASK, pc);
  return 0;
}

int peer_connection_sender_loop(PeerLoopTaskType peerLoopTaskType, PeerConnection* pc) {
  if (pc == NULL ||
      pc->state != PEER_CONNECTION_COMPLETED) {
    return 0;
  }

  int bytes;
  uint8_t* data = NULL;
  switch (peerLoopTaskType) {
    case PEER_VIDEOSEND_TASK:
#if (CONFIG_VIDEO_BUFFER_SIZE) > 0
      data = buffer_peak_head(pc->video_rb, &bytes);
      if (data) {
        rtp_encoder_encode(&pc->vrtp_encoder, data, bytes);
        buffer_pop_head(pc->video_rb);
      }
#endif
      break;
    case PEER_AUDIOSEND_TASK:
#if (CONFIG_AUDIO_BUFFER_SIZE) > 0
      data = buffer_peak_head(pc->audio_rb, &bytes);
      if (data) {
        rtp_encoder_encode(&pc->artp_encoder, data, bytes);
        buffer_pop_head(pc->audio_rb);
      }
#endif
      break;
    case PEER_DATASEND_TASK:
#if (CONFIG_DATA_BUFFER_SIZE) > 0
      data = buffer_peak_head(pc->data_rb, &bytes);
      if (data) {
        if (pc->config.datachannel == DATA_CHANNEL_STRING)
          sctp_outgoing_data(&pc->sctp, (char*)data, bytes, PPID_STRING, 0);
        else
          sctp_outgoing_data(&pc->sctp, (char*)data, bytes, PPID_BINARY, 0);
        buffer_pop_head(pc->data_rb);
      }
#endif
      break;
  }

  return 0;
}

int peer_connection_common_loop(PeerConnection* pc) {
  if (pc == NULL) {
    return 0;
  }

  switch (pc->state) {
    case PEER_CONNECTION_NEW:
      if (!pc->b_local_description_created) {
        peer_connection_state_new(pc, DTLS_SRTP_ROLE_SERVER, 1);
      }
      break;

    case PEER_CONNECTION_CHECKING:
      if (agent_select_candidate_pair(&pc->agent) < 0) {
        STATE_CHANGED(pc, PEER_CONNECTION_FAILED);
      } else if (agent_connectivity_check(&pc->agent) == 0) {
        STATE_CHANGED(pc, PEER_CONNECTION_CONNECTED);
      }
      break;

    case PEER_CONNECTION_CONNECTED:
      if (dtls_srtp_handshake(&pc->dtls_srtp, NULL) == 0) {
        LOGD("DTLS-SRTP handshake done");

        if (pc->config.datachannel) {
          LOGI("SCTP create socket");
          sctp_create_socket(&pc->sctp, &pc->dtls_srtp);
          pc->sctp.userdata = pc->config.user_data;
        }

        STATE_CHANGED(pc, PEER_CONNECTION_COMPLETED);
      }
      break;
    case PEER_CONNECTION_COMPLETED:
      pc->agent_ret = -1;
      memset(pc->agent_buf, 0, sizeof(pc->agent_buf));

      if ((pc->agent_ret = agent_recv(&pc->agent, pc->agent_buf, sizeof(pc->agent_buf))) > 0) {
        LOGD("agent_recv %d", pc->agent_ret);

        if (rtcp_probe(pc->agent_buf, pc->agent_ret)) {
          LOGD("Got RTCP packet");
          dtls_srtp_decrypt_rtcp_packet(&pc->dtls_srtp, pc->agent_buf, &pc->agent_ret);
          peer_connection_incoming_rtcp(pc, pc->agent_buf, pc->agent_ret);

        } else if (dtls_srtp_probe(pc->agent_buf)) {
          int ret = dtls_srtp_read(&pc->dtls_srtp, pc->temp_buf, sizeof(pc->temp_buf));
          LOGD("Got DTLS data %d", ret);

          if (ret > 0) {
            sctp_incoming_data(&pc->sctp, (char*)pc->temp_buf, ret);
          }

        } else if (rtp_packet_validate(pc->agent_buf, pc->agent_ret)) {
          LOGD("Got RTP packet");

          dtls_srtp_decrypt_rtp_packet(&pc->dtls_srtp, pc->agent_buf, &pc->agent_ret);

          uint32_t ssrc = rtp_get_ssrc(pc->agent_buf);
          if (ssrc == pc->remote_assrc) {
            rtp_decoder_decode(&pc->artp_decoder, pc->agent_buf, pc->agent_ret);
          } else if (ssrc == pc->remote_vssrc) {
            rtp_decoder_decode(&pc->vrtp_decoder, pc->agent_buf, pc->agent_ret);
          }

        } else {
          LOGW("Unknown data");
        }
      }

      if (KEEPALIVE_CONNCHECK > 0 && (ports_get_epoch_time() - pc->agent.binding_request_time) > KEEPALIVE_CONNCHECK) {
        LOGI("binding request timeout");
        STATE_CHANGED(pc, PEER_CONNECTION_CLOSED);
      }

      break;
    case PEER_CONNECTION_FAILED:
      break;
    case PEER_CONNECTION_DISCONNECTED:
      break;
    case PEER_CONNECTION_CLOSED:
      break;
    default:
      break;
  }

  return 0;
}

void peer_connection_set_remote_description(PeerConnection* pc, const char* sdp_text) {
  char* start = (char*)sdp_text;
  char* line = NULL;
  char buf[256];
  char* val_start = NULL;
  uint32_t* ssrc = NULL;
  DtlsSrtpRole role = DTLS_SRTP_ROLE_SERVER;
  int is_update = 0;
  Agent* agent = &pc->agent;

  while ((line = strstr(start, "\r\n"))) {
    line = strstr(start, "\r\n");
    strncpy(buf, start, line - start);
    buf[line - start] = '\0';

    if (strstr(buf, "a=setup:passive")) {
      role = DTLS_SRTP_ROLE_CLIENT;
    }

    if (strstr(buf, "a=fingerprint")) {
      strncpy(pc->dtls_srtp.remote_fingerprint, buf + 22, DTLS_SRTP_FINGERPRINT_LENGTH);
    }

    if (strstr(buf, "a=ice-ufrag") &&
        strlen(agent->remote_ufrag) != 0 &&
        (strncmp(buf + strlen("a=ice-ufrag:"), agent->remote_ufrag, strlen(agent->remote_ufrag)) == 0)) {
      is_update = 1;
    }

    if (strstr(buf, "m=video")) {
      ssrc = &pc->remote_vssrc;
    } else if (strstr(buf, "m=audio")) {
      ssrc = &pc->remote_assrc;
    }

    if ((val_start = strstr(buf, "a=ssrc:")) && ssrc) {
      *ssrc = strtoul(val_start + 7, NULL, 10);
      LOGD("SSRC: %" PRIu32, *ssrc);
    }

    start = line + 2;
  }

  if (is_update) {
    return;
  }

  if (!pc->b_local_description_created) {
    peer_connection_state_new(pc, role, 0);
  }

  agent_set_remote_description(&pc->agent, (char*)sdp_text);
  STATE_CHANGED(pc, PEER_CONNECTION_CHECKING);
}

void peer_connection_create_offer(PeerConnection* pc) {
  STATE_CHANGED(pc, PEER_CONNECTION_NEW);
  pc->b_local_description_created = 0;
}

int peer_connection_send_rtcp_pil(PeerConnection* pc, uint32_t ssrc) {
  int ret = -1;
  uint8_t plibuf[128];
  rtcp_get_pli(plibuf, 12, ssrc);

  // TODO: encrypt rtcp packet
  // guint size = 12;
  // dtls_transport_encrypt_rctp_packet(pc->dtls_transport, plibuf, &size);
  // ret = nice_agent_send(pc->nice_agent, pc->stream_id, pc->component_id, size, (gchar*)plibuf);

  return ret;
}

// callbacks
void peer_connection_on_connected(PeerConnection* pc, void (*on_connected)(void* userdata)) {
  pc->on_connected = on_connected;
}

void peer_connection_on_receiver_packet_loss(PeerConnection* pc,
                                             void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void* userdata)) {
  pc->on_receiver_packet_loss = on_receiver_packet_loss;
}

void peer_connection_onicecandidate(PeerConnection* pc, void (*onicecandidate)(char* sdp_text, void* userdata)) {
  pc->onicecandidate = onicecandidate;
}

void peer_connection_oniceconnectionstatechange(PeerConnection* pc,
                                                void (*oniceconnectionstatechange)(PeerConnectionState state, void* userdata)) {
  pc->oniceconnectionstatechange = oniceconnectionstatechange;
}

void peer_connection_ondatachannel(PeerConnection* pc,
                                   void (*onmessage)(char* msg, size_t len, void* userdata, uint16_t sid),
                                   void (*onopen)(void* userdata),
                                   void (*onclose)(void* userdata)) {
  if (pc) {
    sctp_onopen(&pc->sctp, onopen);
    sctp_onclose(&pc->sctp, onclose);
    sctp_onmessage(&pc->sctp, onmessage);
  }
}

int peer_connection_lookup_sid(PeerConnection* pc, const char* label, uint16_t* sid) {
  for (int i = 0; i < pc->sctp.stream_count; i++) {
    if (strncmp(pc->sctp.stream_table[i].label, label, sizeof(pc->sctp.stream_table[i].label)) == 0) {
      *sid = pc->sctp.stream_table[i].sid;
      return 0;
    }
  }
  return -1;  // Not found
}

char* peer_connection_lookup_sid_label(PeerConnection* pc, uint16_t sid) {
  for (int i = 0; i < pc->sctp.stream_count; i++) {
    if (pc->sctp.stream_table[i].sid == sid) {
      return pc->sctp.stream_table[i].label;
    }
  }
  return NULL;  // Not found
}

int peer_connection_add_ice_candidate(PeerConnection* pc, char* candidate) {
  Agent* agent = &pc->agent;
  if (ice_candidate_from_description(&agent->remote_candidates[agent->remote_candidates_count], candidate, candidate + strlen(candidate)) != 0) {
    return -1;
  }

  agent->remote_candidates_count++;
  return 0;
}

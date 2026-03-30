#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "agent.h"
#include "config.h"
#include "ice.h"
#include "dtls_srtp.h"
#include "peer_connection.h"
#include "ports.h"
#include "rtcp.h"
#include "rtp.h"
#include "sctp.h"
#include "sdp.h"

// STUN heartbeat interval in seconds (from libpeer PR #205)
#define STUN_HEARTBEAT_INTERVAL 8000  // milliseconds (ports_get_epoch_time returns ms)

// Track last heartbeat time for STUN keepalive
static uint64_t last_heartbeat_time = 0;

// Diagnostic counters for COMPLETED state (helps debug silent audio)
static uint32_t diag_recv_calls = 0;
static uint32_t diag_recv_data = 0;
static uint32_t diag_rtp_packets = 0;
static uint32_t diag_rtcp_packets = 0;
static uint32_t diag_dtls_packets = 0;
static uint32_t diag_unknown_packets = 0;
static uint32_t diag_srtp_fail = 0;
static uint32_t diag_audio_decoded = 0;
static uint32_t diag_ssrc_mismatch = 0;
static uint64_t diag_last_log_time = 0;

// Diagnostic counters for outgoing RTP (uplink send path)
static uint32_t diag_tx_rtp_packets = 0;
static uint32_t diag_tx_srtp_fail = 0;
static uint32_t diag_tx_send_fail = 0;
static uint32_t diag_tx_send_ok = 0;
static uint32_t diag_tx_bytes = 0;

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

  char sdp[CONFIG_SDP_BUFFER_SIZE];

  void (*onicecandidate)(char* sdp, void* user_data);
  void (*oniceconnectionstatechange)(PeerConnectionState state, void* user_data);
  void (*on_connected)(void* userdata);
  void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void* user_data);

  uint8_t temp_buf[CONFIG_MTU];
  uint8_t agent_buf[CONFIG_MTU];
  int agent_ret;
  int b_local_description_created;

  RtpEncoder artp_encoder;
  RtpEncoder vrtp_encoder;
  RtpDecoder vrtp_decoder;
  RtpDecoder artp_decoder;

  uint32_t remote_assrc;
  uint32_t remote_vssrc;
};

static void peer_connection_outgoing_rtp_packet(uint8_t* data, size_t size, void* user_data) {
  PeerConnection* pc = (PeerConnection*)user_data;
  size_t pre_encrypt_size = size;
  dtls_srtp_encrypt_rtp_packet(&pc->dtls_srtp, data, (int*)&size);
  diag_tx_rtp_packets++;
  if (size == 0) {
    diag_tx_srtp_fail++;
    if (diag_tx_srtp_fail <= 5 || (diag_tx_srtp_fail % 100) == 0) {
      LOGE("TX: SRTP encrypt failed (pre_size=%zu, count=%" PRIu32 ")", pre_encrypt_size, diag_tx_srtp_fail);
    }
    return;
  }
  int ret = agent_send(&pc->agent, data, size);
  if (ret < 0) {
    diag_tx_send_fail++;
    if (diag_tx_send_fail <= 5 || (diag_tx_send_fail % 100) == 0) {
      LOGE("TX: agent_send failed ret=%d size=%zu (count=%" PRIu32 ")", ret, size, diag_tx_send_fail);
    }
  } else {
    diag_tx_send_ok++;
    diag_tx_bytes += (uint32_t)size;
    if (diag_tx_send_ok == 1) {
      LOGI("TX: First RTP packet sent (%zu bytes, SRTP %zu->%zu)", size, pre_encrypt_size, size);
    }
  }
}

static int peer_connection_dtls_srtp_recv(void* ctx, unsigned char* buf, size_t len) {
  int recv_max = 0;
  int ret = -1;
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  PeerConnection* pc = (PeerConnection*)dtls_srtp->user_data;

  // Check if we have buffered data from a previous recv
  if (pc->agent_ret > 0 && pc->agent_ret <= len) {
    memcpy(buf, pc->agent_buf, pc->agent_ret);
    return pc->agent_ret;
  }

  // Poll for incoming data with timeout
  while (recv_max < CONFIG_TLS_READ_TIMEOUT && pc->state == PEER_CONNECTION_CONNECTED) {
    ret = agent_recv(&pc->agent, buf, len);

    if (ret > 0) {
      // Got non-STUN data (DTLS packet)
      LOGD("DTLS recv: got %d bytes (first=0x%02x)", ret, buf[0]);
      return ret;
    }
    // ret == 0 means STUN was processed, ret < 0 means no data
    // Either way, keep polling
    if (recv_max % 500 == 0) {
      LOGD("DTLS recv: polling %d/%d", recv_max, CONFIG_TLS_READ_TIMEOUT);
    }

    recv_max++;
  }

  // Timeout or state changed - return timeout error instead of 0
  // Returning 0 tells mbedtls the connection is closed, which is wrong
  // Returning MBEDTLS_ERR_SSL_TIMEOUT tells it we timed out waiting for data
  if (pc->state != PEER_CONNECTION_CONNECTED) {
    LOGD("DTLS recv: state changed to %d, aborting", pc->state);
    return MBEDTLS_ERR_SSL_CONN_EOF;
  }
  
  LOGD("DTLS recv: timeout after %d polls", recv_max);
  return MBEDTLS_ERR_SSL_TIMEOUT;
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

  if (pc->config.audio_codec) {
    rtp_encoder_init(&pc->artp_encoder, pc->config.audio_codec,
                     peer_connection_outgoing_rtp_packet, (void*)pc);

    rtp_decoder_init(&pc->artp_decoder, pc->config.audio_codec,
                     pc->config.onaudiotrack, pc->config.user_data);
  }

  if (pc->config.video_codec) {
    rtp_encoder_init(&pc->vrtp_encoder, pc->config.video_codec,
                     peer_connection_outgoing_rtp_packet, (void*)pc);

    rtp_decoder_init(&pc->vrtp_decoder, pc->config.video_codec,
                     pc->config.onvideotrack, pc->config.user_data);
  }

  return pc;
}

void peer_connection_destroy(PeerConnection* pc) {
  if (pc) {
    sctp_destroy_association(&pc->sctp);
    dtls_srtp_deinit(&pc->dtls_srtp);
    agent_destroy(&pc->agent);
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
  return rtp_encoder_encode(&pc->artp_encoder, buf, len);
}

int peer_connection_send_video(PeerConnection* pc, const uint8_t* buf, size_t len) {
  if (pc->state != PEER_CONNECTION_COMPLETED) {
    // LOGE("dtls_srtp not connected");
    return -1;
  }
  return rtp_encoder_encode(&pc->vrtp_encoder, buf, len);
}

int peer_connection_datachannel_send(PeerConnection* pc, char* message, size_t len) {
  return peer_connection_datachannel_send_sid(pc, message, len, 0);
}

int peer_connection_datachannel_send_sid(PeerConnection* pc, char* message, size_t len, uint16_t sid) {
  if (!sctp_is_connected(&pc->sctp)) {
    LOGE("sctp not connected");
    return -1;
  }
  if (pc->config.datachannel == DATA_CHANNEL_STRING)
    return sctp_outgoing_data(&pc->sctp, message, len, PPID_STRING, sid);
  else
    return sctp_outgoing_data(&pc->sctp, message, len, PPID_BINARY, sid);
}

int peer_connection_create_datachannel(PeerConnection* pc, DecpChannelType channel_type, uint16_t priority, uint32_t reliability_parameter, char* label, char* protocol) {
  return peer_connection_create_datachannel_sid(pc, channel_type, priority, reliability_parameter, label, protocol, 0);
}

int peer_connection_create_datachannel_sid(PeerConnection* pc, DecpChannelType channel_type, uint16_t priority, uint32_t reliability_parameter, char* label, char* protocol, uint16_t sid) {
  int rtrn = -1;

  if (!sctp_is_connected(&pc->sctp)) {
    LOGE("sctp not connected");
    return rtrn;
  }

  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |  Message Type |  Channel Type |            Priority           |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                    Reliability Parameter                      |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |         Label Length          |       Protocol Length         |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                                                               |
  // |                             Label                             |
  // |                                                               |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                                                               |
  // |                            Protocol                           |
  // |                                                               |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  int msg_size = 12 + strlen(label) + strlen(protocol);
  uint16_t priority_big_endian = htons(priority);
  uint32_t reliability_big_endian = ntohl(reliability_parameter);
  uint16_t label_length = htons(strlen(label));
  uint16_t protocol_length = htons(strlen(protocol));
  char* msg = calloc(1, msg_size);

  msg[0] = DATA_CHANNEL_OPEN;
  memcpy(msg + 2, &priority_big_endian, sizeof(uint16_t));
  memcpy(msg + 4, &reliability_big_endian, sizeof(uint32_t));
  memcpy(msg + 8, &label_length, sizeof(uint16_t));
  memcpy(msg + 10, &protocol_length, sizeof(uint16_t));
  memcpy(msg + 12, label, strlen(label));
  memcpy(msg + 12 + strlen(label), protocol, strlen(protocol));

  rtrn = sctp_outgoing_data(&pc->sctp, msg, msg_size, PPID_CONTROL, sid);
  free(msg);
  return rtrn;
}

/**
 * Returns the SDP setup attribute value.
 *
 * Per RFC 8842:
 * - Offerer MUST use "actpass" (can be either DTLS client or server)
 * - Answerer picks "active" (DTLS client) or "passive" (DTLS server)
 */
static char* peer_connection_dtls_role_setup_value(DtlsSrtpRole role, SdpType sdp_type) {
  if (sdp_type == SDP_TYPE_OFFER) {
    // Offerer must always use actpass per RFC 8842
    return "a=setup:actpass";
  }
  // Answerer picks active or passive based on role
  return role == DTLS_SRTP_ROLE_SERVER ? "a=setup:passive" : "a=setup:active";
}

int peer_connection_loop(PeerConnection* pc) {
  uint32_t ssrc = 0;
  memset(pc->agent_buf, 0, sizeof(pc->agent_buf));
  pc->agent_ret = -1;

  switch (pc->state) {
    case PEER_CONNECTION_NEW:
      break;

    case PEER_CONNECTION_CHECKING:
      if (agent_select_candidate_pair(&pc->agent) < 0) {
        STATE_CHANGED(pc, PEER_CONNECTION_FAILED);
      } else if (agent_connectivity_check(&pc->agent, 0) == 0) {  // 0 = normal connectivity check
        STATE_CHANGED(pc, PEER_CONNECTION_CONNECTED);
      }
      break;

    case PEER_CONNECTION_CONNECTED: {
      LOGI("DTLS: Starting handshake (role=%s)",
           pc->dtls_srtp.role == DTLS_SRTP_ROLE_SERVER ? "SERVER/passive" : "CLIENT/active");
      int dtls_ret = dtls_srtp_handshake(&pc->dtls_srtp, NULL);
      LOGI("DTLS: Handshake returned %d", dtls_ret);
      if (dtls_ret == 0) {
        LOGD("DTLS-SRTP handshake done");

        if (pc->config.datachannel) {
          LOGI("SCTP create socket");
          sctp_create_association(&pc->sctp, &pc->dtls_srtp);
          pc->sctp.userdata = pc->config.user_data;
        }

        // Reset diagnostic counters on entering COMPLETED
        diag_recv_calls = 0;
        diag_recv_data = 0;
        diag_rtp_packets = 0;
        diag_rtcp_packets = 0;
        diag_dtls_packets = 0;
        diag_unknown_packets = 0;
        diag_last_log_time = ports_get_epoch_time();
        LOGI("Entering COMPLETED: ice_fd=%d udp_fd=%d remote_assrc=%" PRIu32 " audio_codec=%d",
             pc->agent.ice_socket.fd, pc->agent.udp_sockets[0].fd,
             pc->remote_assrc, pc->config.audio_codec);
        STATE_CHANGED(pc, PEER_CONNECTION_COMPLETED);
      } else {
        LOGE("DTLS-SRTP handshake failed: %d, transitioning to FAILED", dtls_ret);
        STATE_CHANGED(pc, PEER_CONNECTION_FAILED);
      }
      break;
    }
    case PEER_CONNECTION_COMPLETED: {
      // Send STUN heartbeat every STUN_HEARTBEAT_INTERVAL seconds (from libpeer PR #205)
      // This keeps the TURN allocation and ICE binding alive during long sessions
      uint64_t current_time = ports_get_epoch_time();
      if (current_time - last_heartbeat_time >= STUN_HEARTBEAT_INTERVAL) {
        agent_connectivity_check(&pc->agent, 1);  // 1 = heartbeat mode
        LOGD("STUN heartbeat sent");
        last_heartbeat_time = current_time;
      }

      diag_recv_calls++;
      if ((pc->agent_ret = agent_recv(&pc->agent, pc->agent_buf, sizeof(pc->agent_buf))) > 0) {
        diag_recv_data++;
        LOGD("agent_recv %d", pc->agent_ret);

        if (rtcp_probe(pc->agent_buf, pc->agent_ret)) {
          diag_rtcp_packets++;
          LOGD("Got RTCP packet");
          dtls_srtp_decrypt_rtcp_packet(&pc->dtls_srtp, pc->agent_buf, &pc->agent_ret);
          peer_connection_incoming_rtcp(pc, pc->agent_buf, pc->agent_ret);

        } else if (dtls_srtp_probe(pc->agent_buf)) {
          diag_dtls_packets++;
          int ret = dtls_srtp_read(&pc->dtls_srtp, pc->temp_buf, sizeof(pc->temp_buf));
          LOGD("Got DTLS data %d", ret);

          if (ret > 0) {
            sctp_incoming_data(&pc->sctp, (char*)pc->temp_buf, ret);
          }

        } else if (rtp_packet_validate(pc->agent_buf, pc->agent_ret)) {
          diag_rtp_packets++;
          LOGD("Got RTP packet");

          int pre_decrypt_size = pc->agent_ret;
          dtls_srtp_decrypt_rtp_packet(&pc->dtls_srtp, pc->agent_buf, &pc->agent_ret);

          if (pc->agent_ret == 0) {
            diag_srtp_fail++;
            if (diag_srtp_fail <= 5 || (diag_srtp_fail % 100) == 0) {
              LOGW("SRTP decrypt failed (pre_size=%d fail_count=%u)", pre_decrypt_size, (unsigned)diag_srtp_fail);
            }
          } else {
            ssrc = rtp_get_ssrc(pc->agent_buf);
            // SSRC late binding: if remote SDP omitted a=ssrc, learn from first packet
            if (ssrc != 0 && pc->remote_assrc == 0 && pc->config.audio_codec != CODEC_NONE) {
              pc->remote_assrc = ssrc;
              LOGI("Audio SSRC learned from RTP: %" PRIu32, ssrc);
            }
            if (ssrc == pc->remote_assrc) {
              diag_audio_decoded++;
              rtp_decoder_decode(&pc->artp_decoder, pc->agent_buf, pc->agent_ret);
            } else if (ssrc == pc->remote_vssrc) {
              rtp_decoder_decode(&pc->vrtp_decoder, pc->agent_buf, pc->agent_ret);
            } else {
              diag_ssrc_mismatch++;
            }
          }

        } else {
          diag_unknown_packets++;
          LOGW("Unknown data (len=%d first_byte=0x%02x)", pc->agent_ret, pc->agent_buf[0]);
        }
      }

      // Periodic diagnostic log every 10 seconds to help debug silent audio
      if (current_time - diag_last_log_time >= 10000) {
        LOGI("DIAG: recv=%u data=%u rtp=%u decoded=%u srtp_fail=%u ssrc_miss=%u rtcp=%u dtls=%u unk=%u",
             (unsigned)diag_recv_calls, (unsigned)diag_recv_data,
             (unsigned)diag_rtp_packets, (unsigned)diag_audio_decoded,
             (unsigned)diag_srtp_fail, (unsigned)diag_ssrc_mismatch,
             (unsigned)diag_rtcp_packets, (unsigned)diag_dtls_packets,
             (unsigned)diag_unknown_packets);
        LOGI("DIAG TX: sent=%u srtp_fail=%u send_fail=%u bytes=%u",
             (unsigned)diag_tx_send_ok, (unsigned)diag_tx_srtp_fail,
             (unsigned)diag_tx_send_fail, (unsigned)diag_tx_bytes);
        diag_last_log_time = current_time;
      }

      if (CONFIG_KEEPALIVE_TIMEOUT > 0 && (ports_get_epoch_time() - pc->agent.binding_request_time) > CONFIG_KEEPALIVE_TIMEOUT) {
        LOGI("binding request timeout");
        STATE_CHANGED(pc, PEER_CONNECTION_CLOSED);
      }

      break;
    }
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

void peer_connection_set_remote_description(PeerConnection* pc, const char* sdp, SdpType type) {
  char* start = (char*)sdp;
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
      LOGI("SDP: Remote has setup:passive, we are DTLS CLIENT (active)");
      role = DTLS_SRTP_ROLE_CLIENT;
    } else if (strstr(buf, "a=setup:active")) {
      LOGI("SDP: Remote has setup:active, we are DTLS SERVER (passive)");
      // Default is SERVER, so no change needed
    }

    // Only match SHA-256 fingerprints (libpeer uses SHA-256 for DTLS certificate verification)
    if (strstr(buf, "a=fingerprint:sha-256 ")) {
      char *fp_start = strstr(buf, "sha-256 ");
      if (fp_start) {
        fp_start += 8;
        size_t fp_len = strlen(fp_start);
        if (fp_len >= 95) fp_len = 95;
        strncpy(pc->dtls_srtp.remote_fingerprint, fp_start, fp_len);
        pc->dtls_srtp.remote_fingerprint[fp_len] = '\0';
        LOGI("SDP: Parsed remote fingerprint: %s", pc->dtls_srtp.remote_fingerprint);
      }
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

  agent_set_remote_description(&pc->agent, (char*)sdp);
  if (type == SDP_TYPE_ANSWER) {
    // Only reinitialize DTLS if the role changed from what was set during offer creation.
    // If we reinitialize unnecessarily, we generate a NEW certificate with a DIFFERENT
    // fingerprint, which breaks the fingerprint matching with the remote peer.
    // The offer was created with SERVER role. Only change if remote sent a=setup:passive.
    if (role != pc->dtls_srtp.role) {
      LOGD("DTLS: Role changed, reinitializing");
      dtls_srtp_deinit(&pc->dtls_srtp);
      dtls_srtp_init(&pc->dtls_srtp, role, pc);
      pc->dtls_srtp.udp_recv = peer_connection_dtls_srtp_recv;
      pc->dtls_srtp.udp_send = peer_connection_dtls_srtp_send;
    }

    agent_update_candidate_pairs(&pc->agent);
    STATE_CHANGED(pc, PEER_CONNECTION_CHECKING);
  }
}

/**
 * Helper to append per-media trailing attributes (aiortc format)
 * These go at the end of each media section: candidates, end-of-candidates, ice creds, fingerprint, setup
 */
static void peer_connection_append_media_tail(PeerConnection* pc, SdpType sdp_type, DtlsSrtpRole role) {
  // Candidates
  for (int i = 0; i < pc->agent.local_candidates_count; i++) {
    char candidate_line[256];
    memset(candidate_line, 0, sizeof(candidate_line));
    ice_candidate_to_description(&pc->agent.local_candidates[i], candidate_line, sizeof(candidate_line));
    sdp_append(pc->sdp, "%s", candidate_line);
  }

  // End of candidates marker
  sdp_append(pc->sdp, "a=end-of-candidates");

  // ICE credentials
  sdp_append(pc->sdp, "a=ice-ufrag:%s", pc->agent.local_ufrag);
  sdp_append(pc->sdp, "a=ice-pwd:%s", pc->agent.local_upwd);

  // Fingerprint
  sdp_append(pc->sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);

  // Setup
  sdp_append(pc->sdp, peer_connection_dtls_role_setup_value(role, sdp_type));
}

/**
 * Generate SDP in aiortc-compatible format
 *
 * Structure (matches aiortc 1:1):
 * - Session level: v=, o=, s=, t=, a=group:BUNDLE 0 1, a=msid-semantic:WMS *
 * - Per media section:
 *   - m= line, c= line, media-specific attributes
 *   - a=mid:N (numeric)
 *   - a=candidate:... (all candidates)
 *   - a=end-of-candidates
 *   - a=ice-ufrag:, a=ice-pwd:
 *   - a=fingerprint:sha-256
 *   - a=setup:actpass (or active/passive for answers)
 */
static const char* peer_connection_create_sdp(PeerConnection* pc, SdpType sdp_type) {
  DtlsSrtpRole role = DTLS_SRTP_ROLE_SERVER;
  int mid = 0;

  pc->sctp.connected = 0;

  switch (sdp_type) {
    case SDP_TYPE_OFFER:
      role = DTLS_SRTP_ROLE_SERVER;
      agent_clear_candidates(&pc->agent);
      pc->agent.mode = AGENT_MODE_CONTROLLING;
      break;
    case SDP_TYPE_ANSWER:
      role = DTLS_SRTP_ROLE_CLIENT;
      pc->agent.mode = AGENT_MODE_CONTROLLED;
      break;
    default:
      break;
  }

  dtls_srtp_reset_session(&pc->dtls_srtp);
  int dtls_ret = dtls_srtp_init(&pc->dtls_srtp, role, pc);
  if (dtls_ret != 0) {
    LOGE("dtls_srtp_init failed: %d (fingerprint will be empty!)", dtls_ret);
  } else {
    LOGI("DTLS initialized, fingerprint: %s", pc->dtls_srtp.local_fingerprint);
  }
  pc->dtls_srtp.udp_recv = peer_connection_dtls_srtp_recv;
  pc->dtls_srtp.udp_send = peer_connection_dtls_srtp_send;

  // Generate ICE credentials (same for all media sections in BUNDLE)
  agent_create_ice_credential(&pc->agent);

  // Gather candidates before building SDP (need them for each media section)
  agent_gather_candidate(&pc->agent, NULL, NULL, NULL);  // host address
  for (int i = 0; i < sizeof(pc->config.ice_servers) / sizeof(pc->config.ice_servers[0]); ++i) {
    if (pc->config.ice_servers[i].urls) {
      LOGI("ice server: %s", pc->config.ice_servers[i].urls);
      agent_gather_candidate(&pc->agent, pc->config.ice_servers[i].urls,
                             pc->config.ice_servers[i].username,
                             pc->config.ice_servers[i].credential);
    }
  }

  // === Build SDP ===
  memset(pc->sdp, 0, sizeof(pc->sdp));

  // Session-level header (only these attributes at session level)
  sdp_create(pc->sdp,
             pc->config.video_codec != CODEC_NONE,
             pc->config.audio_codec != CODEC_NONE,
             pc->config.datachannel);

  // === Video media section (if enabled) ===
  if (pc->config.video_codec == CODEC_H264) {
    sdp_append_h264(pc->sdp, mid++);
    peer_connection_append_media_tail(pc, sdp_type, role);
  }

  // === Audio media section (if enabled) ===
  if (pc->config.audio_codec != CODEC_NONE) {
    switch (pc->config.audio_codec) {
      case CODEC_PCMA:
        sdp_append_pcma(pc->sdp, mid++);
        break;
      case CODEC_PCMU:
        sdp_append_pcmu(pc->sdp, mid++);
        break;
      case CODEC_OPUS:
        sdp_append_opus(pc->sdp, mid++);
        break;
      default:
        break;
    }
    peer_connection_append_media_tail(pc, sdp_type, role);
  }

  // === Datachannel media section (if enabled) ===
  if (pc->config.datachannel) {
    sdp_append_datachannel(pc->sdp, mid++);
    peer_connection_append_media_tail(pc, sdp_type, role);
  }

  pc->b_local_description_created = 1;

  if (pc->onicecandidate) {
    pc->onicecandidate(pc->sdp, pc->config.user_data);
  }

  return pc->sdp;
}

const char* peer_connection_create_offer(PeerConnection* pc) {
  return peer_connection_create_sdp(pc, SDP_TYPE_OFFER);
}

const char* peer_connection_create_answer(PeerConnection* pc) {
  const char* sdp = peer_connection_create_sdp(pc, SDP_TYPE_ANSWER);
  agent_update_candidate_pairs(&pc->agent);
  STATE_CHANGED(pc, PEER_CONNECTION_CHECKING);
  return sdp;
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

void peer_connection_onicecandidate(PeerConnection* pc, void (*onicecandidate)(char* sdp, void* userdata)) {
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
  LOGD("Add candidate: %s", candidate);
  agent->remote_candidates_count++;
  return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "peer_connection.h"

#define PEER_MTU 1500

static int peer_connection_dtls_srtp_recv(void *ctx, unsigned char *buf, size_t len) {
 
  int ret; 
  DtlsSrtp *dtls_srtp = (DtlsSrtp *) ctx; 
  PeerConnection *pc = (PeerConnection *) dtls_srtp->user_data;

  if (pc->agent_ret > 0 && len < pc->agent_ret) {

    memcpy(buf, pc->agent_buf, pc->agent_ret);
    return pc->agent_ret;
  }

  while (1) {

    ret = agent_recv(&pc->agent, buf, len);

    if (ret > 0) {
      break;
    }

  }
  return ret;	
}

static int peer_connection_dtls_srtp_send(void *ctx, const uint8_t *buf, size_t len) {
  
  DtlsSrtp *dtls_srtp = (DtlsSrtp *) ctx; 
  PeerConnection *pc = (PeerConnection *) dtls_srtp->user_data;

  //LOGD("send %.4x %.4x, %ld", *(uint16_t*)buf, *(uint16_t*)(buf + 2), len); 
  return agent_send(&pc->agent, buf, len);
  
}

static void peer_connection_incoming_rtcp(PeerConnection *pc, uint8_t *buf, size_t len) {

  RtcpHeader rtcp_header = {0};
  memcpy(&rtcp_header, buf, sizeof(rtcp_header));
  switch(rtcp_header.type) {
    case RTCP_RR:
      if(rtcp_header.rc > 0) {
        RtcpRr rtcp_rr = rtcp_packet_parse_rr(buf);
        uint32_t fraction = ntohl(rtcp_rr.report_block[0].flcnpl) >> 24;
        uint32_t total = ntohl(rtcp_rr.report_block[0].flcnpl) & 0x00FFFFFF;
        if(pc->on_receiver_packet_loss && fraction > 0) {
          pc->on_receiver_packet_loss((float)fraction/256.0, total, pc->user_data);
        }
      }
      break;
    default:
      break;
  }
}


void peer_connection_configure(PeerConnection *pc, PeerOptions *options) {

  memcpy(&pc->options, options, sizeof(PeerOptions));
}

void peer_connection_init(PeerConnection *pc) {

  pc->agent.mode = AGENT_MODE_CONTROLLED;

  dtls_srtp_init(&pc->dtls_srtp, DTLS_SRTP_ROLE_SERVER, pc);

  pc->dtls_srtp.udp_recv = peer_connection_dtls_srtp_recv;
  pc->dtls_srtp.udp_send = peer_connection_dtls_srtp_send;

#ifdef HAVE_GST
  gst_init(NULL, NULL);

  if (pc->options.audio_codec) {
    pc->audio_stream = media_stream_create(pc->options.audio_codec, pc->options.audio_outgoing_pipeline, pc->options.audio_incoming_pipeline);
  }

  if (pc->options.video_codec) {
    pc->video_stream = media_stream_create(pc->options.video_codec, pc->options.video_outgoing_pipeline, pc->options.video_incoming_pipeline);
  }
#endif
}

int peer_connection_datachannel_send(PeerConnection *pc, char *message, size_t len) {

  if(sctp_is_connected(&pc->sctp))
    return sctp_outgoing_data(&pc->sctp, message, len);

  return -1;
}


int peer_connection_loop(PeerConnection *pc) {

  uint8_t data[PEER_MTU];

  uint8_t dtls_data[CONFIG_MTU];

  memset(pc->agent_buf, 0, sizeof(pc->agent_buf));
  pc->agent_ret = -1;
  size_t size = sizeof(data);

  int len;

  size_t packet_size;

  switch (pc->state) {

    case PEER_CONNECTION_NEW:

      break;

    case PEER_CONNECTION_CHECKING:

      agent_select_candidate_pair(&pc->agent);

      if (agent_connectivity_check(&pc->agent)) {

        LOGD("Connectivity check success. pair: %p", pc->agent.nominated_pair);

        pc->state = PEER_CONNECTION_CONNECTED;
        pc->agent.selected_pair = pc->agent.nominated_pair;
      }

      agent_recv(&pc->agent, pc->agent_buf, sizeof(pc->agent_buf));

      break;

    case PEER_CONNECTION_CONNECTED:

      if (pc->dtls_srtp.state == DTLS_SRTP_STATE_INIT) {

        if (dtls_srtp_handshake(&pc->dtls_srtp, NULL) == 0) {

          LOGD("DTLS-SRTP handshake done");

#ifdef HAVE_GST
          if (pc->audio_stream) {
            media_stream_play(pc->audio_stream);
          }

          if (pc->video_stream) {
            media_stream_play(pc->video_stream);
          }
#endif
          if (pc->options.b_datachannel) {
            LOGI("SCTP create socket");
            sctp_create_socket(&pc->sctp, &pc->dtls_srtp);
          }

        }
      } else if (pc->dtls_srtp.state == DTLS_SRTP_STATE_CONNECTED) {
#ifdef HAVE_GST
        if (pc->video_stream && (utils_buffer_pop(pc->video_stream->outgoing_rb, pc->agent_buf, size) == size)) {
          memcpy(&packet_size, pc->agent_buf, sizeof(size_t));
          peer_connection_send_rtp_packet(pc, pc->agent_buf + sizeof(size_t), packet_size);
        }
        if (pc->audio_stream && (utils_buffer_pop(pc->audio_stream->outgoing_rb, pc->agent_buf, size) == size)) {
          memcpy(&packet_size, pc->agent_buf, sizeof(size_t));
          peer_connection_send_rtp_packet(pc, pc->agent_buf + sizeof(size_t), packet_size);
        }
#endif

        memset(pc->agent_buf, 0, sizeof(pc->agent_buf));

        if ((pc->agent_ret = agent_recv(&pc->agent, pc->agent_buf, sizeof(pc->agent_buf))) > 0) {
          LOGD("agent_recv %d", pc->agent_ret);

          if (rtcp_packet_validate(pc->agent_buf, pc->agent_ret)) {
            LOGD("RTCP packet");
            dtls_srtp_decrypt_rtcp_packet(&pc->dtls_srtp, pc->agent_buf, &len);
            peer_connection_incoming_rtcp(pc, pc->agent_buf, len);

          } else if (dtls_srtp_validate(pc->agent_buf)) {

            int ret = dtls_srtp_read(&pc->dtls_srtp, dtls_data, sizeof(dtls_data));
            LOGD("DTLS-SRTP read %d", ret);
            sctp_incoming_data(&pc->sctp, dtls_data, ret);

          } else if (rtp_packet_validate(pc->agent_buf, pc->agent_ret)) {

            LOGD("RTP packet");
          }

        }
      }
      break;
    case PEER_CONNECTION_COMPLETED:
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

void peer_connection_set_remote_description(PeerConnection *pc, const char *sdp_text) {

  agent_set_remote_description(&pc->agent, (char*)sdp_text);
  pc->state = PEER_CONNECTION_CHECKING;
}

const char* peer_connection_create_offer(PeerConnection *pc) {

  char description[512];

  int b_video = pc->options.video_codec != CODEC_NONE;
  int b_audio = pc->options.audio_codec != CODEC_NONE;
  int b_datachannel = pc->options.b_datachannel;

  pc->state = PEER_CONNECTION_NEW;

  memset(description, 0, sizeof(description));

  agent_gather_candidates(&pc->agent);

  agent_get_local_description(&pc->agent, description, sizeof(description));

  // TODO: check if we have video or audio codecs
  sdp_create(&pc->local_sdp, b_video, b_audio, b_datachannel);

  if (pc->options.video_codec == CODEC_H264) {

    sdp_append_h264(&pc->local_sdp);
    sdp_append(&pc->local_sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
    sdp_append(&pc->local_sdp, "a=setup:actpass");
    strcat(pc->local_sdp.content, description);
  }

  if (pc->options.audio_codec == CODEC_PCMA) {

    sdp_append_pcma(&pc->local_sdp);
    sdp_append(&pc->local_sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
    sdp_append(&pc->local_sdp, "a=setup:actpass");
    strcat(pc->local_sdp.content, description);
  }

  if (pc->options.b_datachannel) {
    sdp_append_datachannel(&pc->local_sdp);
    sdp_append(&pc->local_sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
    sdp_append(&pc->local_sdp, "a=setup:actpass");
    strcat(pc->local_sdp.content, description);
  }

  return pc->local_sdp.content;
}

int peer_connection_send_rtp_packet(PeerConnection *pc, uint8_t *packet, int bytes) {

  dtls_srtp_encrypt_rtp_packet(&pc->dtls_srtp, packet, &bytes);

  return agent_send(&pc->agent, packet, bytes);
}

int peer_connection_send_rtcp_pil(PeerConnection *pc, uint32_t ssrc) {

  int ret = -1;
  guint size = 12;
  uint8_t plibuf[128];
  rtcp_packet_get_pli(plibuf, 12, ssrc);
 
  //TODO: encrypt rtcp packet
  //dtls_transport_encrypt_rctp_packet(pc->dtls_transport, plibuf, &size);
  //ret = nice_agent_send(pc->nice_agent, pc->stream_id, pc->component_id, size, (gchar*)plibuf);

  return ret;
}

void peer_connection_set_host_address(PeerConnection *pc, const char *host) {

  Address addr;
  addr.family = AF_INET;
  inet_pton(AF_INET, host, addr.ipv4);
  agent_set_host_address(&pc->agent, &addr);
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
 void (*oniceconnectionstatechange)(IceCandidateState state, void *userdata)) {

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

    sctp_onopen(&pc->sctp, onopen);
    sctp_onclose(&pc->sctp, onclose);
    sctp_onmessage(&pc->sctp, onmessasge);
  }
}


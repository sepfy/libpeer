#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "agent.h"
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

// Max datagrams drained per peer_connection_loop() call. A single video frame
// generates dozens of SACKs; draining one per call throttles the peer to the
// loop period and overflows the UDP receive queue.
#define PEER_CONNECTION_RECV_BURST 16

// ================= NACK 重传缓存 (RFC 4585 generic NACK) =================
// 只缓存"加密后"的 RTP 包原样重发：明文重加密会被 libsrtp 发送侧 rdbx 以
// pkt_idx_old 拒绝，且同 key+index 重加密违反 SRTP 使用规范；重传包保持原
// seq/ts/ssrc/auth-tag，接收侧重放窗口对真丢失包放行、对竞态重复包丢弃。
// Ring 为 struct PeerConnection 末尾的柔性数组，槽位数运行时由
// config.nack_ring_packets 决定：0 = 禁用 (不缓存、不重传、不占内存)，
// 非 0 必须为 2 的幂 (peer_connection_create 校验，非法直接失败)。

#define PEER_CONNECTION_NACK_SLOT_SIZE (CONFIG_MTU + 16) /* 密文 + SRTP auth tag 余量：GCM 16B tag 下满长包(1300+16)恰好容纳 */

/* sendto 失败(典型 ENOBUFS)后退避重试的间隔：让出 CPU 给 TCPIP/wlan 任务
 * 排空 TX skb 池。仅失败路径付出该延迟，成功路径零开销。 */
#define PEER_CONNECTION_SEND_RETRY_DELAY_MS 2

typedef struct {
  uint16_t seq;
  uint16_t len;                                /* len==0 视为空槽 */
  uint8_t data[PEER_CONNECTION_NACK_SLOT_SIZE];
} nack_ring_entry_t;

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

  SdpMediaInfo remote_media[SDP_MEDIA_COUNT]; /* offer 的 per-m-line mid/方向（pc 每会话新建 calloc，无陈旧复用） */
  int h264_pt;                                /* 当前会话 H264 PT，create 默认 PT_H264，可经 set_video_payload_type 覆盖 */

  uint32_t handshake_start_time;

  uint32_t nack_retransmits; /* NACK 重传包计数 (config.nack_ring_packets==0 时恒 0) */
  uint32_t srtp_auth_failures; /* SRTP/SRTCP 入向鉴权失败丢包计数 */

  /* RTP 出包统计：成功投递到 socket 的包数与最终丢弃(重试后仍失败)的包数。
   * sendto 失败(典型 ENOBUFS)在 rtp_encoder_encode_* 返回 0 的掩盖下
   * 对上层不可见，必须在此收敛点显式计数。 */
  uint32_t rtp_packets_sent;
  uint32_t rtp_send_failures;

  /* 拥塞帧放弃：FU-A 帧丢一片整帧即废。视频轨本帧首次发送失败后，
   * 同 timestamp 的剩余分片直接丢弃（等待对端 PLI → IDR 重发恢复）。 */
  uint32_t last_video_ts;
  int      video_frame_aborted;

  /* 柔性数组 (GCC 零长数组扩展, 仓库既有写法, 见 async_delegation.c):
   * NACK 重传 ring, 槽位数 = config.nack_ring_packets (0 时分配 0 字节),
   * 必须是 struct 最后一个成员, 随 create 一次性 calloc 分配。 */
  nack_ring_entry_t nack_ring[0];
};

static void peer_connection_outgoing_rtp_packet(uint8_t* data, size_t size, void* user_data) {
  PeerConnection* pc = (PeerConnection*)user_data;
  if (dtls_srtp_encrypt_rtp_packet(&pc->dtls_srtp, data, (int*)&size) != 0) {
    /* 加密失败不发送：对端必然鉴权丢弃，缓存与发送都无意义 */
    LOGW("srtp_protect failed, drop outbound RTP");
    return;
  }

  int is_video = (pc->config.video_codec != CODEC_NONE) && ((data[1] & 0x7F) == (uint8_t)pc->vrtp_encoder.type);

  /* 缓存密文(含 auth tag)供 NACK 原样重发；只缓存视频轨。
   * 必须在下方的帧放弃判断之前：本帧后续分片被丢弃时，已成功发送的
   * 分片仍可通过 NACK 补发（虽然整帧已不可解，重传主要用于对端 PLI 前
   * 的真实丢包场景，缓存成本极低） */
  if (pc->config.nack_ring_packets > 0 && is_video &&
      size <= PEER_CONNECTION_NACK_SLOT_SIZE) {
    uint16_t seq = ((uint16_t)data[2] << 8) | data[3];
    nack_ring_entry_t* e = &pc->nack_ring[seq & (pc->config.nack_ring_packets - 1)];
    e->seq = seq;
    e->len = (uint16_t)size;
    memcpy(e->data, data, size);
  }

  if (is_video) {
    uint32_t ts = ntohl(((RtpHeader*)data)->timestamp);
    if (ts != pc->last_video_ts) {
      /* 新帧开始，清除上一帧的放弃标志 */
      pc->last_video_ts = ts;
      pc->video_frame_aborted = 0;
    }
    /* FU-A 帧丢任意一片整帧即废：已放弃的帧剩余分片不再发送，
     * 避免在拥塞链路上浪费带宽并拖长推流线程持锁时间 */
    if (pc->video_frame_aborted) {
      return;
    }
  }

  if (agent_send(&pc->agent, data, size) < 0) {
    /* sendto 失败(典型 ENOBUFS)：退避一次让 wlan 排空 TX skb 池再重试，
     * 仍失败即丢弃。失败路径才付出延迟，最多一次、2ms 封顶，不累积 */
    ports_sleep_ms(PEER_CONNECTION_SEND_RETRY_DELAY_MS);
    if (agent_send(&pc->agent, data, size) < 0) {
      pc->rtp_send_failures++;
      if (is_video) {
        pc->video_frame_aborted = 1;
      }
      return;
    }
  }
  pc->rtp_packets_sent++;
}

static int peer_connection_dtls_srtp_recv(void* ctx, unsigned char* buf, size_t len) {
  int recv_max = 0;
  int ret = -1;
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  PeerConnection* pc = (PeerConnection*)dtls_srtp->user_data;

  if (pc->agent_ret > 0 && pc->agent_ret <= len) {
    memcpy(buf, pc->agent_buf, pc->agent_ret);
    return pc->agent_ret;
  }

  while (recv_max < CONFIG_TLS_READ_TIMEOUT && pc->state == PEER_CONNECTION_CONNECTED) {
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

static void peer_connection_nack_retransmit(PeerConnection* pc, uint16_t seq) {
  /* nack_ring_packets==0 (未启用重传) 早退 */
  if (pc->config.nack_ring_packets == 0) {
    return;
  }
  /* 同余槽位校验: seq 超出窗口时槽位内容必不匹配, 天然实现窗口语义 */
  nack_ring_entry_t* e = &pc->nack_ring[seq & (pc->config.nack_ring_packets - 1)];
  if (e->seq == seq && e->len > 0) {
    /* 重传同样计入收发统计：重传失败也是拥塞信号 */
    if (agent_send(&pc->agent, e->data, e->len) < 0) {
      pc->rtp_send_failures++;
    } else {
      pc->rtp_packets_sent++;
    }
    pc->nack_retransmits++;
  }
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
        break;
      }
      case RTCP_RTPFB: {
        int fmt = rtcp_header->rc;
        LOGD("RTCP_RTPFB %d", fmt);
        /* RFC 4585 generic NACK (fmt=1): FCI = PID(2B) + BLP(2B),
         * 位于 pos+12 (RtcpHeader 4B + sender SSRC 4B + media SSRC 4B) */
        if (fmt == 1 && pos + 16 <= len) {
          const uint8_t* fci = buf + pos + 12;
          uint32_t media = ((uint32_t)buf[pos + 8] << 24) | ((uint32_t)buf[pos + 9] << 16) |
                           ((uint32_t)buf[pos + 10] << 8) | buf[pos + 11];
          /* 只响应视频轨的 NACK: media SSRC == 本地视频 SSRC */
          if (media == pc->vrtp_encoder.ssrc) {
            uint16_t base = ((uint16_t)fci[0] << 8) | fci[1];
            uint16_t blp = ((uint16_t)fci[2] << 8) | fci[3];
            peer_connection_nack_retransmit(pc, base);
            for (int i = 0; i < 16; i++) {
              if (blp & (1u << i)) {
                peer_connection_nack_retransmit(pc, (uint16_t)(base + i + 1));
              }
            }
          }
        }
        break;
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

uint32_t peer_connection_get_nack_retransmits(PeerConnection* pc) {
  return pc->nack_retransmits;
}

uint32_t peer_connection_get_srtp_auth_failures(PeerConnection* pc) {
  return pc->srtp_auth_failures;
}

uint32_t peer_connection_get_rtp_packets_sent(PeerConnection* pc) {
  return pc->rtp_packets_sent;
}

uint32_t peer_connection_get_rtp_send_failures(PeerConnection* pc) {
  return pc->rtp_send_failures;
}

void* peer_connection_get_sctp(PeerConnection* pc) {
  return &pc->sctp;
}

PeerConnection* peer_connection_create(PeerConfiguration* config) {
  uint32_t n = config->nack_ring_packets;

  /* n=0 合法(禁用重传); 非 0 必须为 2 的幂 —— n & (n-1) 对 n=0 也为 0 */
  if (n & (n - 1)) {
    LOGE("nack_ring_packets must be a power of 2 (got %" PRIu32 ")", n);
    return NULL;
  }
  /* RV32 下 size_t 为 32 位: n=2^30 时 1320*n 精确回绕为 0,
   * calloc 会成功而 ring 越界写, 必须守卫 */
  if ((size_t)n > SIZE_MAX / sizeof(nack_ring_entry_t)) {
    LOGE("nack_ring_packets too large (got %" PRIu32 ")", n);
    return NULL;
  }

  PeerConnection* pc = calloc(1, sizeof(PeerConnection) + (size_t)n * sizeof(nack_ring_entry_t));
  if (!pc) {
    return NULL;
  }

  memcpy(&pc->config, config, sizeof(PeerConfiguration));

  agent_create(&pc->agent);

  memset(&pc->sctp, 0, sizeof(pc->sctp));

  pc->state = PEER_CONNECTION_NEW;

  pc->h264_pt = PT_H264;

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
  STATE_CHANGED(pc, PEER_CONNECTION_CLOSED);
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

void peer_connection_set_video_timestamp_increment(PeerConnection* pc, uint32_t increment) {
  rtp_encoder_set_timestamp_increment(&pc->vrtp_encoder, increment);
}

int peer_connection_set_video_payload_type(PeerConnection* pc, int payload_type) {
  if (!pc)
    return -1;
  /* 动态 PT 范围 96-127（rtp_packet_validate 同款判据） */
  if (payload_type < 96 || payload_type > 127) {
    LOGE("invalid h264 payload type: %d", payload_type);
    return -1;
  }
  pc->h264_pt = payload_type;
  if (pc->config.video_codec == CODEC_H264)
    pc->vrtp_encoder.type = (RtpPayloadType)payload_type;
  return 0;
}

int peer_connection_datachannel_send(PeerConnection* pc, char* message, size_t len) {
  /**
   * Use the actual sid from the SCTP stream table (negotiated by the browser's DCEP OPEN),
   * instead of hardcoding 0.
   */
  uint16_t sid = (pc->sctp.stream_count > 0) ? pc->sctp.stream_table[0].sid : 0;
  return peer_connection_datachannel_send_sid(pc, message, len, sid);
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
  if (!msg) {
    return rtrn;
  }

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

static char* peer_connection_dtls_role_setup_value(DtlsSrtpRole d) {
  return d == DTLS_SRTP_ROLE_SERVER ? "a=setup:passive" : "a=setup:active";
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
        {
            /**
             * No candidate pairs (browser's mDNS hostname cannot be resolved).
             * Do not directly mark as FAILED; still receive STUN requests —
             * agent_process_stun_request will create a FROZEN candidate pair from the UDP source address,
             * and in the next loop iteration the standard procedure will select the pair,
             * send USE‑CANDIDATE, and establish connectivity.
             */
            uint8_t buf[1400];
            agent_recv(&pc->agent, buf, sizeof(buf));
        }
        if (pc->agent.candidate_pairs_num == 0) {
            /**
             * No candidate pairs, wait for the browser to send STUN.
             * A 15-second timeout prevents failure to exit the
             * PEER_CONNECTION_CHECKING state after the browser disconnects.
             */
            if (pc->agent.binding_request_time == 0) {
                pc->agent.binding_request_time = ports_get_epoch_time();
            } else if ((ports_get_epoch_time() - pc->agent.binding_request_time) > CONFIG_CHECKING_TIMEOUT) {
                STATE_CHANGED(pc, PEER_CONNECTION_FAILED);
            }
        }
      } else if (agent_connectivity_check(&pc->agent) == 0) {
        pc->handshake_start_time = ports_get_epoch_time();
        STATE_CHANGED(pc, PEER_CONNECTION_CONNECTED);
      }
      break;

    case PEER_CONNECTION_CONNECTED:

      if (dtls_srtp_handshake(&pc->dtls_srtp, NULL) == 0) {
        LOGD("DTLS-SRTP handshake done");

        if (pc->dtls_srtp.state != DTLS_SRTP_STATE_CONNECTED) {
          LOGW("DTLS-SRTP key derivation failed");
          STATE_CHANGED(pc, PEER_CONNECTION_FAILED);
          break;
        }

        if (pc->config.datachannel) {
          LOGI("SCTP create socket");
          sctp_create_association(&pc->sctp, &pc->dtls_srtp);
          pc->sctp.userdata = pc->config.user_data;
        }

        /**
         * Reset the keepalive base timestamp to prevent an
         * immediate timeout upon entering PEER_CONNECTION_COMPLETED.
         */
        pc->agent.binding_request_time = ports_get_epoch_time();
        STATE_CHANGED(pc, PEER_CONNECTION_COMPLETED);
      } else {
        if ((ports_get_epoch_time() - pc->handshake_start_time) > CONFIG_DTLS_HANDSHAKE_TIMEOUT) {
            LOGW("handshake timeout");
            STATE_CHANGED(pc, PEER_CONNECTION_FAILED);
        }
      }
      break;
    case PEER_CONNECTION_COMPLETED:
      for (int i = 0; i < PEER_CONNECTION_RECV_BURST; i++) {
        if ((pc->agent_ret = agent_recv(&pc->agent, pc->agent_buf, sizeof(pc->agent_buf))) <= 0) {
          break;
        }
        LOGD("agent_recv %d", pc->agent_ret);

        if (rtcp_probe(pc->agent_buf, pc->agent_ret)) {
          LOGD("Got RTCP packet");
          if (dtls_srtp_decrypt_rtcp_packet(&pc->dtls_srtp, pc->agent_buf, &pc->agent_ret) != 0) {
            /* 鉴权失败丢弃：AEAD/HMAC 未过校验的字节不得进入解析（防伪造反馈注入） */
            LOGW("SRTCP auth failed, drop");
            pc->srtp_auth_failures++;
            continue;
          }
          peer_connection_incoming_rtcp(pc, pc->agent_buf, pc->agent_ret);

        } else if (dtls_srtp_probe(pc->agent_buf)) {
          int ret = dtls_srtp_read(&pc->dtls_srtp, pc->temp_buf, sizeof(pc->temp_buf));
          LOGD("Got DTLS data %d", ret);

          if (ret > 0) {
            sctp_incoming_data(&pc->sctp, (char*)pc->temp_buf, ret);
          }

        } else if (rtp_packet_validate(pc->agent_buf, pc->agent_ret)) {
          LOGD("Got RTP packet");

          if (dtls_srtp_decrypt_rtp_packet(&pc->dtls_srtp, pc->agent_buf, &pc->agent_ret) != 0) {
            /* 鉴权失败丢弃，绝不送解码器；缺口由 NACK 重传/PLI 关键帧恢复 */
            LOGW("SRTP auth failed, drop");
            pc->srtp_auth_failures++;
            continue;
          }

          ssrc = rtp_get_ssrc(pc->agent_buf);
          if (ssrc == pc->remote_assrc) {
            rtp_decoder_decode(&pc->artp_decoder, pc->agent_buf, pc->agent_ret);
          } else if (ssrc == pc->remote_vssrc) {
            rtp_decoder_decode(&pc->vrtp_decoder, pc->agent_buf, pc->agent_ret);
          }

        } else {
          LOGW("Unknown data");
        }
      }

      if (pc->config.datachannel && pc->sctp.association_failed) {
        LOGI("SCTP association failed");
        STATE_CHANGED(pc, PEER_CONNECTION_CLOSED);
        break;
      }

      if (CONFIG_KEEPALIVE_TIMEOUT > 0 && (ports_get_epoch_time() - pc->agent.binding_request_time) > CONFIG_KEEPALIVE_TIMEOUT) {
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

void peer_connection_set_remote_description(PeerConnection* pc, const char* sdp, SdpType type) {
  char* start = (char*)sdp;
  char* line = NULL;
  char buf[256];
  char* val_start = NULL;
  uint32_t* ssrc = NULL;
  SdpMediaKind media_kind = SDP_MEDIA_COUNT;
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

    /* 只取 sha-256 指纹：dtls_srtp_x509_digest 固定用 SHA-256 算对端证书摘要。
     * aiortc 一类实现每个媒体段发 sha-256/384/512 三行，无差别取最后一行（sha-512）
     * 必比对失败、DTLS 反复重试。Chrome 只发 sha-256 单行所以此前未暴露。 */
    if (strstr(buf, "a=fingerprint:sha-256")) {
      strncpy(pc->dtls_srtp.remote_fingerprint, buf + strlen("a=fingerprint:sha-256 "), DTLS_SRTP_FINGERPRINT_LENGTH);
      pc->dtls_srtp.remote_fingerprint[DTLS_SRTP_FINGERPRINT_LENGTH - 1] = '\0';
    }

    if (strstr(buf, "a=ice-ufrag") &&
        strlen(agent->remote_ufrag) != 0 &&
        (strncmp(buf + strlen("a=ice-ufrag:"), agent->remote_ufrag, strlen(agent->remote_ufrag)) == 0)) {
      is_update = 1;
    }

    /* 前缀匹配收紧（strstr "m=video" 可能命中行内其他子串），
     * 并补 m=application 归属：旧代码不认 datachannel 段，其 mid/方向无法解析 */
    if (strncmp(buf, "m=video ", 8) == 0) {
      media_kind = SDP_MEDIA_VIDEO;
      ssrc = &pc->remote_vssrc;
      pc->remote_media[media_kind].mid[0] = '\0';
      pc->remote_media[media_kind].direction = SDP_DIR_NONE;
    } else if (strncmp(buf, "m=audio ", 8) == 0) {
      media_kind = SDP_MEDIA_AUDIO;
      ssrc = &pc->remote_assrc;
      pc->remote_media[media_kind].mid[0] = '\0';
      pc->remote_media[media_kind].direction = SDP_DIR_NONE;
    } else if (strncmp(buf, "m=application ", 14) == 0) {
      media_kind = SDP_MEDIA_APPLICATION;
      pc->remote_media[media_kind].mid[0] = '\0';
      pc->remote_media[media_kind].direction = SDP_DIR_NONE;
    }

    if ((val_start = strstr(buf, "a=ssrc:")) && ssrc) {
      *ssrc = strtoul(val_start + 7, NULL, 10);
      LOGD("SSRC: %" PRIu32, *ssrc);
    }

    if (media_kind < SDP_MEDIA_COUNT) {
      if ((val_start = strstr(buf, "a=mid:"))) {
        strncpy(pc->remote_media[media_kind].mid, val_start + 6, SDP_MID_MAX_LEN - 1);
        pc->remote_media[media_kind].mid[SDP_MID_MAX_LEN - 1] = '\0';
      }
      if (strncmp(buf, "a=sendrecv", 10) == 0) {
        pc->remote_media[media_kind].direction = SDP_DIR_SENDRECV;
      } else if (strncmp(buf, "a=sendonly", 10) == 0) {
        pc->remote_media[media_kind].direction = SDP_DIR_SENDONLY;
      } else if (strncmp(buf, "a=recvonly", 10) == 0) {
        pc->remote_media[media_kind].direction = SDP_DIR_RECVONLY;
      } else if (strncmp(buf, "a=inactive", 10) == 0) {
        pc->remote_media[media_kind].direction = SDP_DIR_INACTIVE;
      }
    }

    start = line + 2;
  }

  if (is_update) {
    return;
  }

  agent_set_remote_description(&pc->agent, (char*)sdp);
  if (type == SDP_TYPE_ANSWER) {
    agent_update_candidate_pairs(&pc->agent);
    STATE_CHANGED(pc, PEER_CONNECTION_CHECKING);
  }
}

static const char* peer_connection_create_sdp(PeerConnection* pc, SdpType sdp_type) {
  uint8_t create_candidate_sdp_flag = 0;
  char* description = (char*)pc->temp_buf;
  memset(pc->temp_buf, 0, sizeof(pc->temp_buf));
  DtlsSrtpRole role = DTLS_SRTP_ROLE_SERVER;
  SdpGenerateParam gen;

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
  dtls_srtp_init(&pc->dtls_srtp, role, pc);
  pc->dtls_srtp.udp_recv = peer_connection_dtls_srtp_recv;
  pc->dtls_srtp.udp_send = peer_connection_dtls_srtp_send;

  memset(pc->sdp, 0, sizeof(pc->sdp));

  /* SDP 生成上下文：ANSWER 按 RFC 5888/8843/3264 归一（mid 回显、端口 0+bundle-only、
   * 方向取反、PT 用 pc->h264_pt），OFFER 与既有输出字节级一致 */
  memset(&gen, 0, sizeof(gen));
  gen.is_answer = (sdp_type == SDP_TYPE_ANSWER);
  gen.h264_pt = pc->h264_pt;
  gen.anchor_port = SDP_DATACHANNEL_PORT;
  memcpy(gen.remote, pc->remote_media, sizeof(gen.remote));

  // TODO: check if we have video or audio codecs
  sdp_create(pc->sdp,
             pc->config.video_codec != CODEC_NONE,
             pc->config.audio_codec != CODEC_NONE,
             pc->config.datachannel,
             &gen);

  agent_create_ice_credential(&pc->agent);
  sdp_append(pc->sdp, "a=ice-ufrag:%s", pc->agent.local_ufrag);
  sdp_append(pc->sdp, "a=ice-pwd:%s", pc->agent.local_upwd);
  sdp_append(pc->sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
  sdp_append(pc->sdp, peer_connection_dtls_role_setup_value(role));

  /* 虽然从 RFC 8840 (SDP for BUNDLE) 和 RFC 5245 (ICE) 的标准来看，Session-level candidate 在语法上是合法的，
   * 但在实际的 WebRTC 工程实践中，Media-level candidate 的兼容性和成功率远高于 Session-level。
   * starfan 20260811: 上面两行为网上查找的结论，下面为实测的结果。
   * a=candidate 必须放在第一个 m= 段段之后作为
   * 否则 Chrome 在 第一个 m= 段 段找不到 candidate 会一直等
   * trickle ICE，从不发起 connectivity check（不发 STUN，也不回 STUN）。
   */
  pc->b_local_description_created = 1;

  agent_gather_candidate(&pc->agent, NULL, NULL, NULL);  // host address
  for (int i = 0; i < sizeof(pc->config.ice_servers) / sizeof(pc->config.ice_servers[0]); ++i) {
    if (pc->config.ice_servers[i].urls) {
      LOGI("ice server: %s", pc->config.ice_servers[i].urls);
      agent_gather_candidate(&pc->agent, pc->config.ice_servers[i].urls, pc->config.ice_servers[i].username, pc->config.ice_servers[i].credential);
    }
  }

  agent_get_local_description(&pc->agent, description, sizeof(pc->temp_buf));

  if (pc->config.video_codec == CODEC_H264) {
    sdp_append_h264(pc->sdp, &gen);
    if(0 == create_candidate_sdp_flag) {
        create_candidate_sdp_flag = 1;
        sdp_append(pc->sdp, description);
    }
  }

  switch (pc->config.audio_codec) {
    case CODEC_PCMA:
      sdp_append_pcma(pc->sdp, &gen);
      if(0 == create_candidate_sdp_flag) {
        create_candidate_sdp_flag = 1;
        sdp_append(pc->sdp, description);
      }
      break;
    case CODEC_PCMU:
      sdp_append_pcmu(pc->sdp, &gen);
      if(0 == create_candidate_sdp_flag) {
        create_candidate_sdp_flag = 1;
        sdp_append(pc->sdp, description);
      }
      break;
    case CODEC_OPUS:
      sdp_append_opus(pc->sdp, &gen);
      if(0 == create_candidate_sdp_flag) {
        create_candidate_sdp_flag = 1;
        sdp_append(pc->sdp, description);
      }
    default:
      break;
  }

  if (pc->config.datachannel) {
    sdp_append_datachannel(pc->sdp, &gen);
  }

  if(0 == create_candidate_sdp_flag) {
    create_candidate_sdp_flag = 1;
    sdp_append(pc->sdp, description);
  }

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

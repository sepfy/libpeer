#include <stdarg.h>
#include <stdio.h>

#include "rtp.h"
#include "sdp.h"

int sdp_append(char* sdp, const char* format, ...) {
  va_list argptr;

  va_start(argptr, format);

  if (sdp[0] == '\0') {
    vsnprintf(sdp, CONFIG_SDP_BUFFER_SIZE, format, argptr);
  } else {
    vsnprintf(sdp + strlen(sdp), CONFIG_SDP_BUFFER_SIZE - strlen(sdp), format, argptr);
  }

  if (sdp[strlen(sdp) - 1] != '\n') {
    strcat(sdp, "\r\n");
  }

  va_end(argptr);
  return 0;
}

void sdp_reset(char* sdp) {
  memset(sdp, 0, CONFIG_SDP_BUFFER_SIZE);
}

static const char* sdp_direction_name(SdpDirection direction) {
  switch (direction) {
    case SDP_DIR_SENDONLY:
      return "sendonly";
    case SDP_DIR_RECVONLY:
      return "recvonly";
    case SDP_DIR_INACTIVE:
      return "inactive";
    default:
      return "sendrecv";
  }
}

/* RFC 3264 §6：answer 方向 = offer 取反（未声明视同 sendrecv） */
static SdpDirection sdp_answer_direction(SdpDirection offer_direction) {
  switch (offer_direction) {
    case SDP_DIR_SENDONLY:
      return SDP_DIR_RECVONLY;
    case SDP_DIR_RECVONLY:
      return SDP_DIR_SENDONLY;
    case SDP_DIR_INACTIVE:
      return SDP_DIR_INACTIVE;
    default:
      return SDP_DIR_SENDRECV;
  }
}

/* ANSWER 回显 offer 同 kind 的 a=mid，缺失回退语义名；OFFER 恒语义名 */
static const char* sdp_mid_of(SdpGenerateParam* gen, SdpMediaKind kind, const char* fallback) {
  if (gen->is_answer && gen->remote[kind].mid[0] != '\0')
    return gen->remote[kind].mid;
  return fallback;
}

void sdp_append_h264(char* sdp, SdpGenerateParam* gen) {
  int is_answer = gen->is_answer;
  int is_anchor = (gen->mline_count++ == 0);
  int port = 9;
  const char* mid = "video";
  SdpDirection direction = SDP_DIR_SENDRECV;

  if (is_answer) {
    port = is_anchor ? gen->anchor_port : 0;
    mid = sdp_mid_of(gen, SDP_MEDIA_VIDEO, "video");
    direction = sdp_answer_direction(gen->remote[SDP_MEDIA_VIDEO].direction);
  }

  sdp_append(sdp, "m=video %d UDP/TLS/RTP/SAVPF %d", port, gen->h264_pt);
  if (is_answer && !is_anchor)
    sdp_append(sdp, "a=bundle-only");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtcp-fb:%d nack", gen->h264_pt);
  sdp_append(sdp, "a=rtcp-fb:%d nack pli", gen->h264_pt);
  sdp_append(sdp, "a=fmtp:%d profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1", gen->h264_pt);
  sdp_append(sdp, "a=rtpmap:%d H264/90000", gen->h264_pt);
  sdp_append(sdp, "a=ssrc:1 cname:webrtc-h264");
  sdp_append(sdp, "a=%s", sdp_direction_name(direction));
  sdp_append(sdp, "a=mid:%s", mid);
  sdp_append(sdp, "a=rtcp-mux");
}

void sdp_append_pcma(char* sdp, SdpGenerateParam* gen) {
  int is_answer = gen->is_answer;
  int is_anchor = (gen->mline_count++ == 0);
  int port = 9;
  const char* mid = "audio";
  SdpDirection direction = SDP_DIR_SENDRECV;

  if (is_answer) {
    port = is_anchor ? gen->anchor_port : 0;
    mid = sdp_mid_of(gen, SDP_MEDIA_AUDIO, "audio");
    direction = sdp_answer_direction(gen->remote[SDP_MEDIA_AUDIO].direction);
  }

  sdp_append(sdp, "m=audio %d UDP/TLS/RTP/SAVPF 8", port);
  if (is_answer && !is_anchor)
    sdp_append(sdp, "a=bundle-only");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtpmap:8 PCMA/8000");
  sdp_append(sdp, "a=ssrc:4 cname:webrtc-pcma");
  sdp_append(sdp, "a=%s", sdp_direction_name(direction));
  sdp_append(sdp, "a=mid:%s", mid);
  sdp_append(sdp, "a=rtcp-mux");
}

void sdp_append_pcmu(char* sdp, SdpGenerateParam* gen) {
  int is_answer = gen->is_answer;
  int is_anchor = (gen->mline_count++ == 0);
  int port = 9;
  const char* mid = "audio";
  SdpDirection direction = SDP_DIR_SENDRECV;

  if (is_answer) {
    port = is_anchor ? gen->anchor_port : 0;
    mid = sdp_mid_of(gen, SDP_MEDIA_AUDIO, "audio");
    direction = sdp_answer_direction(gen->remote[SDP_MEDIA_AUDIO].direction);
  }

  sdp_append(sdp, "m=audio %d UDP/TLS/RTP/SAVPF 0", port);
  if (is_answer && !is_anchor)
    sdp_append(sdp, "a=bundle-only");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtpmap:0 PCMU/8000");
  sdp_append(sdp, "a=ssrc:5 cname:webrtc-pcmu");
  sdp_append(sdp, "a=%s", sdp_direction_name(direction));
  sdp_append(sdp, "a=mid:%s", mid);
  sdp_append(sdp, "a=rtcp-mux");
}

void sdp_append_opus(char* sdp, SdpGenerateParam* gen) {
  int is_answer = gen->is_answer;
  int is_anchor = (gen->mline_count++ == 0);
  int port = 9;
  const char* mid = "audio";
  SdpDirection direction = SDP_DIR_SENDRECV;

  if (is_answer) {
    port = is_anchor ? gen->anchor_port : 0;
    mid = sdp_mid_of(gen, SDP_MEDIA_AUDIO, "audio");
    direction = sdp_answer_direction(gen->remote[SDP_MEDIA_AUDIO].direction);
  }

  sdp_append(sdp, "m=audio %d UDP/TLS/RTP/SAVPF 111", port);
  if (is_answer && !is_anchor)
    sdp_append(sdp, "a=bundle-only");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtpmap:111 opus/48000/2");
  sdp_append(sdp, "a=ssrc:6 cname:webrtc-opus");
  sdp_append(sdp, "a=%s", sdp_direction_name(direction));
  sdp_append(sdp, "a=mid:%s", mid);
  sdp_append(sdp, "a=rtcp-mux");
}

void sdp_append_datachannel(char* sdp, SdpGenerateParam* gen) {
  int is_answer = gen->is_answer;
  int is_anchor = (gen->mline_count++ == 0);
  int port = SDP_DATACHANNEL_PORT;
  const char* mid = "datachannel";

  if (is_answer) {
    port = is_anchor ? gen->anchor_port : 0;
    mid = sdp_mid_of(gen, SDP_MEDIA_APPLICATION, "datachannel");
  }

  sdp_append(sdp, "m=application %d UDP/DTLS/SCTP webrtc-datachannel", port);
  if (is_answer && !is_anchor)
    sdp_append(sdp, "a=bundle-only");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=mid:%s", mid);
  sdp_append(sdp, "a=sctp-port:5000");
  sdp_append(sdp, "a=max-message-size:262144");
}

void sdp_create(char* sdp, int b_video, int b_audio, int b_datachannel, SdpGenerateParam* gen) {
  char bundle[256];
  sdp_append(sdp, "v=0");
  sdp_append(sdp, "o=- 1495799811084970 1495799811084970 IN IP4 0.0.0.0");
  sdp_append(sdp, "s=-");
  sdp_append(sdp, "t=0 0");
  sdp_append(sdp, "a=msid-semantic: iot");
#if ICE_LITE
  sdp_append(sdp, "a=ice-lite");
#endif
  memset(bundle, 0, sizeof(bundle));

  strcat(bundle, "a=group:BUNDLE");

  if (b_video) {
    strcat(bundle, " ");
    strcat(bundle, sdp_mid_of(gen, SDP_MEDIA_VIDEO, "video"));
  }

  if (b_audio) {
    strcat(bundle, " ");
    strcat(bundle, sdp_mid_of(gen, SDP_MEDIA_AUDIO, "audio"));
  }

  if (b_datachannel) {
    strcat(bundle, " ");
    strcat(bundle, sdp_mid_of(gen, SDP_MEDIA_APPLICATION, "datachannel"));
  }

  sdp_append(sdp, bundle);
}

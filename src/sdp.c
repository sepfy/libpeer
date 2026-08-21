#include <stdarg.h>
#include <stdio.h>

#include "sdp.h"

#define SDP_DEFAULT_H264_FMTP \
  "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f"
#define SDP_DEFAULT_VIDEO_MID "1"
#define SDP_DEFAULT_VIDEO_STREAM_ID "libpeer-stream"
#define SDP_DEFAULT_VIDEO_TRACK_ID "webrtc-h264"

static const char* sdp_select_token(const char* value, const char* fallback) {
  size_t index = 0;
  int valid = value != NULL && value[0] != '\0';

  while (valid && value[index] != '\0') {
    unsigned char current = (unsigned char)value[index];
    if (current <= 0x20 || current >= 0x7f || index >= 63) {
      valid = 0;
    } else {
      index++;
    }
  }

  return valid ? value : fallback;
}

static const char* sdp_select_line_value(const char* value,
                                         const char* fallback) {
  size_t index = 0;
  int valid = value != NULL && value[0] != '\0';

  while (valid && value[index] != '\0') {
    if (value[index] == '\r' || value[index] == '\n' || index >= 255) {
      valid = 0;
    } else {
      index++;
    }
  }

  return valid ? value : fallback;
}

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

void sdp_append_h264(char* sdp, uint8_t payload_type, uint32_t ssrc,
                     const char* mid, const char* fmtp, int send_only,
                     const char* stream_id, const char* track_id) {
  const char* selected_mid =
      sdp_select_token(mid, SDP_DEFAULT_VIDEO_MID);
  const char* selected_fmtp =
      sdp_select_line_value(fmtp, SDP_DEFAULT_H264_FMTP);
  const char* selected_stream_id =
      sdp_select_token(stream_id, SDP_DEFAULT_VIDEO_STREAM_ID);
  const char* selected_track_id =
      sdp_select_token(track_id, SDP_DEFAULT_VIDEO_TRACK_ID);

  if (payload_type < 96 || payload_type > 127) {
    payload_type = 96;
  }
  if (ssrc == 0) {
    ssrc = 1;
  }

  sdp_append(sdp, "m=video 9 UDP/TLS/RTP/SAVPF %u", payload_type);
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtcp-fb:%u nack", payload_type);
  sdp_append(sdp, "a=rtcp-fb:%u nack pli", payload_type);
  sdp_append(sdp, "a=fmtp:%u %s", payload_type, selected_fmtp);
  sdp_append(sdp, "a=rtpmap:%u H264/90000", payload_type);
  sdp_append(sdp, "a=ssrc:%u cname:%s", ssrc, selected_track_id);
  sdp_append(sdp, "a=ssrc:%u msid:%s %s", ssrc, selected_stream_id,
             selected_track_id);
  sdp_append(sdp, "a=msid:%s %s", selected_stream_id, selected_track_id);
  sdp_append(sdp, send_only ? "a=sendonly" : "a=sendrecv");
  sdp_append(sdp, "a=mid:%s", selected_mid);
  sdp_append(sdp, "a=rtcp-mux");
}

void sdp_append_pcma(char* sdp) {
  sdp_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 8");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtpmap:8 PCMA/8000");
  sdp_append(sdp, "a=ssrc:4 cname:webrtc-pcma");
  sdp_append(sdp, "a=sendrecv");
  sdp_append(sdp, "a=mid:2");
  sdp_append(sdp, "a=rtcp-mux");
}

void sdp_append_pcmu(char* sdp) {
  sdp_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 0");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtpmap:0 PCMU/8000");
  sdp_append(sdp, "a=ssrc:5 cname:webrtc-pcmu");
  sdp_append(sdp, "a=sendrecv");
  sdp_append(sdp, "a=mid:2");
  sdp_append(sdp, "a=rtcp-mux");
}

void sdp_append_opus(char* sdp) {
  sdp_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 111");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtpmap:111 opus/48000/2");
  sdp_append(sdp, "a=ssrc:6 cname:webrtc-opus");
  sdp_append(sdp, "a=sendrecv");
  sdp_append(sdp, "a=mid:2");
  sdp_append(sdp, "a=rtcp-mux");
}

void sdp_append_datachannel(char* sdp) {
  sdp_append(sdp, "m=application 50712 UDP/DTLS/SCTP webrtc-datachannel");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=mid:0");
  sdp_append(sdp, "a=sctp-port:5000");
  sdp_append(sdp, "a=max-message-size:262144");
}

void sdp_create(char* sdp, int b_video, int b_audio, int b_datachannel,
                const char* video_mid) {
  const char* selected_video_mid =
      sdp_select_token(video_mid, SDP_DEFAULT_VIDEO_MID);
  char bundle[128];

  sdp_append(sdp, "v=0");
  sdp_append(sdp, "o=- 1495799811084970 1495799811084970 IN IP4 0.0.0.0");
  sdp_append(sdp, "s=-");
  sdp_append(sdp, "t=0 0");
  sdp_append(sdp, "a=msid-semantic: WMS *");
#if ICE_LITE
  sdp_append(sdp, "a=ice-lite");
#endif
  snprintf(bundle, sizeof(bundle), "%s", "a=group:BUNDLE");

  if (b_datachannel) {
    strcat(bundle, " 0");
  }

  if (b_video) {
    snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle),
             " %s", selected_video_mid);
  }

  if (b_audio) {
    snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle),
             " %s", "2");
  }

  sdp_append(sdp, bundle);
}

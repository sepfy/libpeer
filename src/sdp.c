/**
 * SDP Generation - aiortc-compatible format
 *
 * This generates SDP that matches aiortc 1:1 for Pipecat interoperability.
 *
 * Structure:
 * - Session level: v=, o=, s=, t=, a=group:BUNDLE, a=msid-semantic:WMS *
 * - Per media section: m=, c=, attributes, candidates, ice-ufrag/pwd, fingerprint, setup
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

/**
 * Append H264 video media section with aiortc-compatible attribute order
 * Order: m=, c=, direction, mid, rtcp, rtcp-mux, ssrc, rtpmap, rtcp-fb, fmtp
 */
void sdp_append_h264(char* sdp, int mid) {
  sdp_append(sdp, "m=video 9 UDP/TLS/RTP/SAVPF 96");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=sendrecv");
  sdp_append(sdp, "a=mid:%d", mid);
  sdp_append(sdp, "a=rtcp:9 IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtcp-mux");
  sdp_append(sdp, "a=ssrc:1 cname:webrtc-h264");
  sdp_append(sdp, "a=rtpmap:96 H264/90000");
  sdp_append(sdp, "a=rtcp-fb:96 nack");
  sdp_append(sdp, "a=rtcp-fb:96 nack pli");
  sdp_append(sdp, "a=fmtp:96 profile-level-id=42e01f;level-asymmetry-allowed=1");
}

/**
 * Append PCMA audio media section with aiortc-compatible attribute order
 * Order: m=, c=, direction, mid, rtcp, rtcp-mux, ssrc, rtpmap
 */
void sdp_append_pcma(char* sdp, int mid) {
  sdp_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVPF 8");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=sendrecv");
  sdp_append(sdp, "a=mid:%d", mid);
  sdp_append(sdp, "a=rtcp:9 IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtcp-mux");
  sdp_append(sdp, "a=ssrc:4 cname:webrtc-pcma");
  sdp_append(sdp, "a=rtpmap:8 PCMA/8000");
}

/**
 * Append PCMU audio media section with aiortc-compatible attribute order
 * Order: m=, c=, direction, mid, rtcp, rtcp-mux, ssrc, rtpmap
 */
void sdp_append_pcmu(char* sdp, int mid) {
  sdp_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVPF 0");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=sendrecv");
  sdp_append(sdp, "a=mid:%d", mid);
  sdp_append(sdp, "a=rtcp:9 IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtcp-mux");
  sdp_append(sdp, "a=ssrc:5 cname:webrtc-pcmu");
  sdp_append(sdp, "a=rtpmap:0 PCMU/8000");
}

/**
 * Append Opus audio media section with aiortc-compatible attribute order
 * Order: m=, c=, direction, mid, rtcp, rtcp-mux, ssrc, rtpmap, rtcp-fb, fmtp
 */
void sdp_append_opus(char* sdp, int mid) {
  sdp_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVPF 111");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=sendrecv");
  sdp_append(sdp, "a=mid:%d", mid);
  sdp_append(sdp, "a=rtcp:9 IN IP4 0.0.0.0");
  sdp_append(sdp, "a=rtcp-mux");
  sdp_append(sdp, "a=ssrc:6 cname:webrtc-opus");
  sdp_append(sdp, "a=rtpmap:111 opus/48000/2");
}

/**
 * Append datachannel media section with aiortc-compatible attribute order
 */
void sdp_append_datachannel(char* sdp, int mid) {
  sdp_append(sdp, "m=application 9 UDP/DTLS/SCTP webrtc-datachannel");
  sdp_append(sdp, "c=IN IP4 0.0.0.0");
  sdp_append(sdp, "a=mid:%d", mid);
  sdp_append(sdp, "a=sctp-port:5000");
  sdp_append(sdp, "a=max-message-size:65536");
}

/**
 * Create SDP session-level header with numeric BUNDLE mids
 *
 * @param sdp Output buffer
 * @param b_video Include video in BUNDLE
 * @param b_audio Include audio in BUNDLE
 * @param b_datachannel Include datachannel in BUNDLE
 */
void sdp_create(char* sdp, int b_video, int b_audio, int b_datachannel) {
  sdp_append(sdp, "v=0");
  sdp_append(sdp, "o=- 1495799811084970 1495799811084970 IN IP4 0.0.0.0");
  sdp_append(sdp, "s=-");
  sdp_append(sdp, "t=0 0");

  // Build BUNDLE with numeric mids (like aiortc)
  // Mid assignment: video=0 (if present), audio=0 or 1, datachannel=last
  char bundle[64];
  memset(bundle, 0, sizeof(bundle));
  strcat(bundle, "a=group:BUNDLE");

  int mid = 0;
  if (b_video) {
    char num[8];
    snprintf(num, sizeof(num), " %d", mid++);
    strcat(bundle, num);
  }
  if (b_audio) {
    char num[8];
    snprintf(num, sizeof(num), " %d", mid++);
    strcat(bundle, num);
  }
  if (b_datachannel) {
    char num[8];
    snprintf(num, sizeof(num), " %d", mid++);
    strcat(bundle, num);
  }

  sdp_append(sdp, bundle);
  sdp_append(sdp, "a=msid-semantic:WMS *");

#if ICE_LITE
  sdp_append(sdp, "a=ice-lite");
#endif
}

#include <stdio.h>
#include <string.h>

#include "sdp.h"

static int failures = 0;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
              #condition);                                                 \
      failures++;                                                          \
    }                                                                      \
  } while (0)

static void test_custom_h264_media_description(void) {
  char sdp[CONFIG_SDP_BUFFER_SIZE];

  sdp_reset(sdp);
  sdp_create(sdp, 1, 0, 0, "0");
  sdp_append_h264(
      sdp, 102, 0x11223344, "0",
      "packetization-mode=1;profile-level-id=64001f", 1,
      "camera-stream", "camera-track");

  CHECK(strstr(sdp, "a=group:BUNDLE 0\r\n") != NULL);
  CHECK(strstr(sdp, "a=msid-semantic: WMS *\r\n") != NULL);
  CHECK(strstr(sdp, "m=video 9 UDP/TLS/RTP/SAVPF 102\r\n") != NULL);
  CHECK(strstr(sdp, "a=rtpmap:102 H264/90000\r\n") != NULL);
  CHECK(strstr(sdp,
               "a=fmtp:102 packetization-mode=1;"
               "profile-level-id=64001f\r\n") != NULL);
  CHECK(strstr(sdp, "a=ssrc:287454020 cname:camera-track\r\n") != NULL);
  CHECK(strstr(sdp,
               "a=ssrc:287454020 msid:camera-stream camera-track\r\n") !=
        NULL);
  CHECK(strstr(sdp, "a=msid:camera-stream camera-track\r\n") != NULL);
  CHECK(strstr(sdp, "a=sendonly\r\n") != NULL);
  CHECK(strstr(sdp, "a=mid:0\r\n") != NULL);
}

static void test_invalid_values_use_safe_defaults(void) {
  char sdp[CONFIG_SDP_BUFFER_SIZE];

  sdp_reset(sdp);
  sdp_create(sdp, 1, 0, 0, "0\r\na=ice-lite");
  sdp_append_h264(sdp, 200, 0, "0\r\na=ice-lite",
                  "profile-level-id=42e01f\r\na=inactive", 0,
                  "bad stream", "bad\r\ntrack");

  CHECK(strstr(sdp, "a=group:BUNDLE 1\r\n") != NULL);
  CHECK(strstr(sdp, "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n") != NULL);
  CHECK(strstr(sdp,
               "a=fmtp:96 level-asymmetry-allowed=1;"
               "packetization-mode=1;profile-level-id=42e01f\r\n") != NULL);
  CHECK(strstr(sdp, "a=ssrc:1 cname:webrtc-h264\r\n") != NULL);
  CHECK(strstr(sdp,
               "a=ssrc:1 msid:libpeer-stream webrtc-h264\r\n") != NULL);
  CHECK(strstr(sdp, "a=sendrecv\r\n") != NULL);
  CHECK(strstr(sdp, "a=mid:1\r\n") != NULL);
  CHECK(strstr(sdp, "\r\na=ice-lite\r\n") == NULL);
  CHECK(strstr(sdp, "\r\na=inactive\r\n") == NULL);
}

int main(void) {
  test_custom_h264_media_description();
  test_invalid_values_use_safe_defaults();

  if (failures != 0) {
    fprintf(stderr, "%d test checks failed\n", failures);
  }
  return failures == 0 ? 0 : 1;
}

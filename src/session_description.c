#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "utils.h"
#include "media_stream.h"
#include "session_description.h"

struct SessionDescription {

  size_t size;
  char content[SDP_MAX_SIZE];

};

SessionDescription* session_description_create(void) {

  SessionDescription *sdp = NULL;
  sdp = (SessionDescription*)malloc(sizeof(SessionDescription));
  if(!sdp)
    return sdp;

  memset(sdp->content, 0, sizeof(sdp->content));
  return sdp;
}

void session_description_destroy(SessionDescription *sdp) {

  if(sdp) {
    free(sdp);
    sdp = NULL;
  }
}

int session_description_update_mdns_of_candidate(char *candidate_src, char *candidate_dst, size_t size) {

  int token_of_mdns = 4;
  char hostname[128] = {0};
  char ipv4addr[64] = {0};
  char prefix[64] = {0};

  char *end;
  char *start = candidate_src;
  while(token_of_mdns > 0) {
    start = strstr(start, " ") + 1;
    token_of_mdns--;
  }
  end = strstr(start, " ");
  strncpy(hostname, start, end - start);
  if(utils_get_ipv4addr(hostname, ipv4addr, sizeof(ipv4addr)) < 0)
    return -1;
  strncpy(prefix, candidate_src, start - candidate_src);
  snprintf(candidate_dst, size, "%s%s%s", prefix, ipv4addr, end);
  return 0;
}

int session_description_append_newline(SessionDescription *sdp, const char *format, ...) {

  va_list argptr;
  char attribute[128] = {0};

  if(strstr(format, "=") == NULL)
    return -1;

  va_start(argptr, format);
  vsnprintf(attribute, sizeof(attribute), format, argptr);
  va_end(argptr);

  strcat(sdp->content, attribute);
  strcat(sdp->content, "\r\n");

  return 0;
}

int session_description_append(SessionDescription *sdp, const char *format, ...) {

  va_list argptr;
  char attribute[128] = {0};

  if(strstr(format, "=") == NULL)
    return -1;

  va_start(argptr, format);
  vsnprintf(attribute, sizeof(attribute), format, argptr);
  va_end(argptr);

  strcat(sdp->content, attribute);
  strcat(sdp->content, "\\r\\n");

  return 0;
}

char* session_description_get_content(SessionDescription *sdp) {

  return sdp->content; 
}


void session_description_add_codec(SessionDescription *sdp, MediaCodec codec,
 TransceiverDirection direction, const char *ufrag, const char *password, const char *fingerprint, int mid) {

  switch(codec) {
    case CODEC_H264:
      session_description_append(sdp, "m=video 9 UDP/TLS/RTP/SAVPF 102");
      session_description_append(sdp, "a=rtpmap:102 H264/90000");
      session_description_append(sdp, "a=fmtp:102 packetization-mode=1");
      session_description_append(sdp, "a=rtcp-fb:102 nack");
      session_description_append(sdp, "a=rtcp-fb:102 nack pli");
      session_description_append(sdp, "a=fmtp:102 x-google-max-bitrate=6000;x-google-min-bitrate=2000;x-google-start-bitrate=4000");
      break;
    case CODEC_OPUS:
      session_description_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 111");
      session_description_append(sdp, "a=rtcp-fb:111 nack");
      session_description_append(sdp, "a=rtpmap:111 opus/48000/2");
      session_description_append(sdp, "a=ssrc:12 cname:YZcxBwerFFm6GH69");
      break;
    case CODEC_PCMA:
      session_description_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 8");
      session_description_append(sdp, "a=rtpmap:8 PCMA/8000");
      session_description_append(sdp, "a=ssrc:12 cname:YZcxBwerFFm6GH69");
      break;
    default:
      return;
  }

  session_description_append(sdp, "c=IN IP4 0.0.0.0");

  switch(direction) {
    case SENDRECV:
      session_description_append(sdp, "a=sendrecv");
      break;
    case RECVONLY:
      session_description_append(sdp, "a=recvonly");
      break;
    case SENDONLY:
      session_description_append(sdp, "a=sendonly");
      break;
    default:
      break;
  }

  session_description_append(sdp, "a=mid:%d", mid);
  session_description_append(sdp, "a=rtcp-mux");
  session_description_append(sdp, "a=ice-ufrag:%s", ufrag);
  session_description_append(sdp, "a=ice-pwd:%s", password);
  session_description_append(sdp, "a=ice-options:trickle");
  session_description_append(sdp, "a=fingerprint:sha-256 %s", fingerprint);
  session_description_append(sdp, "a=setup:passive");

}

uint32_t session_description_find_ssrc(const char *type, const char *sdp) {

  char *media_line = strstr(sdp, type);
  char *ssrc_pos = NULL;
  uint32_t ssrc = 0;

  if(media_line == NULL)
    return ssrc;
  ssrc_pos = strstr(media_line, "ssrc:");

  if(ssrc_pos == NULL)
    return ssrc;

  ssrc = strtoul(ssrc_pos + 5, NULL, 0);
  return ssrc;
}

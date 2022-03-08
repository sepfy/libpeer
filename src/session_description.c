#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <glib.h>

#include "utils.h"
#include "media_stream.h"
#include "session_description.h"

struct SessionDescription {

  RtpMap rtp_map;
  size_t size;
  char content[SDP_MAX_SIZE];

  MediaType mids[2];
};

SessionDescription* session_description_create(void) {

  SessionDescription *sdp = NULL;
  sdp = (SessionDescription*)malloc(sizeof(SessionDescription));
  if(!sdp)
    return sdp;

  sdp->mids[0] = MEDIA_TYPE_NONE;
  sdp->mids[1] = MEDIA_TYPE_NONE;

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
  char attribute[256] = {0};

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


void session_description_add_media(SessionDescription *sdp, MediaType type,
 TransceiverDirection direction, const char *ufrag, const char *password, const char *fingerprint, int mid) {

  switch(type) {
    case MEDIA_TYPE_VIDEO:
      session_description_append(sdp, "m=video 9 UDP/TLS/RTP/SAVPF 102");
      session_description_append(sdp, "a=rtpmap:102 H264/90000");
      session_description_append(sdp, "a=fmtp:102 packetization-mode=1");
      session_description_append(sdp, "a=rtcp-fb:102 nack");
      session_description_append(sdp, "a=rtcp-fb:102 nack pli");
      session_description_append(sdp, "a=fmtp:102 x-google-max-bitrate=6000;x-google-min-bitrate=2000;x-google-start-bitrate=4000");
      break;
    case MEDIA_TYPE_AUDIO:
      session_description_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 8 111");
      session_description_append(sdp, "a=rtcp-fb:111 nack");
      session_description_append(sdp, "a=rtpmap:111 opus/48000/2");
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

void session_description_parse_sdp_text(SessionDescription *sdp, const char *sdp_text, int mdns_enabled) {

  gchar **splits;
  RtpMap rtp_map;
  int i;
  int pt = 0;
  char codec[16];
  char *pt_start, *codec_start, *codec_end;
  char *pos;
  int mid = 0; 

  splits = g_strsplit(sdp_text, "\r\n", 256);

  for(i = 0; splits[i] != NULL; i++) {

    // mid
    if(strstr(splits[i], "m=")) {
      if(strstr(splits[i], "audio")) {
        sdp->mids[mid++] = MEDIA_TYPE_AUDIO;
      }
      else if(strstr(splits[i], "video")) {
        sdp->mids[mid++] = MEDIA_TYPE_VIDEO;
      }
    }

    // resolve candidate
    if(strstr(splits[i], "candidate") != NULL && strstr(splits[i], "local") != NULL) {

      if(mdns_enabled) {
        char buf[256] = {0};
        if(session_description_update_mdns_of_candidate(splits[i], buf, sizeof(buf)) != -1) {
          session_description_append_newline(sdp, buf);
        }
      }
    }
    else {
      session_description_append_newline(sdp, splits[i]);
    }

    // resolve rtpmap
    if(strstr(splits[i], "rtpmap")) {

      pt_start = strstr(splits[i], ":");
      codec_start = strstr(splits[i], " ") + 1;
      codec_end = strstr(splits[i], "/");

      if(pt_start && codec_start && codec_end) {

        pt = atoi(pt_start + 1);
        memset(codec, 0, sizeof(codec));
        strncpy(codec, codec_start, codec_end - codec_start);

        if(strcmp(codec, "H264") == 0)
          sdp->rtp_map.pt_h264 = pt;
        else if(strcmp(codec, "PCMA") == 0)
          sdp->rtp_map.pt_pcma = pt;
        else if(strcmp(codec, "opus") == 0)
          sdp->rtp_map.pt_opus = pt;
      }
    }

#if 0
LOG_INFO("%d, %d\n", sdp->audio_mid, sdp->video_mid);
    //ssrc
    if(strstr(splits[i], "ssrc:")) {
      LOG_INFO("%d\n", splits[i]);
      //ssrc = strtoul(ssrc_pos + 5, NULL, 0);
    }
#endif
  }
}

MediaType* session_description_get_mid(SessionDescription *sdp) {

	return sdp->mids;
}

RtpMap session_description_get_rtpmap(SessionDescription *sdp) {

  //a=rtpmap:111 opus/48000/2
  //a=rtpmap:8 PCMA/8000
  //a=rtpmap:108 H264/90000
  return sdp->rtp_map;
}

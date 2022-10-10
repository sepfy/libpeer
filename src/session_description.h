/**
 * @file session_description.h
 * @brief Struct SessionDescription
 */
#ifndef SESSION_DESCRIPTION_H_
#define SESSION_DESCRIPTION_H_

#include <stdlib.h>
#include <stdint.h>

#include "rtp_packet.h"
#include "media_stream.h"

#define MEDIA_DESCRIPTION_MAX_NUM 10
#define SDP_MAX_SIZE 20480

typedef enum MediaDescription {

  MEDIA_NONE,
  MEDIA_VIDEO,
  MEDIA_AUDIO,
  MEDIA_DATACHANNEL,

} MediaDescription;

typedef struct SessionDescription SessionDescription;

struct SessionDescription {

  size_t size;

  int mdns_enabled:1;

  int datachannel_enabled:1;

  MediaDescription media_descriptions[MEDIA_DESCRIPTION_MAX_NUM];

  RtpMap rtp_map;

  int media_description_num;

  char content[SDP_MAX_SIZE];

};

/**
 * @brief Create a new SessionDescription.
 * @return Pointer of struct SessionDescription.
 */
SessionDescription* session_description_create(char *sdp_text);

/**
 * @brief Destroy a SessionDescription.
 * @param Pointer of struct SessionDescription.
 */
void session_description_destroy(SessionDescription *sdp);

/**
 * @brief Append new attribute to session description content.
 */
int session_description_append(SessionDescription *sdp, const char *attribute, ...);

/**
 * @brief Append new attribute to session description content with newline.
 */
int session_description_append_newline(SessionDescription *sdp, const char *attribute, ...);

/**
 * @brief Change mDNS information to ipv4 addr of a candidate.
 * @param Source candidate string buf.
 * @param Destination candidate string buf.
 * @parma Size of destination candidate string buf.
 */
int session_description_update_mdns_of_candidate(char *candidate_src, char *candidate_dst, size_t size);

/**
 * @brief Get the content of session description protocol content.
 * @param SessionDescription.
 * @return The content of seesion description protocol.
 */
char* session_description_get_content(SessionDescription *sdp);

/**
 * @brief Find RTP ssrc of audio or video in SDP.
 * @param Type is audio or video.
 * @param The content of session description protocol.
 */
uint32_t session_description_find_ssrc(const char *type, const char *sdp);

RtpMap session_description_get_rtpmap(SessionDescription *sdp);

void session_description_set_mdns_enabled(SessionDescription *sdp, int enabled);

MediaDescription* session_description_get_media_descriptions(SessionDescription *sdp, int *num);

#endif // SESSION_DESCRIPTION_H_

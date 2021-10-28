/**
 * @file session_description.h
 * @brief Struct SessionDescription
 */
#ifndef SESSION_DESCRIPTION_H_
#define SESSION_DESCRIPTION_H_

#include <stdlib.h>

#include "media_stream.h"

#define SDP_ATTIBUTES_MAX 10240

typedef enum transceiver_direction_t {

  SENDRECV,
  RECVONLY,
  SENDONLY,

} transceiver_direction_t;

typedef struct transceiver_t {

  transceiver_direction_t audio;
  transceiver_direction_t video;

} transceiver_t;

typedef struct SessionDescription SessionDescription;

/**
 * @brief Create a new SessionDescription.
 * @return Pointer of struct SessionDescription.
 */
SessionDescription* session_description_create(void);

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
 * @brief Append media codec format to session description protocol content.
 * @param SessionDescription.
 * @param MediaCodec.
 * @param The direction of media stream.
 * @param ufrag of DTLS.
 * @param password of DTLS.
 * @param fingerprint of DTLS.
 */
void session_description_add_codec(SessionDescription *sdp, MediaCodec codec,
 transceiver_direction_t direction, const char *ufrag, const char *password, const char *fingerprint);

#endif // SESSION_DESCRIPTION_H_

/**
 * @file session_description.h
 * @brief Struct SessionDescription
 */
#ifndef SESSION_DESCRIPTION_H_
#define SESSION_DESCRIPTION_H_

#include <stdlib.h>
#include <stdint.h>

#include "media_stream.h"

#define SDP_MAX_SIZE 10240

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
 * @param mid.
 */
void session_description_add_codec(SessionDescription *sdp, MediaCodec codec,
 TransceiverDirection direction, const char *ufrag, const char *password, const char *fingerprint, int mid);

/**
 * @brief Find RTP ssrc of audio or video in SDP.
 * @param Type is audio or video.
 * @param The content of session description protocol.
 */
uint32_t session_description_find_ssrc(const char *type, const char *sdp);

#endif // SESSION_DESCRIPTION_H_

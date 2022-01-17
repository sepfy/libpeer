/**
 * @file peer_connection.h
 * @brief Struct PeerConnection
 */
#ifndef PEER_CONNECTION_H_
#define PEER_CONNECTION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <agent.h>

#include "utils.h"
#include "session_description.h"
#include "dtls_transport.h"
#include "media_stream.h"

typedef enum IceConnectionState {

  DISCONNECTED,
  GATHERING,
  CONNECTING,
  CONNECTED,
  READY,
  FAILED,

} IceConnectionState;

typedef struct PeerConnection PeerConnection;

/**
 * @brief Create a struct PeerConnection and initialize it.
 * @return Pointer of PeerConnection.
 */
PeerConnection* peer_connection_create();

/**
 * @brief Destory a struct PeerConnection.
 */
void peer_connection_destroy(PeerConnection *pc);

/**
 * @brief Let PeerConnection send RTCP PIL.
 * @param PeerConnection
 * @param RTP ssrc
 */
int peer_connection_send_rtcp_pil(PeerConnection *pc, uint32_t ssrc);

/**
 * @brief Add audio or video stream to PeerConnection.
 * @param A PeerConnection.
 * @param A MediaStream.
 */
void peer_connection_add_stream(PeerConnection *pc, MediaStream *media_stream);

/**
 * @brief Set the callback function to handle onicecandidate event.
 * @param A PeerConnection.
 * @param A callback function to handle onicecandidate event.
 * @param A userdata which is pass to callback function. 
 */
void peer_connection_onicecandidate(PeerConnection *pc, void (*onicecandidate), void  *userdata);

/**
 * @brief Set the callback function to handle oniceconnectionstatechange event.
 * @param A PeerConnection.
 * @param A callback function to handle oniceconnectionstatechange event.
 * @param A userdata which is pass to callback function. 
 */
void peer_connection_oniceconnectionstatechange(PeerConnection *pc,
 void (*oniceconnectionstatechange), void *userdata);

/**
 * @brief Set the callback function to handle ontrack event.
 * @param A PeerConnection.
 * @param A callback function to handle ontrack event.
 * @param A userdata which is pass to callback function. 
 */
void peer_connection_ontrack(PeerConnection *pc, void (*ontrack), void *userdata);

/**
 * @brief sets the specified session description as the remote peer's current offer or answer.
 * @param PeerConnection.
 * @param SDP string.
 */
void peer_connection_set_remote_description(PeerConnection *pc, char *sdp);

/**
 * @brief Add a new RtpTransceiver to the set of transceivers associated with the PeerConnection.
 * @param PeerConnection.
 * @param RtpTransceiver.
 */
int peer_connection_add_transceiver(PeerConnection *pc, Transceiver transceiver);

/**
 * @brief PeerConnection creates an answer.
 * @param PeerConnection.
 */
int peer_connection_create_answer(PeerConnection *pc);

/**
 * @brief Get audio and video ssrc from a PeerConnection after set remote description.
 * @param PeerConnection.
 * @param Media type of audio and video.
 */
uint32_t peer_connection_get_ssrc(PeerConnection *pc, const char *type);

/**
 * @brief Get payload type of codec in SDP.
 * @param PeerConnection.
 * @param Media Codec of audio or video.
 */
int peer_connection_get_rtpmap(PeerConnection *pc, MediaCodec codec);

/**
 * @brief Support to resolve mDNS candidate.
 * @param PeerConnection.
 * @param Boolean. Default is FALSE.
 */
void peer_connection_enable_mdns(PeerConnection *pc, gboolean b_enabled);

// To confirm:
int peer_connection_send_rtp_packet(PeerConnection *pc, uint8_t *packet, int bytes);

void peer_connection_set_on_transport_ready(PeerConnection *pc, void (*on_transport_ready), void *data);

#ifdef __cplusplus
}
#endif

#endif // PEER_CONNECTION_H_

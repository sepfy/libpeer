/**
 * @file peer_connection.h
 * @brief Struct PeerConnection
 */
#ifndef PEER_CONNECTION_H_
#define PEER_CONNECTION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sctp.h"
#include "agent.h"
#include "dtls_srtp.h"
#include "sdp.h"
#include "codec.h"
#include "task.h"
#include "config.h"
#include "rtp.h"
#include "rtcp_packet.h"


#ifdef HAVE_GST
#include "gst/media_stream.h"
#endif

typedef enum PeerConnectionState {

  PEER_CONNECTION_CLOSED = 0,
  PEER_CONNECTION_NEW,
  PEER_CONNECTION_CHECKING,
  PEER_CONNECTION_CONNECTED,
  PEER_CONNECTION_COMPLETED,
  PEER_CONNECTION_FAILED,
  PEER_CONNECTION_DISCONNECTED,

} PeerConnectionState;

typedef struct PeerOptions {

  MediaCodec audio_codec;
  MediaCodec video_codec;
  int b_datachannel;
  
#ifdef HAVE_GST
  const char *audio_outgoing_pipeline;
  const char *audio_incoming_pipeline;
  const char *video_outgoing_pipeline;
  const char *video_incoming_pipeline;
#endif

} PeerOptions;

typedef struct PeerConnection PeerConnection;

struct PeerConnection {

  PeerOptions options;
  PeerConnectionState state;
  Agent agent;
  DtlsSrtp dtls_srtp;
  Sctp sctp;

  Sdp local_sdp;
  Sdp remote_sdp;

  void (*onicecandidate)(char *sdp, void *user_data);
  void (*oniceconnectionstatechange)(IceCandidateState state, void *user_data);
  void (*ontrack)(uint8_t *packet, size_t bytes, void *user_data);
  void (*on_connected)(void *userdata);
  void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void *user_data);

  void *user_data;

  uint8_t agent_buf[CONFIG_MTU];
  int agent_ret;

#ifdef HAVE_GST
  MediaStream *audio_stream;
  MediaStream *video_stream;
#endif

};

void peer_connection_configure(PeerConnection *pc, PeerOptions *options);

void peer_connection_init(PeerConnection *pc);

void peer_connection_set_remote_description(PeerConnection *pc, const char *sdp);

const char* peer_connection_create_offer(PeerConnection *pc);

int peer_connection_loop(PeerConnection *pc);

/**
 * @brief register callback function to handle packet loss from RTCP receiver report
 * @param[in] peer connection
 * @param[in] callback function void (*cb)(float fraction_loss, uint32_t total_loss, void *userdata)
 * @param[in] userdata for callback function
 */
void peer_connection_on_receiver_packet_loss(PeerConnection *pc,
 void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void *userdata));

/**
 * @brief register callback function to handle event when the connection is established
 * @param[in] peer connection
 * @param[in] callback function void (*cb)(void *userdata)
 * @param[in] userdata for callback function
 */
void peer_connection_on_connected(PeerConnection *pc, void (*on_connected)(void *userdata));
/**
 * @brief Set the callback function to handle onicecandidate event.
 * @param A PeerConnection.
 * @param A callback function to handle onicecandidate event.
 * @param A userdata which is pass to callback function. 
 */
void peer_connection_onicecandidate(PeerConnection *pc, void (*onicecandidate)(char *sdp_text, void *userdata));

/**
 * @brief Set the callback function to handle oniceconnectionstatechange event.
 * @param A PeerConnection.
 * @param A callback function to handle oniceconnectionstatechange event.
 * @param A userdata which is pass to callback function. 
 */
void peer_connection_oniceconnectionstatechange(PeerConnection *pc,
 void (*oniceconnectionstatechange)(IceCandidateState state, void *userdata));

/**
 * @brief Set the callback function to handle ontrack event.
 * @param A PeerConnection.
 * @param A callback function to handle ontrack event.
 * @param A userdata which is pass to callback function. 
 */
void peer_connection_ontrack(PeerConnection *pc, void (*ontrack)(uint8_t *packet, size_t bytes, void *userdata));


/**
 * @brief register callback function to handle event of datachannel
 * @param[in] peer connection
 * @param[in] callback function when message received
 * @param[in] callback function when connection is opened
 * @param[in] callback function when connection is closed
 */
void peer_connection_ondatachannel(PeerConnection *pc,
 void (*onmessasge)(char *msg, size_t len, void *userdata),
 void (*onopen)(void *userdata),
 void (*onclose)(void *userdata));

/**
 * @brief send message to data channel
 * @param[in] peer connection
 * @param[in] message buffer
 * @param[in] length of message
 */
int peer_connection_datachannel_send(PeerConnection *pc, char *message, size_t len);



int peer_connection_send_rtp_packet(PeerConnection *pc, uint8_t *packet, int bytes);


#if 0
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
PeerConnection* peer_connection_create(void *userdata);

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
void peer_connection_add_stream(PeerConnection *pc, MediaCodec codec,
 const char *outgoing_pipeline,
 const char *incoming_pipeline);

/**
 * @brief sets the specified session description as the remote peer's current offer or answer.
 * @param PeerConnection.
 * @param SDP string.
 */
void peer_connection_set_remote_description(PeerConnection *pc, const char *sdp);

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
void peer_connection_enable_mdns(PeerConnection *pc, int b_enabled);

// To confirm:
void peer_connection_media_stream_playback(PeerConnection *pc);
#endif


#ifdef __cplusplus
}
#endif

#endif // PEER_CONNECTION_H_

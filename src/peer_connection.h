/**
 * @file peer_connection.h
 * @brief Struct PeerConnection
 */
#ifndef PEER_CONNECTION_H_
#define PEER_CONNECTION_H_

#include <stdlib.h>
#include <stdint.h>
#include "sctp.h"
#include "agent.h"
#include "dtls_srtp.h"
#include "sdp.h"
#include "mediacodec.h"
#include "rtp.h"
#include "buffer.h"

#ifdef __cplusplus
extern "C" {
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

typedef enum DataChannelType {

  DATA_CHANNEL_NONE = 0, 
  DATA_CHANNEL_STRING,
  DATA_CHANNEL_BINARY,

} DataChannelType;

typedef struct IceServer {

  const char *urls;
  const char *username;
  const char *credential;

} IceServer;

typedef struct PeerConfiguration {

  IceServer ice_servers[5];

  MediaCodec audio_codec;
  MediaCodec video_codec;
  DataChannelType datachannel;

  void (*onaudiotrack)(uint8_t *data, size_t size, void *userdata);
  void (*onvideotrack)(uint8_t *data, size_t size, void *userdata);
  void (*on_request_keyframe)();
  void *user_data;

} PeerConfiguration;

typedef struct PeerConnection {

  PeerConfiguration config;
  PeerConnectionState state;
  Agent agent;
  DtlsSrtp dtls_srtp;
  Sctp sctp;

  Sdp local_sdp;
  Sdp remote_sdp;

  void (*onicecandidate)(char *sdp, void *user_data);
  void (*oniceconnectionstatechange)(PeerConnectionState state, void *user_data);
  void (*on_connected)(void *userdata);
  void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void *user_data);

  uint8_t temp_buf[CONFIG_MTU];
  uint8_t agent_buf[CONFIG_MTU];
  int agent_ret;
  int b_offer_created;

  Buffer *audio_rb;
  Buffer *video_rb;
  Buffer *data_rb;

  RtpEncoder artp_encoder;
  RtpEncoder vrtp_encoder;
  RtpDecoder vrtp_decoder;
  RtpDecoder artp_decoder;

  uint32_t remote_assrc;
  uint32_t remote_vssrc;

} PeerConnection;

const char* peer_connection_state_to_string(PeerConnectionState state);

PeerConnectionState peer_connection_get_state(PeerConnection *pc);

PeerConnection* peer_connection_create(PeerConfiguration *config);

void peer_connection_destroy(PeerConnection *pc);

void peer_connection_close(PeerConnection *pc);

int peer_connection_loop(PeerConnection *pc);
/**
 * @brief send message to data channel
 * @param[in] peer connection
 * @param[in] message buffer
 * @param[in] length of message
 */
int peer_connection_datachannel_send(PeerConnection *pc, char *message, size_t len);

int peer_connection_send_audio(PeerConnection *pc, const uint8_t *packet, size_t bytes);

int peer_connection_send_video(PeerConnection *pc, const uint8_t *packet, size_t bytes);

void peer_connection_set_remote_description(PeerConnection *pc, const char *sdp);

void peer_connection_create_offer(PeerConnection *pc);

/**
 * @brief register callback function to handle packet loss from RTCP receiver report
 * @param[in] peer connection
 * @param[in] callback function void (*cb)(float fraction_loss, uint32_t total_loss, void *userdata)
 * @param[in] userdata for callback function
 */
void peer_connection_on_receiver_packet_loss(PeerConnection *pc,
 void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void *userdata));

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
 void (*oniceconnectionstatechange)(PeerConnectionState state, void *userdata));

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

#ifdef __cplusplus
}
#endif

#endif // PEER_CONNECTION_H_


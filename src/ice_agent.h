#ifndef ICE_AGENT_H_
#define ICE_AGENT_H_

#include <agent.h>
#include "sdp_attribute.h"
#include "dtls_transport.h"

typedef enum iceconnectionstate_t {
  DISCONNECTED,
  GATHERING,
  CONNECTING,
  CONNECTED,
  READY,
  FAILED,
} iceconnectionstate_t;

typedef enum codec_t {
  CODEC_H264,
  CODEC_OPUS,
  CODEC_NONE,
} codec_t;

typedef struct ice_agent_t {

  NiceAgent *nice_agent;
  gboolean controlling;
  guint stream_id;
  guint component_id;
  GMainLoop *gloop;
  GThread *gthread;

  dtls_transport_t *dtls_transport;
  sdp_attribute_t *sdp_attribute;
  codec_t codec;
  int h264_gop;

  void (*on_icecandidate)(char *sdp, void *data);
  void (*on_iceconnectionstatechange)(iceconnectionstate_t state, void *data);
  void (*on_transport_ready)(void *data);
  void (*on_track)(uint8_t *packet, size_t bytes);
  void *on_icecandidate_data;
  void *on_transport_ready_data;
  void *on_iceconnectionstatechange_data;
  void *on_track_data;
} ice_agent_t;


int ice_agent_init(ice_agent_t *ice_agent, dtls_transport_t *dtls_transport);

int ice_agent_send_rtp_packet(ice_agent_t *ice_agent, uint8_t *packet, int *bytes);

void ice_agent_add_stream(ice_agent_t *ice_agent, codec_t codec);

void ice_agent_set_remote_sdp(ice_agent_t *ice_agent, char *sdp);

#endif // ICE_AGENT_H_


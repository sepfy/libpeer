#ifndef SCTP_H_
#define SCTP_H_

#include "config.h"
#include "dtls_srtp.h"
#include "utils.h"
#include "buffer.h"

#ifndef HAVE_USRSCTP

typedef enum DecpMsgType {

  DATA_CHANNEL_OPEN = 0x03,
  DATA_CHANNEL_ACK = 0x02,

} DecpMsgType;

typedef struct SctpChunkParam {

  uint16_t type;
  uint16_t length;
  uint8_t value[0];

} SctpChunkParam;

typedef enum SctpParamType {

  SCTP_PARAM_STATE_COOKIE = 7,

} SctpParamType;

typedef enum SctpHeaderType {

  SCTP_DATA = 0,
  SCTP_INIT = 1,
  SCTP_INIT_ACK = 2,
  SCTP_SACK = 3,
  SCTP_HEARTBEAT = 4,
  SCTP_HEARTBEAT_ACK = 5,
  SCTP_ABORT = 6,
  SCTP_SHUTDOWN = 7,
  SCTP_SHUTDOWN_ACK = 8,
  SCTP_ERROR = 9,
  SCTP_COOKIE_ECHO = 10,
  SCTP_COOKIE_ACK = 11,
  SCTP_ECNE = 12,
  SCTP_CWR = 13,
  SCTP_SHUTDOWN_COMPLETE = 14,
  SCTP_AUTH = 15,
  SCTP_ASCONF_ACK = 128,
  SCTP_ASCONF = 130,
  SCTP_FORWARD_TSN = 192

} SctpHeaderType;

typedef struct SctpChunkCommon {

  uint8_t type;
  uint8_t flags;
  uint16_t length;

} SctpChunkCommon;

typedef struct SctpForwardTsnChunk {

  SctpChunkCommon common;
  uint32_t new_cumulative_tsn;
  uint16_t stream_number;
  uint16_t stream_sequence_number;

} SctpForwardTsnChunk;



typedef struct SctpHeader {

  uint16_t source_port;
  uint16_t destination_port;
  uint32_t verification_tag;
  uint32_t checksum;

} SctpHeader;

typedef struct SctpPacket {

  SctpHeader header;
  uint8_t chunks[0];

} SctpPacket;

typedef struct SctpSackChunk {

  SctpChunkCommon common;
  uint32_t cumulative_tsn_ack;
  uint32_t a_rwnd;
  uint16_t number_of_gap_ack_blocks;
  uint16_t number_of_dup_tsns;
  uint8_t blocks[0];

} SctpSackChunk;

typedef struct SctpDataChunk {

  uint8_t type;
  uint8_t iube;
  uint16_t length;
  uint32_t tsn;
  uint16_t si;
  uint16_t sqn;
  uint32_t ppid;
  uint8_t data[0];

} SctpDataChunk;

typedef struct SctpInitChunk {

  SctpChunkCommon common;
  uint32_t initiate_tag;
  uint32_t a_rwnd;
  uint16_t number_of_outbound_streams;
  uint16_t number_of_inbound_streams;
  uint32_t initial_tsn;
  SctpChunkParam param[0];

} SctpInitChunk;

#endif

typedef enum SctpDataPpid {

  PPID_CONTROL = 50,
  PPID_STRING = 51,
  PPID_BINARY = 53,
  PPID_STRING_EMPTY = 56,
  PPID_BINARY_EMPTY = 57

} SctpDataPpid;

typedef struct Sctp Sctp;

struct Sctp {

  struct socket *sock;

  int local_port;
  int remote_port;
  int connected;
  uint32_t verification_tag;
  uint32_t tsn;
  DtlsSrtp *dtls_srtp;
  Buffer **data_rb;
  /* datachannel */
  void (*onmessage)(char *msg, size_t len, void *userdata);
  void (*onopen)(void *userdata);
  void (*onclose)(void *userdata);

  void *userdata;
  uint8_t buf[CONFIG_MTU];
};


Sctp* sctp_create(DtlsSrtp *dtls_srtp);

void sctp_destroy(Sctp *sctp);

int sctp_create_socket(Sctp *sctp, DtlsSrtp *dtls_srtp);

int sctp_is_connected(Sctp *sctp);

void sctp_incoming_data(Sctp *sctp, char *buf, size_t len);

int sctp_outgoing_data(Sctp *sctp, char *buf, size_t len, SctpDataPpid ppid);

void sctp_onmessage(Sctp *sctp, void (*onmessage)(char *msg, size_t len, void *userdata));

void sctp_onopen(Sctp *sctp, void (*onopen)(void *userdata));

void sctp_onclose(Sctp *sctp, void (*onclose)(void *userdata));

#endif // SCTP_H_

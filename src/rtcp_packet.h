#ifndef RTCP_PACKET_H_
#define RTCP_PACKET_H_

#include <stdint.h>
#include <endian.h>

typedef enum RtcpType {

  RTCP_FIR = 192,
  RTCP_SR = 200,
  RTCP_RR = 201,
  RTCP_SDES = 202,
  RTCP_BYE = 203,
  RTCP_APP = 204,
  RTCP_RTPFB = 205,
  RTCP_PSFB = 206,
  RTCP_XR = 207,

} RtcpType;

typedef struct RtcpHeader {

#if __BYTE_ORDER == __BIG_ENDIAN
  uint16_t version:2;
  uint16_t padding:1;
  uint16_t rc:5;
  uint16_t type:8;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
  uint16_t rc:5;
  uint16_t padding:1;
  uint16_t version:2;
  uint16_t type:8;
#endif
  uint16_t length:16;

} RtcpHeader;

typedef struct RtcpReportBlock {

  uint32_t ssrc;
  uint32_t flcnpl;
  uint32_t ehsnr;
  uint32_t jitter;
  uint32_t lsr;
  uint32_t dlsr;

} RtcpReportBlock;


typedef struct RtcpRr {

  RtcpHeader header;
  uint32_t ssrc;
  RtcpReportBlock report_block[1];

} RtcpRr;

typedef struct RtcpFir {

  uint32_t ssrc;
  uint32_t seqnr;

} RtcpFir;

typedef struct RtcpFb {

  RtcpHeader header;
  uint32_t ssrc;
  uint32_t media;
  char fci[1];

} RtcpFb;

int rtcp_packet_validate(char *packet, size_t size);

int rtcp_packet_get_pli(char *packet, int len, uint32_t ssrc);

int rtcp_packet_get_fir(char *packet, int len, int *seqnr);

RtcpRr rtcp_packet_parse_rr(uint8_t *packet);

#endif // RTCP_PACKET_H_

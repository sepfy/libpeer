#ifndef RTCP_PACKET_H_
#define RTCP_PACKET_H_

#include <stdint.h>
#include <endian.h>

typedef enum {
  RTCP_FIR = 192,
  RTCP_SR = 200,
  RTCP_RR = 201,
  RTCP_SDES = 202,
  RTCP_BYE = 203,
  RTCP_APP = 204,
  RTCP_RTPFB = 205,
  RTCP_PSFB = 206,
  RTCP_XR = 207,
} rtcp_type_t;

typedef struct rtcp_header_t {
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
} rtcp_header_t;

typedef struct rtcp_fir_t {
  uint32_t ssrc;
  uint32_t seqnr;
} rtcp_fir_t;

typedef struct rtcp_fb_t {
  rtcp_header_t header;
  uint32_t ssrc;
  uint32_t media;
  char fci[1];
} rtcp_fb_t;

int rtcp_packet_validate(char *packet, size_t size);

int rtcp_packet_get_pli(char *packet, int len, uint32_t ssrc);

int rtcp_packet_get_fir(char *packet, int len, int *seqnr);

#endif // RTCP_PACKET_H_

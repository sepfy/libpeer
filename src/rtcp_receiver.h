#ifndef RTCP_RECEIVER_H_
#define RTCP_RECEIVER_H_

#include <stdint.h>
#include <endian.h>
#include "rtp_receiver.h"

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
} rtcp_type;

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

/*! \brief RTCP-FB (http://tools.ietf.org/html/rfc4585) */
typedef struct rtcp_fb_t {
        rtcp_header_t header;
        uint32_t ssrc;
        uint32_t media;
        char fci[1];
} rtcp_fb_t;



typedef struct rtcp_receiver_t {

} rtcp_receiver_t;

int rtcp_receiver_is_rtcp(char *buf);

int rtcp_receiver_get_pli(char *packet, int len);

int rtcp_get_fir(char *packet, int len, int *seqnr);

#endif // RTCP_RECEIVER_H_

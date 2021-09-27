#ifndef RTCP_RECEIVER_H_
#define RTCP_RECEIVER_H_

#include <stdint.h>
#include <endian.h>
#include "rtp_receiver.h"

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

typedef struct rtcp_receiver_t {

} rtcp_receiver_t;

int rtcp_receiver_is_rtcp(char *buf);

#endif // RTCP_RECEIVER_H_

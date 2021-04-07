#include <stdio.h>
#include "rtcp_receiver.h"

int rtcp_receiver_is_rtcp(char *buf) {
  rtp_header_t *header = (rtp_header_t *)buf;
  return ((header->type >= 64) && (header->type < 96));
}

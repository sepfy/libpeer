#include <stdio.h>
#include "rtp_packet.h"

int rtp_packet_validate(char *packet, size_t size) {

  if(size < 12)
    return 0;

  RtpHeader *rtp_header = (RtpHeader*)packet;
  return ((rtp_header->type < 64) || (rtp_header->type >= 96));
}

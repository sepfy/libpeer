#include <stdio.h>
#include "rtp_receiver.h"

int rtp_receiver_is_rtp(char *buf, size_t size) {

  if(size < 12)
    return 0;
  rtp_header_t *rtp_header = (rtp_header_t*)buf;
  return ((rtp_header->type < 64) || (rtp_header->type >= 96));

}

#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>

#include "rtcp_packet.h"
#include "rtp_packet.h"

int rtcp_packet_validate(char *packet, size_t size) {

  if(size < 8)
    return -1;

  rtp_header_t *header = (rtp_header_t *)packet;
  return ((header->type >= 64) && (header->type < 96));
}

int rtcp_packet_get_pli(char *packet, int len, uint32_t ssrc) {

  if(packet == NULL || len != 12)
    return -1;

  memset(packet, 0, len);
  rtcp_header_t *rtcp_header = (rtcp_header_t *)packet;
  rtcp_header->version = 2;
  rtcp_header->type = RTCP_PSFB;
  rtcp_header->rc = 1;
  rtcp_header->length = htons((len/4)-1);
  memcpy(packet + 8, &ssrc, 4);

  return 12;
}

int rtcp_packet_get_fir(char *packet, int len, int *seqnr) {

  if(packet == NULL || len != 20 || seqnr == NULL)
    return -1;

  memset(packet, 0, len);
  rtcp_header_t *rtcp = (rtcp_header_t*)packet;
  *seqnr = *seqnr + 1;
  if(*seqnr < 0 || *seqnr >= 256)
   *seqnr = 0;

  rtcp->version = 2;
  rtcp->type = RTCP_PSFB;
  rtcp->rc = 4;
  rtcp->length = htons((len/4)-1);
  rtcp_fb_t *rtcp_fb = (rtcp_fb_t*)rtcp;
  rtcp_fir_t *fir = (rtcp_fir_t*)rtcp_fb->fci;
  fir->seqnr = htonl(*seqnr << 24);

  return 20;
}


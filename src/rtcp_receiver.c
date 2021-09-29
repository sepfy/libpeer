#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>

#include "rtcp_receiver.h"

int rtcp_receiver_is_rtcp(char *buf) {
  rtp_header_t *header = (rtp_header_t *)buf;
  return ((header->type >= 64) && (header->type < 96));
}

int rtcp_receiver_get_pli(char *packet, int len) {

  if(packet == NULL || len != 12)
    return -1;
  memset(packet, 0, len);
  rtcp_header_t *rtcp_header = (rtcp_header_t *)packet;
        /* Set header */
  rtcp_header->version = 2;
  rtcp_header->type = RTCP_PSFB;
  rtcp_header->rc = 1;   /* FMT=1 */
  rtcp_header->length = htons((len/4)-1);
  return 12;
}

int rtcp_get_fir(char *packet, int len, int *seqnr) {

  if(packet == NULL || len != 20 || seqnr == NULL)
    return -1;

  memset(packet, 0, len);
  rtcp_header_t *rtcp = (rtcp_header_t*)packet;
  *seqnr = *seqnr + 1;
  if(*seqnr < 0 || *seqnr >= 256)
   *seqnr = 0;     /* Reset sequence number */

  rtcp->version = 2;
  rtcp->type = RTCP_PSFB;
  rtcp->rc = 4;   /* FMT=4 */
  rtcp->length = htons((len/4)-1);
  rtcp_fb_t *rtcp_fb = (rtcp_fb_t*)rtcp;
  rtcp_fir_t *fir = (rtcp_fir_t*)rtcp_fb->fci;
  fir->seqnr = htonl(*seqnr << 24);       /* FCI: Sequence number */
  return 20;
}


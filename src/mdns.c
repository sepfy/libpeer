#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "utils.h"
#include "address.h"
#include "socket.h"

#define MDNS_GROUP "224.0.0.251"
#define MDNS_PORT 5353

typedef struct DnsHeader {
  uint16_t id;
  uint16_t flags;
  uint16_t qestions;
  uint16_t answer_rrs;
  uint16_t authority_rrs;
  uint16_t additional_rrs;
} DnsHeader;

typedef struct DnsAnswer {
  uint16_t type;
  uint16_t class;
  uint32_t ttl;
  uint16_t length;
  uint8_t data[0];
} DnsAnswer;

typedef struct DnsQuery {
  uint16_t type;
  uint16_t class;
} DnsQuery;

static int mdns_parse_answer(uint8_t *buf, int size, Address *addr) {

  char *pos;
  int flags_qr;
  DnsHeader *header;
  DnsAnswer *answer;

  if (size < sizeof(DnsHeader)) {
    LOGE("response too short");
    return -1;
  }

  header = (DnsHeader *)buf;
  flags_qr = ntohs(header->flags) >> 15;
  if (flags_qr != 1) {
    LOGD("flag is not a DNS response");
    return -1;
  }

  buf += sizeof(DnsHeader);
  pos = strstr((char*)buf, "local");
  LOGI("\nbuf=>%s\n", buf);
  if (pos == NULL) {
    LOGI("not a mDNS response");
    return -1;
  }


  pos += 6;
  answer = (DnsAnswer *)pos;
  LOGI("lenght: %d\n", answer->length);
  uint8_t *debug = (uint8_t*)&answer->ttl;
  printf("%.2x %.2x %.2x %.2x\n", debug[0], debug[1], debug[2], debug[3]);
  printf("type: %d, class: %d, ttl: %lu, length: %d\n", ntohs(answer->type), ntohs(answer->class), ntohl(answer->ttl), ntohs(answer->length));
  if (ntohs(answer->length) != 4) {
    LOGI("invalid length");
    return -1;
  }

  memcpy(&addr->sin.sin_addr, answer->data, 4); 
  return 0;
}

static int mdns_build_query(const char *hostname, uint8_t *buf, int size) {

  int total_size, len, offset;
  const char *label, *dot;
  DnsHeader *dns_header;
  DnsQuery *dns_query;

  total_size = sizeof(dns_header) + strlen(hostname) + sizeof(dns_query) + 2;
  if (size < total_size) {
    printf("buf size is not enough");
    return -1;
  }

  memset(buf, 0, size);
  dns_header = (DnsHeader*)buf;
  dns_header->qestions = 0x0100;
  offset = sizeof(DnsHeader);

  // Append hostname to query
  label = hostname;
  while (*label) {
    dot = strchr(label, '.');
    if (!dot) {
      dot = label + strlen(label);
    }
    len = dot - label;
    buf[offset++] = len;
    memcpy(buf + offset, label, len);
    offset += len;
    label = *dot ? dot + 1 : dot;
  }

  buf[offset++] = 0x00;

  dns_query = (DnsQuery*) (buf + offset);
  dns_query->type = 0x0100;
  dns_query->class = 0x0100;
  return total_size;
}

int mdns_resolve_addr(const char *hostname, Address *addr) {

  Address mcast_addr;
  UdpSocket udp_socket;
  uint8_t buf[256];
  char addr_string[ADDRSTRLEN];
  int send_retry = 3;
  int recv_retry = 5;
  int ret = -1;
  int resolved = 0;
  int size;
  struct timeval tv;
  int maxfd = 0;
  fd_set rfds;
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  memset(&mcast_addr, 0, sizeof(mcast_addr));
  addr_from_string(MDNS_GROUP, &mcast_addr);
  addr_set_port(&mcast_addr, MDNS_PORT);

  if (udp_socket_open(&udp_socket, AF_INET, MDNS_PORT) < 0) {
    LOGE("Failed to create socket");
    return -1;
  }

  FD_ZERO(&rfds);
  maxfd = udp_socket.fd;

  while (!resolved && send_retry--) {
    LOGI("sendto");
    size = mdns_build_query(hostname, buf, sizeof(buf));
    udp_socket_sendto(&udp_socket, &mcast_addr, buf, size);
    recv_retry = 5;
    while (!resolved && recv_retry--) {
      LOGI("select %d", recv_retry);
      FD_SET(udp_socket.fd, &rfds);
      ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);
      if (ret < 0) {
        LOGE("select error");
      } else if (ret == 0) {
        // timeout
      } else {
        if (FD_ISSET(udp_socket.fd, &rfds)) {
          ret = udp_socket_recvfrom(&udp_socket, &mcast_addr, buf, sizeof(buf));
          if (!mdns_parse_answer(buf, ret, addr)) {
            addr_to_string(addr, addr_string, sizeof(addr_string));
	    addr_set_family(addr, AF_INET);
	    resolved = 1;
            break;
          }
        }
      }
    }
  }
  LOGI("Resolve:%d", resolved);
  udp_socket_close(&udp_socket);
  return resolved;
}


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <unistd.h>

#include "address.h"
#include "mdns.h"
#include "socket.h"
#include "utils.h"

#define MDNS_GROUP "224.0.0.251"
#define MDNS_PORT 5353
#define MDNS_HEADER_SIZE 12
#define MDNS_QUERY_ATTEMPTS 3
#define MDNS_RESPONSE_TIMEOUT_SECONDS 1

static int mdns_add_hostname(const char* hostname, uint8_t* buf, int size) {
  const char* label = hostname;
  int offset = 0;
  int result = -1;
  int valid = 1;

  if (hostname == NULL || buf == NULL || size <= 0) {
    valid = 0;
  }

  while (valid && *label != '\0') {
    const char* dot = strchr(label, '.');
    int label_length;

    if (dot == NULL) {
      dot = label + strlen(label);
    }
    label_length = (int)(dot - label);
    if (label_length <= 0 || label_length > 63 ||
        offset > size - label_length - 1) {
      valid = 0;
    } else {
      buf[offset++] = (uint8_t)label_length;
      memcpy(buf + offset, label, label_length);
      offset += label_length;
      label = *dot != '\0' ? dot + 1 : dot;
    }
  }

  if (valid && offset < size) {
    buf[offset++] = 0;
    result = offset;
  }

  return result;
}

/* Decode a DNS name while preserving the caller's position after compression. */
static int mdns_decode_name(const uint8_t* packet, int size, int start,
                            char* name, int name_size, int* next_offset) {
  int result = -1;
  int offset = start;
  int end_offset = -1;
  int name_length = 0;
  int steps = 0;

  if (packet != NULL && name != NULL && name_size > 0 &&
      next_offset != NULL && start >= 0 && start < size) {
    name[0] = '\0';
    while (result < 0 && steps <= size && offset >= 0 && offset < size) {
      int label_length = packet[offset];
      int required_length;

      steps++;
      if ((label_length & 0xc0) == 0xc0) {
        if (offset + 1 < size) {
          int pointer = ((label_length & 0x3f) << 8) | packet[offset + 1];
          if (end_offset < 0) {
            end_offset = offset + 2;
          }
          if (pointer < size) {
            offset = pointer;
          } else {
            steps = size + 1;
          }
        } else {
          steps = size + 1;
        }
      } else if ((label_length & 0xc0) != 0) {
        steps = size + 1;
      } else if (label_length == 0) {
        if (end_offset < 0) {
          end_offset = offset + 1;
        }
        name[name_length] = '\0';
        *next_offset = end_offset;
        result = 0;
      } else {
        offset++;
        required_length = name_length + label_length +
                          (name_length > 0 ? 1 : 0);
        if (offset + label_length <= size && required_length < name_size) {
          if (name_length > 0) {
            name[name_length++] = '.';
          }
          memcpy(name + name_length, packet + offset, label_length);
          name_length += label_length;
          offset += label_length;
        } else {
          steps = size + 1;
        }
      }
    }
  }

  return result;
}

static int mdns_read_u16(const uint8_t* packet, int size, int offset,
                         uint16_t* value) {
  int result = -1;

  if (packet != NULL && value != NULL && offset >= 0 && offset + 1 < size) {
    *value = (uint16_t)(((uint16_t)packet[offset] << 8) |
                        (uint16_t)packet[offset + 1]);
    result = 0;
  }

  return result;
}

int mdns_parse_response(const uint8_t* packet, int size, Address* addr,
                        const char* hostname) {
  int result = -1;
  int offset = MDNS_HEADER_SIZE;
  int valid = 0;
  int index;
  int total_records = 0;
  uint16_t flags = 0;
  uint16_t question_count = 0;
  uint16_t answer_count = 0;
  uint16_t authority_count = 0;
  uint16_t additional_count = 0;
  char record_name[256];

  if (packet != NULL && addr != NULL && hostname != NULL &&
      size >= MDNS_HEADER_SIZE &&
      mdns_read_u16(packet, size, 2, &flags) == 0 &&
      mdns_read_u16(packet, size, 4, &question_count) == 0 &&
      mdns_read_u16(packet, size, 6, &answer_count) == 0 &&
      mdns_read_u16(packet, size, 8, &authority_count) == 0 &&
      mdns_read_u16(packet, size, 10, &additional_count) == 0) {
    valid = 1;
    if ((flags >> 15) != 1) {
      valid = 0;
    }

    for (index = 0; valid && index < question_count; index++) {
      int next_offset = 0;
      if (mdns_decode_name(packet, size, offset, record_name,
                           sizeof(record_name), &next_offset) != 0 ||
          next_offset > size - 4) {
        valid = 0;
      } else {
        offset = next_offset + 4;
      }
    }

    total_records = answer_count + authority_count + additional_count;
    for (index = 0; valid && result < 0 && index < total_records; index++) {
      int next_offset = 0;
      int data_offset = 0;
      int searchable = index < answer_count ||
                       index >= answer_count + authority_count;
      uint16_t record_type = 0;
      uint16_t record_class = 0;
      uint16_t data_length = 0;

      if (mdns_decode_name(packet, size, offset, record_name,
                           sizeof(record_name), &next_offset) != 0 ||
          next_offset > size - 10 ||
          mdns_read_u16(packet, size, next_offset, &record_type) != 0 ||
          mdns_read_u16(packet, size, next_offset + 2, &record_class) != 0 ||
          mdns_read_u16(packet, size, next_offset + 8, &data_length) != 0) {
        valid = 0;
      } else {
        data_offset = next_offset + 10;
        if (data_offset > size - data_length) {
          valid = 0;
        } else {
          if (searchable && record_type == 1 &&
              (record_class & 0x7fff) == 1 && data_length == 4 &&
              strcasecmp(record_name, hostname) == 0) {
            memcpy(&addr->sin.sin_addr, packet + data_offset, 4);
            addr_set_family(addr, AF_INET);
            result = 0;
          }
          offset = data_offset + data_length;
        }
      }
    }
  }

  return result;
}

static int mdns_build_query(const char* hostname, uint8_t* buf, int size) {
  int offset = MDNS_HEADER_SIZE;
  int encoded_length;
  int result = -1;

  if (hostname != NULL && buf != NULL && size >= MDNS_HEADER_SIZE) {
    memset(buf, 0, size);
    buf[5] = 1;
    encoded_length = mdns_add_hostname(hostname, buf + offset, size - offset);
    if (encoded_length > 0 && offset + encoded_length <= size - 4) {
      offset += encoded_length;
      buf[offset++] = 0;
      buf[offset++] = 1;
      buf[offset++] = 0;
      buf[offset++] = 1;
      result = offset;
    }
  }

  return result;
}

int mdns_resolve_addr(const char* hostname, Address* addr) {
  Address multicast_addr = {0};
  UdpSocket udp_socket;
  uint8_t buf[256];
  char addr_string[ADDRSTRLEN];
  int attempt;
  int query_size;
  int result = 0;

  udp_socket.fd = -1;
  query_size = mdns_build_query(hostname, buf, sizeof(buf));
  if (query_size < 0) {
    LOGW("Invalid mDNS hostname");
  } else if (udp_socket_open(&udp_socket, AF_INET, MDNS_PORT) < 0) {
    udp_socket.fd = -1;
    LOGE("Failed to create mDNS socket");
  } else if (addr_from_string(MDNS_GROUP, &multicast_addr) == 0) {
    LOGE("Failed to parse mDNS multicast address");
  } else if (udp_socket_add_multicast_group(&udp_socket, &multicast_addr) < 0) {
    LOGE("Failed to join mDNS multicast group");
  } else {
    addr_set_port(&multicast_addr, MDNS_PORT);
    for (attempt = 0; result == 0 && attempt < MDNS_QUERY_ATTEMPTS; attempt++) {
      struct timeval timeout = {MDNS_RESPONSE_TIMEOUT_SECONDS, 0};
      int waiting = 1;

      if (udp_socket_sendto(&udp_socket, &multicast_addr, buf, query_size) < 0) {
        waiting = 0;
      }
      while (result == 0 && waiting) {
        fd_set read_fds;
        int selected;

        FD_ZERO(&read_fds);
        FD_SET(udp_socket.fd, &read_fds);
        selected = select(udp_socket.fd + 1, &read_fds, NULL, NULL, &timeout);
        if (selected > 0 && FD_ISSET(udp_socket.fd, &read_fds)) {
          int received = udp_socket_recvfrom(&udp_socket, NULL, buf,
                                             sizeof(buf));
          if (received > 0 &&
              mdns_parse_response(buf, received, addr, hostname) == 0) {
            addr_set_port(addr, addr->port);
            addr_to_string(addr, addr_string, sizeof(addr_string));
            LOGI("Resolved %s -> %s", hostname, addr_string);
            result = 1;
          }
        } else {
          waiting = 0;
        }
      }
    }
  }

  if (udp_socket.fd >= 0) {
    udp_socket_close(&udp_socket);
  }
  if (result == 0) {
    LOGI("Failed to resolve mDNS hostname");
  }

  return result;
}

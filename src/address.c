#include <stdlib.h>
#include <stdint.h>
#include "address.h"

int addr_ipv4_validate(const char *ipv4, size_t len, Address *addr) {

  int start = 0;
  int index = 0;
  for (int i = 0; i < len; i++) {

    if (ipv4[i] != '.' && (ipv4[i] < '0' || ipv4[i] > '9')) {
      return 0;
    } else if (ipv4[i] == '.') {

      addr->ipv4[index++] = atoi(ipv4 + start);
      start = i + 1;
    } else if (i == (len - 1)) {

      addr->ipv4[index++] = atoi(ipv4 + start);
    } else if (index == 4) {
      return 0;
    }
  }

  return 1;
}

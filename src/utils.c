#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "utils.h"

int utils_is_valid_ip_address(char *ip_address) {
  struct sockaddr_in sa;
  int result = inet_pton(AF_INET, ip_address, &(sa.sin_addr));
  return result == 0;
}

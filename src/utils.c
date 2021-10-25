#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "utils.h"

// http://haoyuanliu.github.io/2017/01/16/%E5%9C%B0%E5%9D%80%E6%9F%A5%E8%AF%A2%E5%87%BD%E6%95%B0gethostbyname-%E5%92%8Cgetaddrinfo/
int utils_get_ipv4addr(char *hostname, char *ipv4addr, size_t size) {

  struct addrinfo *ai, *aip;
  struct addrinfo hint;
  struct sockaddr_in *sinp;
  int err;

  hint.ai_flags = AI_CANONNAME;
  hint.ai_family = AF_INET;
  hint.ai_socktype = 0;
  hint.ai_protocol = SOCK_DGRAM;
  hint.ai_addrlen = 0;
  hint.ai_canonname = NULL;
  hint.ai_addr = NULL;
  hint.ai_next = NULL;

  if((err = getaddrinfo(hostname, NULL, &hint, &ai)) != 0) {
    printf("ERROR: getaddrinfo error: %s\n", gai_strerror(err));
    return -1;
  }

  for(aip = ai; aip != NULL; aip = aip->ai_next) {
    if(aip->ai_family == AF_INET) {
      sinp = (struct sockaddr_in *)aip->ai_addr;
      inet_ntop(AF_INET, &sinp->sin_addr, ipv4addr, size);
      return 0;
    }
  }

  return -1;
}

int utils_is_valid_ip_address(char *ip_address) {
  struct sockaddr_in sa;
  int result = inet_pton(AF_INET, ip_address, &(sa.sin_addr));
  return result == 0;
}

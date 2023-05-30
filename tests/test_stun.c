#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stun.h"

#define STUN_ADDR "142.250.21.127"
#define STUN_PORT 19302

int main(int argc, char *argv[]) {

  Address local_addr;
  stun_get_local_address(STUN_ADDR, STUN_PORT, &local_addr);
}


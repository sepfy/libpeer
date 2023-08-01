#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <srtp2/srtp.h>

#include "utils.h"
#include "peer.h"

int peer_init() {

  if(srtp_init() != srtp_err_status_ok) {
  
    LOGE("libsrtp init failed");
  }

  return 0;
}

void peer_deinit() {

  srtp_shutdown();
}


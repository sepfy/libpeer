#include <srtp2/srtp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "peer.h"
#include "utils.h"

int peer_init() {
  if (srtp_init() != srtp_err_status_ok) {
    LOGE("libsrtp init failed");
  }

  return 0;
}

void peer_deinit() {
  srtp_shutdown();
}

#include <stdio.h>
#include <stdlib.h>
#include <srtp2/srtp.h>
#include "platform/misc.h"

#include "utils.h"
#include "peer.h"

int peer_init() {
  if(!platform_init()) {
    LOGE("libpeer platform init failed");
    return -1;
  }

  if(srtp_init() != srtp_err_status_ok) {

    LOGE("libsrtp init failed");
  }

  return 0;
}

void peer_deinit() {

  srtp_shutdown();
  platform_deinit();
}

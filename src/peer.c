#include <srtp2/srtp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "peer.h"
#include "sctp.h"
#include "utils.h"

static int s_peer_initialized = 0;

int peer_init() {
  if (s_peer_initialized) {
    LOGD("peer_init: already initialized");
    return 0;
  }

  srtp_err_status_t srtp_ret = srtp_init();
  if (srtp_ret != srtp_err_status_ok) {
    LOGE("libsrtp init failed: %d", srtp_ret);
  }
  sctp_usrsctp_init();
  s_peer_initialized = 1;
  LOGI("peer_init: initialized SRTP and SCTP");
  return 0;
}

void peer_deinit() {
  srtp_shutdown();
  sctp_usrsctp_deinit();
}

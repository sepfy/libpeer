#ifndef PEER_H_
#define PEER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "peer_connection.h"
#include "peer_signaling.h"

int peer_init();

void peer_deinit();

#ifdef __cplusplus
}
#endif

#endif  // PEER_H_

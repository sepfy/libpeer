#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "agent.h"

static int failures = 0;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
              #condition);                                                 \
      failures++;                                                          \
    }                                                                      \
  } while (0)

int main(void) {
  Agent blocker;
  Agent ranged;
  Agent duplicate;
  uint16_t blocked_port;
  uint16_t range_begin;
  uint16_t range_end;
  uint16_t selected_port = 0;
  int blocker_created;
  int ranged_created;

  memset(&blocker, 0, sizeof(blocker));
  memset(&ranged, 0, sizeof(ranged));
  memset(&duplicate, 0, sizeof(duplicate));

  blocker_created = agent_create(&blocker, 0, 0) == 0;
  CHECK(blocker_created);
  if (blocker_created) {
    blocked_port = blocker.udp_sockets[0].bind_addr.port;
    if (blocked_port <= UINT16_MAX - 16) {
      range_begin = blocked_port;
      range_end = blocked_port + 16;
    } else {
      range_begin = blocked_port - 16;
      range_end = blocked_port;
    }

    ranged_created = agent_create(&ranged, range_begin, range_end) == 0;
    CHECK(ranged_created);
    if (ranged_created) {
      selected_port = ranged.udp_sockets[0].bind_addr.port;
      CHECK(selected_port >= range_begin);
      CHECK(selected_port <= range_end);
      CHECK(selected_port != blocked_port);
      CHECK(agent_create(&duplicate, selected_port, selected_port) < 0);
      agent_destroy(&ranged);
    }
    agent_destroy(&blocker);
  }

  CHECK(agent_create(&duplicate, 60000, 59999) < 0);

  if (failures != 0) {
    fprintf(stderr, "%d test checks failed\n", failures);
  }
  return failures == 0 ? 0 : 1;
}

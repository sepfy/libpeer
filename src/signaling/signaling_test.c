#include "signaling.h"

void test_event_cb(SignalingEvent event, const char *buf, size_t len, void *user_data) {

  printf("test_event_cb: event=%d, buf=%s, len=%ld, user_data=%p\n", event, buf, len, user_data);
}

int main(int argc, char *argv[]) {

  char user_data[32] = "user_data";
  signaling_dispatch("mytestdevice", test_event_cb, user_data);

  return 0;
}


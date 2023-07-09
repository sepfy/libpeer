#ifndef SIGNALING_H_
#define SIGNALING_H_

#include <stdlib.h>
#include <stdio.h>
#include <mosquitto.h>

typedef enum SignalingEvent {

  SIGNALING_EVENT_REQUEST_OFFER,
  SIGNALING_EVENT_RESPONSE_ANSWER,

} SignalingEvent;

typedef struct Signaling {

  char client_id[24];
  int run;
  int id;
  struct mosquitto *mosq;
  void (*on_event)(SignalingEvent event, const char *buf, size_t len, void *user_data);
  void *user_data;

} Signaling;

void signaling_dispatch(const char *client_id, void (*on_event)(SignalingEvent event, const char *buf, size_t len, void *user_data), void *user_data);

void signaling_set_local_description(const char *description);

#endif // SIGNALING_H_

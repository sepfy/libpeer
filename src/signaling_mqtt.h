#ifndef SIGNALING_MQTT_H_
#define SIGNALING_MQTT_H_

#include <glib.h>

#include "signaling_observer.h"

typedef struct SignalingMqtt SignalingMqtt;

SignalingMqtt* signaling_mqtt_create(const char *host, int port, const char *topic, const char *tls, const char *username, const char *password, SignalingObserver *signaling_observer);

void signaling_mqtt_destroy(SignalingMqtt *signaling_mqtt);

void signaling_mqtt_dispatch(SignalingMqtt *signaling_mqtt);

void signaling_mqtt_shutdown(SignalingMqtt *signaling_mqtt);

size_t signaling_mqtt_get_answer(SignalingMqtt *signaling_mqtt, char *answer, size_t size);

void signaling_mqtt_set_answer(SignalingMqtt *signaling_mqtt, const char *answer);

#endif // SIGNALING_MQTT_H_

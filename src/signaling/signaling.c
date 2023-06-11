#include <string.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include "signaling.h"
#include "base64.h"
#include "utils.h"

#define MQTT_HOST "mqtt.eclipseprojects.io"
#define MQTT_PORT 1883
#define KEEPALIVE 60

#define JSONRPC_METHOD_REQUEST_OFFER "request_offer"
#define JSONRPC_METHOD_RESPONSE_ANSWER "response_answer"

Signaling g_signaling;

void signaling_set_local_description(const char *description) {

  int rc;

  char topic[128];

  char description_base64[4096];

  memset(description_base64, 0, sizeof(description_base64));

  memset(topic, 0, sizeof(topic));

  base64_encode(description, strlen(description), description_base64, sizeof(description_base64));

  snprintf(topic, sizeof(topic), "webrtc/%s/jsonrpc-reply", g_signaling.client_id);

  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "jsonrpc", "2.0");

  cJSON_AddStringToObject(json, "result", description_base64);

  cJSON_AddStringToObject(json, "id", "1");

  char *payload = cJSON_PrintUnformatted(json);

  rc = mosquitto_publish(g_signaling.mosq, NULL, topic, strlen(payload), payload, 0, false);

  cJSON_Delete(json);

  free(payload);

}

void signaling_connect_callback(struct mosquitto *mosq, void *obj, int result) {

  printf("connect callback, rc=%d\n", result);
}

void signaling_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {

  printf("topic: %s\n", message->topic);
  printf("payload: %s\n", (char *)message->payload);

  char *payload = (char *)message->payload;

  cJSON *json = cJSON_Parse(payload);

  if (json) {

    cJSON *method = cJSON_GetObjectItem(json, "method");

    if (method) {

      if (strcmp(method->valuestring, JSONRPC_METHOD_REQUEST_OFFER) == 0) {

        g_signaling.on_event(SIGNALING_EVENT_REQUEST_OFFER, NULL, 0, g_signaling.user_data);

      } else if (strcmp(method->valuestring, JSONRPC_METHOD_RESPONSE_ANSWER) == 0) {

        cJSON *params = cJSON_GetObjectItem(json, "params");

        if (params && cJSON_IsString(params)) {

          char description[4096];
          memset(description, 0, sizeof(description));
          base64_decode(params->valuestring, strlen(params->valuestring), description, sizeof(description));
          g_signaling.on_event(SIGNALING_EVENT_RESPONSE_ANSWER, description, strlen(description), g_signaling.user_data);
        }
      }


    }
  }

  cJSON_Delete(json);

}

void signaling_dispatch(const char *client_id, void (*on_event)(SignalingEvent event, const char *data, size_t len, void *user_data), void *user_data) {

  int rc;

  char topic[128];

  g_signaling.user_data = user_data;

  g_signaling.on_event = on_event;

  g_signaling.run = 1;

  strncpy(g_signaling.client_id, client_id, sizeof(g_signaling.client_id));

  memset(topic, 0, sizeof(topic));

  snprintf(topic, sizeof(topic), "webrtc/%s/jsonrpc", client_id);

  mosquitto_lib_init();

  g_signaling.mosq = mosquitto_new(client_id, true, 0);

  if (g_signaling.mosq) {

    mosquitto_connect_callback_set(g_signaling.mosq, signaling_connect_callback);
    mosquitto_message_callback_set(g_signaling.mosq, signaling_message_callback);

    rc = mosquitto_connect(g_signaling.mosq, MQTT_HOST, MQTT_PORT, KEEPALIVE);
printf("topic: %s\n", topic);
    mosquitto_subscribe(g_signaling.mosq, NULL, topic, 0);

    while (g_signaling.run) {

      rc = mosquitto_loop(g_signaling.mosq, -1, 1);

      if (g_signaling.run && rc) {

        printf("connection error!\n");
        sleep(1);
        mosquitto_reconnect(g_signaling.mosq);
      }
    }

    mosquitto_destroy(g_signaling.mosq);
  }

  mosquitto_lib_cleanup();
}


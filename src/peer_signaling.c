#include <string.h>
#include <signal.h>
#include <assert.h>
#include <cjson/cJSON.h>
#include <libwebsockets.h>

#include <mqtt.h>
#include "posix_sockets.h"
#include "base64.h"
#include "peer_signaling.h"
#include "utils.h"

#define JSONRPC_METHOD_REQUEST_OFFER "request_offer"
#define JSONRPC_METHOD_RESPONSE_ANSWER "response_answer"

#define MQTT_HOST "mqtt.eclipseprojects.io"
#define MQTT_PORT "1883"
#define BUF_SIZE 4096

typedef struct PeerSignaling {

  const char *client_id;
  struct mqtt_client client;
  uint8_t sendbuf[BUF_SIZE];
  uint8_t recvbuf[BUF_SIZE];
  char textbuf[BUF_SIZE];
  int id;
  int sockfd;
  PeerConnection *pc;

} PeerSignaling;

static PeerSignaling g_ps;

void peer_signaling_process_request(const char *msg, size_t size) {

  cJSON *json = cJSON_Parse(msg);
  
  if (json) {
    
    cJSON *method = cJSON_GetObjectItem(json, "method");
    
    cJSON *id = cJSON_GetObjectItem(json, "id");
    
    if (!id && !cJSON_IsNumber(id)) {
      
      LOGE("invalid id");
      return;
    }

    g_ps.id = id->valueint;

    if (method) {

      if (strcmp(method->valuestring, JSONRPC_METHOD_REQUEST_OFFER) == 0) {

          peer_connection_create_offer(g_ps.pc);
      
      } else if (strcmp(method->valuestring, JSONRPC_METHOD_RESPONSE_ANSWER) == 0) {
        
        cJSON *params = cJSON_GetObjectItem(json, "params");
        
        if (params && cJSON_IsString(params)) {
          memset(g_ps.textbuf, 0, sizeof(g_ps.textbuf));
          base64_decode(params->valuestring, strlen(params->valuestring), g_ps.textbuf, sizeof(g_ps.textbuf));
          peer_connection_set_remote_description(g_ps.pc, g_ps.textbuf);
        }
      }
    }
  }

  cJSON_Delete(json);
}

void peer_signaling_onmessage_cb(void** unused, struct mqtt_response_publish *published) {

  peer_signaling_process_request((const char*)published->application_message, published->application_message_size);
}


static void peer_signaling_onicecandidate(char *description, void *userdata) {

  char topic[128];

  char *payload;

  snprintf(topic, sizeof(topic), "webrtc/%s/jsonrpc-reply", g_ps.client_id);

  memset(g_ps.textbuf, 0, sizeof(g_ps.textbuf));
  
  base64_encode(description, strlen(description), g_ps.textbuf, sizeof(g_ps.textbuf));
  
  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "jsonrpc", "2.0");

  cJSON_AddNumberToObject(json, "id", g_ps.id);

  cJSON_AddStringToObject(json, "result", g_ps.textbuf);

  payload = cJSON_PrintUnformatted(json);

  mqtt_publish(&g_ps.client, topic, payload, strlen(payload), MQTT_PUBLISH_QOS_0);

  free(payload);

  cJSON_Delete(json);
}

int peer_signaling_join_channel(const char *client_id, PeerConnection *pc) {

  uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;
  char topic[64];
  memset(topic, 0, sizeof(topic));

  snprintf(topic, sizeof(topic), "webrtc/%s/jsonrpc", client_id);
  g_ps.client_id = client_id;

  if ((g_ps.sockfd = open_nb_socket(MQTT_HOST, MQTT_PORT)) < 0) {

    LOGE("error: create socket");
    return -1;
  }

  mqtt_init(&g_ps.client, g_ps.sockfd,
   g_ps.sendbuf, sizeof(g_ps.sendbuf), g_ps.recvbuf, sizeof(g_ps.recvbuf), peer_signaling_onmessage_cb);

  mqtt_connect(&g_ps.client, client_id, NULL, NULL, 0, NULL, NULL, connect_flags, 400);

  LOGI("Subscribing to %s", topic);

  if (g_ps.client.error != MQTT_OK) {

    LOGE("error: %s\n", mqtt_error_str(g_ps.client.error));
    return -1;
  }

  mqtt_subscribe(&g_ps.client, topic, 0);

  g_ps.pc = pc;
  peer_connection_onicecandidate(pc, peer_signaling_onicecandidate);

  return 0;
}

int peer_signaling_loop() {

  return mqtt_sync((struct mqtt_client*) &g_ps.client);
}

void peer_signaling_leave_channel() {

  if (g_ps.sockfd != -1)
    close(g_ps.sockfd);
}


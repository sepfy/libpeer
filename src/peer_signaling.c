#include <string.h>
#include <signal.h>
#include <assert.h>
#include <cJSON.h>
#include <unistd.h>

#ifndef WITHOUT_MQTT
#define MQTT_USE_MBEDTLS
#include <mqtt.h>
#include <mbedtls_sockets.h>
#endif

#include "config.h"
#include "base64.h"
#include "peer_signaling.h"
#include "utils.h"

#define JRPC_PEER_OFFER "PEER_OFFER"
#define JRPC_PEER_ANSWER "PEER_ANSWER"
#define JRPC_PEER_CLOSE "PEER_CLOSE"

#define BUF_SIZE 4096
#define TOPIC_SIZE 128

typedef struct PeerSignaling {

#ifndef WITHOUT_MQTT
  struct mqtt_client client;
  uint8_t sendbuf[BUF_SIZE];
  uint8_t recvbuf[BUF_SIZE];
  struct mbedtls_context ctx;
  mqtt_pal_socket_handle sockfd;
#endif

  char textbuf[BUF_SIZE];
  char subtopic[TOPIC_SIZE];
  char pubtopic[TOPIC_SIZE];

  int id;
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

      if (strcmp(method->valuestring, JRPC_PEER_OFFER) == 0) {

          peer_connection_create_offer(g_ps.pc);
      
      } else if (strcmp(method->valuestring, JRPC_PEER_ANSWER) == 0) {
        
        cJSON *params = cJSON_GetObjectItem(json, "params");
        
        if (params && cJSON_IsString(params)) {
          memset(g_ps.textbuf, 0, sizeof(g_ps.textbuf));
          base64_decode(params->valuestring, strlen(params->valuestring), (uint8_t*)g_ps.textbuf, sizeof(g_ps.textbuf));
          peer_connection_set_remote_description(g_ps.pc, g_ps.textbuf);
        }
      }
    }
  }

  cJSON_Delete(json);
}

void peer_signaling_onmessage_cb(void** unused, struct mqtt_response_publish *published) {

#ifndef WITHOUT_MQTT
  peer_signaling_process_request((const char*)published->application_message, published->application_message_size);
#endif
}

char* peer_signaling_create_offer(char *description) {

  char *payload = NULL;

  memset(g_ps.textbuf, 0, sizeof(g_ps.textbuf));
  
  base64_encode((const unsigned char *)description, strlen(description), g_ps.textbuf, sizeof(g_ps.textbuf));
  
  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "jsonrpc", "2.0");

  cJSON_AddNumberToObject(json, "id", g_ps.id);

  cJSON_AddStringToObject(json, "result", g_ps.textbuf);

  payload = cJSON_PrintUnformatted(json);

  cJSON_Delete(json);

  return payload;
}

static void peer_signaling_onicecandidate(char *description, void *userdata) {

  char *payload;

#ifndef WITHOUT_MQTT
  payload = peer_signaling_create_offer(description);
  mqtt_publish(&g_ps.client, g_ps.pubtopic, payload, strlen(payload), MQTT_PUBLISH_QOS_0);
#endif

  free(payload);
}

int peer_signaling_join_channel(const char *client_id, PeerConnection *pc, const char *cacert) {
#ifndef WITHOUT_MQTT
  uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;

  snprintf(g_ps.subtopic, sizeof(g_ps.subtopic), "webrtc/%s/jsonrpc", client_id);
  snprintf(g_ps.pubtopic, sizeof(g_ps.pubtopic), "webrtc/%s/jsonrpc-reply", client_id);

  open_nb_socket(&g_ps.ctx, MQTT_HOST, MQTT_PORT, cacert);
  g_ps.sockfd = &g_ps.ctx.ssl_ctx;

  LOGI("Connecting to %s", MQTT_HOST);

  if (g_ps.sockfd == NULL) {

    LOGE("error: create socket");
    return -1;
  }

  mqtt_init(&g_ps.client, g_ps.sockfd,
   g_ps.sendbuf, sizeof(g_ps.sendbuf), g_ps.recvbuf, sizeof(g_ps.recvbuf), peer_signaling_onmessage_cb);

  mqtt_connect(&g_ps.client, client_id, NULL, NULL, 0, NULL, NULL, connect_flags, 400);

  LOGI("Subscribing to %s", g_ps.subtopic);

  if (g_ps.client.error != MQTT_OK) {

    LOGE("error: %s\n", mqtt_error_str(g_ps.client.error));
    return -1;
  }

  mqtt_subscribe(&g_ps.client, g_ps.subtopic, 0);
  peer_connection_onicecandidate(pc, peer_signaling_onicecandidate);
#endif
  g_ps.pc = pc;
  return 0;
}

int peer_signaling_loop() {
#ifndef WITHOUT_MQTT
  return mqtt_sync((struct mqtt_client*) &g_ps.client);
#else
  return 0;
#endif

}

void peer_signaling_leave_channel() {
#ifndef WITHOUT_MQTT
  mbedtls_ssl_free(g_ps.sockfd);
#endif
}


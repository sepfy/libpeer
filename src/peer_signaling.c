#include <string.h>
#include <signal.h>
#include <assert.h>
#include <cJSON.h>

#include <core_mqtt.h>
#include <core_http_client.h>

#include "config.h"
#include "base64.h"
#include "utils.h"
#include "ssl_transport.h"
#include "peer_signaling.h"

#define KEEP_ALIVE_TIMEOUT_SECONDS 60
#define CONNACK_RECV_TIMEOUT_MS 1000

#define JRPC_PEER_STATE "state"
#define JRPC_PEER_OFFER "offer"
#define JRPC_PEER_ANSWER "answer"
#define JRPC_PEER_CLOSE "close"

#define BUF_SIZE 4096
#define TOPIC_SIZE 128

typedef struct PeerSignaling {

  MQTTContext_t mqtt_ctx;
  MQTTFixedBuffer_t mqtt_fixed_buf;

  TransportInterface_t transport;
  NetworkContext_t net_ctx;

  uint8_t mqtt_buf[BUF_SIZE];
  uint8_t http_buf[BUF_SIZE];

  char subtopic[TOPIC_SIZE];
  char pubtopic[TOPIC_SIZE];

  uint16_t packet_id;
  int id;
  PeerConnection *pc;

} PeerSignaling;

static PeerSignaling g_ps;

static void peer_signaling_mqtt_publish(MQTTContext_t *mqtt_ctx, const char *message) {

  MQTTStatus_t status;
  MQTTPublishInfo_t pub_info;

  memset(&pub_info, 0, sizeof(pub_info));

  pub_info.qos = MQTTQoS0;
  pub_info.retain = false;
  pub_info.pTopicName = g_ps.pubtopic;
  pub_info.topicNameLength = strlen(g_ps.pubtopic);
  pub_info.pPayload = message;
  pub_info.payloadLength = strlen(message);

  status = MQTT_Publish(mqtt_ctx, &pub_info, MQTT_GetPacketId(mqtt_ctx));
  if (status != MQTTSuccess) {

    LOGE("MQTT_Publish failed: Status=%s.", MQTT_Status_strerror(status));
  } else {

    LOGD("MQTT_Publish succeeded.");
  }
}


static cJSON* peer_signaling_create_response(int id) {

  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "jsonrpc", "2.0");

  cJSON_AddNumberToObject(json, "id", id);

  return json;
}

static void peer_signaling_process_response(cJSON *json) {

  char *payload = cJSON_PrintUnformatted(json);

  if (payload) {

    peer_signaling_mqtt_publish(&g_ps.mqtt_ctx, payload);

    free(payload);
  }

  cJSON_Delete(json);
}

static void peer_signaling_process_request(const char *msg, size_t size) {

  cJSON *request, *response = NULL;
  cJSON *method, *id;

  request = cJSON_Parse(msg);

  if (!request) {

    LOGW("Parse json failed");
    return;
  }

  do {

    method = cJSON_GetObjectItem(request, "method");

    id = cJSON_GetObjectItem(request, "id");

    if (!id && !cJSON_IsNumber(id)) {

      LOGW("Cannot find id");
      break;
    }

    if (!method && cJSON_IsString(method)) {

      LOGW("Cannot find method");
      break;
    }

    PeerConnectionState state = peer_connection_get_state(g_ps.pc);
    response = peer_signaling_create_response(id->valueint);

    if (strcmp(method->valuestring, JRPC_PEER_OFFER) == 0) {

      if (state == PEER_CONNECTION_CLOSED) {

        g_ps.id = id->valueint;
        peer_connection_create_offer(g_ps.pc);
        cJSON_Delete(response);
        response = NULL;

      } else {

        cJSON_AddStringToObject(response, "result", "busy");
      }

    } else if (strcmp(method->valuestring, JRPC_PEER_ANSWER) == 0) {

      if (state == PEER_CONNECTION_NEW) {

        cJSON *params = cJSON_GetObjectItem(request, "params");

        if (!params && !cJSON_IsString(params)) {

          LOGW("Cannot find params");
          break;
        }

        peer_connection_set_remote_description(g_ps.pc, params->valuestring);
        cJSON_AddStringToObject(response, "result", "");
      }

    } else if (strcmp(method->valuestring, JRPC_PEER_STATE) == 0) {

       cJSON_AddStringToObject(response, "result", peer_connection_state_to_string(state));

    } else if (strcmp(method->valuestring, JRPC_PEER_CLOSE) == 0) {

      peer_connection_close(g_ps.pc);
      cJSON_AddStringToObject(response, "result", "");

    } else {

      LOGW("Unsupport method");
    }

  } while (0);

  if (response) {

    peer_signaling_process_response(response);
  }

  cJSON_Delete(request);
}

HTTPResponse_t peer_signaling_http_request(const TransportInterface_t *transport_interface,
 const char *method, size_t method_len,
 const char *host, size_t host_len,
 const char *path, size_t path_len,
 const char *body, size_t body_len) {

  HTTPStatus_t status = HTTPSuccess;
  HTTPRequestInfo_t request_info = {0};
  HTTPResponse_t response = {0};
  HTTPRequestHeaders_t request_headers = {0};

  request_info.pMethod = method;
  request_info.methodLen = method_len;
  request_info.pHost = host;
  request_info.hostLen = host_len;
  request_info.pPath = path;
  request_info.pathLen = path_len;
  request_info.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

  request_headers.pBuffer = g_ps.http_buf;
  request_headers.bufferLen = sizeof(g_ps.http_buf);

  status = HTTPClient_InitializeRequestHeaders(&request_headers, &request_info);

  if (status == HTTPSuccess) {

    HTTPClient_AddHeader(&request_headers,
     "Content-Type", strlen("Content-Type"), "application/sdp", strlen("application/sdp"));

    response.pBuffer = g_ps.http_buf;
    response.bufferLen = sizeof(g_ps.http_buf);

    status = HTTPClient_Send(transport_interface,
     &request_headers, (uint8_t*)body, body ? body_len : 0, &response, 0);

  } else {

    LOGE("Failed to initialize HTTP request headers: Error=%s.", HTTPClient_strerror(status));
  }

  return response;
}

static int peer_signaling_http_post(const char *hostname, const char *path, int port, const char *body) {
  int32_t ret = EXIT_SUCCESS;

  TransportInterface_t trans_if = {0};
  NetworkContext_t net_ctx;

  trans_if.recv = ssl_transport_recv;
  trans_if.send = ssl_transport_send;
  trans_if.pNetworkContext = &net_ctx;

  ret = ssl_transport_connect(&net_ctx, hostname, port, NULL);

  if (ret < 0) {
    LOGE("Failed to connect to %s:%d", hostname, port);
    return ret;
  }

  HTTPResponse_t response = peer_signaling_http_request(&trans_if, "POST", 4, hostname, strlen(hostname), path, strlen(path), body, strlen(body));

  LOGI("Received HTTP response from %s%s...\n"
   "Response Headers: %s\nResponse Status: %u\nResponse Body: %s\n",
   hostname, path, response.pHeaders, response.statusCode, response.pBody);

  ssl_transport_disconnect(&net_ctx);

  if (response.statusCode == 201) {

   peer_connection_set_remote_description(g_ps.pc, (const char*)response.pBody);
  }
  return 0;
}

static void peer_signaling_mqtt_cb(MQTTContext_t *mqtt_ctx,
 MQTTPacketInfo_t *packet_info, MQTTDeserializedInfo_t *deserialized_info) {

  switch (packet_info->type) {

    case MQTT_PACKET_TYPE_CONNACK:
      LOGI("MQTT_PACKET_TYPE_CONNACK");
      break;
    case MQTT_PACKET_TYPE_PUBLISH:
      LOGI("MQTT_PACKET_TYPE_PUBLISH");
      peer_signaling_process_request(deserialized_info->pPublishInfo->pPayload,
       deserialized_info->pPublishInfo->payloadLength);
      break;
    case MQTT_PACKET_TYPE_SUBACK:
      LOGD("MQTT_PACKET_TYPE_SUBACK");
      break;
    default:
      break;
  }
}

static int peer_signaling_mqtt_connect(const char *hostname, int port) {

  MQTTStatus_t status;
  MQTTConnectInfo_t conn_info;
  bool session_present;

  if (ssl_transport_connect(&g_ps.net_ctx, hostname, port, NULL) < 0) {
    return -1;
  }

  g_ps.transport.recv = ssl_transport_recv;
  g_ps.transport.send = ssl_transport_send;
  g_ps.transport.pNetworkContext = &g_ps.net_ctx;
  g_ps.mqtt_fixed_buf.pBuffer = g_ps.mqtt_buf;
  g_ps.mqtt_fixed_buf.size = sizeof(g_ps.mqtt_buf);
  status = MQTT_Init(&g_ps.mqtt_ctx, &g_ps.transport,
   utils_get_timestamp, peer_signaling_mqtt_cb, &g_ps.mqtt_fixed_buf);

  memset(&conn_info, 0, sizeof(conn_info));

  conn_info.cleanSession = false;
  conn_info.pUserName = NULL;
  conn_info.userNameLength = 0U;
  conn_info.pPassword = NULL;
  conn_info.passwordLength = 0U;
  conn_info.pClientIdentifier = "peer";
  conn_info.clientIdentifierLength = 4;

  conn_info.keepAliveSeconds = KEEP_ALIVE_TIMEOUT_SECONDS;

  status = MQTT_Connect(&g_ps.mqtt_ctx,
   &conn_info, NULL, CONNACK_RECV_TIMEOUT_MS, &session_present);

  if (status != MQTTSuccess) {
    LOGE("MQTT_Connect failed: Status=%s.", MQTT_Status_strerror(status));
    return -1;
  }

  LOGI("MQTT_Connect succeeded.");
  return 0;
}

static int peer_signaling_mqtt_subscribe(int subscribed) {

  MQTTStatus_t status = MQTTSuccess;
  MQTTSubscribeInfo_t sub_info;

  uint16_t packet_id = MQTT_GetPacketId(&g_ps.mqtt_ctx);

  memset(&sub_info, 0, sizeof(sub_info));
  sub_info.qos = MQTTQoS0;
  sub_info.pTopicFilter = g_ps.subtopic;
  sub_info.topicFilterLength = strlen(g_ps.subtopic);

  if (subscribed) {
    status = MQTT_Subscribe(&g_ps.mqtt_ctx, &sub_info, 1, packet_id);
  } else {
    status = MQTT_Unsubscribe(&g_ps.mqtt_ctx, &sub_info, 1, packet_id);
  }
  if (status != MQTTSuccess) {
    LOGE("MQTT_Subscribe failed: Status=%s.", MQTT_Status_strerror(status));
    return -1;
  }

  status = MQTT_ProcessLoop(&g_ps.mqtt_ctx);

  if (status != MQTTSuccess) {
    LOGE("MQTT_ProcessLoop failed: Status=%s.", MQTT_Status_strerror(status));
    return -1;
  }

  LOGD("MQTT Subscribe/Unsubscribe succeeded.");
  return 0;
}

static void peer_signaling_onicecandidate(char *description, void *userdata) {
#if CONFIG_HTTP
  peer_signaling_http_post(WHIP_HOST, WHIP_PATH, WHIP_PORT, description);
#endif

#if CONFIG_MQTT
  cJSON *json = peer_signaling_create_response(g_ps.id);
  cJSON_AddStringToObject(json, "result", description);
  peer_signaling_process_response(json);
  g_ps.id = 0;
#endif
}

int peer_signaling_join_channel(const char *client_id, PeerConnection *pc) {

  g_ps.pc = pc;
  peer_connection_onicecandidate(pc, peer_signaling_onicecandidate);

#if CONFIG_HTTP
  peer_connection_create_offer(pc);
#endif

#if CONFIG_MQTT
  snprintf(g_ps.subtopic, sizeof(g_ps.subtopic), "webrtc/%s/jsonrpc", client_id);
  snprintf(g_ps.pubtopic, sizeof(g_ps.pubtopic), "webrtc/%s/jsonrpc-reply", client_id);
  peer_signaling_mqtt_connect(MQTT_HOST, MQTT_PORT);
  peer_signaling_mqtt_subscribe(1);
#endif

  return 0;
}

int peer_signaling_loop() {
#if CONFIG_MQTT
  MQTT_ProcessLoop(&g_ps.mqtt_ctx);
#endif
  return 0;
}

void peer_signaling_leave_channel() {
  // TODO: HTTP DELETE with Location?
#if CONFIG_MQTT
  MQTTStatus_t status = MQTTSuccess;

  if (peer_signaling_mqtt_subscribe(0) == 0) {

    status = MQTT_Disconnect(&g_ps.mqtt_ctx);
  }

  if(status != MQTTSuccess) {

    LOGE("Failed to disconnect with broker: %s", MQTT_Status_strerror(status));
  }
#endif
}

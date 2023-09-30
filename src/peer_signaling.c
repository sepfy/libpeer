#include <string.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <cJSON.h>

#include "core_mqtt.h"
#include "core_http_client.h"
#include "plaintext_posix.h"

#include "config.h"
#include "base64.h"
#include "utils.h"
#include "peer_signaling.h"

#define KEEP_ALIVE_TIMEOUT_SECONDS 60
#define CONNACK_RECV_TIMEOUT_MS 1000
#define TRANSPORT_SEND_RECV_TIMEOUT_MS 1000

#define JRPC_PEER_OFFER "PEER_OFFER"
#define JRPC_PEER_ANSWER "PEER_ANSWER"
#define JRPC_PEER_CLOSE "PEER_CLOSE"

#define BUF_SIZE 4096
#define TOPIC_SIZE 128

struct NetworkContext {
  PlaintextParams_t *pParams;
};

typedef struct PeerSignaling {

  MQTTContext_t mqtt_ctx;
  MQTTFixedBuffer_t mqtt_fixed_buf;

  TransportInterface_t transport;
  NetworkContext_t net_ctx;
  PlaintextParams_t plaintext_params;

  uint8_t mqtt_buf[BUF_SIZE];
  uint8_t http_buf[BUF_SIZE];
  char textbuf[BUF_SIZE];

  char subtopic[TOPIC_SIZE];
  char pubtopic[TOPIC_SIZE];

  uint16_t packet_id;
  int id;
  PeerConnection *pc;

} PeerSignaling;

static PeerSignaling g_ps;

static void peer_signaling_process_request(const char *msg, size_t size) {

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


static char* peer_signaling_create_offer(char *description) {

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

int32_t peer_signaling_tcp_connect(NetworkContext_t *net_ctx, const char *host, size_t host_len, const unsigned int port) {

  int32_t ret = EXIT_FAILURE;
  SocketStatus_t socketStatus;
  ServerInfo_t serverInfo;

  serverInfo.pHostName = host;
  serverInfo.hostNameLength = host_len;
  serverInfo.port = port;

  socketStatus = Plaintext_Connect(net_ctx, &serverInfo, TRANSPORT_SEND_RECV_TIMEOUT_MS, TRANSPORT_SEND_RECV_TIMEOUT_MS);

  if (socketStatus == SOCKETS_SUCCESS) {
    ret = EXIT_SUCCESS;
  } else {
    ret = EXIT_FAILURE;
  }

  return ret;
}

HTTPResponse_t peer_signaling_http_request(const TransportInterface_t *pTransportInterface,
 const char *pMethod, size_t methodLen,
 const char *pHost, size_t host_len,
 const char *pPath, size_t pathLen,
 const char *pBody, size_t bodyLen) {

  HTTPStatus_t httpStatus = HTTPSuccess;
  HTTPRequestInfo_t requestInfo = {0};
  HTTPResponse_t response = {0};
  HTTPRequestHeaders_t requestHeaders = {0};

  requestInfo.pMethod = pMethod;
  requestInfo.methodLen = methodLen;
  requestInfo.pHost = pHost;
  requestInfo.hostLen = host_len;
  requestInfo.pPath = pPath;
  requestInfo.pathLen = pathLen;
  requestInfo.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

  requestHeaders.pBuffer = g_ps.http_buf;
  requestHeaders.bufferLen = sizeof(g_ps.http_buf);

  httpStatus = HTTPClient_InitializeRequestHeaders(&requestHeaders, &requestInfo);


  if (httpStatus == HTTPSuccess) {

    HTTPClient_AddHeader(&requestHeaders, "Content-Type", strlen("Content-Type"), "application/sdp", strlen("application/sdp"));

    response.pBuffer = g_ps.http_buf;
    response.bufferLen = sizeof(g_ps.http_buf);

    httpStatus = HTTPClient_Send( pTransportInterface,
                                  &requestHeaders,
                                  (uint8_t*)pBody,
                                  pBody ? bodyLen : 0,
                                  &response,
                                  0 );
  } else {
    LOGE("Failed to initialize HTTP request headers: Error=%s.", HTTPClient_strerror(httpStatus));
  }

  return response;
}

static int peer_signaling_http_post(const char *hostname, const char *path, int port, const char *body) { 

  int32_t ret = EXIT_SUCCESS;

  TransportInterface_t transportInterface = {0};
  NetworkContext_t networkContext;
  PlaintextParams_t plaintextParams;
  networkContext.pParams = &plaintextParams;

  ret = peer_signaling_tcp_connect(&networkContext, hostname, strlen(hostname), port);

  transportInterface.recv = Plaintext_Recv;
  transportInterface.send = Plaintext_Send;
  transportInterface.pNetworkContext = &networkContext;

  HTTPResponse_t response = peer_signaling_http_request(&transportInterface, "POST", 4, hostname, strlen(hostname), path, strlen(path), body, strlen(body));

  LOGD("Received HTTP response from %s%s...\n"
   "Response Headers: %s\n"
   "Response Status: %u\n"
   "Response Body: %s\n",
   hostname, path,
   response.pHeaders,
   response.statusCode,
   response.pBody);

  Plaintext_Disconnect(&networkContext);

  if (response.statusCode == 201) {

   peer_connection_set_remote_description(g_ps.pc, (const char*)response.pBody);
  }
  return 0;
}

static void peer_signaling_mqtt_cb(MQTTContext_t * mqtt_ctx, MQTTPacketInfo_t * pxPacketInfo,
 MQTTDeserializedInfo_t * pxDeserializedInfo ) {

  switch (pxPacketInfo->type) {

    case MQTT_PACKET_TYPE_CONNACK:
      LOGI("MQTT_PACKET_TYPE_CONNACK");
      break;
    case MQTT_PACKET_TYPE_PUBLISH:
      LOGI("MQTT_PACKET_TYPE_PUBLISH");
      peer_signaling_process_request(pxDeserializedInfo->pPublishInfo->pPayload, pxDeserializedInfo->pPublishInfo->payloadLength);
      break;
    default:
      break;
  }
}

static int peer_signaling_mqtt_connect(const char *hostname, int port) {

  MQTTStatus_t status;
  MQTTConnectInfo_t conn_info;
  bool session_present;

  g_ps.net_ctx.pParams = &g_ps.plaintext_params;

  if (peer_signaling_tcp_connect(&g_ps.net_ctx, hostname, strlen(hostname), port) < 0) {
    return -1;
  }

  g_ps.transport.recv = Plaintext_Recv;
  g_ps.transport.send = Plaintext_Send;
  g_ps.transport.pNetworkContext = &g_ps.net_ctx;
  g_ps.mqtt_fixed_buf.pBuffer = g_ps.mqtt_buf;
  g_ps.mqtt_fixed_buf.size = sizeof(g_ps.mqtt_buf);
  status = MQTT_Init(&g_ps.mqtt_ctx, &g_ps.transport, utils_get_timestamp, peer_signaling_mqtt_cb, &g_ps.mqtt_fixed_buf);

  memset(&conn_info, 0, sizeof(conn_info));

  conn_info.cleanSession = true;

  conn_info.pClientIdentifier = "test";
  conn_info.clientIdentifierLength = ( uint16_t ) strlen("test");

  conn_info.keepAliveSeconds = KEEP_ALIVE_TIMEOUT_SECONDS;

  status = MQTT_Connect(&g_ps.mqtt_ctx,
    &conn_info,
    NULL,
    CONNACK_RECV_TIMEOUT_MS,
    &session_present);

  if (status != MQTTSuccess) {
    LOGE("MQTT_Connect failed: Status=%s.", MQTT_Status_strerror(status));
    return -1;
  }
  LOGI("MQTT_Connect succeeded.");
  return 0;
}

static int peer_signaling_mqtt_subscribe() {

  MQTTStatus_t status = MQTTSuccess;
  MQTTSubscribeInfo_t sub_info;

  uint16_t packet_id = MQTT_GetPacketId(&g_ps.mqtt_ctx);

  memset(&sub_info, 0, sizeof(sub_info));
  sub_info.qos = MQTTQoS0;
  sub_info.pTopicFilter = g_ps.subtopic;
  sub_info.topicFilterLength = strlen(g_ps.subtopic);

  status = MQTT_Subscribe(&g_ps.mqtt_ctx, &sub_info, 1, packet_id);

  if (status != MQTTSuccess) {
    LOGE("MQTT_Subscribe failed: Status=%s.", MQTT_Status_strerror(status));
    return -1;
  }

  status = MQTT_ProcessLoop(&g_ps.mqtt_ctx);

  if (status != MQTTSuccess) {
    LOGE("MQTT_ProcessLoop failed: Status=%s.", MQTT_Status_strerror(status));
    return -1;
  }
  LOGI("MQTT_Subscribe succeeded.");
  return 0;
}

static void peer_signaling_mqtt_publish(MQTTContext_t * mqtt_ctx, const char *message) {

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
    LOGI("MQTT_Publish succeeded.");
  }
}

static void peer_signaling_onicecandidate(char *description, void *userdata) {

#if defined(HAVE_HTTP)
  peer_signaling_http_post(WHIP_HOST, WHIP_PATH, WHIP_PORT, description);
#elif defined(HAVE_MQTT)
  char *payload = NULL;
  payload = peer_signaling_create_offer(description);
  peer_signaling_mqtt_publish(&g_ps.mqtt_ctx, payload);
  free(payload);
#endif

}

int peer_signaling_join_channel(const char *client_id, PeerConnection *pc, const char *cacert) {

  g_ps.pc = pc;
#if defined(HAVE_HTTP)
  peer_connection_onicecandidate(pc, peer_signaling_onicecandidate);
  peer_connection_create_offer(pc);
#elif defined(HAVE_MQTT)
  snprintf(g_ps.subtopic, sizeof(g_ps.subtopic), "webrtc/%s/jsonrpc", client_id);
  snprintf(g_ps.pubtopic, sizeof(g_ps.pubtopic), "webrtc/%s/jsonrpc-reply", client_id);
  peer_signaling_mqtt_connect(MQTT_HOST, MQTT_PORT);
  peer_signaling_mqtt_subscribe();
#endif
  return 0;
}

int peer_signaling_loop() {
#if defined(HAVE_MQTT)
  MQTT_ProcessLoop(&g_ps.mqtt_ctx);
//  LOGI("MQTT_ProcessLoop");
#endif
  return 0;
}

void peer_signaling_leave_channel() {

  // TODO: HTTP DELETE with Location?
  // TODO: MQTT unsubscribe
}


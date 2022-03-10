#include <string.h>
#include <cJSON.h>
#include <MQTTClient.h>

#include "signaling_observer.h"
#include "signaling_mqtt.h"
#include "session_description.h"
#include "utils.h"

struct SignalingMqtt {

  SignalingObserver *signaling_observer;

  MQTTClient client;
  MQTTClient_connectOptions conn_opts;
  MQTTClient_SSLOptions ssl_opts;

  gchar topic[128];

  gint qos;
  gboolean dispatching;

  char *answer;
};

void signaling_mqtt_message_delivered(void *context, MQTTClient_deliveryToken dt) {

}

int signaling_mqtt_message_arrived(void *context, char *topic_name, int topic_len,
 MQTTClient_message *message) {

  SignalingMqtt *signaling_mqtt = (SignalingMqtt*)context;

  size_t payload_len = message->payloadlen;
  char *payload_ptr = message->payload;
  cJSON *json = NULL;
  cJSON *type = NULL;
  cJSON *sdp = NULL;

  guchar *offer = NULL;
  //LOG_DEBUG("%s", payload_ptr);

  do {
    offer = g_base64_decode(payload_ptr, &payload_len);
    if(!offer)
      break;

    json = cJSON_Parse(offer);
    if(!json) {
      LOG_ERROR("Cannot parse signaling message");
      break;
    }

    type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if(!cJSON_IsString(type) || (type->valuestring == NULL)) {
      LOG_ERROR("Cannot get type from signaling message");
      break;
    }

    sdp = cJSON_GetObjectItemCaseSensitive(json, "sdp");
    if(!cJSON_IsString(sdp) || (sdp->valuestring == NULL)) {
      LOG_ERROR("Cannot get sdp from signaling message");
      break;
    }

    if(strcmp(type->valuestring, "offer") == 0) {
      signaling_observer_notify_event(signaling_mqtt->signaling_observer,
       SIGNALING_EVENT_GET_OFFER, sdp->valuestring);
    }

  } while(0);

  if(offer)
    free(offer);

  if(json)
    cJSON_Delete(json);

  MQTTClient_freeMessage(&message);
  MQTTClient_free(topic_name);

  return 1;

}

void signaling_mqtt_connection_lost(void *context, char *cause) {

  SignalingMqtt *signaling_mqtt = (SignalingMqtt*)context;
  LOG_INFO("Connect lost"); 
}

void signaling_mqtt_set_answer(SignalingMqtt *signaling_mqtt, const char *sdp) {

  MQTTClient_deliveryToken token;

  MQTTClient_message pubmsg = MQTTClient_message_initializer;

  char answer[SDP_MAX_SIZE] = {0};
  snprintf(answer, SDP_MAX_SIZE, "{\"type\": \"answer\", \"sdp\": \"%s\"}", sdp);

  if(signaling_mqtt->answer) {
    free(signaling_mqtt->answer);
  }

  signaling_mqtt->answer = g_base64_encode(answer, strlen(answer));

  pubmsg.payload = signaling_mqtt->answer;
  pubmsg.payloadlen = strlen(signaling_mqtt->answer);
  pubmsg.qos = signaling_mqtt->qos;
  pubmsg.retained = 0;

  MQTTClient_publishMessage(signaling_mqtt->client, signaling_mqtt->topic, &pubmsg, &token);
  MQTTClient_waitForCompletion(signaling_mqtt->client, token, 1);
}


SignalingMqtt* signaling_mqtt_create(const char *host, int port, const char *topic, const char *tls, const char *username, const char *password, SignalingObserver *signaling_observer) {

  SignalingMqtt *signaling_mqtt = (SignalingMqtt*)malloc(sizeof(SignalingMqtt));
  if(!signaling_mqtt)
    return NULL;

  gchar broker[128];
  g_snprintf(broker, sizeof(broker), "ssl://%s:%d", host, port);
  memset(signaling_mqtt->topic, 0, sizeof(signaling_mqtt->topic));
  strncpy(signaling_mqtt->topic, topic, strlen(topic));

  signaling_mqtt->conn_opts = (MQTTClient_connectOptions)MQTTClient_connectOptions_initializer;
  signaling_mqtt->ssl_opts = (MQTTClient_SSLOptions)MQTTClient_SSLOptions_initializer;

  MQTTClient_create(&signaling_mqtt->client, broker, "", MQTTCLIENT_PERSISTENCE_NONE, NULL);

  signaling_mqtt->conn_opts.keepAliveInterval = 20;
  signaling_mqtt->conn_opts.cleansession = 1;

  if(username)
    signaling_mqtt->conn_opts.username = username; 

  if(password)
    signaling_mqtt->conn_opts.password = password;

  MQTTClient_setCallbacks(signaling_mqtt->client, signaling_mqtt,
   signaling_mqtt_connection_lost,
   signaling_mqtt_message_arrived,
   signaling_mqtt_message_delivered);

  if(tls) {
    signaling_mqtt->conn_opts.ssl = &signaling_mqtt->ssl_opts;
    signaling_mqtt->ssl_opts.trustStore = tls;
    signaling_mqtt->ssl_opts.enableServerCertAuth = FALSE;
    signaling_mqtt->ssl_opts.sslVersion = 3;
  }

  signaling_mqtt->dispatching = FALSE;

  signaling_mqtt->qos = 1;
  signaling_mqtt->answer = NULL;

  signaling_mqtt->signaling_observer = signaling_observer;

  if(MQTTClient_connect(signaling_mqtt->client, &signaling_mqtt->conn_opts) != MQTTCLIENT_SUCCESS) {
    LOG_ERROR("Cannot connect to %s", broker);
    free(signaling_mqtt);
    return NULL;
  }

  LOG_INFO("Connected");
  return signaling_mqtt;
}

void signaling_mqtt_destroy(SignalingMqtt *signaling_mqtt) {

  if(!signaling_mqtt)
    return;

  MQTTClient_disconnect(signaling_mqtt->client, 10000);
  MQTTClient_destroy(&signaling_mqtt->client);

  free(signaling_mqtt);
  signaling_mqtt = NULL;
}


void signaling_mqtt_dispatch(SignalingMqtt *signaling_mqtt) {

  LOG_DEBUG("Subscribe topic %s", signaling_mqtt->topic);

  MQTTClient_subscribe(signaling_mqtt->client, signaling_mqtt->topic, signaling_mqtt->qos);
  signaling_mqtt->dispatching = TRUE;

  while(signaling_mqtt->dispatching) {
    g_usleep(500000);
  }
}

void signaling_mqtt_shutdown(SignalingMqtt *signaling_mqtt) {

  signaling_mqtt->dispatching = FALSE;
}

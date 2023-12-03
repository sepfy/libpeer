#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>
#include <sys/param.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "mdns.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"

#include "peer.h"

static const char *TAG = "webrtc";

static TaskHandle_t xPcTaskHandle = NULL;
static TaskHandle_t xCameraTaskHandle = NULL;
static TaskHandle_t xPsTaskHandle = NULL;

extern esp_err_t camera_init();
extern void camera_task(void *pvParameters);
extern void wifi_init_sta();

PeerConnection *g_pc;
PeerConnectionState eState = PEER_CONNECTION_CLOSED;
int gDataChannelOpened = 0;

int64_t get_timestamp() {

  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

static void oniceconnectionstatechange(PeerConnectionState state, void *user_data) {

  ESP_LOGI(TAG, "PeerConnectionState: %d", state);
  eState = state;
  // not support datachannel close event
  if (eState != PEER_CONNECTION_COMPLETED) {
    gDataChannelOpened = 0;
  }
}

static void onmessage(char *msg, size_t len, void *userdata) {

  ESP_LOGI(TAG, "Datachannel message: %.*s, size", len, msg);
}

void onopen(void *userdata) {

  ESP_LOGI(TAG, "Datachannel opened");
  gDataChannelOpened = 1;
}

static void onclose(void *userdata) {

}

void peer_signaling_task(void *arg) {

  ESP_LOGI(TAG, "peer_signaling_task started");

  for(;;) {

    peer_signaling_loop();

    vTaskDelay(pdMS_TO_TICKS(10));
  }

}

void peer_connection_task(void *arg) {

  ESP_LOGI(TAG, "peer_connection_task started");

  for(;;) {

    peer_connection_loop(g_pc);

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void app_main(void) {

  static char deviceid[32] = {0};
  uint8_t mac[8] = {0};

  PeerConfiguration config = {
   .ice_servers = {
    { .urls = "stun:stun.l.google.com:19302" }
   },
   .datachannel = DATA_CHANNEL_BINARY,
  };

  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(mdns_init());

  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    sprintf(deviceid, "esp32-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device ID: %s", deviceid);
  }

  wifi_init_sta();

  peer_init();

  camera_init();

  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, oniceconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);

  peer_signaling_join_channel(deviceid, g_pc);

  xTaskCreatePinnedToCore(camera_task, "camera", 4096, NULL, 6, &xCameraTaskHandle, 0);

  xTaskCreatePinnedToCore(peer_connection_task, "peer_connection", 8192, NULL, 10, &xPcTaskHandle, 1);

  xTaskCreatePinnedToCore(peer_signaling_task, "peer_signaling", 8192, NULL, 10, &xPsTaskHandle, 0);

  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "open https://sepfy.github.io/webrtc?deviceId=%s", deviceid);
}

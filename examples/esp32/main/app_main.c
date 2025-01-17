#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "peer.h"

static const char* TAG = "webrtc";

static TaskHandle_t xPcTaskHandle = NULL;
static TaskHandle_t xCameraTaskHandle = NULL;
static TaskHandle_t xAudioTaskHandle = NULL;

extern esp_err_t camera_init();
extern esp_err_t audio_init();
extern void camera_task(void* pvParameters);
extern void audio_task(void* pvParameters);

SemaphoreHandle_t xSemaphore = NULL;

PeerConnection* g_pc;
PeerConnectionState eState = PEER_CONNECTION_CLOSED;
int gDataChannelOpened = 0;

int64_t get_timestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

static void oniceconnectionstatechange(PeerConnectionState state, void* user_data) {
  ESP_LOGI(TAG, "PeerConnectionState: %d", state);
  eState = state;
  // not support datachannel close event
  if (eState != PEER_CONNECTION_COMPLETED) {
    gDataChannelOpened = 0;
  }
}

static void onmessage(char* msg, size_t len, void* userdata, uint16_t sid) {
  ESP_LOGI(TAG, "Datachannel message: %.*s", len, msg);
}

void onopen(void* userdata) {
  ESP_LOGI(TAG, "Datachannel opened");
  gDataChannelOpened = 1;
}

static void onclose(void* userdata) {
}

void peer_connection_task(void* arg) {
  ESP_LOGI(TAG, "peer_connection_task started");

  for (;;) {
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY)) {
      peer_connection_loop(g_pc);
      xSemaphoreGive(xSemaphore);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void app_main(void) {
  PeerConfiguration config = {
      .ice_servers = {
          {.urls = "stun:stun.l.google.com:19302"}},
      .audio_codec = CODEC_PCMA,
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
  ESP_ERROR_CHECK(example_connect());

  xSemaphore = xSemaphoreCreateMutex();

  peer_init();

  camera_init();

#if defined(CONFIG_ESP32S3_XIAO_SENSE)
  audio_init();
#endif

  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, oniceconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);
  peer_signaling_connect(CONFIG_SIGNALING_URL, CONFIG_SIGNALING_TOKEN, g_pc);

#if defined(CONFIG_ESP32S3_XIAO_SENSE)
  StackType_t* stack_memory = (StackType_t*)heap_caps_malloc(8192 * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
  StaticTask_t task_buffer;
  if (stack_memory) {
    xAudioTaskHandle = xTaskCreateStaticPinnedToCore(audio_task, "audio", 8192, NULL, 7, stack_memory, &task_buffer, 0);
  }
#endif

  xTaskCreatePinnedToCore(camera_task, "camera", 4096, NULL, 8, &xCameraTaskHandle, 1);

  xTaskCreatePinnedToCore(peer_connection_task, "peer_connection", 8192, NULL, 5, &xPcTaskHandle, 1);

  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  printf("============= Configuration =============\n");
  printf(" %-5s : %s\n", "URL", CONFIG_SIGNALING_URL);
  printf(" %-5s : %s\n", "Token", CONFIG_SIGNALING_TOKEN);
  printf("=========================================\n");

  while (1) {
    peer_signaling_loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

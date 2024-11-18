/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "hardware/clocks.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "task.h"

#include "hardware/dma.h"

#include "pcm-g711/pcm-g711/g711.h"
#include "peer.h"
#include "rp2040_i2s_example/i2s.h"

static __attribute__((aligned(8))) pio_i2s i2s;

#define TEST_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

TaskHandle_t xPcTaskHandle;
PeerConnection* g_pc;
PeerConnectionState eState = PEER_CONNECTION_CLOSED;
int gDataChannelOpened = 0;

static void oniceconnectionstatechange(PeerConnectionState state, void* user_data) {
  eState = state;
  printf("state = %d\n", state);
}

static void onmessage(char* msg, size_t len, void* userdata, uint16_t sid) {
  if (strncmp(msg, "ping", 4) == 0) {
    printf("Got ping, send pong\n");
    peer_connection_datachannel_send(g_pc, "pong", 4);
  }
}

void onopen(void* userdata) {
  gDataChannelOpened = 1;
}

static void onclose(void* userdata) {
}
#if 1
uint32_t get_epoch_time() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif
static void dma_i2s_in_handler(void) {
  int8_t alaw[AUDIO_BUFFER_FRAMES];
  int16_t pcm[AUDIO_BUFFER_FRAMES];
  int32_t* input_buffer;
  if (*(int32_t**)dma_hw->ch[i2s.dma_ch_in_ctrl].read_addr == i2s.input_buffer) {
    input_buffer = i2s.input_buffer;
  } else {
    input_buffer = i2s.input_buffer + STEREO_BUFFER_SIZE;
  }
  for (int i = 0; i < AUDIO_BUFFER_FRAMES; i++) {
    pcm[i] = (int16_t)(input_buffer[2 * i + 1] >> 16);
    alaw[i] = ALaw_Encode(pcm[i]);
  }

#if 1
  static uint32_t total_bytes = 0;
  static uint32_t last_time = 0;
  total_bytes += AUDIO_BUFFER_FRAMES;
  uint32_t current_time = get_epoch_time();
  if (current_time - last_time > 1000) {
    printf("AUDIO_BUFFER_FRAMES: %d, bps: %d\n", AUDIO_BUFFER_FRAMES, 1000 * total_bytes * 8 / (current_time - last_time));
    total_bytes = 0;
    last_time = current_time;
  }
#endif

  if (eState == PEER_CONNECTION_COMPLETED) {
    peer_connection_send_audio(g_pc, alaw, AUDIO_BUFFER_FRAMES);
  }

  dma_hw->ints0 = 1u << i2s.dma_ch_in_data;  // clear the IRQ
}

void peer_connection_task() {
  printf("Run peer connection task on the core: %d\n", portGET_CORE_ID());
  while (1) {
    peer_connection_loop(g_pc);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void main_task(__unused void* params) {
  if (cyw43_arch_init()) {
    printf("failed to initialise\n");
    vTaskDelete(NULL);
  }
  cyw43_arch_enable_sta_mode();
  printf("Connecting to Wi-Fi...\n");

  while (1) {
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 100000)) {
      printf("failed to connect.\n");
      vTaskDelay(1000);
    } else {
      printf("Connected.\n");
      break;
    }
  }

  PeerConfiguration config = {
      .ice_servers = {
          {.urls = "stun:stun.l.google.com:19302"}},
      .audio_codec = CODEC_PCMA,
      .datachannel = DATA_CHANNEL_STRING,
  };

  peer_init();
  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, oniceconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);

  ServiceConfiguration service_config = SERVICE_CONFIG_DEFAULT();
  service_config.client_id = "mypico";
  service_config.pc = g_pc;
  peer_signaling_set_config(&service_config);
  peer_signaling_join_channel();

  xTaskCreate(peer_connection_task, "PeerConnectionTask", 4096, NULL, TEST_TASK_PRIORITY, &xPcTaskHandle);

  i2s_program_start_synched(pio0, &i2s_config_default, dma_i2s_in_handler, &i2s);

  printf("Run main task on the core: %d\n", portGET_CORE_ID());
  printf("open https://sepfy.github.io/webrtc?deviceId=mypico\n");
  while (true) {
    peer_signaling_loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  cyw43_arch_deinit();
}

void vLaunch(void) {
  TaskHandle_t task;
  xTaskCreate(main_task, "TestMainThread", 4096, NULL, TEST_TASK_PRIORITY, &task);
  vTaskCoreAffinitySet(task, 1);
  vTaskStartScheduler();
}

int main(void) {
  stdio_init_all();
  // set_sys_clock_khz(132000, true);
  vLaunch();
  return 0;
}

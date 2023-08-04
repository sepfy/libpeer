/*
 * i2s_acc.c
 *
 *  Created on: 2020/09/10
 *      Author: nishi
 *
 *  https://docs.espressif.com/projects/esp-idf/en/v3.3/api-reference/peripherals/i2s.html#_CPPv417i2s_comm_format_t
 */

#include "freertos/FreeRTOS.h"

#include "driver/i2s.h"
#include "esp_log.h"
#include "peer_connection.h"
#include "g711.h"

static const char *TAG = "I2S_ACC";

extern PeerConnectionState eState;
extern PeerConnection *g_pc;

void audio_init(void) {

  i2s_config_t i2s_config = {
   .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
   .sample_rate = 8000,
   .bits_per_sample = (i2s_bits_per_sample_t)I2S_BITS_PER_SAMPLE_16BIT,
   .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
   .communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT,
   .intr_alloc_flags = 0,
   .dma_buf_count = 6,
   .dma_buf_len = 512,
   .use_apll = false,
   .tx_desc_auto_clear = false,
   .fixed_mclk = -1,
  };

  i2s_pin_config_t pin_config = {
      .bck_io_num = 26,    // IIS_SCLK
      .ws_io_num = 32,     // IIS_LCLK
      .data_out_num = -1,  // IIS_DSIN
      .data_in_num = 33,   // IIS_DOUT
  };

  esp_err_t ret = 0;
  ret = i2s_driver_install((i2s_port_t)1, &i2s_config, 0, NULL);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error in i2s_driver_install");
  }

  ret = i2s_set_pin(I2S_NUM_1, &pin_config);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error in i2s_set_pin");
  }

  ret = i2s_zero_dma_buffer(I2S_NUM_1);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error in initializing dma buffer with 0");
  }

}

int32_t getSample(uint8_t *dt,int32_t dl){

  size_t bytes_read;

  i2s_read((i2s_port_t)1, (void *)dt, dl,&bytes_read, portMAX_DELAY);
  if (bytes_read <= 0 || dl != bytes_read) {
    ESP_LOGE(TAG, "Error in I2S read : %d", bytes_read);
  }
  return bytes_read;

}


void audio_task(void *arg) {

  int ret;
  int16_t audioraw[160];
  uint8_t pcma[160];

  for (;;) {

    // send audio data when connected
    if (eState == PEER_CONNECTION_CONNECTED) {

      ret = getSample((uint8_t*)audioraw, sizeof(audioraw));

      if (ret == sizeof(audioraw)) {

        for (int i = 0; i < sizeof(pcma); i++) {

          // amplify?
          pcma[i] = ALaw_Encode(audioraw[i] << 4);
        }
        peer_connection_send_audio(g_pc, pcma, sizeof(pcma));
      }

    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }


}


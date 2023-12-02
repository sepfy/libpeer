#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_pdm.h"

#include "esp_audio_enc.h"
#include "esp_audio_enc_def.h"
#include "esp_audio_def.h"
#include "esp_opus_enc.h"

#include "peer_connection.h"

#define I2S_CLK_GPIO 42
#define I2S_DATA_GPIO 41

static const char *TAG = "AUDIO";

extern PeerConnection *g_pc;
extern PeerConnectionState eState;
extern int get_timestamp();

i2s_chan_handle_t rx_handle = NULL;

esp_audio_enc_handle_t enc_handle = NULL;
esp_audio_enc_in_frame_t aenc_in_frame = { 0 };
esp_audio_enc_out_frame_t aenc_out_frame = { 0 };

esp_err_t audio_codec_init() {

  uint8_t *inbuf = NULL;
  uint8_t *outbuf = NULL;
  int aenc_in_frame_size = 0;
  int aenc_out_frame_size = 0;

  esp_audio_err_t ret = ESP_AUDIO_ERR_OK;

  esp_opus_enc_config_t config = ESP_OPUS_ENC_CONFIG_DEFAULT();
  config.sample_rate = ESP_AUDIO_SAMPLE_RATE_8K;
  config.channel = ESP_AUDIO_MONO;
  config.bitrate = 18000;
  config.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_40_MS;

  ret = esp_opus_enc_open(&config, sizeof(esp_opus_enc_config_t), &enc_handle);
  if (ret != ESP_AUDIO_ERR_OK) {
    ESP_LOGE(TAG, "audio encoder open failed");
    return ESP_FAIL;
  }

  ret = esp_opus_enc_get_frame_size(enc_handle, &aenc_in_frame_size, &aenc_out_frame_size);
  if (ret != ESP_AUDIO_ERR_OK) {
    ESP_LOGE(TAG, "audio encoder get frame size failed");
    return ESP_FAIL;
  }

  inbuf = calloc(1, aenc_in_frame_size*2);
  if (!inbuf) {
    ESP_LOGE(TAG, "inbuf malloc failed");
    return ESP_FAIL;
  }

  outbuf = calloc(1, aenc_out_frame_size);
  if (!outbuf) {
    ESP_LOGE(TAG, "outbuf malloc failed");
    return ESP_FAIL;
  }

  aenc_in_frame.buffer = inbuf;
  aenc_in_frame.len = aenc_in_frame_size;
  aenc_out_frame.buffer = outbuf;
  aenc_out_frame.len = aenc_out_frame_size;
  ESP_LOGI(TAG, "audio codec init done. in_frame_size=%d, out_frame_size=%d", aenc_in_frame_size, aenc_out_frame_size);
  return 0;
}

esp_err_t audio_init(void) {

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

  i2s_pdm_rx_config_t pdm_rx_cfg = {
   .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(8000),
   .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
   .gpio_cfg = {
    .clk = I2S_CLK_GPIO,
    .din = I2S_DATA_GPIO,
    .invert_flags = {
     .clk_inv = false,
    },
   },
  };

  ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

  return audio_codec_init();
}

void audio_deinit(void) {

  ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
  ESP_ERROR_CHECK(i2s_del_channel(rx_handle));
}

int32_t audio_get_samples(uint8_t *buf, size_t size) {

  size_t bytes_read;

  if (i2s_channel_read(rx_handle, (char *)buf, size, &bytes_read, 1000) != ESP_OK) {
    ESP_LOGE(TAG, "i2s read error");
  }

  return bytes_read;
}

void audio_task(void *arg) {

  int ret;
  static int64_t last_time;
  int64_t curr_time;
  float bytes = 0;

  last_time = get_timestamp();
  ESP_LOGI(TAG, "audio task started");

  for (;;) {

    if (eState == PEER_CONNECTION_COMPLETED) {

      ret = audio_get_samples(aenc_in_frame.buffer, aenc_in_frame.len);

      if (ret == aenc_in_frame.len) {

        if (esp_opus_enc_process(enc_handle, &aenc_in_frame, &aenc_out_frame) == ESP_AUDIO_ERR_OK) {

          peer_connection_send_audio(g_pc, aenc_out_frame.buffer, aenc_out_frame.encoded_bytes);

          bytes += aenc_out_frame.encoded_bytes;
          if (bytes > 50000) {

            curr_time = get_timestamp();
            ESP_LOGI(TAG, "audio bitrate: %.1f bps", 1000.0 * (bytes * 8.0 / (float)(curr_time - last_time)));
            last_time = curr_time;
            bytes = 0;
          }
        }
      }
      vTaskDelay(pdMS_TO_TICKS(5));

    } else {

      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_h264_enc.h"
#include "esp_camera.h"
#include "esp_timer.h"

#include "peer_connection.h"

extern PeerConnection *g_pc;
extern PeerConnectionState eState;
extern int get_timestamp();
static const char *TAG = "VIDEO";

esp_h264_enc_t handle = NULL;
esp_h264_enc_frame_t out_frame = { 0 };
esp_h264_raw_frame_t in_frame = { 0 };
esp_h264_enc_cfg_t cfg = DEFAULT_H264_ENCODER_CONFIG();

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 10
#define CAM_PIN_SIOD 40
#define CAM_PIN_SIOC 39
#define CAM_PIN_D7 48
#define CAM_PIN_D6 11
#define CAM_PIN_D5 12
#define CAM_PIN_D4 14
#define CAM_PIN_D3 16
#define CAM_PIN_D2 18
#define CAM_PIN_D1 17
#define CAM_PIN_D0 15
#define CAM_PIN_VSYNC 38
#define CAM_PIN_HREF 47
#define CAM_PIN_PCLK 13

static camera_config_t camera_config = {

  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,
  .pin_d7 = CAM_PIN_D7,
  .pin_d6 = CAM_PIN_D6,
  .pin_d5 = CAM_PIN_D5,
  .pin_d4 = CAM_PIN_D4,
  .pin_d3 = CAM_PIN_D3,
  .pin_d2 = CAM_PIN_D2,
  .pin_d1 = CAM_PIN_D1,
  .pin_d0 = CAM_PIN_D0,
  .pin_xclk = CAM_PIN_XCLK,
  .pin_pclk = CAM_PIN_PCLK,
  .pin_vsync = CAM_PIN_VSYNC,
  .pin_href = CAM_PIN_HREF,
  .pin_sccb_sda = CAM_PIN_SIOD,
  .pin_sccb_scl = CAM_PIN_SIOC,
  .pin_pwdn  = CAM_PIN_PWDN,
  .pin_reset = CAM_PIN_RESET,
  .xclk_freq_hz = 10000000,
  .pixel_format = PIXFORMAT_YUV422,
  .frame_size = FRAMESIZE_96X96,
  .jpeg_quality = 20,
  .fb_count = 3,
  .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

esp_err_t video_init(){

  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "Camera Init Failed");
      return err;
  }

  int one_image_size = 0;
  esp_h264_err_t ret = ESP_H264_ERR_OK;

  cfg.fps = 7;
  cfg.height = 96;
  cfg.width = 96;
  cfg.pic_type = ESP_H264_RAW_FMT_YUV422;
  one_image_size = cfg.height * cfg.width * 2.0;
  ESP_LOGI(TAG, "one_image_size %d", one_image_size);
  in_frame.raw_data.buffer = (uint8_t *)heap_caps_aligned_alloc(16, one_image_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

  if (in_frame.raw_data.buffer == NULL) {

    ESP_LOGE(TAG, "Failed to allocate memory for in_frame.raw_data.buffer");
    return ESP_FAIL;
  }

  ret = esp_h264_enc_open(&cfg, &handle);
  if (ret != ESP_H264_ERR_OK) {
    ESP_LOGE(TAG, "Open h264 encoder failed. ret %d", ret);
    return ESP_FAIL;

  } else {
    ESP_LOGI(TAG, "Open OK. handle: %p", handle);
  }

  return ESP_OK;
}

void video_task(void *pvParameters) {

  int frame_count = 0;
  static int fps = 0;
  static int64_t last_time;
  int64_t curr_time;
  int64_t start_time;
  int64_t end_time;

  camera_fb_t * fb = NULL;

  ESP_LOGI(TAG, "video task started");

  last_time = get_timestamp();

  for(;;) {

    if (eState == PEER_CONNECTION_COMPLETED) {

      fb = esp_camera_fb_get();

      if (!fb) {

        ESP_LOGE(TAG, "Camera capture failed");
      }

      memcpy(in_frame.raw_data.buffer, fb->buf, fb->len);

      in_frame.pts = frame_count * (1000 / cfg.fps);

      start_time = get_timestamp();

      if (esp_h264_enc_process(handle, &in_frame, &out_frame) != ESP_H264_ERR_OK) {

        ESP_LOGE(TAG, "h264 encode failed");

      } else {

          if (out_frame.frame_type_t == 0) {

            peer_connection_send_video(g_pc, out_frame.layer_data[0].buffer, out_frame.layer_data[0].len);
            peer_connection_send_video(g_pc, out_frame.layer_data[1].buffer, out_frame.layer_data[1].len);
          } else {

            peer_connection_send_video(g_pc, out_frame.layer_data[0].buffer, out_frame.layer_data[0].len);
          }

#if 0
      for (int layer = 0; layer < out_frame.layer_num; layer++) {
          printf("layer number: %d, type: %d, size: %d, ", layer, out_frame.frame_type_t, out_frame.layer_data[layer].len);
          for (int i = 0; i < 6; i++) {
            printf("%02x ", out_frame.layer_data[layer].buffer[i]);
          }
          printf("\r\n");
        }
#endif
        frame_count++;
        //ESP_LOGI(TAG, "Camera captured. size=%zu, timestamp=%llu", fb->len, fb->timestamp);
      }

      end_time = get_timestamp();
      //ESP_LOGI(TAG, "h264 encode time: %lld", end_time - start_time);
      fps++;

      if ((fps % 100) == 0) {

        curr_time = get_timestamp();
        ESP_LOGI(TAG, "Camera FPS=%.2f", 1000.0f / (float)(curr_time - last_time) * 100.0f);
        last_time = curr_time;
      }

      esp_camera_fb_return(fb);
      vTaskDelay(pdMS_TO_TICKS(90));

    } else {

      vTaskDelay(pdMS_TO_TICKS(1000));
    }

  }

}

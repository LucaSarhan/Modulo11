#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

const char *ssid = "Inteli.Iot";
const char *password = "@Intelix10T#";

#define CAMERA_MODEL_AI_THINKER

SemaphoreHandle_t semImageReady;
SemaphoreHandle_t semImageSent;
camera_fb_t* frameBuffer = NULL;

const char* serverUrl = "http://10.128.0.83:8000/upload";

#define CAMERA_MODEL_AI_THINKER
#define pwdnGpioNum     32
#define resetGpioNum    -1
#define xclkGpioNum      0
#define siodGpioNum     26
#define siocGpioNum     27
#define y9GpioNum       35
#define y8GpioNum       34
#define y7GpioNum       39
#define y6GpioNum       36
#define y5GpioNum       21
#define y4GpioNum       19
#define y3GpioNum       18
#define y2GpioNum        5
#define vsyncGpioNum    25
#define hrefGpioNum     23
#define pclkGpioNum     22

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = y2GpioNum;
  config.pin_d1 = y3GpioNum;
  config.pin_d2 = y4GpioNum;
  config.pin_d3 = y5GpioNum;
  config.pin_d4 = y6GpioNum;
  config.pin_d5 = y7GpioNum;
  config.pin_d6 = y8GpioNum;
  config.pin_d7 = y9GpioNum;
  config.pin_xclk = xclkGpioNum;
  config.pin_pclk = pclkGpioNum;
  config.pin_vsync = vsyncGpioNum;
  config.pin_href = hrefGpioNum;
  config.pin_sscb_sda = siodGpioNum;
  config.pin_sscb_scl = siocGpioNum;
  config.pin_pwdn = pwdnGpioNum;
  config.pin_reset = resetGpioNum;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  Serial.println("Camera init succeeded");

  semImageReady = xSemaphoreCreateBinary();
  semImageSent = xSemaphoreCreateBinary();

  xTaskCreatePinnedToCore(captureImageTask, "CaptureImageTask", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(sendImageTask, "SendImageTask", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(receiveDetectionTask, "ReceiveDetectionTask", 2048, NULL, 1, NULL, 0);
}

void loop() {
  delay(10000);
}

void captureImageTask(void *pvParameters) {
  for (;;) {
    frameBuffer = esp_camera_fb_get();
    if (frameBuffer) {
      xSemaphoreGive(semImageReady);
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

void sendImageTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(semImageReady, portMAX_DELAY) == pdTRUE) {
      sendImageToBackend();
      frameBuffer = NULL;
      xSemaphoreGive(semImageSent);
    }
  }
}

void receiveDetectionTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(semImageSent, portMAX_DELAY) == pdTRUE) {
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://10.128.0.83:8000/get_faces");
        int httpResponseCode = http.GET();
        
        if (httpResponseCode == 200) {
          String response = http.getString();
          Serial.println(response);
        } else {
          Serial.print("GET request failed with error: ");
          Serial.println(httpResponseCode);
        }
        
        http.end();
      } else {
        Serial.println("WiFi not connected");
      }
    }
  }
}

void sendImageToBackend() {
  if (WiFi.status() == WL_CONNECTED && frameBuffer != NULL) {
    HTTPClient http;
    http.begin(serverUrl);

    String boundary = "------------------------14737809831466499882746641449";
    String contentType = "multipart/form-data; boundary=" + boundary;
    http.addHeader("Content-Type", contentType);

    String head = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";

    size_t allLen = head.length() + frameBuffer->len + tail.length();
    uint8_t *allBuf = (uint8_t *)malloc(allLen);
    if (!allBuf) {
      Serial.println("Failed to allocate memory");
      return;
    }

    memcpy(allBuf, head.c_str(), head.length());
    memcpy(allBuf + head.length(), frameBuffer->buf, frameBuffer->len);
    memcpy(allBuf + head.length() + frameBuffer->len, tail.c_str(), tail.length());

    int httpResponseCode = http.POST(allBuf, allLen);
    free(allBuf);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(response);
    } else {
      Serial.print("HTTP error: ");
      Serial.println(httpResponseCode);
    }

    http.end();
    esp_camera_fb_return(frameBuffer);
  } else {
    Serial.println("WiFi not connected");
  }
}
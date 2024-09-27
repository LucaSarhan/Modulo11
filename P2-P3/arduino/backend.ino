#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

const char *ssid = "Inteli.Iot";
const char *password = "@Intelix10T#";

// URL of the backend where images will be uploaded
String serverUrl = "http://10.128.0.83:8000/upload"; // Backend IP

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
  
  // Connecting to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Wi-Fi connected");

  // Camera configuration
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Initialize the camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error initializing camera: 0x%x", err);
    return;
  }
  
  Serial.println("Camera initialized successfully!");
}

void loop() {
  // Capture a frame from the camera
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to capture image");
    return;
  }

  // Send the captured image to the backend
  sendImageToBackend(fb);

  // Release the frame buffer from the camera
  esp_camera_fb_return(fb);

  delay(5000); // Wait 5 seconds before capturing the next image
}

void sendImageToBackend(camera_fb_t * fb) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(serverUrl);

    // Boundary for multipart/form-data
    String boundary = "--------------------------133747188241686651551404";
    String contentType = "multipart/form-data; boundary=" + boundary;
    http.addHeader("Content-Type", contentType);

    // Construct the request body in multipart/form-data format
    String bodyStart = "--" + boundary + "\r\n" +
                       "Content-Disposition: form-data; name=\"file\"; filename=\"image.jpg\"\r\n" +
                       "Content-Type: image/jpeg\r\n\r\n";

    String bodyEnd = "\r\n--" + boundary + "--\r\n";

    // Calculate total length of the request body
    int totalLength = bodyStart.length() + fb->len + bodyEnd.length();

    // Allocate memory for the complete request body
    uint8_t *requestBody = (uint8_t *)malloc(totalLength);
    if (requestBody == NULL) {
      Serial.println("Error allocating memory for request body");
      return;
    }

    // Copy bodyStart, the image, and bodyEnd into the buffer
    memcpy(requestBody, bodyStart.c_str(), bodyStart.length());
    memcpy(requestBody + bodyStart.length(), fb->buf, fb->len);
    memcpy(requestBody + bodyStart.length() + fb->len, bodyEnd.c_str(), bodyEnd.length());

    // Send the POST request with the complete multipart body
    int httpResponseCode = http.POST(requestBody, totalLength);

    // Free the memory buffer
    free(requestBody);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode); // Server response code
      Serial.println(response);         // Server response message
    } else {
      Serial.printf("Error sending image: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    // End the HTTP connection
    http.end();
  } else {
    Serial.println("Wi-Fi connection lost");
  }
}

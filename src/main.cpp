#include <Arduino.h>
#include "esp_camera.h"
#include "camera_pins.h"
#include <WiFi.h>
#include <WebServer.h>

void configInitCamera();
void handleRoot();
void handleStream();

// WiFi credentials
const char* ssid = "VirusFactory";
const char* password = "Otto&Bobbi";

const int LED_BUILTIN = 33;
WebServer server(80);

void handleStream() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  unsigned long frameCount = 0;
  unsigned long startTime = millis();
  unsigned long lastReport = 0;

  while (true) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }

    frameCount++;
    
    // Report only every second instead of every frame
    if (millis() - lastReport > 1000) {
      float fps = frameCount / ((millis() - startTime) / 1000.0);
      Serial.printf("FPS: %.2f, Last frame size: %lu bytes\n", fps, fb->len);
      lastReport = millis();
    }

    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);
    server.sendContent((char *)fb->buf, fb->len);
    server.sendContent("\r\n");

    esp_camera_fb_return(fb);
    
    delay(200);

    if (!client.connected()) {
      break;
    }
  }
}

// Camera configuration function
void configInitCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Initial settings
  // FRAMESIZE_QQVGA (160x120)
  // FRAMESIZE_QVGA  (320x240)
  // FRAMESIZE_VGA   (640x480)
  // FRAMESIZE_SVGA  (800x600)
  // FRAMESIZE_XGA   (1024x768)
  // FRAMESIZE_SXGA  (1280x1024)
  // FRAMESIZE_UXGA  (1600x1200)
  config.frame_size = FRAMESIZE_VGA;  // Try a higher resolution
  config.jpeg_quality = 5;  // 0-63, lower means higher quality
  config.fb_count = 2;

  // Initialize the camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void handleRoot() {
  String html = "<html><head>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }";
  html += "img { max-width: 100%; height: auto; border: 1px solid #ccc; }";
  html += "</style>";
  html += "</head><body>";
  html += "<h1>ESP32-CAM Stream</h1>";
  html += "<img src='/stream' onerror=\"this.src=''; this.alt='Stream Failed - Please Refresh'\"/>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-CAM Starting...");
  
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Initialize the camera
  configInitCamera();

  // Adjust camera settings
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 0);     // -2 to 2
    // s->set_contrast(s, 0);       // -2 to 2
    // s->set_saturation(s, 0);     // -2 to 2
    // s->set_sharpness(s, 0);      // -2 to 2
    // s->set_denoise(s, 1);        // 0 to 1
    // s->set_quality(s, 10);       // 0 to 63
    // s->set_gainceiling(s, (gainceiling_t)6);  // 0 to 6
    // s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
    // s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    // s->set_wb_mode(s, 0);        // 0 to 4
    s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
    // s->set_aec2(s, 1);           // 0 = disable , 1 = enable
    // s->set_ae_level(s, 0);       // -2 to 2
    // s->set_aec_value(s, 300);    // 0 to 1200
  }

  Serial.println("Camera initialized!");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("/stream' to connect");

  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/stream", handleStream);  // Add this line
  server.begin();
}

void loop() {
  static int counter = 0;
  counter++;
  if (counter >= 2) {  // After ~5000 loop iterations
    counter = 0;
    Serial.printf("WiFi Signal Strength: %d dBm\n", WiFi.RSSI());
  }
  server.handleClient();
  //sleep for 1000ms
  delay(1000);
}

#include <Arduino.h>
#include "esp_camera.h"
#include "camera_pins.h"
#include <WiFi.h>
#include <WebServer.h>
#include <vector>

void configInitCamera();
void handleRoot();
void handleStream();
void cleanupClients();

// WiFi credentials
const char* ssid = "VirusFactory";
const char* password = "Otto&Bobbi";

const int LED_BUILTIN = 33;
const int MAX_CLIENTS = 10;  // Maximum number of simultaneous clients
WebServer server(80);

// Client management
struct ClientStream {
    WiFiClient client;
    unsigned long lastFrameTime;
    bool active;
};
std::vector<ClientStream> clients;

void cleanupClients() {
    for (auto it = clients.begin(); it != clients.end();) {
        if (!it->client.connected() || !it->active) {
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

void sendFrame(WiFiClient& client, camera_fb_t* fb) {
    String response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    client.write(response.c_str(), response.length());
    client.write((char *)fb->buf, fb->len);
    client.write("\r\n", 2);
}

void addCorsHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleStream() {
    if (clients.size() >= MAX_CLIENTS) {
        addCorsHeaders();
        server.send(503, "text/plain", "Server is at maximum capacity");
        return;
    }

    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Methods: GET\r\n";
    response += "Access-Control-Allow-Headers: Content-Type\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";

    client.write(response.c_str(), response.length());
    ClientStream newClient = {client, millis(), true};
    clients.push_back(newClient);
}

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
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;  // Slightly reduced quality for better performance
    config.fb_count = 2;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
}

void handleRoot() {
    addCorsHeaders();
    String html = "<html><head>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }";
    html += "img { max-width: 100%; height: auto; border: 1px solid #ccc; }";
    html += "</style>";
    html += "</head><body>";
    html += "<h1>ESP32-CAM Stream</h1>";
    html += "<img src='/stream' onerror=\"this.src=''; this.alt='Stream Failed - Please Refresh'\"/>";
    html += "<p>Active Clients: " + String(clients.size()) + "/" + String(MAX_CLIENTS) + "</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32-CAM Starting...");
    
    pinMode(LED_BUILTIN, OUTPUT);
    
    configInitCamera();

    sensor_t * s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 0);
        s->set_exposure_ctrl(s, 1);
    }

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi connected");
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("/stream' to connect");

    server.on("/stream", HTTP_OPTIONS, []() {
        addCorsHeaders();
        server.send(204);
    });

    server.on("/", handleRoot);
    server.on("/stream", handleStream);
    server.begin();
}

void loop() {
    server.handleClient();
    
    static unsigned long lastCleanup = 0;
    static unsigned long lastFrameTime = 0;
    static unsigned long frameCount = 0;
    static unsigned long startTime = millis();
    
    // Cleanup disconnected clients every second
    if (millis() - lastCleanup > 1000) {
        cleanupClients();
        lastCleanup = millis();
        
        // Print stats
        float fps = frameCount / ((millis() - startTime) / 1000.0);
        Serial.printf("Active clients: %d, FPS: %.2f\n", clients.size(), fps);
    }
    
    // Only capture and send frames if we have clients
    if (!clients.empty() && millis() - lastFrameTime > 50) {  // Max 20 FPS
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) {
            frameCount++;
            
            // Send frame to each connected client
            for (auto& client : clients) {
                if (client.client.connected()) {
                    sendFrame(client.client, fb);
                } else {
                    client.active = false;
                }
            }
            
            esp_camera_fb_return(fb);
            lastFrameTime = millis();
        }
    }
    
    // Small delay to prevent watchdog timer issues
    delay(1);
}
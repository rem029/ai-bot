#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char *ssid = "Ooredoo_94";
const char *password = "freebanana244";

WebServer server(80);

// Camera pin configuration for AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Add power control pin
#define ESP32_CAM_POWER_PIN 21  // ESP32-S3 GPIO to control ESP32-CAM power

bool cameraInitialized = false;
bool s3Connected = false;
unsigned long lastActivity = 0;
const unsigned long IDLE_TIMEOUT = 300000; // 5 minutes in milliseconds
bool cameraSleeping = false;

void sleepCamera() {
  if (cameraInitialized && !cameraSleeping) {
    // Put camera to sleep by disabling power
    gpio_set_level((gpio_num_t)PWDN_GPIO_NUM, 1);
    cameraSleeping = true;
    Serial.println("Camera entered sleep mode");
  }
}

void wakeCamera() {
  if (cameraSleeping) {
    // Wake camera by enabling power
    gpio_set_level((gpio_num_t)PWDN_GPIO_NUM, 0);
    delay(100); // Give camera time to wake up
    cameraSleeping = false;
    Serial.println("Camera woke up from sleep");
  }
}

void initializeCamera() {
  // Wake camera if sleeping
  wakeCamera();
  
  // Camera configuration
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
  
  // Optimized for serial communication
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("{\"ready\":false,\"error\":\"Camera init failed: 0x%x\"}\n", err);
    cameraInitialized = false;
  } else {
    Serial.println("{\"ready\":true,\"message\":\"ESP32-CAM ready for S3 communication\"}");
    cameraInitialized = true;
    lastActivity = millis(); // Reset activity timer
  }
}

void sendImageBase64() {
  // Wake camera if sleeping
  wakeCamera();
  lastActivity = millis(); // Update activity timer
  
  if (!cameraInitialized) {
    Serial.println("{\"success\":false,\"error\":\"Camera not initialized\"}");
    return;
  }

  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("{\"success\":false,\"error\":\"Capture failed\"}");
    return;
  }
  
  // Send image info first
  Serial.printf("{\"success\":true,\"size\":%d,\"width\":%d,\"height\":%d}\n", 
                fb->len, fb->width, fb->height);
  
  // Wait a moment for the response to be processed
  delay(100);
  
  // Convert to base64 and send in chunks to avoid memory issues
  const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  
  // Send base64 marker
  Serial.println("BASE64_START");
  
  // Process image in chunks to avoid memory overflow
  const size_t chunk_size = 3000; // Process 3000 bytes at a time
  for (size_t offset = 0; offset < fb->len; offset += chunk_size) {
    size_t remaining = fb->len - offset;
    size_t current_chunk = (remaining < chunk_size) ? remaining : chunk_size;
    
    String encoded = "";
    for (size_t i = 0; i < current_chunk; i += 3) {
      size_t pos = offset + i;
      uint32_t b = (fb->buf[pos] << 16) | 
                   ((pos + 1 < fb->len ? fb->buf[pos + 1] : 0) << 8) | 
                   (pos + 2 < fb->len ? fb->buf[pos + 2] : 0);
      
      encoded += chars[b >> 18];
      encoded += chars[(b >> 12) & 0x3F];
      encoded += (pos + 1 < fb->len) ? chars[(b >> 6) & 0x3F] : '=';
      encoded += (pos + 2 < fb->len) ? chars[b & 0x3F] : '=';
    }
    
    Serial.println(encoded);
    yield(); // Allow other processes
  }
  
  Serial.println("BASE64_END");
  esp_camera_fb_return(fb);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM Web Server</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; text-align: center; margin: 0px auto; padding-top: 30px; }
        .button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;
                 text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; }
        .button2 { background-color: #77878A; }
        img { width: auto; max-width: 100%; height: auto; }
    </style>
</head>
<body>
    <h1>ESP32-CAM Web Server</h1>
    <p><a href="/capture"><button class="button">Capture Photo</button></a></p>
    <p><a href="/stream"><button class="button button2">Start Stream</button></a></p>
    <p><img src="/stream" id="photo"></p>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleCapture() {
  // Wake camera if sleeping
  wakeCamera();
  lastActivity = millis(); // Update activity timer
  
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }
  
  server.sendHeader("Content-disposition", "inline; filename=capture.jpg");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void handleStream() {
  // Wake camera if sleeping
  wakeCamera();
  lastActivity = millis(); // Update activity timer
  
  WiFiClient client = server.client();
  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "multipart/x-mixed-replace; boundary=123456789000000000000987654321");
  
  while(client.connected()) {
    lastActivity = millis(); // Keep updating while streaming
    
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }
    
    client.print("--123456789000000000000987654321\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %u\r\n\r\n", fb->len);
    
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb);
    
    if (!client.connected()) break;
    delay(30);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1000);
  
  // Configure power control pin
  pinMode(ESP32_CAM_POWER_PIN, OUTPUT);
  digitalWrite(ESP32_CAM_POWER_PIN, HIGH); // Power on ESP32-CAM
  
  // Always initialize camera on startup
  delay(1000); // Give system time to stabilize
  initializeCamera();
  
  // Keep trying to initialize if failed
  if (!cameraInitialized) {
    for (int retry = 0; retry < 3 && !cameraInitialized; retry++) {
      delay(2000);
      Serial.printf("Retry camera init attempt %d...\n", retry + 1);
      initializeCamera();
    }
  }
  
  // Send ready signal for ESP32-S3
  Serial.println("ESP32_CAM_READY");
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  // Display IP address prominently
  String ipAddress = WiFi.localIP().toString();
  Serial.println("==========================================");
  Serial.println("        ESP32-CAM WEB SERVER READY       ");
  Serial.println("==========================================");
  Serial.print("WiFi connected! IP address: ");
  Serial.println(ipAddress);
  Serial.println("Web interface available at: http://" + ipAddress);
  Serial.println("Endpoints:");
  Serial.println("  - http://" + ipAddress + "/ (Main page)");
  Serial.println("  - http://" + ipAddress + "/capture (Photo)");
  Serial.println("  - http://" + ipAddress + "/stream (Live stream)");
  Serial.println("==========================================");
  Serial.println("Also ready for ESP32-S3 serial commands:");
  Serial.println("CAPTURE, STATUS, PING, REINIT, RESET");
  Serial.println("==========================================");
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/stream", handleStream);
  
  server.begin();
  Serial.println("Web server started successfully!");
}

void loop() {
  // Handle web server requests
  server.handleClient();
  
  // Check for idle timeout and sleep camera
  if (cameraInitialized && !cameraSleeping && 
      (millis() - lastActivity > IDLE_TIMEOUT)) {
    sleepCamera();
  }
  
  // Always listen for commands from ESP32-S3
  if (Serial.available() > 0) {
    lastActivity = millis(); // Update activity on serial communication
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "SLEEP") {
      sleepCamera();
      Serial.println("Camera manually put to sleep");
    }
    else if (command == "WAKE") {
      wakeCamera();
      Serial.println("Camera manually woken up");
    }
    else if (command == "S3_CONNECTED") {
      s3Connected = true;
      Serial.println("CAM_ACKNOWLEDGED");
    }
    else if (command == "CAPTURE") {
      sendImageBase64();
    }
    else if (command == "STATUS") {
      if (cameraInitialized) {
        Serial.println("{\"ready\":true,\"camera\":\"ok\",\"wifi\":\"" + WiFi.localIP().toString() + "\",\"s3_connected\":" + String(s3Connected ? "true" : "false") + "}");
      } else {
        Serial.println("{\"ready\":false,\"camera\":\"failed\",\"wifi\":\"" + WiFi.localIP().toString() + "\",\"s3_connected\":" + String(s3Connected ? "true" : "false") + "}");
      }
    }
    else if (command == "GET_IP") {
      Serial.println("IP:" + WiFi.localIP().toString());
    }
    else if (command == "REINIT") {
      initializeCamera();
    }
    else if (command == "PING") {
      Serial.println("PONG");
      s3Connected = true;
    }
    else if (command == "RESET") {
      ESP.restart();
    }
    else {
      Serial.printf("{\"error\":\"Unknown command: %s\"}\n", command.c_str());
    }
  }
  
  // Send periodic ready signals if S3 not connected (but don't reset activity timer)
  static unsigned long lastReadySignal = 0;
  if (!s3Connected && millis() - lastReadySignal > 10000) {
    Serial.println("ESP32_CAM_READY - IP: " + WiFi.localIP().toString() + 
                   (cameraSleeping ? " (Camera sleeping)" : " (Camera active)"));
    lastReadySignal = millis();
  }
  
  // Heartbeat every 30 seconds
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    Serial.println("HEARTBEAT - Web: http://" + WiFi.localIP().toString() + 
                   (cameraSleeping ? " (Camera sleeping)" : " (Camera active)"));
    lastHeartbeat = millis();
  }
}
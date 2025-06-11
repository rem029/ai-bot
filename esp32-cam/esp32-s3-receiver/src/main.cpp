#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// Serial communication pins for ESP32-CAM
#define CAM_RX_PIN 17  // Connect to ESP32-CAM TX
#define CAM_TX_PIN 18  // Connect to ESP32-CAM RX

// WiFi credentials from build flags
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WebServer server(80);
HardwareSerial CamSerial(1);

String lastCapturedImage = "";
bool camConnected = false;
unsigned long lastPing = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize serial communication with ESP32-CAM
  CamSerial.begin(115200, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
  
  Serial.println("ESP32-S3 Camera Controller Starting...");
  
  // Connect to WiFi (optional)
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/status", handleStatus);
  server.on("/image", handleImage);
  
  server.begin();
  Serial.println("ESP32-S3 Web server started");
  
  // Send connection signal to ESP32-CAM
  CamSerial.println("S3_CONNECTED");
  
  delay(1000);
}

void loop() {
  server.handleClient();
  
  // Read responses from ESP32-CAM
  if (CamSerial.available()) {
    String response = CamSerial.readStringUntil('\n');
    response.trim();
    handleCamResponse(response);
  }
  
  // Send periodic ping to ESP32-CAM
  if (millis() - lastPing > 30000) {
    CamSerial.println("PING");
    lastPing = millis();
  }
}

void handleCamResponse(String response) {
  Serial.println("CAM Response: " + response);
  
  if (response == "ESP32_CAM_READY") {
    Serial.println("ESP32-CAM is ready!");
    CamSerial.println("S3_CONNECTED");
  }
  else if (response == "CAM_ACKNOWLEDGED") {
    camConnected = true;
    Serial.println("ESP32-CAM connection established");
  }
  else if (response == "PONG") {
    camConnected = true;
  }
  else if (response.startsWith("BASE64_START")) {
    Serial.println("Starting to receive image...");
    lastCapturedImage = "";
  }
  else if (response.startsWith("BASE64_END")) {
    Serial.println("Image received completely!");
  }
  else if (response.startsWith("{")) {
    // JSON response
    Serial.println("JSON: " + response);
  }
  else if (lastCapturedImage.length() > 0 || response.length() > 50) {
    // This is likely base64 image data
    lastCapturedImage += response;
  }
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-S3 Camera Controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; text-align: center; margin: 0px auto; padding-top: 30px; }
        .button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;
                 text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; }
        .status { padding: 20px; margin: 20px; border-radius: 5px; }
        .connected { background-color: #d4edda; color: #155724; }
        .disconnected { background-color: #f8d7da; color: #721c24; }
    </style>
</head>
<body>
    <h1>ESP32-S3 Camera Controller</h1>
    <div class="status )rawliteral";
  
  html += camConnected ? "connected" : "disconnected";
  html += R"rawliteral(">
        Camera Status: )rawliteral";
  html += camConnected ? "Connected" : "Disconnected";
  html += R"rawliteral(
    </div>
    <p><a href="/capture"><button class="button">Capture Photo</button></a></p>
    <p><a href="/status"><button class="button">Get Status</button></a></p>
    <p><a href="/image"><button class="button">View Last Image</button></a></p>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleCapture() {
  if (!camConnected) {
    server.send(500, "text/plain", "ESP32-CAM not connected");
    return;
  }
  
  Serial.println("Requesting image capture...");
  CamSerial.println("CAPTURE");
  
  server.send(200, "text/plain", "Capture command sent to ESP32-CAM. Check /image for result.");
}

void handleStatus() {
  CamSerial.println("STATUS");
  server.send(200, "text/plain", "Status request sent. Check serial monitor for response.");
}

void handleImage() {
  if (lastCapturedImage.length() == 0) {
    server.send(404, "text/plain", "No image available. Capture one first.");
    return;
  }
  
  // Decode base64 and serve as JPEG
  server.send(200, "text/html", 
    "<html><body><h1>Last Captured Image</h1>"
    "<img src='data:image/jpeg;base64," + lastCapturedImage + "' style='max-width:100%'>"
    "</body></html>");
}

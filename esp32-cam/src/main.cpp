#include "wifi_manager.h"
#include "camera_manager.h"
#include "web_server.h"

bool s3Connected = false;

void handleSerialCommands() {
    if (Serial1.available() > 0) {
        cameraManager.updateActivity();
        
        String command = Serial1.readStringUntil('\n');
        command.trim();
        
        Serial.printf("Received command: %s\n", command.c_str());
        
        if (command.startsWith("QUALITY:")) {
            int quality = command.substring(8).toInt();
            cameraManager.setQuality(quality);
        }
        else if (command.startsWith("SETWIFI:")) {
            int commaIndex = command.indexOf(',');
            if (commaIndex > 8 && commaIndex < command.length() - 1) {
                String newSSID = command.substring(8, commaIndex);
                String newPassword = command.substring(commaIndex + 1);
                wifiManager.saveCredentials(newSSID, newPassword);
                Serial.println("{\"status\":\"wifi_updated_restarting\"}");
                delay(1000);
                ESP.restart();
            } else {
                Serial.println("{\"error\":\"Invalid SETWIFI format. Use: SETWIFI:SSID,PASSWORD\"}");
            }
        }
        else if (command == "CLEARWIFI") {
            wifiManager.clearCredentials();
            Serial.println("{\"status\":\"wifi_cleared_restarting\"}");
            delay(1000);
            ESP.restart();
        }
        else if (command == "GETWIFI") {
            Serial.println("{\"ssid\":\"" + wifiManager.getSSID() + "\",\"configured\":" + String(wifiManager.isConfigured() ? "true" : "false") + "}");
        }
        else if (command == "GETIMAGE") {
            cameraManager.sendImageBase64();
        }
        else if (command == "VERSION") {
            Serial.println("{\"version\":\"ESP32-CAM v1.0\",\"chip\":\"ESP32\"}");
        }
        else if (command == "SLEEP") {
            cameraManager.sleep();
        }
        else if (command == "WAKE") {
            cameraManager.wake();
        }
        else if (command == "S3_CONNECTED") {
            s3Connected = true;
            Serial.println("{\"status\":\"CAM_ACKNOWLEDGED\"}");
        }
        else if (command == "CAPTURE") {
            if (!cameraManager.isInitialized() || cameraManager.isSleeping()) {
                Serial.println("{\"success\":false,\"error\":\"Camera not ready\"}");
            } else {
                camera_fb_t * fb = cameraManager.captureImage();
                if(!fb) {
                    Serial.println("{\"success\":false,\"error\":\"Capture failed\"}");
                } else {
                    Serial.printf("{\"success\":true,\"size\":%d,\"width\":%d,\"height\":%d}\n", 
                                  fb->len, fb->width, fb->height);
                    esp_camera_fb_return(fb);
                }
            }
        }
        else if (command == "STATUS") {
            String status = "{\"ready\":" + String(cameraManager.isInitialized() && !cameraManager.isSleeping() ? "true" : "false") + 
                           ",\"camera\":\"" + String(cameraManager.isInitialized() ? "ok" : "failed") + "\"" +
                           ",\"wifi\":\"" + WiFi.localIP().toString() + "\"" +
                           ",\"ssid\":\"" + wifiManager.getSSID() + "\"" +
                           ",\"s3_connected\":" + String(s3Connected ? "true" : "false") + 
                           ",\"sleeping\":" + String(cameraManager.isSleeping() ? "true" : "false") + "}";
            Serial.println(status);
        }
        else if (command == "GET_IP") {
            Serial.println("{\"ip\":\"" + WiFi.localIP().toString() + "\"}");
        }
        else if (command == "REINIT") {
            if (cameraManager.isSleeping()) {
                cameraManager.wake();
            } else {
                cameraManager.initialize();
            }
        }
        else if (command == "PING") {
            Serial.println("PONG");
            s3Connected = true;
        }
        else if (command == "RESET") {
            Serial.println("{\"status\":\"resetting\"}");
            delay(100);
            ESP.restart();
        }
        else {
            Serial.printf("{\"error\":\"Unknown command: %s\"}\n", command.c_str());
        }
    }
}

void setup() {
    Serial.begin(115200);
 
    Serial.setTimeout(1000);
    
    Serial.println("==========================================");
    Serial.println("        ESP32-CAM STARTING UP            ");
    Serial.println("==========================================");
    
   Serial1.begin(9600, SERIAL_8N1, 14, 15);  // RX1 = GPIO4, TX1 = GPIO5

    // Initialize WiFi
    if (!wifiManager.begin()) {
        delay(2000);
        ESP.restart();
    }
    
    // Initialize camera
    delay(1000);
    cameraManager.initialize();
    
    // Start web server
    webServerManager.begin();
    
    String ipAddress = WiFi.localIP().toString();
    Serial.println("==========================================");
    Serial.println("        ESP32-CAM WEB SERVER READY       ");
    Serial.println("==========================================");
    Serial.println("Web interface available at: http://" + ipAddress);
    Serial.println("Endpoints:");
    Serial.println("  - http://" + ipAddress + "/ (Main page)");
    Serial.println("  - http://" + ipAddress + "/capture (Photo)");
    Serial.println("  - http://" + ipAddress + "/stream (Live stream)");
    Serial.println("  - http://" + ipAddress + "/image (Single image)");
    Serial.println("  - http://" + ipAddress + "/clearwifi (Clear WiFi)");
    Serial.println("==========================================");
}

void loop() {
    webServerManager.handleClient();
    cameraManager.checkIdleTimeout();
    handleSerialCommands();
    
    // Send periodic status if S3 not connected
    static unsigned long lastReadySignal = 0;
    if (!s3Connected && millis() - lastReadySignal > 10000) {
        Serial.println("{\"status\":\"ESP32_CAM_READY\",\"ip\":\"" + WiFi.localIP().toString() + 
                       "\",\"sleeping\":" + String(cameraManager.isSleeping() ? "true" : "false") + "}");
        lastReadySignal = millis();
    }
    
    // Heartbeat every 30 seconds
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 30000) {
        Serial.println("{\"heartbeat\":\"alive\",\"web\":\"http://" + WiFi.localIP().toString() + 
                       "\",\"sleeping\":" + String(cameraManager.isSleeping() ? "true" : "false") + "}");
        Serial1.println("PING");
        lastHeartbeat = millis();
    }
}
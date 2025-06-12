#include "web_server.h"

WebServerManager webServerManager;

WebServerManager::WebServerManager() : server(80) {
}

void WebServerManager::begin() {
    server.on("/", [this]() { handleRoot(); });
    server.on("/capture", [this]() { handleCapture(); });
    server.on("/stream", [this]() { handleStream(); });
    server.on("/image", [this]() { handleImage(); });
    server.on("/clearwifi", [this]() { handleClearWiFi(); });
    
    server.begin();
    Serial.println("Web server started successfully!");
}

void WebServerManager::handleClient() {
    server.handleClient();
}

void WebServerManager::handleRoot() {
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
        .clear-btn { background-color: #f44336; }
        .clear-btn:hover { background-color: #da190b; }
        img { width: auto; max-width: 100%; height: auto; }
        .status { background-color: #f0f0f0; padding: 10px; margin: 10px; }
        .stream-container { margin: 20px; }
        #stream { max-width: 100%; height: auto; border: 2px solid #ccc; }
    </style>
    <script>
        function startStream() {
            document.getElementById('stream').src = '/stream?' + new Date().getTime();
        }
        function stopStream() {
            document.getElementById('stream').src = '';
        }
    </script>
</head>
<body>
    <h1>ESP32-CAM Web Server</h1>
    <div class='status'>
        <p>WiFi SSID: )rawliteral" + wifiManager.getSSID() + R"rawliteral(</p>
        <p>IP Address: )rawliteral" + WiFi.localIP().toString() + R"rawliteral(</p>
        <p>Camera Status: )rawliteral" + String(cameraManager.isInitialized() && !cameraManager.isSleeping() ? "Ready" : "Sleeping/Error") + R"rawliteral(</p>
    </div>
    
    <div>
        <button class="button" onclick="location.href='/capture'">Capture Photo</button>
        <button class="button button2" onclick="startStream()">Start Stream</button>
        <button class="button" onclick="stopStream()">Stop Stream</button>
    </div>
    
    <div class="stream-container">
        <h3>Live Stream:</h3>
        <img id="stream" alt="Video stream will appear here">
    </div>
    
    <div>
        <button class="clear-btn" onclick="return confirm('Clear WiFi credentials and restart?'); location.href='/clearwifi'">Clear WiFi Settings</button>
    </div>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", html);
}

void WebServerManager::handleCapture() {
    cameraManager.updateActivity();
    
    camera_fb_t * fb = cameraManager.captureImage();
    if(!fb) {
        Serial.println("Camera capture failed");
        server.send(500, "text/plain", "Camera capture failed");
        return;
    }
    
    server.sendHeader("Content-disposition", "inline; filename=capture.jpg");
    server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void WebServerManager::handleStream() {
    cameraManager.wake();
    cameraManager.updateActivity();
    
    WiFiClient client = server.client();
    
    if (!client) {
        Serial.println("No client for streaming");
        return;
    }
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=123456789000000000000987654321");
    client.println("Connection: close");
    client.println();
    
    Serial.println("Starting video stream...");
    
    while(client.connected()) {
        cameraManager.updateActivity();
        
        camera_fb_t * fb = cameraManager.captureImage();
        if (!fb) {
            Serial.println("Camera capture failed during stream");
            break;
        }
        
        client.println("--123456789000000000000987654321");
        client.println("Content-Type: image/jpeg");
        client.printf("Content-Length: %u\r\n", fb->len);
        client.println();
        
        size_t written = client.write(fb->buf, fb->len);
        if (written != fb->len) {
            Serial.printf("Warning: Only wrote %d of %d bytes\n", written, fb->len);
        }
        
        client.println();
        esp_camera_fb_return(fb);
        
        if (!client.connected()) {
            Serial.println("Client disconnected from stream");
            break;
        }
        
        delay(50);
        yield();
    }
    
    Serial.println("Video stream ended");
}

void WebServerManager::handleImage() {
    cameraManager.updateActivity();
    
    camera_fb_t * fb = cameraManager.captureImage();
    if(!fb) {
        Serial.println("Camera capture failed");
        server.send(500, "text/plain", "Camera capture failed");
        return;
    }
    
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void WebServerManager::handleClearWiFi() {
    server.send(200, "text/html", 
        "<!DOCTYPE html><html><body><h1>WiFi Credentials Cleared</h1>"
        "<p>Device will restart in 3 seconds...</p>"
        "<script>setTimeout(function(){window.close();}, 3000);</script>"
        "</body></html>");
    
    delay(1000);
    wifiManager.clearCredentials();
    delay(2000);
    ESP.restart();
}

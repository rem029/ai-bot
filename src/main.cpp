#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "oled_display.h"     // Include the OLED display helper
#include "esp32cam_manager.h" // Include the ESP32-CAM manager
#include "wifi_manager.h"     // Include the WiFi manager

#define LED_PIN 48
#define NUM_PIXELS 1

// EEPROM settings (keeping for compatibility, but WiFi settings moved to WiFiManager)
#define EEPROM_SIZE 512

Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP32CamManager camManager;
WiFiManager wifiManager;

// Helper function to set color and delay
void setPixelColor(uint8_t r, uint8_t g, uint8_t b, int delayMs = 0)
{
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
    if (delayMs > 0)
    {
        delay(delayMs);
    }
}

// Camera status callback to handle status changes
void onCameraStatusChange(bool connected, bool statusChanged)
{
    if (statusChanged)
    {
        // Update display with status change
        displayMultiLine("Camera status:",
                         (connected ? "Connected" : "Disconnected"),
                         "Auto-detected", "");
        delay(2000); // Show status for 2 seconds

        // Flash LED to indicate status change
        if (connected)
        {
            // Green flash for connected
            for (int i = 0; i < 3; i++)
            {
                setPixelColor(0, 255, 0, 200);
                setPixelColor(0, 0, 0, 200);
            }
            setPixelColor(0, 255, 0); // Return to green
        }
        else
        {
            // Red flash for disconnected
            for (int i = 0; i < 3; i++)
            {
                setPixelColor(255, 0, 0, 200);
                setPixelColor(0, 0, 0, 200);
            }
            setPixelColor(255, 100, 0); // Orange for no camera
        }
    }
}

// WiFi status callback to handle status changes
void onWiFiStatusChange(bool connected, String ip, int rssi)
{
    if (connected)
    {
        displayMultiLine("Wi-Fi connected!",
                         "IP: " + ip,
                         "RSSI: " + String(rssi) + " dBm",
                         camManager.isCameraAvailable() ? "Camera: OK" : "Camera: FAIL");
    }
}

// Display callback for WiFi manager
void onWiFiDisplayUpdate(String line1, String line2, String line3, String line4)
{
    if (line2.isEmpty() && line3.isEmpty() && line4.isEmpty())
    {
        displayText(line1);
    }
    else
    {
        displayMultiLine(line1, line2, line3, line4);
    }
}

// Generate HTML page with optional image and WiFi config
String getHtmlPage(String message, bool showImage = false)
{
    String html = "<!DOCTYPE html><html>";
    html += "<head><title>ESP32 Camera Control</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }";
    html += "button { background-color: #4CAF50; color: white; padding: 10px 20px; ";
    html += "margin: 10px; border: none; border-radius: 4px; cursor: pointer; }";
    html += "button:hover { background-color: #45a049; }";
    html += ".ping-btn { background-color: #2196F3; }";
    html += ".ping-btn:hover { background-color: #0b7dda; }";
    html += ".clear-btn { background-color: #f44336; }";
    html += ".clear-btn:hover { background-color: #da190b; }";
    html += "img { margin-top: 20px; max-width: 100%; border: 1px solid #ddd; }";
    html += ".status { background-color: #f0f0f0; padding: 10px; margin: 10px; }";
    html += "</style></head><body>";
    html += "<h1>ESP32 Camera Control</h1>";
    html += "<div class='status'>";
    html += "<p>Camera Status: " + String(camManager.isCameraAvailable() ? "Connected" : "Disconnected") + "</p>";
    html += "<p>WiFi SSID: " + wifiManager.getSSID() + "</p>";
    html += "<p>" + message + "</p>";
    html += "</div>";
    html += "<div><button onclick=\"location.href='/LED_ON'\">Turn LED ON</button>";
    html += "<button class='ping-btn' onclick=\"location.href='/ping'\">PING Camera</button>";

    if (camManager.isCameraAvailable())
    {
        html += "<button onclick=\"location.href='/capture'\">Take Photo</button>";
        html += "<button onclick=\"location.href='/stream'\">Stream Camera</button>";
        html += "<button onclick=\"location.href='/test'\">Test Camera</button></div>";

        if (showImage && camManager.hasImage())
        {
            html += "<div><h2>Latest Image:</h2>";
            html += "<img src='data:image/jpeg;base64," + camManager.getLastImageBase64() + "' />";
            html += "</div>";
        }
    }
    else
    {
        html += "</div><p>Camera not available</p>";
    }

    html += "<div><button class='clear-btn' onclick=\"if(confirm('Clear WiFi credentials and restart?')) location.href='/clearwifi'\">Clear WiFi Settings</button></div>";
    html += "<br><a href='/'>Refresh Page</a>";
    html += "</body></html>";
    return html;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting ESP32-CAM Web Server...");
    pixels.begin();
    pixels.setBrightness(100);
    delay(10);
    pinMode(LED_BUILTIN, OUTPUT);

    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);

    // Initialize OLED display
    if (!initOLED())
    {

        Serial.println("OLED initialization failed!");
        while (true)
            ; // Halt if OLED fails
    }
    displayText("Initializing...");

    // Initialize camera manager
    camManager.setStatusCallback(onCameraStatusChange);
    displayText("Init Camera...");
    bool camInit = camManager.begin();
    if (camInit)
    {
        displayText("Camera ready!");
        Serial.println("Camera initialized!");
    }
    else
    {
        displayText("Camera failed!");
        Serial.println("Camera initialization failed!");
    }
    delay(2000);

    // Setup WiFi manager with callbacks
    wifiManager.setStatusCallback(onWiFiStatusChange);
    wifiManager.setDisplayCallback(onWiFiDisplayUpdate);

    // Initialize and connect WiFi (includes server setup)
    bool wifiConnected = wifiManager.begin(80);

    if (wifiConnected)
    {
        displayMultiLine("Server started!",
                         wifiManager.getLocalIP(),
                         camManager.isCameraAvailable() ? "Camera: OK" : "Camera: FAIL",
                         "Ready!");
    }
    else
    {
        displayText("No web server");
    }

    // Indicate server availability with green LED
    setPixelColor(0, 255, 0); // Solid Green
}

void loop()
{
    // Perform periodic camera availability check
    camManager.checkCameraAvailability();

    // Check for client connections
    WiFiClient client = wifiManager.getServer()->available();
    if (client)
    {
        Serial.println("New client connected!");
        displayText("Client connected!");

        // Flash LED to indicate client connection
        for (int i = 0; i < 3; i++)
        {
            setPixelColor(255, 255, 255, 100); // Flash White
            setPixelColor(0, 0, 0, 100);       // Turn Off
        }

        while (client.connected())
        {
            if (client.available())
            {
                String request = client.readStringUntil('\r');
                Serial.println("Request: " + request);
                client.flush();

                // Check for different request types
                if (request.indexOf("/LED_ON") != -1)
                {
                    Serial.println("Button pressed: Turning LED ON");
                    setPixelColor(255, 0, 0); // Solid Red

                    // Update OLED display
                    displayText("LED ON");

                    // Send web response
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println();
                    client.println(getHtmlPage("LED turned ON"));
                }
                else if (request.indexOf("/clearwifi") != -1)
                {
                    Serial.println("Clearing WiFi credentials and restarting");

                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println();
                    client.println("<!DOCTYPE html><html><body><h1>WiFi Credentials Cleared</h1><p>Device will restart in 3 seconds...</p><script>setTimeout(function(){window.close();}, 3000);</script></body></html>");

                    delay(1000);
                    client.stop();
                    wifiManager.clearCredentials();
                    delay(2000);
                    ESP.restart();
                }
                else if (request.indexOf("/test") != -1)
                {
                    Serial.println("Testing camera connection");
                    bool cameraStatus = camManager.checkCameraStatus();

                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println();

                    String testResult = "Camera: " + String(cameraStatus ? "OK" : "FAIL");
                    client.println(getHtmlPage(testResult));
                }
                else if (request.indexOf("/capture") != -1)
                {
                    Serial.println("Capture photo requested");

                    // Ensure camera is ready before capture
                    if (camManager.ensureCameraReady())
                    {
                        displayText("Taking photo...");
                        bool success = camManager.capturePhoto();

                        // Send web response
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println();

                        if (success)
                        {
                            displayText("Photo received!");
                            client.println(getHtmlPage("Photo captured successfully!", true));
                        }
                        else
                        {
                            displayText("Photo failed!");
                            client.println(getHtmlPage("Failed to capture photo"));
                        }
                    }
                    else
                    {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println();
                        client.println(getHtmlPage("Camera not available - check connection"));
                    }
                }
                else if (request.indexOf("/stream") != -1)
                {
                    Serial.println("Stream requested");
                    displayText("Streaming...");

                    // Send MJPEG Header
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
                    client.println();

                    while (client.connected())
                    {
                        camera_fb_t *fb = camManager.getFrame();
                        if (!fb)
                        {
                            Serial.println("Frame capture failed");
                            break;
                        }

                        // Send Frame Header and Data
                        client.println("--frame");
                        client.println("Content-Type: image/jpeg");
                        client.println("Content-Length: " + String(fb->len));
                        client.println();
                        client.write(fb->buf, fb->len);
                        client.println();

                        camManager.releaseFrame(fb);
                    }
                    displayText("Stream ended");
                }
                else if (request.indexOf("/ping") != -1)
                {
                    Serial.println("PING camera requested");

                    // Update OLED display
                    displayText("Pinging camera...");

                    // Send PING command
                    bool pingSuccess = camManager.ping();

                    Serial.println("PING response: " + String(pingSuccess ? "PONG" : "FAILED"));

                    // Update display with result
                    if (pingSuccess)
                    {
                        displayMultiLine("PING: SUCCESS", "Response: PONG", "", "");
                    }
                    else
                    {
                        displayMultiLine("PING: FAILED",
                                         "No response",
                                         "", "");
                    }

                    // Send web response
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println();

                    String result = "PING Result: " + String(pingSuccess ? "SUCCESS (PONG)" : "FAILED - No response");
                    client.println(getHtmlPage(result));
                }
                else
                {
                    // Default home page
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println();
                    client.println(getHtmlPage("ESP32 Camera Control Panel"));
                }
                break;
            }
        }
        client.stop();
        Serial.println("Client disconnected.");

        // Return to appropriate LED color based on camera status
        if (camManager.isCameraAvailable())
        {
            setPixelColor(0, 255, 0); // Green for camera available
        }
        else
        {
            setPixelColor(255, 100, 0); // Orange for no camera
        }

        // Update OLED display with status
        displayStatus("Connected",
                      camManager.isCameraAvailable() ? "Ready" : "Not Found",
                      "Waiting...");
    }
    else
    {
        // Breathing LED effect when idle (color depends on camera status)
        static int brightness = 0;
        static int direction = 1;
        brightness += direction * 2;
        if (brightness >= 50 || brightness <= 0)
            direction *= -1;

        if (camManager.isCameraAvailable())
        {
            setPixelColor(0, brightness, 0); // Green breathing for camera OK
        }
        else
        {
            setPixelColor(brightness, brightness / 2, 0); // Orange breathing for no camera
        }
        delay(50);
    }
}

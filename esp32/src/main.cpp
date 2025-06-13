#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "oled_display.h" // Include the OLED display helper
#include "esp32cam_manager.h" // Include the ESP32-CAM manager

#define LED_PIN 48
#define NUM_PIXELS 1

// EEPROM settings
#define EEPROM_SIZE 512
#define EEPROM_WIFI_FLAG 0   // 1 byte for flag
#define EEPROM_SSID_START 1  // 33 bytes for SSID (32 + null terminator)
#define EEPROM_PASS_START 34 // 65 bytes for password (64 + null terminator)

Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP32CamManager camManager;

// WiFi credentials variables
String wifi_ssid = "";
String wifi_password = "";
bool wifiConfigured = false;

// Create a Wi-Fi server
WiFiServer server(80);

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
void onCameraStatusChange(bool connected, bool statusChanged) {
    if (statusChanged) {
        // Update display with status change
        displayMultiLine("Camera status:",
                         (connected ? "Connected" : "Disconnected"),
                         "Auto-detected", "");
        delay(2000); // Show status for 2 seconds

        // Flash LED to indicate status change
        if (connected) {
            // Green flash for connected
            for (int i = 0; i < 3; i++) {
                setPixelColor(0, 255, 0, 200);
                setPixelColor(0, 0, 0, 200);
            }
            setPixelColor(0, 255, 0); // Return to green
        } else {
            // Red flash for disconnected
            for (int i = 0; i < 3; i++) {
                setPixelColor(255, 0, 0, 200);
                setPixelColor(0, 0, 0, 200);
            }
            setPixelColor(255, 100, 0); // Orange for no camera
        }
    }
}

// Function to save WiFi credentials to EEPROM
void saveWiFiCredentials(String ssid, String password)
{
    Serial.println("Saving WiFi credentials to EEPROM...");

    // Set flag to indicate WiFi is configured
    EEPROM.write(EEPROM_WIFI_FLAG, 1);

    // Save SSID
    for (int i = 0; i < 32; i++)
    {
        if (i < ssid.length())
        {
            EEPROM.write(EEPROM_SSID_START + i, ssid[i]);
        }
        else
        {
            EEPROM.write(EEPROM_SSID_START + i, 0);
        }
    }

    // Save Password
    for (int i = 0; i < 64; i++)
    {
        if (i < password.length())
        {
            EEPROM.write(EEPROM_PASS_START + i, password[i]);
        }
        else
        {
            EEPROM.write(EEPROM_PASS_START + i, 0);
        }
    }

    EEPROM.commit();
    Serial.println("WiFi credentials saved!");
}

// Function to load WiFi credentials from EEPROM
bool loadWiFiCredentials()
{
    Serial.println("Loading WiFi credentials from EEPROM...");

    // Check if WiFi is configured
    if (EEPROM.read(EEPROM_WIFI_FLAG) != 1)
    {
        Serial.println("No WiFi credentials found in EEPROM");
        return false;
    }

    // Load SSID
    wifi_ssid = "";
    for (int i = 0; i < 32; i++)
    {
        char c = EEPROM.read(EEPROM_SSID_START + i);
        if (c == 0)
            break;
        wifi_ssid += c;
    }

    // Load Password
    wifi_password = "";
    for (int i = 0; i < 64; i++)
    {
        char c = EEPROM.read(EEPROM_PASS_START + i);
        if (c == 0)
            break;
        wifi_password += c;
    }

    if (wifi_ssid.length() > 0)
    {
        Serial.println("WiFi credentials loaded from EEPROM");
        Serial.println("SSID: " + wifi_ssid);
        return true;
    }

    return false;
}

// Function to clear WiFi credentials
void clearWiFiCredentials()
{
    Serial.println("Clearing WiFi credentials...");
    EEPROM.write(EEPROM_WIFI_FLAG, 0);
    EEPROM.commit();
    wifi_ssid = "";
    wifi_password = "";
    wifiConfigured = false;
    Serial.println("WiFi credentials cleared!");
}

// Function to get WiFi credentials via Serial
void getWiFiCredentialsFromSerial()
{
    Serial.println("\n==========================================");
    Serial.println("         WiFi CONFIGURATION SETUP        ");
    Serial.println("==========================================");
    Serial.println("No WiFi credentials found or connection failed.");
    Serial.println("Please enter your WiFi credentials:");

    displayMultiLine("WiFi Setup", "Check Serial", "Monitor", "");

    // Get SSID
    Serial.print("Enter WiFi SSID: ");
    while (!Serial.available())
    {
        delay(100);
    }
    wifi_ssid = Serial.readString();
    wifi_ssid.trim();
    Serial.println(wifi_ssid);

    // Get Password
    Serial.print("Enter WiFi Password: ");
    while (!Serial.available())
    {
        delay(100);
    }
    wifi_password = Serial.readString();
    wifi_password.trim();
    Serial.println("Password entered (hidden)");

    // Confirm credentials
    Serial.println("\nCredentials entered:");
    Serial.println("SSID: " + wifi_ssid);
    Serial.println("Password: " + String(wifi_password.length()) + " characters");
    Serial.print("Save these credentials? (y/n): ");

    while (!Serial.available())
    {
        delay(100);
    }
    String confirm = Serial.readString();
    confirm.trim();
    confirm.toLowerCase();

    if (confirm == "y" || confirm == "yes")
    {
        saveWiFiCredentials(wifi_ssid, wifi_password);
        wifiConfigured = true;
        Serial.println("Credentials saved! Restarting...");
        displayCenteredText("Saved! Restarting...");
        delay(2000);
        ESP.restart();
    }
    else
    {
        Serial.println("Credentials not saved. Please restart to try again.");
        displayCenteredText("Not saved!");
        while (true)
        {
            delay(1000);
        }
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
    html += "<p>WiFi SSID: " + wifi_ssid + "</p>";
    html += "<p>" + message + "</p>";
    html += "</div>";
    html += "<div><button onclick=\"location.href='/LED_ON'\">Turn LED ON</button>";
    html += "<button class='ping-btn' onclick=\"location.href='/ping'\">PING Camera</button>";

    if (camManager.isCameraAvailable())
    {
        html += "<button onclick=\"location.href='/capture'\">Take Photo</button>";
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
    displayText("Testing serial...");
    bool serialTest = camManager.begin();
    if (serialTest)
    {
        displayText("Serial OK!");
        Serial.println("Serial communication working!");

        // Camera status is already checked in begin()
        if (camManager.isCameraAvailable())
        {
            displayText("Camera ready!");
        }
        else
        {
            displayText("Camera not ready!");
        }
    }
    else
    {
        displayText("Serial failed!");
        Serial.println("Serial communication failed!");
    }
    delay(2000);

    // Load WiFi credentials from EEPROM
    wifiConfigured = loadWiFiCredentials();

    if (!wifiConfigured)
    {
        getWiFiCredentialsFromSerial();
    }

    // Connect to Wi-Fi with debugging
    Serial.println("==========================================");
    Serial.println("         ESP32-S3 WiFi CONNECTION        ");
    Serial.println("==========================================");
    Serial.printf("WiFi SSID: %s\n", wifi_ssid.c_str());
    Serial.printf("WiFi Password: %s\n", wifi_password.length() > 0 ? "***configured***" : "***NOT SET***");
    Serial.printf("WiFi Password Length: %d characters\n", wifi_password.length());
    Serial.println("==========================================");

    displayMultiLine("WiFi Debug:",
                     "SSID: " + wifi_ssid,
                     "Pass: " + String(wifi_password.length() > 0 ? "SET" : "EMPTY"),
                     "");
    delay(2000);

    Serial.println("Connecting to Wi-Fi...");
    displayText("Connecting to Wi-Fi...");

    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30)
    {
        delay(500);
        Serial.print(".");
        wifiAttempts++;

        // Update display every 5 attempts
        if (wifiAttempts % 5 == 0)
        {
            displayText("Connecting..." + String(wifiAttempts) + "/30");
        }

        // Print detailed status every 5 attempts
        if (wifiAttempts % 5 == 0)
        {
            Serial.printf("\nWiFi Status: %d (attempt %d/30)\n", WiFi.status(), wifiAttempts);
            Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
        }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n==========================================");
        Serial.println("         WiFi CONNECTED SUCCESSFULLY     ");
        Serial.println("==========================================");
        Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("Subnet: %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("Channel: %d\n", WiFi.channel());
        Serial.println("==========================================");

        // Display Wi-Fi connection status
        displayMultiLine("Wi-Fi connected!",
                         "IP: " + WiFi.localIP().toString(),
                         "RSSI: " + String(WiFi.RSSI()) + " dBm",
                         camManager.isCameraAvailable() ? "Camera: OK" : "Camera: FAIL");
    }
    else
    {
        Serial.println("\n==========================================");
        Serial.println("         WiFi CONNECTION FAILED          ");
        Serial.println("==========================================");
        Serial.printf("Final Status: %d\n", WiFi.status());
        Serial.println("WiFi connection failed! Clearing credentials and restarting...");
        clearWiFiCredentials();
        displayCenteredText("WiFi Failed!");
        delay(2000);
        ESP.restart();
    }

    // Start the server only if WiFi connected
    if (WiFi.status() == WL_CONNECTED)
    {
        server.begin();
        Serial.println("Web server started!");
        Serial.printf("Access at: http://%s\n", WiFi.localIP().toString().c_str());
        displayMultiLine("Server started!",
                         WiFi.localIP().toString(),
                         camManager.isCameraAvailable() ? "Camera: OK" : "Camera: FAIL",
                         "Ready!");
    }
    else
    {
        Serial.println("Web server NOT started (no WiFi)");
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
    WiFiClient client = server.available();
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
                    clearWiFiCredentials();
                    delay(2000);
                    ESP.restart();
                }
                else if (request.indexOf("/test") != -1)
                {
                    Serial.println("Testing camera connection");
                    bool serialTest = camManager.testSerialCommunication();
                    bool cameraStatus = camManager.checkCameraStatus();

                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println();

                    String testResult = "Serial: " + String(serialTest ? "OK" : "FAIL") +
                                        ", Camera: " + String(cameraStatus ? "OK" : "FAIL");
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

#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 42
#define OLED_SCL 41
#define OLED_RESET -1 // Reset pin not used

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define LED_PIN 48
#define NUM_PIXELS 1

// Define camera serial connection
#define CAM_SERIAL_TX 17  // Connect to ESP32-CAM RX (VOR)
#define CAM_SERIAL_RX 18  // Connect to ESP32-CAM TX (VOT)

// EEPROM settings
#define EEPROM_SIZE 512
#define EEPROM_WIFI_FLAG 0    // 1 byte for flag
#define EEPROM_SSID_START 1   // 33 bytes for SSID (32 + null terminator)
#define EEPROM_PASS_START 34  // 65 bytes for password (64 + null terminator)

HardwareSerial CamSerial(1); // Use Serial1 for camera communication

Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// WiFi credentials variables
String wifi_ssid = "";
String wifi_password = "";
bool wifiConfigured = false;

// Create a Wi-Fi server
WiFiServer server(80);

// Camera status
bool cameraAvailable = false;
String lastImageBase64 = "";

// New variables for periodic checking:
unsigned long lastCameraCheck = 0;
const unsigned long CAMERA_CHECK_INTERVAL = 30000; // Check every 30 seconds
bool lastCameraState = false;

// Function to save WiFi credentials to EEPROM
void saveWiFiCredentials(String ssid, String password) {
    Serial.println("Saving WiFi credentials to EEPROM...");
    
    // Set flag to indicate WiFi is configured
    EEPROM.write(EEPROM_WIFI_FLAG, 1);
    
    // Save SSID
    for (int i = 0; i < 32; i++) {
        if (i < ssid.length()) {
            EEPROM.write(EEPROM_SSID_START + i, ssid[i]);
        } else {
            EEPROM.write(EEPROM_SSID_START + i, 0);
        }
    }
    
    // Save Password
    for (int i = 0; i < 64; i++) {
        if (i < password.length()) {
            EEPROM.write(EEPROM_PASS_START + i, password[i]);
        } else {
            EEPROM.write(EEPROM_PASS_START + i, 0);
        }
    }
    
    EEPROM.commit();
    Serial.println("WiFi credentials saved!");
}

// Function to load WiFi credentials from EEPROM
bool loadWiFiCredentials() {
    Serial.println("Loading WiFi credentials from EEPROM...");
    
    // Check if WiFi is configured
    if (EEPROM.read(EEPROM_WIFI_FLAG) != 1) {
        Serial.println("No WiFi credentials found in EEPROM");
        return false;
    }
    
    // Load SSID
    wifi_ssid = "";
    for (int i = 0; i < 32; i++) {
        char c = EEPROM.read(EEPROM_SSID_START + i);
        if (c == 0) break;
        wifi_ssid += c;
    }
    
    // Load Password
    wifi_password = "";
    for (int i = 0; i < 64; i++) {
        char c = EEPROM.read(EEPROM_PASS_START + i);
        if (c == 0) break;
        wifi_password += c;
    }
    
    if (wifi_ssid.length() > 0) {
        Serial.println("WiFi credentials loaded from EEPROM");
        Serial.println("SSID: " + wifi_ssid);
        return true;
    }
    
    return false;
}

// Function to clear WiFi credentials
void clearWiFiCredentials() {
    Serial.println("Clearing WiFi credentials...");
    EEPROM.write(EEPROM_WIFI_FLAG, 0);
    EEPROM.commit();
    wifi_ssid = "";
    wifi_password = "";
    wifiConfigured = false;
    Serial.println("WiFi credentials cleared!");
}

// Function to get WiFi credentials via Serial
void getWiFiCredentialsFromSerial() {
    Serial.println("\n==========================================");
    Serial.println("         WiFi CONFIGURATION SETUP        ");
    Serial.println("==========================================");
    Serial.println("No WiFi credentials found or connection failed.");
    Serial.println("Please enter your WiFi credentials:");
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Setup");
    display.println("Check Serial Monitor");
    display.display();
    
    // Get SSID
    Serial.print("Enter WiFi SSID: ");
    while (!Serial.available()) {
        delay(100);
    }
    wifi_ssid = Serial.readString();
    wifi_ssid.trim();
    Serial.println(wifi_ssid);
    
    // Get Password
    Serial.print("Enter WiFi Password: ");
    while (!Serial.available()) {
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
    
    while (!Serial.available()) {
        delay(100);
    }
    String confirm = Serial.readString();
    confirm.trim();
    confirm.toLowerCase();
    
    if (confirm == "y" || confirm == "yes") {
        saveWiFiCredentials(wifi_ssid, wifi_password);
        wifiConfigured = true;
        Serial.println("Credentials saved! Restarting...");
        delay(2000);
        ESP.restart();
    } else {
        Serial.println("Credentials not saved. Please restart to try again.");
        while (true) {
            delay(1000);
        }
    }
}

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

// Function to send command to ESP32-CAM and get response
String sendCameraCommand(String command, bool expectLargeData = false) {
    // Clear any pending data first
    while (CamSerial.available()) {
        CamSerial.read();
    }
    
    Serial.println("Sending command: '" + command + "'");
    CamSerial.println(command);
    
    if (!expectLargeData) {
        // Standard response handling with debugging
        unsigned long startTime = millis();
        String response = "";
        
        while (millis() - startTime < 5000) { // 5-second timeout
            if (CamSerial.available()) {
                char c = CamSerial.read();
                
                // Debug: show raw byte values for first few characters
                if (response.length() < 10) {
                    Serial.print("Raw byte: ");
                    Serial.print((int)c);
                    Serial.print(" (char: '");
                    if (c >= 32 && c <= 126) {
                        Serial.print(c);
                    } else {
                        Serial.print("?");
                    }
                    Serial.println("')");
                }
                
                response += c;
                
                if (c == '\n') {
                    break;
                }
            }
            yield();
        }
        
        response.trim();
        Serial.println("Final response: '" + response + "' (length: " + String(response.length()) + ")");
        return response;
    } else {
        // Handle chunked base64 data
        String fullResponse = "";
        unsigned long startTime = millis();
        bool readingBase64 = false;
        
        while (millis() - startTime < 30000) { // 30-second timeout for large data
            if (CamSerial.available()) {
                String line = CamSerial.readStringUntil('\n');
                line.trim();
                
                if (line == "BASE64_START") {
                    readingBase64 = true;
                    continue;
                } else if (line == "BASE64_END") {
                    break;
                } else if (readingBase64) {
                    fullResponse += line;
                } else if (!readingBase64 && line.length() > 0) {
                    // This might be the initial response
                    return line;
                }
                
                startTime = millis(); // Reset timeout on activity
            }
            yield();
        }
        
        return fullResponse;
    }
}

// Test basic serial communication
bool testCameraSerial() {
    Serial.println("Testing camera serial communication...");
    
    String response = sendCameraCommand("PING");
    
    Serial.println("Camera PING response: '" + response + "'");
    
    if (response == "PONG") {
        // Test version command
        response = sendCameraCommand("VERSION");
        Serial.println("Camera VERSION response: '" + response + "'");
        return true;
    }
    
    return false;
}

// Function to capture photo from ESP32-CAM
bool capturePhoto() {
    Serial.println("Requesting photo from ESP32-CAM");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Taking photo...");
    display.display();
    
    // First, send CAPTURE command to take the photo
    String response = sendCameraCommand("CAPTURE");
    
    Serial.println("CAPTURE response: " + response);
    
    // Parse the JSON response
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        Serial.println("Raw response: " + response);
        display.println("Camera error!");
        display.display();
        return false;
    }
    
    // Check if photo was successful
    bool success = doc["success"];
    if (success) {
        // Request the image data with chunked reading
        display.println("Getting image...");
        display.display();
        
        Serial.println("Requesting image data...");
        String imageData = sendCameraCommand("GETIMAGE", true); // Enable large data mode
        
        if (imageData.length() > 100) // Basic check for base64 data
        {
            lastImageBase64 = imageData;
            display.println("Photo received!");
            display.display();
            Serial.println("Image received, size: " + String(imageData.length()));
            return true;
        } else {
            Serial.println("Invalid image response, size: " + String(imageData.length()));
            display.println("Invalid image!");
            display.display();
            return false;
        }
    } else {
        String error = doc["error"];
        Serial.println("Capture failed: " + error);
        display.println("Photo failed!");
        display.display();
        return false;
    }
}

// Check if camera is responding
bool checkCamera() {
    String response = sendCameraCommand("STATUS");
    
    Serial.println("STATUS response: " + response);
    
    if (response.length() == 0) {
        Serial.println("No response from camera");
        return false;
    }
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        Serial.println("Failed to parse camera status: " + response);
        return false;
    }
    
    bool status = doc["ready"];
    return status;
}

// Generate HTML page with optional image and WiFi config
String getHtmlPage(String message, bool showImage = false) {
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
    html += "<p>Camera Status: " + String(cameraAvailable ? "Connected" : "Disconnected") + "</p>";
    html += "<p>WiFi SSID: " + wifi_ssid + "</p>";
    html += "<p>" + message + "</p>";
    html += "</div>";
    html += "<div><button onclick=\"location.href='/LED_ON'\">Turn LED ON</button>";
    html += "<button class='ping-btn' onclick=\"location.href='/ping'\">PING Camera</button>";
    
    if (cameraAvailable) {
        html += "<button onclick=\"location.href='/capture'\">Take Photo</button>";
        html += "<button onclick=\"location.href='/test'\">Test Camera</button></div>";
        
        if (showImage && lastImageBase64.length() > 0) {
            html += "<div><h2>Latest Image:</h2>";
            html += "<img src='data:image/jpeg;base64," + lastImageBase64 + "' />";
            html += "</div>";
        }
    } else {
        html += "</div><p>Camera not available</p>";
    }
    
    html += "<div><button class='clear-btn' onclick=\"if(confirm('Clear WiFi credentials and restart?')) location.href='/clearwifi'\">Clear WiFi Settings</button></div>";
    html += "<br><a href='/'>Refresh Page</a>";
    html += "</body></html>";
    return html;
}

// Periodic camera health check function
void checkCameraAvailability() {
    unsigned long currentTime = millis();
    
    // Check if it's time for periodic check
    if (currentTime - lastCameraCheck >= CAMERA_CHECK_INTERVAL) {
        Serial.println("Performing periodic camera health check...");
        
        // Quick ping test (faster than full status check)
        String response = sendCameraCommand("PING");
        bool newCameraState = (response == "PONG");
        
        // Check if camera state changed
        if (newCameraState != cameraAvailable) {
            Serial.println("Camera state changed: " + String(newCameraState ? "Connected" : "Disconnected"));
            cameraAvailable = newCameraState;
            
            // Update display with status change
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("Camera status:");
            display.println(cameraAvailable ? "Connected" : "Disconnected");
            display.println("Auto-detected");
            display.display();
            delay(2000); // Show status for 2 seconds
            
            // Flash LED to indicate status change
            if (cameraAvailable) {
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
        } else if (cameraAvailable) {
            // Camera still available, just log it
            Serial.println("Camera health check: OK");
        } else {
            // Camera still unavailable
            Serial.println("Camera health check: Still disconnected");
        }
        
        lastCameraCheck = currentTime;
    }
}

// Add this function to check camera before operations:
bool ensureCameraReady() {
    if (!cameraAvailable) {
        Serial.println("Camera not available, testing connection...");
        String response = sendCameraCommand("PING");
        cameraAvailable = (response == "PONG");
        
        if (cameraAvailable) {
            Serial.println("Camera reconnected!");
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("Camera reconnected!");
            display.display();
            delay(1000);
        }
    }
    return cameraAvailable;
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
    
    // Initialize camera serial connection
    CamSerial.begin(9600, SERIAL_8N1, CAM_SERIAL_RX, CAM_SERIAL_TX);
    delay(5000); // Give ESP32-CAM time to boot

    // Initialize OLED display
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(0x3C, OLED_RESET))
    {
        Serial.println("OLED initialization failed!");
        while (true)
            ;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Initializing...");
    display.display();

    // Test basic serial communication first
    display.println("Testing serial...");
    display.display();
    bool serialTest = testCameraSerial();
    if (serialTest) {
        display.println("Serial OK!");
        Serial.println("Serial communication working!");
        
        // Now check camera status
        display.println("Checking camera...");
        display.display();
        cameraAvailable = checkCamera();
        if (cameraAvailable) {
            display.println("Camera ready!");
        } else {
            display.println("Camera not ready!");
        }
    } else {
        display.println("Serial failed!");
        Serial.println("Serial communication failed!");
        cameraAvailable = false;
    }
    display.display();
    delay(2000);

    // Load WiFi credentials from EEPROM
    wifiConfigured = loadWiFiCredentials();
    
    if (!wifiConfigured) {
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
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Debug:");
    display.printf("SSID: %s\n", wifi_ssid.c_str());
    display.printf("Pass: %s\n", wifi_password.length() > 0 ? "SET" : "EMPTY");
    display.display();
    delay(2000);
    
    Serial.println("Connecting to Wi-Fi...");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting to Wi-Fi...");
    display.display();
    
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30)
    {
        delay(500);
        Serial.print(".");
        display.print(".");
        display.display();
        wifiAttempts++;
        
        // Print detailed status every 5 attempts
        if (wifiAttempts % 5 == 0) {
            Serial.printf("\nWiFi Status: %d (attempt %d/30)\n", WiFi.status(), wifiAttempts);
            Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
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
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Wi-Fi connected!");
        display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        display.printf("RSSI: %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println("\n==========================================");
        Serial.println("         WiFi CONNECTION FAILED          ");
        Serial.println("==========================================");
        Serial.printf("Final Status: %d\n", WiFi.status());
        Serial.println("WiFi connection failed! Clearing credentials and restarting...");
        clearWiFiCredentials();
        delay(2000);
        ESP.restart();
    }
    
    if (cameraAvailable) {
        display.println("Camera: OK");
    } else {
        display.println("Camera: FAIL");
    }
    display.display();

    // Start the server only if WiFi connected
    if (WiFi.status() == WL_CONNECTED) {
        server.begin();
        Serial.println("Web server started!");
        Serial.printf("Access at: http://%s\n", WiFi.localIP().toString().c_str());
        display.println("Server started!");
    } else {
        Serial.println("Web server NOT started (no WiFi)");
        display.println("No web server");
    }
    display.display();

    // Indicate server availability with green LED
    setPixelColor(0, 255, 0); // Solid Green
}

void loop()
{
    // Perform periodic camera availability check
    checkCameraAvailability();
    
    // Check for client connections
    WiFiClient client = server.available();
    if (client)
    {
        Serial.println("New client connected!");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Client connected!");
        display.display();

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
                    display.clearDisplay();
                    display.setCursor(0, 0);
                    display.println("LED ON");
                    display.display();
                    
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
                    bool serialTest = testCameraSerial();
                    cameraAvailable = checkCamera();
                    
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println();
                    
                    String testResult = "Serial: " + String(serialTest ? "OK" : "FAIL") + 
                                      ", Camera: " + String(cameraAvailable ? "OK" : "FAIL");
                    client.println(getHtmlPage(testResult));
                }
                else if (request.indexOf("/capture") != -1)
                {
                    Serial.println("Capture photo requested");
                    
                    // Ensure camera is ready before capture
                    if (ensureCameraReady()) {
                        bool success = capturePhoto();
                        
                        // Send web response
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println();
                        
                        if (success) {
                            client.println(getHtmlPage("Photo captured successfully!", true));
                        } else {
                            client.println(getHtmlPage("Failed to capture photo"));
                        }
                    } else {
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
                    display.clearDisplay();
                    display.setCursor(0, 0);
                    display.println("Pinging camera...");
                    display.display();
                    
                    // Send PING command
                    String response = sendCameraCommand("PING");
                    
                    Serial.println("PING response: '" + response + "'");
                    
                    // Update camera availability based on ping result
                    cameraAvailable = (response == "PONG");
                    
                    // Update display with result
                    display.clearDisplay();
                    display.setCursor(0, 0);
                    if (response == "PONG") {
                        display.println("PING: SUCCESS");
                        display.println("Response: PONG");
                    } else {
                        display.println("PING: FAILED");
                        display.println("Response:");
                        display.println(response.substring(0, 20)); // Show first 20 chars
                    }
                    display.display();
                    
                    // Send web response
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println();
                    
                    String result = "PING Result: " + (response == "PONG" ? "SUCCESS (PONG)" : "FAILED - Response: '" + response + "'");
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
        if (cameraAvailable) {
            setPixelColor(0, 255, 0); // Green for camera available
        } else {
            setPixelColor(255, 100, 0); // Orange for no camera
        }

        // Update OLED display
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Ready for clients");
        display.printf("Camera: %s\n", cameraAvailable ? "OK" : "FAIL");
        display.display();
    }
    else
    {
        // Breathing LED effect when idle (color depends on camera status)
        static int brightness = 0;
        static int direction = 1;
        brightness += direction * 2;
        if (brightness >= 50 || brightness <= 0) direction *= -1;
        
        if (cameraAvailable) {
            setPixelColor(0, brightness, 0); // Green breathing for camera OK
        } else {
            setPixelColor(brightness, brightness/2, 0); // Orange breathing for no camera
        }
        delay(50);
    }
}


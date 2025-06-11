#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

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

HardwareSerial CamSerial(1); // Use Serial1 for camera communication

Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Wi-Fi credentials
const char *ssid = "Ooredoo_94";
const char *password = "freebanana244";

// Create a Wi-Fi server
WiFiServer server(80);

// Camera status
bool cameraAvailable = false;
String lastImageBase64 = "";

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
    
    CamSerial.println(command);
    
    if (!expectLargeData) {
        // Standard response handling
        unsigned long startTime = millis();
        String response = "";
        
        while (millis() - startTime < 5000) { // 5-second timeout
            if (CamSerial.available()) {
                char c = CamSerial.read();
                response += c;
                
                if (c == '\n') {
                    break;
                }
            }
            yield();
        }
        
        response.trim();
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
        
        if (imageData.length() > 100) { // Basic check for base64 data
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

// Generate HTML page with optional image
String getHtmlPage(String message, bool showImage = false) {
    String html = "<!DOCTYPE html><html>";
    html += "<head><title>ESP32 Camera Control</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }";
    html += "button { background-color: #4CAF50; color: white; padding: 10px 20px; ";
    html += "margin: 10px; border: none; border-radius: 4px; cursor: pointer; }";
    html += "button:hover { background-color: #45a049; }";
    html += "img { margin-top: 20px; max-width: 100%; border: 1px solid #ddd; }";
    html += ".status { background-color: #f0f0f0; padding: 10px; margin: 10px; }";
    html += "</style></head><body>";
    html += "<h1>ESP32 Camera Control</h1>";
    html += "<div class='status'>";
    html += "<p>Camera Status: " + String(cameraAvailable ? "Connected" : "Disconnected") + "</p>";
    html += "<p>" + message + "</p>";
    html += "</div>";
    html += "<div><button onclick=\"location.href='/LED_ON'\">Turn LED ON</button>";
    
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

    // Initialize camera serial connection
    CamSerial.begin(115200, SERIAL_8N1, CAM_SERIAL_RX, CAM_SERIAL_TX);
    delay(1000); // Give ESP32-CAM time to boot

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

    // Connect to Wi-Fi
    Serial.println("Connecting to Wi-Fi...");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting to Wi-Fi...");
    display.display();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        display.print(".");
        display.display();
    }
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Display Wi-Fi connection status
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Wi-Fi connected!");
    display.print("IP: ");
    display.println(WiFi.localIP());
    if (cameraAvailable) {
        display.println("Camera: OK");
    } else {
        display.println("Camera: FAIL");
    }
    display.display();

    // Start the server
    server.begin();
    Serial.println("Server started!");
    display.println("Server started!");
    display.display();

    // Indicate server availability with green LED
    setPixelColor(0, 255, 0); // Solid Green
}

void loop()
{
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
                else if (request.indexOf("/capture") != -1 && cameraAvailable)
                {
                    Serial.println("Capture photo requested");
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

        // Return to green LED after client disconnects
        setPixelColor(0, 255, 0); // Solid Green

        // Update OLED display
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Ready for clients");
        display.display();
    }
    else
    {
        // Breathing LED effect when idle
        static int brightness = 0;
        static int direction = 1;
        brightness += direction * 2;
        if (brightness >= 50 || brightness <= 0) direction *= -1;
        setPixelColor(0, brightness, 0);
        delay(50);
    }
}


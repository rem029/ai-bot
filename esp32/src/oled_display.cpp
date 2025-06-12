#include "oled_display.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 42  // Adjust based on your wiring
#define OLED_SCL 41  // Adjust based on your wiring
#define OLED_RESET -1

// Create the display object
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool initOLED() {
    Wire.begin(OLED_SDA, OLED_SCL); // Initialize I2C with your pins
    
    if (!display.begin(0x3C)) {
        return false; // Initialization failed
    }
    
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.display();
    return true;
}

void clearDisplay() {
    display.clearDisplay();
    display.display();
}

void displayText(const String& text) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.print(text);
    display.display();
}

void displayMultiLine(const String& line1, const String& line2, const String& line3, const String& line4) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Line 1
    if (line1.length() > 0) {
        display.setCursor(0, 0);
        display.print(line1);
    }
    
    // Line 2
    if (line2.length() > 0) {
        display.setCursor(0, 12);
        display.print(line2);
    }
    
    // Line 3
    if (line3.length() > 0) {
        display.setCursor(0, 24);
        display.print(line3);
    }
    
    // Line 4
    if (line4.length() > 0) {
        display.setCursor(0, 36);
        display.print(line4);
    }
    
    display.display();
}

void displayStatus(const String& wifiStatus, const String& cameraStatus, const String& connectionStatus) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // WiFi Status
    display.setCursor(0, 0);
    display.print("WiFi: " + wifiStatus);
    
    // Camera Status
    display.setCursor(0, 12);
    display.print("Camera: " + cameraStatus);
    
    // Connection Status (if provided)
    if (connectionStatus.length() > 0) {
        display.setCursor(0, 24);
        display.print("Conn: " + connectionStatus);
    }
    
    // Add timestamp or uptime
    display.setCursor(0, 48);
    display.print("Time: " + String(millis() / 1000) + "s");
    
    display.display();
}

void displayCenteredText(const String& text) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Calculate centered position
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;
    
    display.setCursor(x, y);
    display.print(text);
    display.display();
}

void updateDisplay() {
    display.display();
}

void setTextSize(int size) {
    display.setTextSize(size);
}

void setCursor(int x, int y) {
    display.setCursor(x, y);
}

void printText(const String& text) {
    display.print(text);
}

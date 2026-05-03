#include "oled_display.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 40 // Adjust based on your wiring
#define OLED_SCL 39 // Adjust based on your wiring
#define OLED_RESET -1

// Assets from Lopaka
static const unsigned char PROGMEM image_direction_forward_bits[] = {0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x0f, 0xff, 0xff, 0x00, 0x0f, 0xff, 0xff, 0x00, 0x0f, 0xff, 0xff, 0x00, 0x0f, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xf0};
static const unsigned char PROGMEM image_wifi_bits[] = {0x12, 0x00, 0x4c, 0x80, 0xa1, 0x40, 0x52, 0x80, 0x21, 0x00, 0x12, 0x00, 0x0c, 0x00, 0x00, 0x00};

// Create the display object
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool initOLED()
{
    Wire.begin(OLED_SDA, OLED_SCL); // Initialize I2C with your pins

    if (!display.begin(0x3C))
    {
        return false; // Initialization failed
    }

    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.display();
    return true;
}

void drawIntro(const String &status)
{
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextWrap(false);
    display.setFont(&Org_01);

    display.setTextSize(3);
    display.setCursor(2, 22);
    display.print("Bot");

    display.setTextSize(1);
    display.setCursor(2, 51);
    display.print(status);

    display.drawLine(0, 44, 126, 44, SH110X_WHITE);

    display.setTextSize(2);
    display.setCursor(2, 36);
    display.print("Wanderer");

    display.display();
}

void drawMain(const String &ip, const String &status, const String &direction, const String &measure)
{
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextWrap(false);
    display.setFont(&Org_01);
    display.setTextSize(1);

    // line
    display.drawLine(2, 12, 125, 12, SH110X_WHITE);

    // ip_text
    display.setCursor(2, 8);
    display.print(ip);

    // status_text
    display.setCursor(2, 61);
    display.print(status);

    // line
    display.drawLine(2, 54, 125, 54, SH110X_WHITE);

    // direction_forward
    display.drawBitmap(17, 24, image_direction_forward_bits, 28, 16, SH110X_WHITE);

    // direction_text
    display.setCursor(24, 49);
    display.print(direction);

    // measure_text
    display.setCursor(71, 33);
    display.print(measure);

    // wifi
    display.drawBitmap(116, 3, image_wifi_bits, 10, 8, SH110X_WHITE);

    // line
    display.drawLine(64, 13, 64, 53, SH110X_WHITE);

    display.display();
}

void clearDisplay()
{
    display.setFont(NULL); // Reset to default font for other functions
    display.clearDisplay();
    display.display();
}

void displayText(const String &text)
{
    display.setFont(NULL);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.print(text);
    display.display();
}

void displayMultiLine(const String &line1, const String &line2, const String &line3, const String &line4)
{
    display.setFont(NULL);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    // Line 1
    if (line1.length() > 0)
    {
        display.setCursor(0, 0);
        display.print(line1);
    }
    // ... rest of the file ...

    // Line 2
    if (line2.length() > 0)
    {
        display.setCursor(0, 12);
        display.print(line2);
    }

    // Line 3
    if (line3.length() > 0)
    {
        display.setCursor(0, 24);
        display.print(line3);
    }

    // Line 4
    if (line4.length() > 0)
    {
        display.setCursor(0, 36);
        display.print(line4);
    }

    display.display();
}

void displayStatus(const String &wifiStatus, const String &cameraStatus, const String &connectionStatus)
{
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
    if (connectionStatus.length() > 0)
    {
        display.setCursor(0, 24);
        display.print("Conn: " + connectionStatus);
    }

    // Add timestamp or uptime
    display.setCursor(0, 48);
    display.print("Time: " + String(millis() / 1000) + "s");

    display.display();
}

void displayCenteredText(const String &text)
{
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

void updateDisplay()
{
    display.display();
}

void setTextSize(int size)
{
    display.setTextSize(size);
}

void setCursor(int x, int y)
{
    display.setCursor(x, y);
}

void printText(const String &text)
{
    display.print(text);
}

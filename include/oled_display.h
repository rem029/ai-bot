#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Adafruit_SH110X.h>
#include <Wire.h>
#include <Fonts/Org_01.h>

// Function declarations for OLED display management
bool initOLED();
void clearDisplay();
void displayText(const String &text);
void displayMultiLine(const String &line1, const String &line2 = "", const String &line3 = "", const String &line4 = "");
void displayStatus(const String &wifiStatus, const String &cameraStatus, const String &connectionStatus = "");

// New Lopaka UI Functions
void drawIntro(const String &status);
void drawMain(const String &ip, const String &status, const String &direction, const String &measure);

void displayCenteredText(const String &text);
void updateDisplay();

// Advanced display functions
void setTextSize(int size);
void setCursor(int x, int y);
void printText(const String &text);

#endif

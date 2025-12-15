#ifndef AI_BOT_MANAGER_H
#define AI_BOT_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include "esp32cam_manager.h"
#include "wifi_manager.h"

class AIBotManager
{
public:
    AIBotManager();
    void begin(ESP32CamManager *cam, WiFiManager *wifi);
    void loop();

    void setApiUrl(String url);
    String getApiUrl();
    bool testConnection();

    void startBot();
    void stopBot();
    bool isBotRunning();
    String getLastBotStatus();

private:
    ESP32CamManager *camManager;
    WiFiManager *wifiManager;

    String apiUrl;
    bool botRunning;
    unsigned long lastRequestTime;
    String lastBotStatus;

    // EEPROM Configuration
    // WiFi Manager uses first ~100 bytes. We start at 200 to be safe.
    const int EEPROM_API_URL_ADDR = 200;
    const int EEPROM_API_URL_SIZE = 150; // Max URL length

    void loadApiUrl();
    void saveApiUrlToEEPROM(String url);
    void sendBotRequest();
    String getHealthUrl();
};

#endif

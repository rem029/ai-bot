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

    void setApiConfig(String baseUrl, String messageRoute, String healthRoute);
    String getApiBaseUrl();
    String getApiMessageRoute();
    String getApiHealthRoute();
    bool testConnection();

    void startBot();
    void stopBot();
    bool isBotRunning();
    String getLastBotStatus();

private:
    ESP32CamManager *camManager;
    WiFiManager *wifiManager;

    String apiBaseUrl;
    String apiMessageRoute;
    String apiHealthRoute;
    String sessionId;

    bool botRunning;
    unsigned long lastRequestTime;
    String lastBotStatus;

    // EEPROM Configuration
    // WiFi Manager uses first ~100 bytes. We start at 200 to be safe.
    const int EEPROM_BASE_URL_ADDR = 200;
    const int EEPROM_BASE_URL_SIZE = 100;
    const int EEPROM_MSG_ROUTE_ADDR = 300;
    const int EEPROM_MSG_ROUTE_SIZE = 50;
    const int EEPROM_HEALTH_ROUTE_ADDR = 350;
    const int EEPROM_HEALTH_ROUTE_SIZE = 50;

    void loadApiConfig();
    void saveApiConfigToEEPROM(String baseUrl, String messageRoute, String healthRoute);
    void sendBotRequest();
    String getHealthUrl();
    String getMessageUrl();
};

#endif

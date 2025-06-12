#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <EEPROM.h>

// EEPROM settings for WiFi credentials
#define EEPROM_SIZE 512
#define EEPROM_WIFI_FLAG 0    // 1 byte for flag
#define EEPROM_SSID_START 1   // 33 bytes for SSID (32 + null terminator)
#define EEPROM_PASS_START 34  // 65 bytes for password (64 + null terminator)

class WiFiManager {
public:
    WiFiManager();
    bool begin();
    void saveCredentials(String ssid, String password);
    bool loadCredentials();
    void clearCredentials();
    void getCredentialsFromSerial();
    String getSSID() { return wifi_ssid; }
    String getPassword() { return wifi_password; }
    bool isConfigured() { return wifiConfigured; }
    
private:
    String wifi_ssid;
    String wifi_password;
    bool wifiConfigured;
};

extern WiFiManager wifiManager;

#endif

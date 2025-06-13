#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <EEPROM.h>
#include <Arduino.h>

// EEPROM settings for WiFi credentials
#define EEPROM_WIFI_FLAG 0   // 1 byte for flag
#define EEPROM_SSID_START 1  // 33 bytes for SSID (32 + null terminator)
#define EEPROM_PASS_START 34 // 65 bytes for password (64 + null terminator)

class WiFiManager {
private:
    String wifi_ssid;
    String wifi_password;
    bool wifiConfigured;
    WiFiServer* server;
    
    // Private helper methods
    void saveCredentialsToEEPROM(String ssid, String password);
    bool loadCredentialsFromEEPROM();
    void getCredentialsFromSerial();
    
public:
    // Constructor
    WiFiManager();
    
    // Initialization and connection
    bool begin(int serverPort = 80);
    bool connect();
    void disconnect();
    
    // Credential management
    void clearCredentials();
    bool areCredentialsConfigured();
    String getSSID();
    String getPasswordMasked(); // Returns masked password for display
    int getPasswordLength();
    
    // Connection status
    bool isConnected();
    String getLocalIP();
    String getGatewayIP();
    String getSubnetMask();
    String getDNSIP();
    int getRSSI();
    int getChannel();
    String getMACAddress();
    wl_status_t getStatus();
    
    // Server management
    WiFiServer* getServer();
    bool startServer();
    void stopServer();
    
    // Status display callback function type
    typedef void (*StatusCallback)(bool connected, String ip, int rssi);
    void setStatusCallback(StatusCallback callback);
    
    // Display interface callback function type  
    typedef void (*DisplayCallback)(String line1, String line2, String line3, String line4);
    void setDisplayCallback(DisplayCallback callback);
    
private:
    StatusCallback statusCallback;
    DisplayCallback displayCallback;
    
    // Helper methods for display
    void displayText(String text);
    void displayMultiLine(String line1, String line2, String line3, String line4);
    void displayCenteredText(String text);
};

#endif // WIFI_MANAGER_H

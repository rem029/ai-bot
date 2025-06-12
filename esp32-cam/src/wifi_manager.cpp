#include "wifi_manager.h"

WiFiManager wifiManager;

WiFiManager::WiFiManager() {
    wifi_ssid = "";
    wifi_password = "";
    wifiConfigured = false;
}

bool WiFiManager::begin() {
    EEPROM.begin(EEPROM_SIZE);
    wifiConfigured = loadCredentials();
    
    if (!wifiConfigured) {
        getCredentialsFromSerial();
    }
    
    // Connect to WiFi
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    Serial.print("Connecting to WiFi");
    
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
        delay(500);
        Serial.print(".");
        wifiAttempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi connected successfully!");
        String ipAddress = WiFi.localIP().toString();
        Serial.println("IP address: " + ipAddress);
        return true;
    } else {
        Serial.println("\nERROR: Failed to connect to WiFi!");
        clearCredentials();
        return false;
    }
}

void WiFiManager::saveCredentials(String ssid, String password) {
    Serial.println("{\"status\":\"saving_wifi_credentials\"}");
    
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
    wifi_ssid = ssid;
    wifi_password = password;
    wifiConfigured = true;
    Serial.println("{\"status\":\"wifi_credentials_saved\"}");
}

bool WiFiManager::loadCredentials() {
    Serial.println("{\"status\":\"loading_wifi_credentials\"}");
    
    if (EEPROM.read(EEPROM_WIFI_FLAG) != 1) {
        Serial.println("{\"status\":\"no_wifi_credentials_found\"}");
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
        Serial.println("{\"status\":\"wifi_credentials_loaded\",\"ssid\":\"" + wifi_ssid + "\"}");
        return true;
    }
    
    return false;
}

void WiFiManager::clearCredentials() {
    Serial.println("{\"status\":\"clearing_wifi_credentials\"}");
    EEPROM.write(EEPROM_WIFI_FLAG, 0);
    EEPROM.commit();
    wifi_ssid = "";
    wifi_password = "";
    wifiConfigured = false;
    Serial.println("{\"status\":\"wifi_credentials_cleared\"}");
}

void WiFiManager::getCredentialsFromSerial() {
    Serial.println("{\"status\":\"wifi_setup_required\"}");
    Serial.println("==========================================");
    Serial.println("         ESP32-CAM WiFi SETUP            ");
    Serial.println("==========================================");
    Serial.println("No WiFi credentials found or connection failed.");
    Serial.println("Please enter your WiFi credentials:");
    
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
        saveCredentials(wifi_ssid, wifi_password);
        Serial.println("{\"status\":\"credentials_saved_restarting\"}");
        delay(2000);
        ESP.restart();
    } else {
        Serial.println("{\"status\":\"credentials_not_saved\"}");
        Serial.println("Credentials not saved. Please restart to try again.");
        while (true) {
            delay(1000);
        }
    }
}

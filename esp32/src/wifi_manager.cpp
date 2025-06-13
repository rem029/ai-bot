#include "wifi_manager.h"

WiFiManager::WiFiManager() : wifiConfigured(false), server(nullptr), statusCallback(nullptr), displayCallback(nullptr) {
    wifi_ssid = "";
    wifi_password = "";
}

bool WiFiManager::begin(int serverPort) {
    // Load WiFi credentials from EEPROM
    wifiConfigured = loadCredentialsFromEEPROM();
    
    if (!wifiConfigured) {
        getCredentialsFromSerial();
    }
    
    // Create server instance
    server = new WiFiServer(serverPort);
    
    // Attempt to connect
    bool connected = connect();
    
    if (connected) {
        startServer();
    }
    
    return connected;
}

bool WiFiManager::connect() {
    // Connect to Wi-Fi with debugging
    Serial.println("==========================================");
    Serial.println("         ESP32-S3 WiFi CONNECTION        ");
    Serial.println("==========================================");
    Serial.printf("WiFi SSID: %s\n", wifi_ssid.c_str());
    Serial.printf("WiFi Password: %s\n", wifi_password.length() > 0 ? "***configured***" : "***NOT SET***");
    Serial.printf("WiFi Password Length: %d characters\n", wifi_password.length());
    Serial.println("==========================================");

    displayMultiLine("WiFi Debug:",
                     "SSID: " + wifi_ssid,
                     "Pass: " + String(wifi_password.length() > 0 ? "SET" : "EMPTY"),
                     "");
    delay(2000);

    Serial.println("Connecting to Wi-Fi...");
    displayText("Connecting to Wi-Fi...");

    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
        delay(500);
        Serial.print(".");
        wifiAttempts++;

        // Update display every 5 attempts
        if (wifiAttempts % 5 == 0) {
            displayText("Connecting..." + String(wifiAttempts) + "/30");
        }

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

        // Call status callback if registered
        if (statusCallback) {
            statusCallback(true, WiFi.localIP().toString(), WiFi.RSSI());
        }

        return true;
    } else {
        Serial.println("\n==========================================");
        Serial.println("         WiFi CONNECTION FAILED          ");
        Serial.println("==========================================");
        Serial.printf("Final Status: %d\n", WiFi.status());
        Serial.println("WiFi connection failed! Clearing credentials and restarting...");
        clearCredentials();
        displayCenteredText("WiFi Failed!");
        delay(2000);
        ESP.restart();
        return false;
    }
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
    if (server) {
        server->stop();
    }
}

void WiFiManager::saveCredentialsToEEPROM(String ssid, String password) {
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

bool WiFiManager::loadCredentialsFromEEPROM() {
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

void WiFiManager::clearCredentials() {
    Serial.println("Clearing WiFi credentials...");
    EEPROM.write(EEPROM_WIFI_FLAG, 0);
    EEPROM.commit();
    wifi_ssid = "";
    wifi_password = "";
    wifiConfigured = false;
    Serial.println("WiFi credentials cleared!");
}

void WiFiManager::getCredentialsFromSerial() {
    Serial.println("\n==========================================");
    Serial.println("         WiFi CONFIGURATION SETUP        ");
    Serial.println("==========================================");
    Serial.println("No WiFi credentials found or connection failed.");
    Serial.println("Please enter your WiFi credentials:");

    displayMultiLine("WiFi Setup", "Check Serial", "Monitor", "");

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
        saveCredentialsToEEPROM(wifi_ssid, wifi_password);
        wifiConfigured = true;
        Serial.println("Credentials saved! Restarting...");
        displayCenteredText("Saved! Restarting...");
        delay(2000);
        ESP.restart();
    } else {
        Serial.println("Credentials not saved. Please restart to try again.");
        displayCenteredText("Not saved!");
        while (true) {
            delay(1000);
        }
    }
}

bool WiFiManager::areCredentialsConfigured() {
    return wifiConfigured;
}

String WiFiManager::getSSID() {
    return wifi_ssid;
}

String WiFiManager::getPasswordMasked() {
    return wifi_password.length() > 0 ? "***configured***" : "***NOT SET***";
}

int WiFiManager::getPasswordLength() {
    return wifi_password.length();
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getLocalIP() {
    return WiFi.localIP().toString();
}

String WiFiManager::getGatewayIP() {
    return WiFi.gatewayIP().toString();
}

String WiFiManager::getSubnetMask() {
    return WiFi.subnetMask().toString();
}

String WiFiManager::getDNSIP() {
    return WiFi.dnsIP().toString();
}

int WiFiManager::getRSSI() {
    return WiFi.RSSI();
}

int WiFiManager::getChannel() {
    return WiFi.channel();
}

String WiFiManager::getMACAddress() {
    return WiFi.macAddress();
}

wl_status_t WiFiManager::getStatus() {
    return WiFi.status();
}

WiFiServer* WiFiManager::getServer() {
    return server;
}

bool WiFiManager::startServer() {
    if (server && isConnected()) {
        server->begin();
        Serial.println("Web server started!");
        Serial.printf("Access at: http://%s\n", getLocalIP().c_str());
        return true;
    }
    return false;
}

void WiFiManager::stopServer() {
    if (server) {
        server->stop();
        Serial.println("Web server stopped!");
    }
}

void WiFiManager::setStatusCallback(StatusCallback callback) {
    statusCallback = callback;
}

void WiFiManager::setDisplayCallback(DisplayCallback callback) {
    displayCallback = callback;
}

void WiFiManager::displayText(String text) {
    if (displayCallback) {
        displayCallback(text, "", "", "");
    }
}

void WiFiManager::displayMultiLine(String line1, String line2, String line3, String line4) {
    if (displayCallback) {
        displayCallback(line1, line2, line3, line4);
    }
}

void WiFiManager::displayCenteredText(String text) {
    if (displayCallback) {
        displayCallback(text, "", "", "");
    }
}

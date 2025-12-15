#include "ai_bot_manager.h"

// Context string from the user snippet
const char *ROBOT_CONTEXT = R"raw(
You are RobotNavBrain, the vision + navigation controller for a wheeled robot. 
Your sole mission: reach a cat safely and quickly.

INPUT:
1) One RGB image (primary source).
2) Optional text context.

RULES:
- Look for the cat; if seen or likely in a direction, prefer that way if safe.
- If the cat is NOT visible, prioritize turning 'left' or 'right' to scan the room. Avoid going 'forward' blindly unless following a clear path.
- If cat is <0.5 m away or in danger, STOP.
- Safety-first: stop if obstacle/drop/void within 0.8 m, poor visibility, moving hazard, or low confidence.
- Avoid rapid L/R flips; if unsure, stop.

OUTPUT: Return exactly one JSON object, no extra text:
{
"description": "<2–3 vivid sentences (~90–160 tokens). Mention at least 4 concrete details (colors, counts, object positions, distances) and the reason for the chosen move.>",
"direction": "forward" | "left" | "right" | "backward" | "stop",
"distance_m": <float>,
"goal_found": <true|false>
}

MOVEMENT:
- Default distances: F≤0.5, L/R≤0.4, B≤0.4 m.
      
---
ADDITIONAL CONTEXT (may be empty):
)raw";

AIBotManager::AIBotManager() : camManager(nullptr), wifiManager(nullptr), botRunning(false), lastRequestTime(0)
{
    apiBaseUrl = "";
    apiMessageRoute = "/message";
    apiHealthRoute = "/health";
    lastBotStatus = "Idle";
    sessionId = "esp32-bot-" + String(random(100000, 999999));
}

void AIBotManager::begin(ESP32CamManager *cam, WiFiManager *wifi)
{
    camManager = cam;
    wifiManager = wifi;
    loadApiConfig();
}

void AIBotManager::loadApiConfig()
{
    // Load Base URL
    apiBaseUrl = "";
    for (int i = 0; i < EEPROM_BASE_URL_SIZE; i++)
    {
        char c = EEPROM.read(EEPROM_BASE_URL_ADDR + i);
        if (c == 0)
            break;
        apiBaseUrl += c;
    }
    // Basic validation
    if (!apiBaseUrl.startsWith("http"))
    {
        apiBaseUrl = "";
    }

    // Load Message Route
    apiMessageRoute = "";
    for (int i = 0; i < EEPROM_MSG_ROUTE_SIZE; i++)
    {
        char c = EEPROM.read(EEPROM_MSG_ROUTE_ADDR + i);
        if (c == 0)
            break;
        apiMessageRoute += c;
    }
    if (apiMessageRoute.length() == 0)
        apiMessageRoute = "/message";

    // Load Health Route
    apiHealthRoute = "";
    for (int i = 0; i < EEPROM_HEALTH_ROUTE_SIZE; i++)
    {
        char c = EEPROM.read(EEPROM_HEALTH_ROUTE_ADDR + i);
        if (c == 0)
            break;
        apiHealthRoute += c;
    }
    if (apiHealthRoute.length() == 0)
        apiHealthRoute = "/health";

    Serial.println("Loaded API Config:");
    Serial.println("Base: " + apiBaseUrl);
    Serial.println("Msg: " + apiMessageRoute);
    Serial.println("Health: " + apiHealthRoute);
}

void AIBotManager::saveApiConfigToEEPROM(String baseUrl, String messageRoute, String healthRoute)
{
    Serial.println("Saving API Config to EEPROM");

    // Save Base URL
    for (int i = 0; i < EEPROM_BASE_URL_SIZE; i++)
    {
        EEPROM.write(EEPROM_BASE_URL_ADDR + i, (i < baseUrl.length()) ? baseUrl[i] : 0);
    }

    // Save Message Route
    for (int i = 0; i < EEPROM_MSG_ROUTE_SIZE; i++)
    {
        EEPROM.write(EEPROM_MSG_ROUTE_ADDR + i, (i < messageRoute.length()) ? messageRoute[i] : 0);
    }

    // Save Health Route
    for (int i = 0; i < EEPROM_HEALTH_ROUTE_SIZE; i++)
    {
        EEPROM.write(EEPROM_HEALTH_ROUTE_ADDR + i, (i < healthRoute.length()) ? healthRoute[i] : 0);
    }

    EEPROM.commit();

    apiBaseUrl = baseUrl;
    apiMessageRoute = messageRoute;
    apiHealthRoute = healthRoute;
}

void AIBotManager::setApiConfig(String baseUrl, String messageRoute, String healthRoute)
{
    baseUrl.trim();
    // Remove trailing slash if present
    if (baseUrl.endsWith("/"))
    {
        baseUrl = baseUrl.substring(0, baseUrl.length() - 1);
    }

    messageRoute.trim();
    if (!messageRoute.startsWith("/"))
        messageRoute = "/" + messageRoute;

    healthRoute.trim();
    if (!healthRoute.startsWith("/"))
        healthRoute = "/" + healthRoute;

    saveApiConfigToEEPROM(baseUrl, messageRoute, healthRoute);
}

String AIBotManager::getApiBaseUrl()
{
    return apiBaseUrl;
}

String AIBotManager::getApiMessageRoute()
{
    return apiMessageRoute;
}

String AIBotManager::getApiHealthRoute()
{
    return apiHealthRoute;
}

String AIBotManager::getHealthUrl()
{
    return apiBaseUrl + apiHealthRoute;
}

String AIBotManager::getMessageUrl()
{
    return apiBaseUrl + apiMessageRoute;
}

bool AIBotManager::testConnection()
{
    if (apiBaseUrl.length() == 0)
        return false;

    if (!wifiManager->isConnected())
        return false;

    HTTPClient http;
    String healthUrl = getHealthUrl();
    Serial.println("Testing connection to: " + healthUrl);

    http.begin(healthUrl);
    int httpCode = http.GET();
    http.end();

    Serial.printf("Health check HTTP Code: %d\n", httpCode);
    return (httpCode == 200);
}

void AIBotManager::startBot()
{
    if (apiBaseUrl.length() > 0)
    {
        botRunning = true;
        lastBotStatus = "Running";
        Serial.println("AI Bot Started");
    }
    else
    {
        Serial.println("Cannot start bot: No API URL");
    }
}

void AIBotManager::stopBot()
{
    botRunning = false;
    lastBotStatus = "Stopped";
    Serial.println("AI Bot Stopped");
}

bool AIBotManager::isBotRunning()
{
    return botRunning;
}

String AIBotManager::getLastBotStatus()
{
    return lastBotStatus;
}

void AIBotManager::loop()
{
    if (!botRunning)
        return;

    unsigned long currentMillis = millis();
    if (currentMillis - lastRequestTime >= 15000)
    { // 15 seconds
        lastRequestTime = currentMillis;
        sendBotRequest();
    }
}

void AIBotManager::sendBotRequest()
{
    if (!wifiManager->isConnected())
    {
        Serial.println("Bot: WiFi not connected");
        lastBotStatus = "WiFi Error";
        return;
    }

    if (!camManager->isCameraAvailable())
    {
        Serial.println("Bot: Camera not available");
        lastBotStatus = "Cam Error";
        return;
    }

    Serial.println("Bot: Capturing image...");
    if (!camManager->capturePhoto())
    {
        Serial.println("Bot: Capture failed");
        lastBotStatus = "Capture Fail";
        return;
    }

    String imageBase64 = camManager->getLastImageBase64();
    if (imageBase64.length() == 0)
    {
        Serial.println("Bot: Empty image");
        lastBotStatus = "Image Error";
        return;
    }

    Serial.println("Bot: Sending request...");
    lastBotStatus = "Sending...";

    HTTPClient http;
    http.begin(getMessageUrl());
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(60000); // 60s timeout to wait for AI response

    // Construct JSON payload manually to avoid memory issues with large Base64 strings in JsonDocument
    String payload;
    payload.reserve(imageBase64.length() + 2048);

    payload = "{";
    payload += "\"text\":\"Describe the scene and suggest a direction.\",";
    payload += "\"stream\":false,";
    payload += "\"context\":\"";

    String escapedContext = String(ROBOT_CONTEXT);
    escapedContext.replace("\\", "\\\\"); // Escape backslashes first
    escapedContext.replace("\"", "\\\""); // Escape quotes
    escapedContext.replace("\n", "\\n");  // Escape newlines
    escapedContext.replace("\r", "");     // Remove carriage returns

    payload += escapedContext;
    payload += "\",";
    payload += "\"session_id\":\"" + sessionId + "\",";
    payload += "\"audioResponse\":true,";
    payload += "\"image\":\"" + imageBase64 + "\"";
    payload += "}";

    Serial.println("Bot: Sending JSON payload:");
    Serial.println(payload);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.printf("Bot: HTTP Response code: %d\n", httpResponseCode);
        Serial.println("Bot: Response: " + response);
        lastBotStatus = "OK: " + String(httpResponseCode);
    }
    else
    {
        Serial.printf("Bot: Error code: %d\n", httpResponseCode);
        lastBotStatus = "Err: " + String(httpResponseCode);
    }

    http.end();
}

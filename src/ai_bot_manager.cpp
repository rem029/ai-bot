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
    apiUrl = "";
    lastBotStatus = "Idle";
}

void AIBotManager::begin(ESP32CamManager *cam, WiFiManager *wifi)
{
    camManager = cam;
    wifiManager = wifi;
    loadApiUrl();
}

void AIBotManager::loadApiUrl()
{
    apiUrl = "";
    for (int i = 0; i < EEPROM_API_URL_SIZE; i++)
    {
        char c = EEPROM.read(EEPROM_API_URL_ADDR + i);
        if (c == 0)
            break;
        apiUrl += c;
    }
    // Basic validation
    if (!apiUrl.startsWith("http"))
    {
        apiUrl = "";
    }
    Serial.println("Loaded API URL: " + apiUrl);
}

void AIBotManager::saveApiUrlToEEPROM(String url)
{
    Serial.println("Saving API URL to EEPROM: " + url);
    for (int i = 0; i < EEPROM_API_URL_SIZE; i++)
    {
        if (i < url.length())
        {
            EEPROM.write(EEPROM_API_URL_ADDR + i, url[i]);
        }
        else
        {
            EEPROM.write(EEPROM_API_URL_ADDR + i, 0);
        }
    }
    EEPROM.commit();
    apiUrl = url;
}

void AIBotManager::setApiUrl(String url)
{
    url.trim();
    saveApiUrlToEEPROM(url);
}

String AIBotManager::getApiUrl()
{
    return apiUrl;
}

String AIBotManager::getHealthUrl()
{
    // Assume apiUrl is like http://ip:port/message
    // We want http://ip:port/health
    String healthUrl = apiUrl;
    int lastSlash = healthUrl.lastIndexOf('/');
    if (lastSlash != -1)
    {
        // Check if it ends with /message
        if (healthUrl.endsWith("/message"))
        {
            healthUrl = healthUrl.substring(0, healthUrl.length() - 8); // remove /message
        }
        else
        {
            healthUrl = healthUrl.substring(0, lastSlash);
        }
    }

    if (!healthUrl.endsWith("/"))
        healthUrl += "/";
    healthUrl += "health";

    return healthUrl;
}

bool AIBotManager::testConnection()
{
    if (apiUrl.length() == 0)
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
    if (apiUrl.length() > 0)
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
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000); // 10s timeout

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
    payload += "\"session_id\":\"esp32-bot\",";
    payload += "\"audioResponse\":false,";
    payload += "\"image\":\"" + imageBase64 + "\"";
    payload += "}";

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.printf("Bot: HTTP Response code: %d\n", httpResponseCode);
        // Serial.println("Bot: Response: " + response); // Optional: print response
        lastBotStatus = "OK: " + String(httpResponseCode);
    }
    else
    {
        Serial.printf("Bot: Error code: %d\n", httpResponseCode);
        lastBotStatus = "Err: " + String(httpResponseCode);
    }

    http.end();
}

#include "esp32cam_manager.h"

ESP32CamManager::ESP32CamManager() : cameraAvailable(false), lastCameraCheck(0), statusCallback(nullptr) {
    camSerial = new HardwareSerial(1); // Use Serial1 for camera communication
}

bool ESP32CamManager::begin() {
    // Initialize camera serial connection
    camSerial->begin(9600, SERIAL_8N1, CAM_SERIAL_RX, CAM_SERIAL_TX);
    delay(5000); // Give ESP32-CAM time to boot
    
    // Test basic serial communication first
    bool serialTest = testSerialCommunication();
    if (serialTest) {
        Serial.println("Serial communication working!");
        
        // Now check camera status
        cameraAvailable = checkCameraStatus();
        if (cameraAvailable) {
            Serial.println("Camera ready!");
        } else {
            Serial.println("Camera not ready!");
        }
    } else {
        Serial.println("Serial communication failed!");
        cameraAvailable = false;
    }
    
    return serialTest;
}

String ESP32CamManager::sendCommand(String command, bool expectLargeData) {
    // Clear any pending data first
    while (camSerial->available()) {
        camSerial->read();
    }

    Serial.println("Sending command: '" + command + "'");
    camSerial->println(command);

    if (!expectLargeData) {
        // Standard response handling with debugging
        unsigned long startTime = millis();
        String response = "";

        while (millis() - startTime < 5000) { // 5-second timeout
            if (camSerial->available()) {
                char c = camSerial->read();

                // Debug: show raw byte values for first few characters
                if (response.length() < 10) {
                    Serial.print("Raw byte: ");
                    Serial.print((int)c);
                    Serial.print(" (char: '");
                    if (c >= 32 && c <= 126) {
                        Serial.print(c);
                    } else {
                        Serial.print("?");
                    }
                    Serial.println("')");
                }

                response += c;

                if (c == '\n') {
                    break;
                }
            }
            yield();
        }

        response.trim();
        Serial.println("Final response: '" + response + "' (length: " + String(response.length()) + ")");
        return response;
    } else {
        // Handle chunked base64 data
        String fullResponse = "";
        unsigned long startTime = millis();
        bool readingBase64 = false;

        while (millis() - startTime < 30000) { // 30-second timeout for large data
            if (camSerial->available()) {
                String line = camSerial->readStringUntil('\n');
                line.trim();

                if (line == "BASE64_START") {
                    readingBase64 = true;
                    continue;
                } else if (line == "BASE64_END") {
                    break;
                } else if (readingBase64) {
                    fullResponse += line;
                } else if (!readingBase64 && line.length() > 0) {
                    // This might be the initial response
                    return line;
                }

                startTime = millis(); // Reset timeout on activity
            }
            yield();
        }

        return fullResponse;
    }
}

String ESP32CamManager::sendCameraCommand(String command, bool expectLargeData) {
    return sendCommand(command, expectLargeData);
}

bool ESP32CamManager::testSerialCommunication() {
    Serial.println("Testing camera serial communication...");
    
    String response = sendCommand("PING");
    Serial.println("Camera PING response: '" + response + "'");
    
    if (response == "PONG") {
        // Test version command
        response = sendCommand("VERSION");
        Serial.println("Camera VERSION response: '" + response + "'");
        return true;
    }
    
    return false;
}

bool ESP32CamManager::checkCameraStatus() {
    String response = sendCommand("STATUS");
    Serial.println("STATUS response: " + response);

    if (response.length() == 0) {
        Serial.println("No response from camera");
        return false;
    }

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        Serial.println("Failed to parse camera status: " + response);
        return false;
    }

    bool status = doc["ready"];
    return status;
}

bool ESP32CamManager::isCameraAvailable() {
    return cameraAvailable;
}

bool ESP32CamManager::ensureCameraReady() {
    if (!cameraAvailable) {
        Serial.println("Camera not available, testing connection...");
        String response = sendCommand("PING");
        cameraAvailable = (response == "PONG");

        if (cameraAvailable) {
            Serial.println("Camera reconnected!");
        }
    }
    return cameraAvailable;
}

void ESP32CamManager::checkCameraAvailability() {
    unsigned long currentTime = millis();

    // Check if it's time for periodic check
    if (currentTime - lastCameraCheck >= CAMERA_CHECK_INTERVAL) {
        Serial.println("Performing periodic camera health check...");

        // Quick ping test (faster than full status check)
        String response = sendCommand("PING");
        bool newCameraState = (response == "PONG");

        // Check if camera state changed
        if (newCameraState != cameraAvailable) {
            Serial.println("Camera state changed: " + String(newCameraState ? "Connected" : "Disconnected"));
            cameraAvailable = newCameraState;

            // Call status callback if registered
            if (statusCallback) {
                statusCallback(cameraAvailable, true); // true = status changed
            }
        } else if (cameraAvailable) {
            // Camera still available, just log it
            Serial.println("Camera health check: OK");
        } else {
            // Camera still unavailable
            Serial.println("Camera health check: Still disconnected");
        }

        lastCameraCheck = currentTime;
    }
}

bool ESP32CamManager::capturePhoto() {
    Serial.println("Requesting photo from ESP32-CAM");

    // First, send CAPTURE command to take the photo
    String response = sendCommand("CAPTURE");
    Serial.println("CAPTURE response: " + response);

    // Parse the JSON response
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        Serial.println("Raw response: " + response);
        return false;
    }

    // Check if photo was successful
    bool success = doc["success"];
    if (success) {
        // Request the image data with chunked reading
        Serial.println("Requesting image data...");
        String imageData = sendCommand("GETIMAGE", true); // Enable large data mode

        if (imageData.length() > 100) { // Basic check for base64 data
            lastImageBase64 = imageData;
            Serial.println("Image received, size: " + String(imageData.length()));
            return true;
        } else {
            Serial.println("Invalid image response, size: " + String(imageData.length()));
            return false;
        }
    } else {
        String error = doc["error"];
        Serial.println("Capture failed: " + error);
        return false;
    }
}

String ESP32CamManager::getLastImageBase64() {
    return lastImageBase64;
}

bool ESP32CamManager::hasImage() {
    return lastImageBase64.length() > 0;
}

bool ESP32CamManager::ping() {
    String response = sendCommand("PING");
    bool success = (response == "PONG");
    
    // Update availability based on ping result
    cameraAvailable = success;
    
    return success;
}

String ESP32CamManager::getVersion() {
    return sendCommand("VERSION");
}

void ESP32CamManager::setStatusCallback(StatusCallback callback) {
    statusCallback = callback;
}

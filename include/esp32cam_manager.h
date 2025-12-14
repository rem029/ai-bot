#ifndef ESP32CAM_MANAGER_H
#define ESP32CAM_MANAGER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

// Define camera serial connection pins
#define CAM_SERIAL_TX 43 // Connect to ESP32-CAM RX (VOR)
#define CAM_SERIAL_RX 44 // Connect to ESP32-CAM TX (VOT)

class ESP32CamManager {
private:
    HardwareSerial* camSerial;
    bool cameraAvailable;
    String lastImageBase64;
    unsigned long lastCameraCheck;
    const unsigned long CAMERA_CHECK_INTERVAL = 30000; // Check every 30 seconds
    
    // Private helper methods
    String sendCommand(String command, bool expectLargeData = false);
    
public:
    // Constructor
    ESP32CamManager();
    
    // Initialization
    bool begin();
    
    // Communication methods
    bool testSerialCommunication();
    String sendCameraCommand(String command, bool expectLargeData = false);
    
    // Camera status and control
    bool checkCameraStatus();
    bool isCameraAvailable();
    bool ensureCameraReady();
    void checkCameraAvailability();
    
    // Photo capture
    bool capturePhoto();
    String getLastImageBase64();
    bool hasImage();
    
    // Utility methods
    bool ping();
    String getVersion();
    
    // Status callback function type
    typedef void (*StatusCallback)(bool cameraConnected, bool statusChanged);
    void setStatusCallback(StatusCallback callback);
    
private:
    StatusCallback statusCallback;
};

#endif // ESP32CAM_MANAGER_H

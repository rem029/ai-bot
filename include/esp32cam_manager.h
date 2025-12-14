#ifndef ESP32CAM_MANAGER_H
#define ESP32CAM_MANAGER_H

#include <Arduino.h>
#include "esp_camera.h"

// Freenove ESP32-S3-WROOM Camera Pin Definition
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5

#define Y9_GPIO_NUM 16
#define Y8_GPIO_NUM 17
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 12
#define Y5_GPIO_NUM 10
#define Y4_GPIO_NUM 8
#define Y3_GPIO_NUM 9
#define Y2_GPIO_NUM 11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

class ESP32CamManager
{
private:
    bool cameraAvailable;
    String lastImageBase64;

    // Status callback function type
    typedef void (*StatusCallback)(bool cameraConnected, bool statusChanged);
    StatusCallback statusCallback;

public:
    // Constructor
    ESP32CamManager();

    // Initialization
    bool begin();

    // Communication methods
    bool testSerialCommunication(); // Deprecated/Dummy

    // Camera status and control
    bool checkCameraStatus();
    bool isCameraAvailable();
    bool ensureCameraReady();
    void checkCameraAvailability();

    // Photo capture
    bool capturePhoto();
    String getLastImageBase64();
    bool hasImage();

    // Streaming support
    camera_fb_t *getFrame();
    void releaseFrame(camera_fb_t *fb);

    // Utility methods
    bool ping();

    void setStatusCallback(StatusCallback callback);
};

#endif // ESP32CAM_MANAGER_H

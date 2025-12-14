#include "esp32cam_manager.h"
#include "mbedtls/base64.h"

ESP32CamManager::ESP32CamManager() : cameraAvailable(false), statusCallback(nullptr)
{
}

bool ESP32CamManager::begin()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG; // for streaming
    config.grab_mode = CAMERA_GRAB_LATEST;

    if (psramFound())
    {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        Serial.println("PSRAM found, using UXGA config");
    }
    else
    {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        Serial.println("PSRAM not found, using SVGA config");
    }

    // Camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        // Retry with lower XCLK
        config.xclk_freq_hz = 10000000;
        err = esp_camera_init(&config);
        if (err != ESP_OK)
        {
            Serial.printf("Camera init retry failed with error 0x%x\n", err);
            cameraAvailable = false;
            return false;
        }
    }

    sensor_t *s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID)
    {
        s->set_vflip(s, 1);       // flip it back
        s->set_brightness(s, 1);  // up the brightness just a bit
        s->set_saturation(s, -2); // lower the saturation
    }

    Serial.println("Camera initialized successfully!");
    cameraAvailable = true;
    return true;
}

bool ESP32CamManager::testSerialCommunication()
{
    return true; // Dummy for compatibility
}

bool ESP32CamManager::checkCameraStatus()
{
    return cameraAvailable;
}

bool ESP32CamManager::isCameraAvailable()
{
    return cameraAvailable;
}

bool ESP32CamManager::ensureCameraReady()
{
    return cameraAvailable;
}

void ESP32CamManager::checkCameraAvailability()
{
    // Local camera doesn't need periodic polling like UART
}

bool ESP32CamManager::capturePhoto()
{
    if (!cameraAvailable)
        return false;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("Camera capture failed");
        return false;
    }

    // Calculate output length for Base64
    size_t outputLength = ((fb->len + 2) / 3) * 4;

    // Allocate buffer (add 1 for null terminator)
    char *encoded = (char *)malloc(outputLength + 1);
    if (encoded == NULL)
    {
        Serial.println("Memory allocation failed for base64");
        esp_camera_fb_return(fb);
        return false;
    }

    size_t olen = 0;
    int ret = mbedtls_base64_encode((unsigned char *)encoded, outputLength + 1, &olen, fb->buf, fb->len);

    if (ret != 0)
    {
        Serial.println("Base64 encoding failed");
        free(encoded);
        esp_camera_fb_return(fb);
        return false;
    }

    encoded[olen] = '\0'; // Ensure null termination
    lastImageBase64 = String(encoded);
    free(encoded);

    esp_camera_fb_return(fb);
    return true;
}

String ESP32CamManager::getLastImageBase64()
{
    return lastImageBase64;
}

bool ESP32CamManager::hasImage()
{
    return lastImageBase64.length() > 0;
}

camera_fb_t *ESP32CamManager::getFrame()
{
    if (!cameraAvailable)
        return nullptr;
    return esp_camera_fb_get();
}

void ESP32CamManager::releaseFrame(camera_fb_t *fb)
{
    if (fb)
        esp_camera_fb_return(fb);
}

bool ESP32CamManager::ping()
{
    return cameraAvailable;
}

void ESP32CamManager::setStatusCallback(StatusCallback callback)
{
    statusCallback = callback;
}

#include "camera_manager.h"
#include <Arduino.h>

CameraManager cameraManager;

CameraManager::CameraManager() {
    cameraInitialized = false;
    cameraSleeping = false;
    lastActivity = 0;
}

bool CameraManager::initialize() {
    if (cameraSleeping) {
        cameraSleeping = false;
    }
    
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
    config.pixel_format = PIXFORMAT_JPEG;
    
    if(psramFound()){
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("{\"ready\":false,\"error\":\"Camera init failed: 0x%x\"}\n", err);
        cameraInitialized = false;
        return false;
    } else {
        Serial.println("{\"ready\":true,\"message\":\"ESP32-CAM initialized for streaming\"}");
        cameraInitialized = true;
        lastActivity = millis();
        
        // Additional sensor settings
        sensor_t * s = esp_camera_sensor_get();
        if (s != NULL) {
            s->set_brightness(s, 0);
            s->set_contrast(s, 0);
            s->set_saturation(s, 0);
            s->set_special_effect(s, 0);
            s->set_whitebal(s, 1);
            s->set_awb_gain(s, 1);
            s->set_wb_mode(s, 0);
            s->set_exposure_ctrl(s, 1);
            s->set_aec2(s, 0);
            s->set_ae_level(s, 0);
            s->set_aec_value(s, 300);
            s->set_gain_ctrl(s, 1);
            s->set_agc_gain(s, 0);
            s->set_gainceiling(s, (gainceiling_t)0);
            s->set_bpc(s, 0);
            s->set_wpc(s, 1);
            s->set_raw_gma(s, 1);
            s->set_lenc(s, 1);
            s->set_hmirror(s, 0);
            s->set_vflip(s, 0);
            s->set_dcw(s, 1);
            s->set_colorbar(s, 0);
        }
        return true;
    }
}

void CameraManager::sleep() {
    if (cameraInitialized && !cameraSleeping) {
        esp_camera_deinit();
        cameraSleeping = true;
        Serial.println("{\"status\":\"camera_sleeping\"}");
    }
}

void CameraManager::wake() {
    if (cameraSleeping) {
        initialize();
        cameraSleeping = false;
        Serial.println("{\"status\":\"camera_awake\"}");
    }
}

void CameraManager::updateActivity() {
    lastActivity = millis();
}

void CameraManager::checkIdleTimeout() {
    if (cameraInitialized && !cameraSleeping && 
        (millis() - lastActivity > IDLE_TIMEOUT)) {
        sleep();
    }
}

void CameraManager::setQuality(int quality) {
    if (!cameraInitialized || cameraSleeping) {
        Serial.println("{\"success\":false,\"error\":\"Camera not ready\"}");
        return;
    }
    
    if (quality >= 0 && quality <= 63) {
        sensor_t * s = esp_camera_sensor_get();
        if (s != NULL) {
            s->set_quality(s, quality);
            Serial.printf("{\"success\":true,\"quality\":%d}\n", quality);
            updateActivity();
        } else {
            Serial.println("{\"success\":false,\"error\":\"Failed to get sensor\"}");
        }
    } else {
        Serial.println("{\"success\":false,\"error\":\"Invalid quality range (0-63)\"}");
    }
}

camera_fb_t* CameraManager::captureImage() {
    wake();
    updateActivity();
    
    if (!cameraInitialized) {
        return nullptr;
    }
    
    return esp_camera_fb_get();
}

void CameraManager::sendImageBase64() {
    wake();
    updateActivity();
    
    if (!cameraInitialized) {
        Serial.println("{\"success\":false,\"error\":\"Camera not initialized\"}");
        return;
    }

    camera_fb_t * fb = esp_camera_fb_get();
    if(!fb) {
        Serial.println("{\"success\":false,\"error\":\"Capture failed\"}");
        return;
    }
    
    Serial.printf("{\"success\":true,\"size\":%d,\"width\":%d,\"height\":%d}\n", 
                  fb->len, fb->width, fb->height);
    
    delay(100);
    
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    Serial.println("BASE64_START");
    
    const size_t chunk_size = 3000;
    for (size_t offset = 0; offset < fb->len; offset += chunk_size) {
        size_t remaining = fb->len - offset;
        size_t current_chunk = (remaining < chunk_size) ? remaining : chunk_size;
        
        String encoded = "";
        for (size_t i = 0; i < current_chunk; i += 3) {
            size_t pos = offset + i;
            uint32_t b = (fb->buf[pos] << 16) | 
                         ((pos + 1 < fb->len ? fb->buf[pos + 1] : 0) << 8) | 
                         (pos + 2 < fb->len ? fb->buf[pos + 2] : 0);
            
            encoded += chars[b >> 18];
            encoded += chars[(b >> 12) & 0x3F];
            encoded += (pos + 1 < fb->len) ? chars[(b >> 6) & 0x3F] : '=';
            encoded += (pos + 2 < fb->len) ? chars[b & 0x3F] : '=';
        }
        
        Serial.println(encoded);
        yield();
    }
    
    Serial.println("BASE64_END");
    esp_camera_fb_return(fb);
}

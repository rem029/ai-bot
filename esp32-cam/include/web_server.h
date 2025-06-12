#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include "camera_manager.h"
#include "wifi_manager.h"

class WebServerManager {
public:
    WebServerManager();
    void begin();
    void handleClient();
    
private:
    WebServer server;
    
    void handleRoot();
    void handleCapture();
    void handleStream();
    void handleImage();
    void handleClearWiFi();
};

extern WebServerManager webServerManager;

#endif

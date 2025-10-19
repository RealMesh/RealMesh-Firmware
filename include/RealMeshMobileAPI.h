#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include "RealMeshNode.h"

class RealMeshAPI {
public:
    RealMeshAPI(RealMeshNode* node);
    
    // Initialize API interfaces
    bool beginWiFi(const String& ssid, const String& password, uint16_t port = 8080);
    
    // Process API requests
    void loop();
    
    // JSON API methods
    String processCommand(const String& command);
    String getStatus();
    String getNodes();
    String sendMessage(const String& address, const String& message);
    String getNetworkStats();
    
private:
    RealMeshNode* meshNode;
    
    // WiFi/TCP interface
    WiFiServer* tcpServer;
    bool wifiEnabled;
    
    // Helper methods
    String createResponse(bool success, const String& data = "", const String& error = "");
    void handleTcpClient();
    String processJsonCommand(const String& jsonStr);
};
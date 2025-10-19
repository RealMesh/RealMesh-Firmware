#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "RealMeshNode.h"

// Forward declaration for friend class
class RealMeshBLECharacteristicCallbacks;

class RealMeshAPI {
    // Make callback class a friend so it can access private methods
    friend class RealMeshBLECharacteristicCallbacks;
    
public:
    RealMeshAPI(RealMeshNode* node);
    
    // Initialize API interfaces
    bool beginBLE(const String& deviceName);
    bool beginWiFi(const String& ssid, const String& password, uint16_t port = 8080);
    void stopWiFi();
    void stopBLE();
    
    // Process API requests
    void loop();
    
    // Status methods
    bool isBLEEnabled() const { return bleEnabled; }
    bool isWiFiEnabled() const { return wifiEnabled; }
    String getBLEDeviceName() const { return bleDeviceName; }
    
    // JSON API methods
    String processCommand(const String& command);
    String getStatus();
    String getNodes();
    String sendMessage(const String& address, const String& message);
    String getNetworkStats();
    
private:
    RealMeshNode* meshNode;
    
    // BLE interface
    BLEServer* bleServer;
    BLECharacteristic* bleCharacteristic;
    bool bleEnabled;
    String bleDeviceName;
    
    // WiFi/TCP interface
    WiFiServer* tcpServer;
    bool wifiEnabled;
    
    // Helper methods
    String createResponse(bool success, const String& data = "", const String& error = "");
    void handleTcpClient();
    void handleBLEClient();
    String processJsonCommand(const String& jsonStr);
};
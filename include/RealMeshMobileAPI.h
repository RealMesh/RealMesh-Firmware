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
#include "RealMeshDisplay.h"

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
    
    // Message notification (called when messages are received)
    void notifyMessageReceived(const String& from, const String& message);
    
    // JSON API methods
    String processJsonCommand(const String& jsonStr);
    String getStatus();
    String getNodes();
    String sendMessage(const String& address, const String& message);
    String getNetworkStats();
    String controlLED(const JsonDocument& doc);
    String controlDisplay(const JsonDocument& doc);
    String changeName(const JsonDocument& doc);
    
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
};
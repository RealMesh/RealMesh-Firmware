#include "RealMeshMobileAPI.h"
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>

RealMeshAPI::RealMeshAPI(RealMeshNode* node) : 
    meshNode(node), 
    tcpServer(nullptr), 
    wifiEnabled(false),
    bleServer(nullptr),
    bleCharacteristic(nullptr),
    bleEnabled(false) {
}

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

class BLECallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("BLE client connected");
    }
    
    void onDisconnect(BLEServer* pServer) {
        Serial.println("BLE client disconnected");
        // Restart advertising
        pServer->getAdvertising()->start();
    }
};

class RealMeshBLECharacteristicCallbacks: public BLECharacteristicCallbacks {
private:
    RealMeshAPI* api;
    
public:
    RealMeshBLECharacteristicCallbacks(RealMeshAPI* api) : api(api) {}
    
    void onWrite(BLECharacteristic *pCharacteristic) {
        String command = pCharacteristic->getValue().c_str();
        command.trim();
        
        if (command.length() > 0) {
            Serial.printf("BLE command: %s\n", command.c_str());
            String response = api->processJsonCommand(command);
            pCharacteristic->setValue(response.c_str());
            pCharacteristic->notify();
        }
    }
};

bool RealMeshAPI::beginBLE(const String& deviceName) {
    Serial.println("Starting BLE for mobile API...");
    
    bleDeviceName = deviceName;
    
    // Initialize BLE
    BLEDevice::init(deviceName.c_str());
    
    // Create BLE Server
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new BLECallbacks());
    
    // Create BLE Service
    BLEService *pService = bleServer->createService(SERVICE_UUID);
    
    // Create BLE Characteristic
    bleCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    
    bleCharacteristic->setCallbacks(new RealMeshBLECharacteristicCallbacks(this));
    bleCharacteristic->addDescriptor(new BLE2902());
    
    // Start service
    pService->start();
    
    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    
    // Set device as connectable and discoverable
    pAdvertising->setAppearance(0x0080); // Generic Computer
    
    // Start advertising with proper settings
    BLEDevice::startAdvertising();
    
    Serial.printf("📡 BLE Advertising started\n");
    Serial.printf("   Device Name: %s\n", deviceName.c_str());
    Serial.printf("   Service UUID: %s\n", SERVICE_UUID);
    Serial.printf("   Device should be visible as: %s\n", deviceName.c_str());
    Serial.println("   Try scanning with a BLE scanner app like:");
    Serial.println("   - nRF Connect (Nordic) - recommended");
    Serial.println("   - BLE Scanner");
    Serial.println("   - LightBlue Explorer");
    
    // Add a delay to ensure advertising starts properly
    delay(100);
    
    bleEnabled = true;
    
    Serial.println("✅ BLE API ready");
    Serial.printf("   Device Name: %s\n", deviceName.c_str());
    Serial.println("   Ready for pairing!");
    
    return true;
}

void RealMeshAPI::stopBLE() {
    if (bleEnabled) {
        BLEDevice::deinit(false);
        bleServer = nullptr;
        bleCharacteristic = nullptr;
        bleEnabled = false;
        Serial.println("BLE API stopped");
    }
}

bool RealMeshAPI::beginWiFi(const String& ssid, const String& password, uint16_t port) {
    Serial.println("Starting WiFi AP for mobile API...");
    
    // Create WiFi Access Point
    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP(ssid.c_str(), password.c_str());
    
    if (!success) {
        Serial.println("Failed to create WiFi AP");
        return false;
    }
    
    Serial.print("WiFi AP started: ");
    Serial.println(WiFi.softAPIP());
    
    // Start TCP server
    tcpServer = new WiFiServer(port);
    tcpServer->begin();
    
    wifiEnabled = true;
    Serial.printf("TCP API server listening on port %d\n", port);
    return true;
}

void RealMeshAPI::loop() {
    if (wifiEnabled) {
        handleTcpClient();
    }
    // BLE handles callbacks automatically, no polling needed
}

void RealMeshAPI::handleTcpClient() {
    WiFiClient client = tcpServer->available();
    if (client) {
        Serial.println("TCP client connected");
        
        String request = "";
        while (client.connected() && client.available()) {
            request += (char)client.read();
        }
        
        if (request.length() > 0) {
            String response = processJsonCommand(request);
            
            // Send HTTP response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Access-Control-Allow-Origin: *");
            client.println("Connection: close");
            client.println();
            client.println(response);
        }
        
        client.stop();
        Serial.println("TCP client disconnected");
    }
}

void RealMeshAPI::stopWiFi() {
    if (wifiEnabled) {
        if (tcpServer) {
            delete tcpServer;
            tcpServer = nullptr;
        }
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
        wifiEnabled = false;
        Serial.println("WiFi API stopped");
    }
}

String RealMeshAPI::processJsonCommand(const String& jsonStr) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        return createResponse(false, "", "Invalid JSON");
    }
    
    String command = doc["command"];
    
    if (command == "status") {
        return getStatus();
    } else if (command == "nodes") {
        return getNodes();
    } else if (command == "send") {
        String address = doc["address"];
        String message = doc["message"];
        return sendMessage(address, message);
    } else if (command == "stats") {
        return getNetworkStats();
    } else {
        return createResponse(false, "", "Unknown command: " + command);
    }
}

String RealMeshAPI::getStatus() {
    if (!meshNode) {
        return createResponse(false, "", "Node not initialized");
    }
    
    DynamicJsonDocument doc(512);
    doc["address"] = meshNode->getOwnAddress().getFullAddress();
    doc["state"] = (int)meshNode->getCurrentState();
    doc["uptime"] = millis() / 1000;
    doc["stationary"] = meshNode->isStationary();
    
    String result;
    serializeJson(doc, result);
    return createResponse(true, result);
}

String RealMeshAPI::getNodes() {
    if (!meshNode) {
        return createResponse(false, "", "Node not initialized");
    }
    
    DynamicJsonDocument doc(1024);
    JsonArray nodes = doc.createNestedArray("nodes");
    
    auto knownNodes = meshNode->getKnownNodes();
    for (const String& node : knownNodes) {
        nodes.add(node);
    }
    
    doc["count"] = meshNode->getKnownNodesCount();
    
    String result;
    serializeJson(doc, result);
    return createResponse(true, result);
}

String RealMeshAPI::sendMessage(const String& address, const String& message) {
    if (!meshNode) {
        return createResponse(false, "", "Node not initialized");
    }
    
    if (address.isEmpty() || message.isEmpty()) {
        return createResponse(false, "", "Address and message required");
    }
    
    bool success = meshNode->sendMessage(address, message);
    if (success) {
        return createResponse(true, "Message sent successfully");
    } else {
        return createResponse(false, "", "Failed to send message");
    }
}

String RealMeshAPI::getNetworkStats() {
    if (!meshNode) {
        return createResponse(false, "", "Node not initialized");
    }
    
    auto stats = meshNode->getNetworkStats();
    
    DynamicJsonDocument doc(512);
    doc["messagesSent"] = stats.messagesSent;
    doc["messagesReceived"] = stats.messagesReceived;
    doc["messagesDropped"] = stats.messagesDropped;
    doc["routingTableSize"] = stats.routingTableSize;
    doc["avgRSSI"] = stats.avgRSSI;
    doc["lastHeartbeat"] = stats.lastHeartbeat;
    
    String result;
    serializeJson(doc, result);
    return createResponse(true, result);
}

String RealMeshAPI::createResponse(bool success, const String& data, const String& error) {
    DynamicJsonDocument doc(1024);
    doc["success"] = success;
    doc["timestamp"] = millis();
    
    if (success && !data.isEmpty()) {
        DynamicJsonDocument dataDoc(1024);
        deserializeJson(dataDoc, data);
        doc["data"] = dataDoc;
    } else if (!success && !error.isEmpty()) {
        doc["error"] = error;
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}
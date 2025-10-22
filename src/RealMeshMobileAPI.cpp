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
    } else if (command == "led") {
        return controlLED(doc);
    } else if (command == "display") {
        return controlDisplay(doc);
    } else if (command == "changeName") {
        return changeName(doc);
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
    // Supports direct messages (node@domain) or public broadcast (use "svet" or "@")
    if (!meshNode) {
        return createResponse(false, "", "Node not initialized");
    }
    
    if (address.isEmpty() || message.isEmpty()) {
        return createResponse(false, "", "Address and message required");
    }
    
    bool success = meshNode->sendMessage(address, message);
    if (success) {
        String info = (address == "svet" || address == "@") ? " to public channel" : " to " + address;
        return createResponse(true, "Message sent" + info);
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

String RealMeshAPI::controlLED(const JsonDocument& doc) {
    // Get reference to global LED manager
    extern RealMeshLEDManager* ledManager;
    
    if (!ledManager) {
        return createResponse(false, "", "LED manager not available");
    }
    
    String action = doc["action"];
    
    if (action == "on") {
        ledManager->setLED(true);
        return createResponse(true, "{\"state\":\"on\"}");
    } else if (action == "off") {
        ledManager->setLED(false);
        return createResponse(true, "{\"state\":\"off\"}");
    } else if (action == "toggle") {
        ledManager->toggleLED();
        bool state = ledManager->getLEDState();
        return createResponse(true, "{\"state\":\"" + String(state ? "on" : "off") + "\"}");
    } else if (action == "heartbeat") {
        bool enabled = doc["enabled"];
        ledManager->setHeartbeatEnabled(enabled);
        return createResponse(true, "{\"heartbeat\":" + String(enabled ? "true" : "false") + "}");
    } else if (action == "interval") {
        int interval = doc["interval"];
        if (interval >= 100 && interval <= 10000) {
            ledManager->setHeartbeatInterval(interval);
            return createResponse(true, "{\"interval\":" + String(interval) + "}");
        } else {
            return createResponse(false, "", "Invalid interval (100-10000ms)");
        }
    } else if (action == "status") {
        DynamicJsonDocument statusDoc(256);
        statusDoc["state"] = ledManager->getLEDState() ? "on" : "off";
        statusDoc["heartbeat"] = ledManager->isHeartbeatEnabled();
        statusDoc["interval"] = ledManager->getHeartbeatInterval();
        
        String statusStr;
        serializeJson(statusDoc, statusStr);
        return createResponse(true, statusStr);
    } else if (action == "flash") {
        String pattern = doc["pattern"];
        if (pattern == "success") {
            ledManager->flashSuccess(2);
        } else if (pattern == "error") {
            ledManager->flashError(3);
        } else if (pattern == "warning") {
            ledManager->flashWarning(4);
        } else {
            return createResponse(false, "", "Invalid flash pattern");
        }
        return createResponse(true, "{\"flash\":\"" + pattern + "\"}");
    } else {
        return createResponse(false, "", "Invalid LED action");
    }
}

String RealMeshAPI::controlDisplay(const JsonDocument& doc) {
    // Get reference to global display manager
    extern RealMeshDisplayManager* displayManager;
    
    if (!displayManager) {
        return createResponse(false, "", "Display manager not available");
    }
    
    String action = doc["action"];
    
    if (action == "next") {
        displayManager->nextScreen();
        return createResponse(true, "{\"screen\":" + String(displayManager->getCurrentScreen()) + "}");
    } else if (action == "prev") {
        displayManager->previousScreen();
        return createResponse(true, "{\"screen\":" + String(displayManager->getCurrentScreen()) + "}");
    } else if (action == "set") {
        int screen = doc["screen"];
        if (screen >= 0 && screen < 4) {
            displayManager->setCurrentScreen((DisplayScreen)screen);
            return createResponse(true, "{\"screen\":" + String(screen) + "}");
        } else {
            return createResponse(false, "", "Invalid screen number (0-3)");
        }
    } else if (action == "message") {
        String title = doc["title"];
        String message = doc["message"];
        String type = doc["type"];
        int duration = doc["duration"];
        
        if (duration <= 0) duration = 5000;
        
        DisplayMessageType msgType = DISPLAY_MSG_INFO;
        if (type == "error") msgType = DISPLAY_MSG_ERROR;
        else if (type == "warning") msgType = DISPLAY_MSG_WARNING;
        else if (type == "success") msgType = DISPLAY_MSG_SUCCESS;
        
        displayManager->showTemporaryMessage(title, message, msgType, duration);
        return createResponse(true, "{\"message\":\"shown\"}");
    } else if (action == "status") {
        DynamicJsonDocument statusDoc(256);
        statusDoc["currentScreen"] = displayManager->getCurrentScreen();
        statusDoc["batteryPercent"] = displayManager->getBatteryPercentage();
        statusDoc["unreadMessages"] = displayManager->getUnreadCount();
        
        String statusStr;
        serializeJson(statusDoc, statusStr);
        return createResponse(true, statusStr);
    } else {
        return createResponse(false, "", "Invalid display action");
    }
}

String RealMeshAPI::changeName(const JsonDocument& doc) {
    if (!meshNode) {
        return createResponse(false, "", "Node not initialized");
    }
    
    String nodeId = doc["nodeId"];
    String subdomain = doc["subdomain"];
    
    if (nodeId.isEmpty() || subdomain.isEmpty()) {
        return createResponse(false, "", "Both nodeId and subdomain are required");
    }
    
    String currentAddress = meshNode->getOwnAddress().getFullAddress();
    String newAddress = nodeId + "@" + subdomain;
    
    meshNode->setDesiredName(nodeId, subdomain);
    
    // Update display immediately
    if (displayManager) {
        displayManager->setNodeName(nodeId);
        displayManager->setNodeAddress(newAddress);
        displayManager->showTemporaryMessage("Name Changed", 
            "New: " + newAddress + "\nReboot required", DISPLAY_MSG_INFO, 8000);
    }
    
    DynamicJsonDocument responseDoc(256);
    responseDoc["oldAddress"] = currentAddress;
    responseDoc["newAddress"] = newAddress;
    responseDoc["rebootRequired"] = true;
    
    String responseStr;
    serializeJson(responseDoc, responseStr);
    return createResponse(true, responseStr, "Name change scheduled. Reboot required to apply.");
}

void RealMeshAPI::notifyMessageReceived(const String& from, const String& message) {
    if (!bleEnabled || !bleCharacteristic) {
        return;
    }
    
    // Create notification JSON
    DynamicJsonDocument doc(512);
    doc["type"] = "message";
    doc["from"] = from;
    doc["message"] = message;
    doc["timestamp"] = millis() / 1000;
    
    String notification;
    serializeJson(doc, notification);
    
    // Send as BLE notification
    bleCharacteristic->setValue(notification.c_str());
    bleCharacteristic->notify();
    
    Serial.printf("📱 Notified mobile app: Message from %s\n", from.c_str());
}
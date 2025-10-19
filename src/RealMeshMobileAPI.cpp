#include "RealMeshMobileAPI.h"

RealMeshAPI::RealMeshAPI(RealMeshNode* node) : 
    meshNode(node), 
    tcpServer(nullptr), 
    wifiEnabled(false) {
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
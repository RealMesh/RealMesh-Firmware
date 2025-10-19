#ifndef REALMESH_API_SIMPLE_H
#define REALMESH_API_SIMPLE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "RealMeshNode.h"

// Simple API Response
struct APIResponse {
    bool success;
    String message;
    
    APIResponse(bool s = false, const String& m = "") : success(s), message(m) {}
};

// Simple API class
class RealMeshAPI {
public:
    explicit RealMeshAPI(RealMeshNode* node);
    bool begin();
    
    // Basic node operations
    APIResponse setNodeName(const String& nodeId);
    APIResponse getNodeConfig();
    APIResponse sendMessage(const String& targetAddress, const String& messageText);
    APIResponse sendPublicMessage(const String& messageText);
    APIResponse scanNetwork(uint32_t timeout = 30000);
    APIResponse getSystemInfo();
    APIResponse factoryReset();
    
private:
    RealMeshNode* meshNode;
};

#endif // REALMESH_API_SIMPLE_H
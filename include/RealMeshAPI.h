#ifndef REALMESH_API_H
#define REALMESH_API_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>
#include "RealMeshTypes.h"

// Forward declaration to avoid circular dependency
class RealMeshNode;

// Constants
#define FIRMWARE_VERSION "1.0.0-alpha"
#define MAX_MESSAGE_LENGTH 200
#define MAX_EVENT_CALLBACKS 10

// ============================================================================
// RealMesh API - Unified interface for CLI, Bluetooth, TCP, Web, etc.
// ============================================================================

class RealMeshAPI {
public:
    // API Response structure
    struct APIResponse {
        bool success;
        String message;
        JsonDocument data;
        int errorCode;
        
        APIResponse() : success(false), errorCode(0) {}
        APIResponse(bool s, const String& m, int code = 0) : success(s), message(m), errorCode(code) {}
        
        String toJSON() const;
        String toString() const;
    };
    
    // Node type enumeration
    enum NodeType {
        NODE_TYPE_CLIENT = 0,       // Client node (simple mesh)
        NODE_TYPE_BACKBONE = 1,     // Backbone/Magistralni node (routing)
        NODE_TYPE_HYBRID = 2        // Combined client + limited backbone
    };
    
    // Message received callback
    typedef std::function<void(const String& from, const String& message, uint32_t timestamp)> MessageCallback;
    typedef std::function<void(const String& event, const JsonDocument& data)> EventCallback;
    
    // Constructor
    RealMeshAPI(RealMeshNode* node);
    
    // Initialize API
    bool begin();
    
    // Main processing loop
    void loop();
    
    // ========================================================================
    // Node Configuration API
    // ========================================================================
    
    APIResponse setNodeName(const String& nodeId);
    APIResponse setSubdomain(const String& subdomain);  
    APIResponse setNodeType(NodeType type);
    APIResponse getNodeConfig();
    APIResponse saveConfig();
    APIResponse loadConfig();
    APIResponse factoryReset();
    
    // ========================================================================
    // Messaging API
    // ========================================================================
    
    APIResponse sendMessage(const String& targetAddress, const String& message);
    APIResponse sendPublicMessage(const String& message);
    APIResponse sendEmergencyMessage(const String& message);
    APIResponse getMessages(int limit = 50, uint32_t since = 0);
    APIResponse clearMessages();
    APIResponse getMessageCount();
    
    // ========================================================================
    // Network Discovery API  
    // ========================================================================
    
    APIResponse scanNetwork(uint32_t timeoutMs = 30000);
    APIResponse getKnownNodes();
    APIResponse getRoutingTable();
    APIResponse pingNode(const String& targetAddress, uint32_t timeoutMs = 5000);
    APIResponse traceRoute(const String& targetAddress);
    APIResponse whoHearsMe();
    
    // ========================================================================
    // Statistics and Monitoring API
    // ========================================================================
    
    APIResponse getNodeStats();
    APIResponse getNetworkStats(); 
    APIResponse getSignalStats();
    APIResponse getSystemInfo();
    APIResponse getUptimeInfo();
    
    // ========================================================================
    // Radio Configuration API
    // ========================================================================
    
    APIResponse getRadioConfig();
    APIResponse setTransmitPower(int8_t powerDbm);
    APIResponse setFrequency(float frequencyMhz);
    APIResponse setSpreadingFactor(uint8_t sf);
    APIResponse setBandwidth(float bandwidthKhz);
    APIResponse testRadio();
    
    // ========================================================================
    // Advanced Features API
    // ========================================================================
    
    APIResponse runDiagnostics();
    APIResponse exportConfiguration();
    APIResponse importConfiguration(const String& configJson);
    APIResponse getFirmwareInfo();
    APIResponse getLogEntries(int limit = 100);
    APIResponse clearLog();
    
    // ========================================================================
    // Real-time Event System
    // ========================================================================
    
    void setMessageCallback(MessageCallback callback) { messageCallback = callback; }
    void setEventCallback(EventCallback callback) { eventCallback = callback; }
    
    // Subscribe to specific events
    void subscribeToEvents(const std::vector<String>& eventTypes);
    void unsubscribeFromEvents(const std::vector<String>& eventTypes);
    
    // ========================================================================
    // Batch Operations
    // ========================================================================
    
    APIResponse executeBatch(const std::vector<String>& commands);
    APIResponse getMultipleStats();
    
private:
    RealMeshNode* node;
    NodeType currentNodeType;
    
    // Message storage
    struct StoredMessage {
        String from;
        String message;  
        uint32_t timestamp;
        bool isPublic;
        bool isEmergency;
    };
    
    std::vector<StoredMessage> messageHistory;
    static const size_t MAX_MESSAGE_HISTORY = 100;
    
    // Event system
    MessageCallback messageCallback;
    EventCallback eventCallback;
    std::vector<String> subscribedEvents;
    
    // Internal state
    bool initialized;
    uint32_t lastStatsUpdate;
    JsonDocument cachedStats;
    
    // Node callbacks setup
    void setupNodeCallbacks();
    // Member variables
    RealMeshNode* meshNode;
    int eventCallbackCount;
    
    struct EventCallbackEntry {
        String eventType;
        EventCallback callback;
    };
    EventCallbackEntry eventCallbacks[MAX_EVENT_CALLBACKS];
    
    // Helper functions
    APIResponse createSuccess(const String& message, const JsonDocument& data = JsonDocument());
    APIResponse createError(const String& message, const JsonDocument& data = JsonDocument());
    
    String nodeTypeToString(NodeType type);
    String messageTypeToString(MessageType type);
    JsonArray getNodeCapabilities(NodeType type);
    
    bool parseNodeAddress(const String& addressStr, NodeAddress& address);
    String formatNodeAddress(const NodeAddress& address);
    
    uint32_t generateMessageId();
    uint32_t getCurrentTimestamp();
    String formatUptime(uint32_t seconds);
    
    JsonDocument createMessageEventData(const MessagePacket& msg);
    JsonDocument createNodeEventData(const NodeAddress& address);
    
    bool isAlphaNumeric(char c);
    
    void triggerEvent(const String& eventType, const JsonDocument& eventData);
};

#endif // REALMESH_API_H
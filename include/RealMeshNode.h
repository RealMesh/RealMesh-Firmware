#ifndef REALMESH_NODE_H
#define REALMESH_NODE_H

#include "RealMeshTypes.h"
#include "RealMeshRadio.h"
#include "RealMeshRouter.h"
#include "RealMeshPacket.h"
#include <Preferences.h>

// ============================================================================
// Node Identity and Management System
// ============================================================================

class RealMeshNode {
public:
    // Node states
    enum NodeState {
        STATE_INITIALIZING,
        STATE_NAME_CONFLICT,
        STATE_DISCOVERING,
        STATE_OPERATIONAL,
        STATE_ERROR
    };
    
    // Constructor
    RealMeshNode();
    
    // Initialize node (loads or creates identity)
    bool begin(const String& desiredNodeId = "", const String& desiredSubdomain = "");
    
    // Main processing loop (call regularly)
    void loop();
    
    // Shutdown node
    void shutdown();
    
    // Identity management
    NodeAddress getOwnAddress() const { return ownAddress; }
    NodeUUID getOwnUUID() const { return ownAddress.uuid; }
    NodeState getCurrentState() const { return currentState; }
    void setDesiredName(const String& nodeId, const String& subdomain);
    bool isNameAvailable(const String& nodeId, const String& subdomain);
    
    // Node status
    void setStationary(bool stationary);
    bool isStationary() const { return router ? router->getOwnStatus() == NODE_STATIONARY : false; }
    
    // Messaging interface
    bool sendMessage(const String& targetAddress, const String& message);
    bool sendPublicMessage(const String& message);
    bool sendEmergencyMessage(const String& message);
    
    // Event callbacks
    typedef std::function<void(const String& from, const String& message)> OnMessageReceived;
    typedef std::function<void(const String& event, const String& details)> OnNetworkEvent;
    typedef std::function<void(NodeState oldState, NodeState newState)> OnStateChanged;
    
    void setOnMessageReceived(OnMessageReceived callback) { messageReceivedCallback = callback; }
    void setOnNetworkEvent(OnNetworkEvent callback) { networkEventCallback = callback; }
    void setOnStateChanged(OnStateChanged callback) { stateChangedCallback = callback; }
    
    // Network information
    size_t getKnownNodesCount();
    std::vector<String> getKnownNodes();
    NetworkStats getNetworkStats();
    
    // Configuration
    void setAutoHeartbeat(bool enabled) { autoHeartbeat = enabled; }
    void setVerboseLogging(bool enabled) { verboseLogging = enabled; }
    
    // Debug and maintenance
    void printNodeInfo();
    void printNetworkInfo();
    void runDiagnostics();
    void factoryReset();
    
private:
    // Core components
    RealMeshRadio* radio;
    RealMeshRouter* router;
    Preferences preferences;
    
    // Node identity
    NodeAddress ownAddress;
    NodeState currentState;
    String desiredNodeId;
    String desiredSubdomain;
    String baseNodeId;
    bool hasValidIdentity;
    
    // Name conflict resolution
    uint32_t nameConflictStartTime;
    uint8_t nameConflictRetries;
    std::vector<String> rejectedNames;
    bool nameConflictActive;
    
    // Network discovery
    uint32_t lastDiscoveryBroadcast;
    uint32_t discoveryStartTime;
    bool discoveryComplete;
    
    // Timing and maintenance
    uint32_t lastHeartbeat;
    uint32_t lastMaintenanceRun;
    uint32_t nodeStartTime;
    
    // Node statistics
    NodeStats nodeStats;
    
    // Configuration
    bool autoHeartbeat;
    bool verboseLogging;
    
    // Callbacks
    OnMessageReceived messageReceivedCallback;
    OnNetworkEvent networkEventCallback;
    OnStateChanged stateChangedCallback;
    
    // Initialization helpers
    bool loadStoredIdentity();
    bool createNewIdentity();
    bool storeIdentity();
    NodeUUID generateUUID();
    bool validateStoredIdentity();
    
    // Name management
    void startNameConflictResolution();
    void handleNameConflictTimeout();
    String generateAlternateName();
    bool isNameInRejectedList(const String& nodeId);
    void addToRejectedList(const String& nodeId);
    
    // Network discovery
    void startNetworkDiscovery();
    void broadcastPresence();
    void handleDiscoveryTimeout();
    void processDiscoveryResponse(const MessagePacket& packet);
    
    // State management
    void changeState(NodeState newState);
    void handleStateTransition(NodeState oldState, NodeState newState);
    
    // Radio and router callbacks
    void onRadioMessageReceived(const MessagePacket& packet, int16_t rssi, float snr);
    void onRadioTransmitComplete(bool success, const String& error);
    void onRouterMessageForUs(const MessagePacket& packet);
    void onRouteUpdate(const String& update);
    
    // Maintenance tasks
    void runPeriodicMaintenance();
    void cleanupOldData();
    void updateNodeStatistics();
    
    // Utility functions
    String addressToString(const NodeAddress& address);
    NodeAddress parseAddress(const String& addressString);
    bool isValidNodeId(const String& nodeId);
    bool isValidSubdomain(const String& subdomain);
    void logEvent(const String& level, const String& message);
    
    // EEPROM/NVS storage keys
    static const char* STORAGE_NAMESPACE;
    static const char* KEY_NODE_ID;
    static const char* KEY_SUBDOMAIN;
    static const char* KEY_UUID;
    static const char* KEY_FIRST_BOOT;
    static const char* KEY_BOOT_COUNT;
    static const char* KEY_TOTAL_UPTIME;
};

#endif // REALMESH_NODE_H
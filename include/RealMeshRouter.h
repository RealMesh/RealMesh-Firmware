#ifndef REALMESH_ROUTER_H
#define REALMESH_ROUTER_H

#include "RealMeshTypes.h"
#include "RealMeshPacket.h"
#include <map>
#include <vector>
#include <functional>

// ============================================================================
// Advanced Routing Engine
// ============================================================================

class RealMeshRouter {
public:
    // Callback types
    typedef std::function<bool(const MessagePacket&)> OnSendPacket;
    typedef std::function<void(const MessagePacket&)> OnMessageForUs;
    typedef std::function<void(const String&)> OnRouteUpdate;
    
    // Constructor
    RealMeshRouter(const NodeAddress& ownAddress);
    
    // Initialize routing engine
    bool begin();
    
    // Process incoming packet (returns true if we should forward it)
    bool processIncomingPacket(const MessagePacket& packet, int16_t rssi, float snr);
    
    // Route an outgoing message
    bool routeMessage(const NodeAddress& destination, const String& message, MessagePriority priority = PRIORITY_DIRECT);
    
    // Send different types of messages
    bool sendDirectMessage(const NodeAddress& destination, const String& message);
    bool sendPublicMessage(const String& message);
    bool sendEmergencyMessage(const String& message);
    bool sendHeartbeat();
    
    // Routing table management
    void addRoute(const NodeAddress& destination, const NodeAddress& nextHop, uint8_t hopCount = 1);
    void removeRoute(const NodeAddress& destination);
    void updateRouteQuality(const NodeAddress& destination, int16_t rssi, bool success);
    RoutingEntry* findRoute(const NodeAddress& destination);
    
    // Subdomain management
    void updateSubdomainInfo(const String& subdomain, const std::vector<NodeAddress>& nodes);
    std::vector<NodeAddress> getSubdomainNodes(const String& subdomain);
    bool isStationaryHub(const NodeAddress& node);
    void addStationaryHub(const NodeAddress& hub);
    
    // Intermediary bridge management
    void recordBridge(const NodeAddress& nodeA, const NodeAddress& nodeB);
    bool canBridge(const NodeAddress& nodeA, const NodeAddress& nodeB);
    std::vector<NodeAddress> findBridgeNodes(const String& targetSubdomain);
    
    // Network analysis
    size_t getRoutingTableSize() const { return routingTable.size(); }
    size_t getSubdomainCount() const { return subdomains.size(); }
    size_t getIntermediaryCount() const { return intermediaryMemory.size(); }
    NetworkStats getNetworkStats() const { return stats; }
    
    // Configuration
    void setOwnStatus(NodeStatus status);
    NodeStatus getOwnStatus() const { return ownStatus; }
    void setCallbacks(OnSendPacket sendCallback, OnMessageForUs messageCallback, OnRouteUpdate routeCallback);
    
    // Debugging
    void printRoutingTable();
    void printSubdomainInfo();
    void printIntermediaryMemory();
    void printNetworkStats();
    
private:
    // Core data
    NodeAddress ownAddress;
    NodeStatus ownStatus;
    std::map<String, RoutingEntry> routingTable;  // Key: full address
    std::map<String, SubdomainInfo> subdomains;   // Key: subdomain name
    std::vector<IntermediaryEntry> intermediaryMemory;
    NetworkStats stats;
    
    // Callbacks
    OnSendPacket sendCallback;
    OnMessageForUs messageCallback;
    OnRouteUpdate routeCallback;
    
    // Timing
    uint32_t lastHeartbeat;
    uint32_t lastRoutingTableCleanup;
    
    // Message processing helpers
    bool handleDataMessage(const MessagePacket& packet, int16_t rssi);
    bool handleControlMessage(const MessagePacket& packet, int16_t rssi);
    bool handleHeartbeatMessage(const MessagePacket& packet, int16_t rssi);
    bool handleAckMessage(const MessagePacket& packet, int16_t rssi);
    bool handleNameConflictMessage(const MessagePacket& packet, int16_t rssi);
    
    // Routing logic
    bool routePacketDirect(MessagePacket& packet);
    bool routePacketSubdomain(MessagePacket& packet);
    bool routePacketFlood(MessagePacket& packet);
    bool shouldForwardPacket(const MessagePacket& packet);
    void updatePathFromPacket(const MessagePacket& packet, int16_t rssi);
    
    // Subdomain routing intelligence
    std::vector<NodeAddress> findSubdomainHelpers(const String& targetSubdomain);
    bool isInOurSubdomain(const NodeAddress& address);
    void broadcastToSubdomain(const MessagePacket& packet);
    
    // Route discovery
    void initiateRouteDiscovery(const NodeAddress& destination);
    void handleRouteRequest(const MessagePacket& packet);
    void handleRouteReply(const MessagePacket& packet);
    
    // Table maintenance
    void cleanupRoutingTable();
    void cleanupIntermediaryMemory();
    void updateNetworkStats();
    bool isRouteExpired(const RoutingEntry& entry);
    
    // Utility functions
    String addressToKey(const NodeAddress& address);
    bool isValidPacket(const MessagePacket& packet);
    bool isPacketForUs(const MessagePacket& packet);
    void addToPathHistory(MessagePacket& packet);
    bool isInPathHistory(const MessagePacket& packet, const NodeAddress& address);
    uint8_t calculateHopDistance(const NodeAddress& destination);
};

#endif // REALMESH_ROUTER_H
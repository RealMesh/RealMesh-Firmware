#ifndef REALMESH_TYPES_H
#define REALMESH_TYPES_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "RealMeshConfig.h"

// ============================================================================
// Core Data Types
// ============================================================================

// Message Types
enum MessageType : uint8_t {
    MSG_DATA = 0x01,
    MSG_CONTROL = 0x02,
    MSG_HEARTBEAT = 0x03,
    MSG_ACK = 0x04,
    MSG_NACK = 0x05,
    MSG_ROUTE_REQUEST = 0x06,
    MSG_ROUTE_REPLY = 0x07,
    MSG_NAME_CONFLICT = 0x08
};

// Message Priority
enum MessagePriority : uint8_t {
    PRIORITY_EMERGENCY = 0x00,
    PRIORITY_DIRECT = 0x01,
    PRIORITY_PUBLIC = 0x02,
    PRIORITY_CONTROL = 0x03
};

// Routing Flags
enum RoutingFlags : uint8_t {
    ROUTE_DIRECT = 0x01,
    ROUTE_SUBDOMAIN_RETRY = 0x02,
    ROUTE_FLOOD = 0x04,
    ROUTE_INTERMEDIARY_ASSIST = 0x08,
    ROUTE_ENCRYPTED = 0x10
};

// Node Status
enum NodeStatus : uint8_t {
    NODE_OFFLINE = 0x00,
    NODE_MOBILE = 0x01,
    NODE_STATIONARY = 0x02,
    NODE_CONFLICT = 0x03
};

// UUID Structure (8 bytes)
struct NodeUUID {
    uint8_t bytes[RM_UUID_LENGTH];
    
    bool operator==(const NodeUUID& other) const {
        return memcmp(bytes, other.bytes, RM_UUID_LENGTH) == 0;
    }
    
    String toString() const {
        String result = "";
        for (int i = 0; i < RM_UUID_LENGTH; i++) {
            if (bytes[i] < 16) result += "0";
            result += String(bytes[i], HEX);
        }
        return result;
    }
};

// Node Address Structure
struct NodeAddress {
    String nodeId;           // e.g., "nicole1"
    String subdomain;        // e.g., "beograd"
    NodeUUID uuid;           // Hidden persistent identifier
    
    String getFullAddress() const {
        return nodeId + "@" + subdomain;
    }
    
    String getInternalAddress() const {
        return nodeId + "@" + subdomain + "_" + uuid.toString().substring(0, 4);
    }
    
    bool isValid() const {
        return !nodeId.isEmpty() && !subdomain.isEmpty();
    }
};

// Message Header Structure (32 bytes)
struct __attribute__((packed)) MessageHeader {
    uint32_t messageId;          // Unique message identifier
    uint32_t timestamp;          // Unix timestamp
    uint16_t sequenceNumber;     // Message sequence
    uint8_t protocolVersion;     // Protocol version
    uint8_t messageType;         // MessageType enum
    uint8_t priority;            // MessagePriority enum
    uint8_t routingFlags;        // RoutingFlags enum
    uint8_t hopCount;            // Current hop count
    uint8_t maxHops;             // Maximum allowed hops
    uint8_t payloadLength;       // Payload size in bytes
    uint8_t reserved;            // Reserved for future use
    uint8_t pathHistory[RM_PATH_HISTORY_SIZE]; // Last 3 hop node IDs
    uint16_t checksum;           // Header checksum
};

// Complete Message Packet
struct MessagePacket {
    MessageHeader header;
    NodeAddress source;
    NodeAddress destination;
    uint8_t payload[RM_MAX_PAYLOAD_SIZE];
    
    size_t getTotalSize() const {
        return sizeof(MessageHeader) + header.payloadLength;
    }
};

// Routing Table Entry
struct RoutingEntry {
    NodeAddress destination;
    NodeAddress nextHop;
    NodeAddress backupHop;
    uint32_t lastUsed;           // Last successful use timestamp
    uint16_t hopCount;           // Number of hops to destination
    uint8_t signalStrength;      // RSSI of last transmission
    uint8_t reliability;         // Success rate (0-100)
    bool isValid;
};

// Intermediary Memory Entry
struct IntermediaryEntry {
    NodeAddress nodeA;           // First node in connection
    NodeAddress nodeB;           // Second node in connection
    uint32_t lastBridged;        // Last time we bridged these nodes
    uint16_t bridgeCount;        // Number of times we've bridged them
    bool isActive;
};

// Subdomain Info
struct SubdomainInfo {
    String subdomainName;
    std::vector<NodeAddress> knownNodes;
    std::vector<NodeAddress> stationaryHubs;
    uint32_t lastUpdated;
    bool isLocal;                // True if this is our subdomain
};

// Message Queue Entry
struct QueueEntry {
    MessagePacket packet;
    uint32_t queuedTime;
    uint8_t retryCount;
    uint32_t nextRetryTime;
    MessagePriority priority;
};

// Node Statistics
struct NodeStats {
    uint32_t uptimeSeconds;
    uint32_t messagesReceived;
    uint32_t messagesSent;
    uint32_t lastHeartbeat;
};

// Network Statistics
struct NetworkStats {
    uint32_t messagesSent;
    uint32_t messagesReceived;
    uint32_t messagesForwarded;
    uint32_t messagesDropped;
    uint32_t routingTableSize;
    uint32_t lastHeartbeat;
    float avgRSSI;
    uint8_t networkLoad;         // 0-100 percentage
};

// Heartbeat Data Structure
struct HeartbeatData {
    NodeAddress sender;
    NodeStatus status;
    std::vector<NodeAddress> directContacts;
    std::vector<String> bridgedSubdomains;
    NetworkStats stats;
    uint32_t uptime;
};

#endif // REALMESH_TYPES_H
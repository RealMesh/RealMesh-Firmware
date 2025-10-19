#ifndef REALMESH_PACKET_H
#define REALMESH_PACKET_H

#include "RealMeshTypes.h"
#include <vector>
#include <Arduino.h>

// ============================================================================
// Message Packet Serialization/Deserialization
// ============================================================================

class RealMeshPacket {
public:
    // Serialize a message packet to byte array for transmission
    static std::vector<uint8_t> serialize(const MessagePacket& packet);
    
    // Deserialize byte array back to message packet
    static bool deserialize(const std::vector<uint8_t>& data, MessagePacket& packet);
    
    // Calculate message ID based on source and content
    static uint32_t generateMessageId(const NodeAddress& source, uint32_t timestamp, uint16_t sequence);
    
    // Validate packet header checksum
    static bool validateChecksum(const MessageHeader& header);
    
    // Calculate header checksum
    static uint16_t calculateChecksum(const MessageHeader& header);
    
    // Create different types of packets
    static MessagePacket createDataPacket(
        const NodeAddress& source,
        const NodeAddress& destination,
        const String& message,
        MessagePriority priority = PRIORITY_DIRECT,
        bool encrypted = false
    );
    
    static MessagePacket createHeartbeatPacket(
        const NodeAddress& source,
        const HeartbeatData& heartbeat
    );
    
    static MessagePacket createAckPacket(
        const NodeAddress& source,
        const NodeAddress& destination,
        uint32_t originalMessageId
    );
    
    static MessagePacket createNameConflictPacket(
        const NodeAddress& source,
        const NodeAddress& conflictingNode,
        const String& reason
    );
    
    static MessagePacket createRouteRequestPacket(
        const NodeAddress& source,
        const NodeAddress& destination,
        uint8_t maxHops = RM_MAX_HOP_COUNT
    );
    
    // Utility functions
    static String packetToString(const MessagePacket& packet);
    static void printPacketDebug(const MessagePacket& packet);
    
private:
    // Internal serialization helpers
    static void serializeNodeAddress(std::vector<uint8_t>& buffer, const NodeAddress& address);
    static bool deserializeNodeAddress(const uint8_t*& data, size_t& remaining, NodeAddress& address);
    static void serializeString(std::vector<uint8_t>& buffer, const String& str);
    static bool deserializeString(const uint8_t*& data, size_t& remaining, String& str);
    static void serializeUUID(std::vector<uint8_t>& buffer, const NodeUUID& uuid);
    static bool deserializeUUID(const uint8_t*& data, size_t& remaining, NodeUUID& uuid);
};

#endif // REALMESH_PACKET_H
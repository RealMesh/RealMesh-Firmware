#include "RealMeshPacket.h"
#include "RealMeshConfig.h"
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>

// ============================================================================
// Message Packet Implementation
// ============================================================================

std::vector<uint8_t> RealMeshPacket::serialize(const MessagePacket& packet) {
    std::vector<uint8_t> buffer;
    buffer.reserve(RM_MAX_PACKET_SIZE);
    
    // Serialize header (fixed size)
    const uint8_t* headerPtr = reinterpret_cast<const uint8_t*>(&packet.header);
    buffer.insert(buffer.end(), headerPtr, headerPtr + sizeof(MessageHeader));
    
    // Serialize source address
    serializeNodeAddress(buffer, packet.source);
    
    // Serialize destination address  
    serializeNodeAddress(buffer, packet.destination);
    
    // Serialize payload
    buffer.insert(buffer.end(), packet.payload, packet.payload + packet.header.payloadLength);
    
    return buffer;
}

bool RealMeshPacket::deserialize(const std::vector<uint8_t>& data, MessagePacket& packet) {
    if (data.size() < sizeof(MessageHeader)) {
        return false;
    }
    
    const uint8_t* ptr = data.data();
    size_t remaining = data.size();
    
    // Deserialize header
    memcpy(&packet.header, ptr, sizeof(MessageHeader));
    ptr += sizeof(MessageHeader);
    remaining -= sizeof(MessageHeader);
    
    // Validate header checksum
    if (!validateChecksum(packet.header)) {
        return false;
    }
    
    // Deserialize source address
    if (!deserializeNodeAddress(ptr, remaining, packet.source)) {
        return false;
    }
    
    // Deserialize destination address
    if (!deserializeNodeAddress(ptr, remaining, packet.destination)) {
        return false;
    }
    
    // Deserialize payload
    if (remaining < packet.header.payloadLength) {
        return false;
    }
    
    memcpy(packet.payload, ptr, packet.header.payloadLength);
    
    return true;
}

uint32_t RealMeshPacket::generateMessageId(const NodeAddress& source, uint32_t timestamp, uint16_t sequence) {
    // Create unique message ID based on source UUID, timestamp, and sequence
    uint32_t id = 0;
    
    // XOR first 4 bytes of UUID
    for (int i = 0; i < 4; i++) {
        id ^= (source.uuid.bytes[i] << (i * 8));
    }
    
    // Mix in timestamp and sequence
    id ^= timestamp;
    id ^= (sequence << 16);
    
    return id;
}

bool RealMeshPacket::validateChecksum(const MessageHeader& header) {
    MessageHeader temp = header;
    uint16_t originalChecksum = temp.checksum;
    temp.checksum = 0;
    
    return calculateChecksum(temp) == originalChecksum;
}

uint16_t RealMeshPacket::calculateChecksum(const MessageHeader& header) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&header);
    uint32_t sum = 0;
    
    for (size_t i = 0; i < sizeof(MessageHeader) - sizeof(uint16_t); i++) {
        sum += data[i];
    }
    
    return (uint16_t)(sum & 0xFFFF);
}

MessagePacket RealMeshPacket::createDataPacket(
    const NodeAddress& source,
    const NodeAddress& destination,  
    const String& message,
    MessagePriority priority,
    bool encrypted
) {
    MessagePacket packet = {};
    static uint16_t sequenceCounter = 0;
    
    // Fill header
    packet.header.protocolVersion = RM_PROTOCOL_VERSION;
    packet.header.messageType = MSG_DATA;
    packet.header.priority = priority;
    packet.header.routingFlags = ROUTE_DIRECT;
    if (encrypted) packet.header.routingFlags |= ROUTE_ENCRYPTED;
    packet.header.hopCount = 0;
    packet.header.maxHops = RM_MAX_HOP_COUNT;
    packet.header.timestamp = millis() / 1000; // Unix timestamp
    packet.header.sequenceNumber = ++sequenceCounter;
    packet.header.messageId = generateMessageId(source, packet.header.timestamp, packet.header.sequenceNumber);
    
    // Copy message to payload
    size_t messageLen = std::min((size_t)message.length(), (size_t)RM_MAX_PAYLOAD_SIZE - 1);
    message.getBytes(packet.payload, messageLen + 1);
    packet.header.payloadLength = messageLen;
    
    // Clear path history
    memset(packet.header.pathHistory, 0, RM_PATH_HISTORY_SIZE);
    
    // Set addresses
    packet.source = source;
    packet.destination = destination;
    
    // Calculate checksum
    packet.header.checksum = calculateChecksum(packet.header);
    
    return packet;
}

MessagePacket RealMeshPacket::createHeartbeatPacket(const NodeAddress& source, const HeartbeatData& heartbeat) {
    MessagePacket packet = {};
    static uint16_t sequenceCounter = 0;
    
    // Fill header
    packet.header.protocolVersion = RM_PROTOCOL_VERSION;
    packet.header.messageType = MSG_HEARTBEAT;
    packet.header.priority = PRIORITY_CONTROL;
    packet.header.routingFlags = ROUTE_FLOOD; // Heartbeats are always flooded
    packet.header.hopCount = 0;
    packet.header.maxHops = 3; // Limited flood for heartbeats
    packet.header.timestamp = millis() / 1000;
    packet.header.sequenceNumber = ++sequenceCounter;
    packet.header.messageId = generateMessageId(source, packet.header.timestamp, packet.header.sequenceNumber);
    
    // Serialize heartbeat data to JSON
    JsonDocument doc;
    doc["status"] = heartbeat.status;
    doc["uptime"] = heartbeat.uptime;
    doc["contacts"] = heartbeat.directContacts.size();
    doc["bridges"] = heartbeat.bridgedSubdomains.size();
    doc["sent"] = heartbeat.stats.messagesSent;
    doc["recv"] = heartbeat.stats.messagesReceived;
    doc["rssi"] = heartbeat.stats.avgRSSI;
    doc["load"] = heartbeat.stats.networkLoad;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    size_t jsonLen = std::min((size_t)jsonString.length(), (size_t)RM_MAX_PAYLOAD_SIZE - 1);
    jsonString.getBytes(packet.payload, jsonLen + 1);
    packet.header.payloadLength = jsonLen;
    
    // Set addresses (heartbeat destination is broadcast)
    packet.source = source;
    packet.destination = {}; // Empty destination = broadcast
    
    // Calculate checksum
    packet.header.checksum = calculateChecksum(packet.header);
    
    return packet;
}

MessagePacket RealMeshPacket::createAckPacket(
    const NodeAddress& source,
    const NodeAddress& destination,
    uint32_t originalMessageId
) {
    MessagePacket packet = {};
    static uint16_t sequenceCounter = 0;
    
    // Fill header
    packet.header.protocolVersion = RM_PROTOCOL_VERSION;
    packet.header.messageType = MSG_ACK;
    packet.header.priority = PRIORITY_CONTROL;
    packet.header.routingFlags = ROUTE_DIRECT;
    packet.header.hopCount = 0;
    packet.header.maxHops = RM_MAX_HOP_COUNT;
    packet.header.timestamp = millis() / 1000;
    packet.header.sequenceNumber = ++sequenceCounter;
    packet.header.messageId = generateMessageId(source, packet.header.timestamp, packet.header.sequenceNumber);
    
    // Put original message ID in payload
    memcpy(packet.payload, &originalMessageId, sizeof(uint32_t));
    packet.header.payloadLength = sizeof(uint32_t);
    
    // Set addresses
    packet.source = source;
    packet.destination = destination;
    
    // Calculate checksum
    packet.header.checksum = calculateChecksum(packet.header);
    
    return packet;
}

MessagePacket RealMeshPacket::createNameConflictPacket(
    const NodeAddress& source,
    const NodeAddress& conflictingNode,
    const String& reason
) {
    MessagePacket packet = {};
    static uint16_t sequenceCounter = 0;
    
    // Fill header
    packet.header.protocolVersion = RM_PROTOCOL_VERSION;
    packet.header.messageType = MSG_NAME_CONFLICT;
    packet.header.priority = PRIORITY_CONTROL;
    packet.header.routingFlags = ROUTE_DIRECT;
    packet.header.hopCount = 0;
    packet.header.maxHops = 1; // Direct only
    packet.header.timestamp = millis() / 1000;
    packet.header.sequenceNumber = ++sequenceCounter;
    packet.header.messageId = generateMessageId(source, packet.header.timestamp, packet.header.sequenceNumber);
    
    // Put reason in payload
    size_t reasonLen = std::min((size_t)reason.length(), (size_t)RM_MAX_PAYLOAD_SIZE - 1);
    reason.getBytes(packet.payload, reasonLen + 1);
    packet.header.payloadLength = reasonLen;
    
    // Set addresses
    packet.source = source;
    packet.destination = conflictingNode;
    
    // Calculate checksum
    packet.header.checksum = calculateChecksum(packet.header);
    
    return packet;
}

String RealMeshPacket::packetToString(const MessagePacket& packet) {
    String result = "Packet[";
    result += "ID:" + String(packet.header.messageId, HEX);
    result += " Type:" + String(packet.header.messageType);
    result += " From:" + packet.source.getFullAddress();
    result += " To:" + packet.destination.getFullAddress();
    result += " Hops:" + String(packet.header.hopCount);
    result += " Len:" + String(packet.header.payloadLength);
    result += "]";
    return result;
}

void RealMeshPacket::printPacketDebug(const MessagePacket& packet) {
    Serial.println("=== PACKET DEBUG ===");
    Serial.printf("Message ID: 0x%08X\n", packet.header.messageId);
    Serial.printf("Type: %d, Priority: %d\n", packet.header.messageType, packet.header.priority);
    Serial.printf("Routing Flags: 0x%02X\n", packet.header.routingFlags);
    Serial.printf("Hop Count: %d/%d\n", packet.header.hopCount, packet.header.maxHops);
    Serial.printf("Source: %s\n", packet.source.getFullAddress().c_str());
    Serial.printf("Destination: %s\n", packet.destination.getFullAddress().c_str());
    Serial.printf("Payload Length: %d\n", packet.header.payloadLength);
    Serial.printf("Timestamp: %d\n", packet.header.timestamp);
    Serial.println("==================");
}

// Private helper methods

void RealMeshPacket::serializeNodeAddress(std::vector<uint8_t>& buffer, const NodeAddress& address) {
    serializeString(buffer, address.nodeId);
    serializeString(buffer, address.subdomain);
    serializeUUID(buffer, address.uuid);
}

bool RealMeshPacket::deserializeNodeAddress(const uint8_t*& data, size_t& remaining, NodeAddress& address) {
    return deserializeString(data, remaining, address.nodeId) &&
           deserializeString(data, remaining, address.subdomain) &&
           deserializeUUID(data, remaining, address.uuid);
}

void RealMeshPacket::serializeString(std::vector<uint8_t>& buffer, const String& str) {
    uint8_t len = (uint8_t)std::min((size_t)str.length(), (size_t)255);
    buffer.push_back(len);
    for (int i = 0; i < len; i++) {
        buffer.push_back(str[i]);
    }
}

bool RealMeshPacket::deserializeString(const uint8_t*& data, size_t& remaining, String& str) {
    if (remaining < 1) return false;
    
    uint8_t len = *data++;
    remaining--;
    
    if (remaining < len) return false;
    
    str = "";
    for (int i = 0; i < len; i++) {
        str += (char)data[i];
    }
    
    data += len;
    remaining -= len;
    return true;
}

void RealMeshPacket::serializeUUID(std::vector<uint8_t>& buffer, const NodeUUID& uuid) {
    buffer.insert(buffer.end(), uuid.bytes, uuid.bytes + RM_UUID_LENGTH);
}

bool RealMeshPacket::deserializeUUID(const uint8_t*& data, size_t& remaining, NodeUUID& uuid) {
    if (remaining < RM_UUID_LENGTH) return false;
    
    memcpy(uuid.bytes, data, RM_UUID_LENGTH);
    data += RM_UUID_LENGTH;
    remaining -= RM_UUID_LENGTH;
    return true;
}
#include "RealMeshRouter.h"
#include "RealMeshConfig.h"
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include <algorithm>

// ============================================================================
// Advanced Routing Engine Implementation
// ============================================================================

RealMeshRouter::RealMeshRouter(const NodeAddress& ownAddress) :
    ownAddress(ownAddress),
    ownStatus(NODE_MOBILE),
    lastHeartbeat(0),
    lastRoutingTableCleanup(0),
    sendCallback(nullptr),
    messageCallback(nullptr),
    routeCallback(nullptr) {
    
    // Initialize network stats
    stats = {};
    stats.lastHeartbeat = millis();
}

bool RealMeshRouter::begin() {
    Serial.printf("[ROUTER] Starting routing engine for %s\n", ownAddress.getFullAddress().c_str());
    
    // Initialize our own subdomain info
    SubdomainInfo& ourSubdomain = subdomains[ownAddress.subdomain];
    ourSubdomain.subdomainName = ownAddress.subdomain;
    ourSubdomain.knownNodes.push_back(ownAddress);
    ourSubdomain.lastUpdated = millis();
    ourSubdomain.isLocal = true;
    
    // If we're stationary, add ourselves as a hub
    if (ownStatus == NODE_STATIONARY) {
        addStationaryHub(ownAddress);
    }
    
    Serial.println("[ROUTER] Routing engine started successfully");
    return true;
}

bool RealMeshRouter::processIncomingPacket(const MessagePacket& packet, int16_t rssi, float snr) {
    if (!isValidPacket(packet)) {
        Serial.println("[ROUTER] Invalid packet received");
        return false;
    }
    
    // Update statistics
    stats.messagesReceived++;
    stats.avgRSSI = (stats.avgRSSI * 0.9f) + (rssi * 0.1f);
    
    // Learn route from this packet if it's not from us
    if (packet.source.getFullAddress() != ownAddress.getFullAddress()) {
        updatePathFromPacket(packet, rssi);
    }
    
    // Check if packet is for us
    if (isPacketForUs(packet)) {
        // Handle different message types
        switch (packet.header.messageType) {
            case MSG_DATA:
                return handleDataMessage(packet, rssi);
            case MSG_CONTROL:
            case MSG_ROUTE_REQUEST:
            case MSG_ROUTE_REPLY:
                return handleControlMessage(packet, rssi);
            case MSG_HEARTBEAT:
                return handleHeartbeatMessage(packet, rssi);
            case MSG_ACK:
            case MSG_NACK:
                return handleAckMessage(packet, rssi);
            case MSG_NAME_CONFLICT:
                return handleNameConflictMessage(packet, rssi);
            default:
                Serial.printf("[ROUTER] Unknown message type: %d\n", packet.header.messageType);
                return false;
        }
    }
    
    // Check if we should forward this packet
    return shouldForwardPacket(packet);
}

bool RealMeshRouter::routeMessage(const NodeAddress& destination, const String& message, MessagePriority priority) {
    if (!sendCallback) {
        Serial.println("[ROUTER] No send callback configured");
        return false;
    }
    
    // Create data packet
    MessagePacket packet = RealMeshPacket::createDataPacket(ownAddress, destination, message, priority);
    
    Serial.printf("[ROUTER] Routing message to %s: %s\n", 
                 destination.getFullAddress().c_str(), message.c_str());
    
    // Try different routing strategies in order
    if (routePacketDirect(packet)) {
        return true;
    }
    
    if (routePacketSubdomain(packet)) {
        return true;
    }
    
    if (routePacketFlood(packet)) {
        return true;
    }
    
    Serial.printf("[ROUTER] Failed to route message to %s\n", destination.getFullAddress().c_str());
    return false;
}

bool RealMeshRouter::sendDirectMessage(const NodeAddress& destination, const String& message) {
    return routeMessage(destination, message, PRIORITY_DIRECT);
}

bool RealMeshRouter::sendPublicMessage(const String& message) {
    NodeAddress broadcast = {};  // Empty address = broadcast
    return routeMessage(broadcast, message, PRIORITY_PUBLIC);
}

bool RealMeshRouter::sendEmergencyMessage(const String& message) {
    NodeAddress broadcast = {};  // Emergency is always broadcast
    return routeMessage(broadcast, message, PRIORITY_EMERGENCY);
}

bool RealMeshRouter::sendHeartbeat() {
    // Allow heartbeats every 3 seconds during initial discovery, then use normal intervals
    uint32_t minInterval = (millis() < 60000) ? 3000 : (ownStatus == NODE_STATIONARY ? RM_HEARTBEAT_STATIONARY : RM_HEARTBEAT_MOBILE);
    
    if (millis() - lastHeartbeat < minInterval) {
        return true; // Too soon for another heartbeat
    }
    
    // Prepare heartbeat data
    HeartbeatData heartbeat;
    heartbeat.sender = ownAddress;
    heartbeat.status = ownStatus;
    heartbeat.stats = stats;
    heartbeat.uptime = millis();
    
    // Add direct contacts from our subdomain
    if (subdomains.find(ownAddress.subdomain) != subdomains.end()) {
        heartbeat.directContacts = subdomains[ownAddress.subdomain].knownNodes;
    }
    
    // Add bridged subdomains
    for (const auto& entry : intermediaryMemory) {
        if (entry.isActive && entry.nodeA.subdomain != entry.nodeB.subdomain) {
            String bridgedDomain = entry.nodeA.subdomain == ownAddress.subdomain ? 
                                  entry.nodeB.subdomain : entry.nodeA.subdomain;
            if (std::find(heartbeat.bridgedSubdomains.begin(), heartbeat.bridgedSubdomains.end(), bridgedDomain) == heartbeat.bridgedSubdomains.end()) {
                heartbeat.bridgedSubdomains.push_back(bridgedDomain);
            }
        }
    }
    
    // Create and send heartbeat packet
    MessagePacket packet = RealMeshPacket::createHeartbeatPacket(ownAddress, heartbeat);
    
    if (sendCallback && sendCallback(packet)) {
        lastHeartbeat = millis();
        stats.lastHeartbeat = lastHeartbeat;
        stats.messagesSent++;
        Serial.printf("[ROUTER] Sent heartbeat (status: %d, contacts: %d, bridges: %d)\n", 
                     ownStatus, heartbeat.directContacts.size(), heartbeat.bridgedSubdomains.size());
        return true;
    }
    
    return false;
}

void RealMeshRouter::addRoute(const NodeAddress& destination, const NodeAddress& nextHop, uint8_t hopCount) {
    String key = addressToKey(destination);
    
    RoutingEntry& entry = routingTable[key];
    entry.destination = destination;
    entry.nextHop = nextHop;
    entry.hopCount = hopCount;
    entry.lastUsed = millis();
    entry.signalStrength = 0; // Will be updated on first use
    entry.reliability = 100;   // Start optimistic
    entry.isValid = true;
    
    Serial.printf("[ROUTER] Added route: %s -> %s (hops: %d)\n",
                 destination.getFullAddress().c_str(),
                 nextHop.getFullAddress().c_str(),
                 hopCount);
    
    if (routeCallback) {
        routeCallback("Route added: " + destination.getFullAddress());
    }
}

void RealMeshRouter::removeRoute(const NodeAddress& destination) {
    String key = addressToKey(destination);
    
    if (routingTable.find(key) != routingTable.end()) {
        Serial.printf("[ROUTER] Removed route to %s\n", destination.getFullAddress().c_str());
        routingTable.erase(key);
        
        if (routeCallback) {
            routeCallback("Route removed: " + destination.getFullAddress());
        }
    }
}

void RealMeshRouter::updateRouteQuality(const NodeAddress& destination, int16_t rssi, bool success) {
    String key = addressToKey(destination);
    
    if (routingTable.find(key) != routingTable.end()) {
        RoutingEntry& entry = routingTable[key];
        entry.lastUsed = millis();
        entry.signalStrength = rssi;
        
        // Update reliability score
        if (success) {
            entry.reliability = min(100, entry.reliability + 5);
        } else {
            entry.reliability = max(0, entry.reliability - 20);
        }
        
        // Remove route if reliability drops too low
        if (entry.reliability < 20) {
            Serial.printf("[ROUTER] Route to %s reliability too low, removing\n", 
                         destination.getFullAddress().c_str());
            removeRoute(destination);
        }
    }
}

RoutingEntry* RealMeshRouter::findRoute(const NodeAddress& destination) {
    String key = addressToKey(destination);
    
    auto it = routingTable.find(key);
    if (it != routingTable.end() && it->second.isValid && !isRouteExpired(it->second)) {
        return &it->second;
    }
    
    return nullptr;
}

void RealMeshRouter::recordBridge(const NodeAddress& nodeA, const NodeAddress& nodeB) {
    // Check if we already have this bridge recorded
    for (auto& entry : intermediaryMemory) {
        if ((entry.nodeA.getFullAddress() == nodeA.getFullAddress() && 
             entry.nodeB.getFullAddress() == nodeB.getFullAddress()) ||
            (entry.nodeA.getFullAddress() == nodeB.getFullAddress() && 
             entry.nodeB.getFullAddress() == nodeA.getFullAddress())) {
            
            entry.lastBridged = millis();
            entry.bridgeCount++;
            entry.isActive = true;
            return;
        }
    }
    
    // Add new bridge entry
    IntermediaryEntry newEntry;
    newEntry.nodeA = nodeA;
    newEntry.nodeB = nodeB;
    newEntry.lastBridged = millis();
    newEntry.bridgeCount = 1;
    newEntry.isActive = true;
    
    intermediaryMemory.push_back(newEntry);
    
    Serial.printf("[ROUTER] Recorded bridge: %s <-> %s\n",
                 nodeA.getFullAddress().c_str(),
                 nodeB.getFullAddress().c_str());
}

bool RealMeshRouter::canBridge(const NodeAddress& nodeA, const NodeAddress& nodeB) {
    // We can bridge if we know routes to both nodes
    return findRoute(nodeA) != nullptr && findRoute(nodeB) != nullptr;
}

void RealMeshRouter::setOwnStatus(NodeStatus status) {
    if (ownStatus != status) {
        Serial.printf("[ROUTER] Node status changed: %d -> %d\n", ownStatus, status);
        ownStatus = status;
        
        // Update subdomain hub status
        if (status == NODE_STATIONARY) {
            addStationaryHub(ownAddress);
        }
        
        // Send immediate heartbeat to announce status change
        sendHeartbeat();
    }
}

void RealMeshRouter::setCallbacks(OnSendPacket sendCallback, OnMessageForUs messageCallback, OnRouteUpdate routeCallback) {
    this->sendCallback = sendCallback;
    this->messageCallback = messageCallback;
    this->routeCallback = routeCallback;
}

// Private implementation methods

bool RealMeshRouter::handleDataMessage(const MessagePacket& packet, int16_t rssi) {
    Serial.printf("[ROUTER] Received data message from %s: %.*s\n",
                 packet.source.getFullAddress().c_str(),
                 packet.header.payloadLength,
                 (char*)packet.payload);
    
    // Send ACK back to sender
    MessagePacket ackPacket = RealMeshPacket::createAckPacket(ownAddress, packet.source, packet.header.messageId);
    if (sendCallback) {
        sendCallback(ackPacket);
    }
    
    // Deliver message to application
    if (messageCallback) {
        messageCallback(packet);
    }
    
    return false; // Don't forward - message was for us
}

bool RealMeshRouter::routePacketDirect(MessagePacket& packet) {
    RoutingEntry* route = findRoute(packet.destination);
    
    if (route) {
        Serial.printf("[ROUTER] Using direct route to %s via %s\n",
                     packet.destination.getFullAddress().c_str(),
                     route->nextHop.getFullAddress().c_str());
        
        packet.header.routingFlags = ROUTE_DIRECT;
        addToPathHistory(packet);
        
        if (sendCallback && sendCallback(packet)) {
            stats.messagesSent++;
            route->lastUsed = millis();
            return true;
        }
    }
    
    return false;
}

bool RealMeshRouter::routePacketSubdomain(MessagePacket& packet) {
    // Only try subdomain routing if destination has different subdomain
    if (packet.destination.subdomain == ownAddress.subdomain) {
        return false;
    }
    
    // Find stationary hubs in target subdomain
    std::vector<NodeAddress> helpers = findSubdomainHelpers(packet.destination.subdomain);
    
    for (const NodeAddress& helper : helpers) {
        RoutingEntry* route = findRoute(helper);
        if (route) {
            Serial.printf("[ROUTER] Using subdomain route to %s via hub %s\n",
                         packet.destination.getFullAddress().c_str(),
                         helper.getFullAddress().c_str());
            
            packet.header.routingFlags = ROUTE_SUBDOMAIN_RETRY;
            addToPathHistory(packet);
            
            // Temporarily change destination to the helper
            NodeAddress originalDest = packet.destination;
            packet.destination = helper;
            
            if (sendCallback && sendCallback(packet)) {
                // Restore original destination
                packet.destination = originalDest;
                stats.messagesSent++;
                return true;
            }
            
            packet.destination = originalDest;
        }
    }
    
    return false;
}

bool RealMeshRouter::routePacketFlood(MessagePacket& packet) {
    Serial.printf("[ROUTER] Using flood routing for %s\n", packet.destination.getFullAddress().c_str());
    
    packet.header.routingFlags = ROUTE_FLOOD;
    packet.header.hopCount = 0;
    addToPathHistory(packet);
    
    if (sendCallback && sendCallback(packet)) {
        stats.messagesSent++;
        return true;
    }
    
    return false;
}

bool RealMeshRouter::shouldForwardPacket(const MessagePacket& packet) {
    // Don't forward if we've seen this packet before (loop prevention)
    if (isInPathHistory(packet, ownAddress)) {
        return false;
    }
    
    // Don't forward if hop count exceeded
    if (packet.header.hopCount >= packet.header.maxHops) {
        return false;
    }
    
    // If we're a stationary hub and this is for our subdomain, help forward it
    if (ownStatus == NODE_STATIONARY && 
        packet.destination.subdomain == ownAddress.subdomain &&
        (packet.header.routingFlags & ROUTE_SUBDOMAIN_RETRY)) {
        
        Serial.printf("[ROUTER] Acting as subdomain hub for %s\n", packet.destination.getFullAddress().c_str());
        
        // Try to forward to the actual destination
        RoutingEntry* route = findRoute(packet.destination);
        if (route) {
            MessagePacket forwardPacket = packet;
            forwardPacket.header.hopCount++;
            addToPathHistory(forwardPacket);
            
            if (sendCallback && sendCallback(forwardPacket)) {
                stats.messagesForwarded++;
                
                // Record this as a successful bridge
                recordBridge(packet.source, packet.destination);
                return true;
            }
        }
    }
    
    // Forward flood messages (with hop limit)
    if (packet.header.routingFlags & ROUTE_FLOOD) {
        MessagePacket forwardPacket = packet;
        forwardPacket.header.hopCount++;
        addToPathHistory(forwardPacket);
        
        if (sendCallback && sendCallback(forwardPacket)) {
            stats.messagesForwarded++;
            return true;
        }
    }
    
    return false;
}

void RealMeshRouter::updatePathFromPacket(const MessagePacket& packet, int16_t rssi) {
    // If packet came directly to us, we have a direct route to sender
    if (packet.header.hopCount == 0) {
        addRoute(packet.source, packet.source, 1);
        updateRouteQuality(packet.source, rssi, true);
    } else {
        // Learn multi-hop route (source is reachable via previous hop)
        if (packet.header.pathHistory[0] != 0) {
            // TODO: Decode previous hop from path history and add route
        }
    }
}

std::vector<NodeAddress> RealMeshRouter::findSubdomainHelpers(const String& targetSubdomain) {
    std::vector<NodeAddress> helpers;
    
    if (subdomains.find(targetSubdomain) != subdomains.end()) {
        const SubdomainInfo& info = subdomains[targetSubdomain];
        for (const NodeAddress& hub : info.stationaryHubs) {
            if (findRoute(hub) != nullptr) {
                helpers.push_back(hub);
            }
        }
    }
    
    return helpers;
}

void RealMeshRouter::addStationaryHub(const NodeAddress& hub) {
    SubdomainInfo& info = subdomains[hub.subdomain];
    
    // Check if already in the list
    for (const NodeAddress& existing : info.stationaryHubs) {
        if (existing.getFullAddress() == hub.getFullAddress()) {
            return;
        }
    }
    
    info.stationaryHubs.push_back(hub);
    Serial.printf("[ROUTER] Added stationary hub: %s for subdomain %s\n",
                 hub.getFullAddress().c_str(),
                 hub.subdomain.c_str());
}

String RealMeshRouter::addressToKey(const NodeAddress& address) {
    return address.getFullAddress();
}

bool RealMeshRouter::isValidPacket(const MessagePacket& packet) {
    return packet.source.isValid() && 
           packet.header.protocolVersion == RM_PROTOCOL_VERSION &&
           packet.header.payloadLength <= RM_MAX_PAYLOAD_SIZE;
}

bool RealMeshRouter::isPacketForUs(const MessagePacket& packet) {
    // Check if destination matches our address
    if (packet.destination.getFullAddress() == ownAddress.getFullAddress()) {
        return true;
    }
    
    // Check if it's a broadcast (empty destination)
    if (packet.destination.nodeId.isEmpty() && packet.destination.subdomain.isEmpty()) {
        return true;
    }
    
    // Check if it's a subdomain broadcast for our subdomain
    if (packet.destination.nodeId.isEmpty() && packet.destination.subdomain == ownAddress.subdomain) {
        return true;
    }
    
    return false;
}

void RealMeshRouter::addToPathHistory(MessagePacket& packet) {
    // Shift path history and add ourselves
    for (int i = RM_PATH_HISTORY_SIZE - 1; i > 0; i--) {
        packet.header.pathHistory[i] = packet.header.pathHistory[i-1];
    }
    
    // Add our node ID (simplified to first byte of UUID)
    packet.header.pathHistory[0] = ownAddress.uuid.bytes[0];
}

bool RealMeshRouter::isInPathHistory(const MessagePacket& packet, const NodeAddress& address) {
    uint8_t nodeId = address.uuid.bytes[0];
    
    for (int i = 0; i < RM_PATH_HISTORY_SIZE; i++) {
        if (packet.header.pathHistory[i] == nodeId) {
            return true;
        }
    }
    
    return false;
}

bool RealMeshRouter::isRouteExpired(const RoutingEntry& entry) {
    // Routes expire after 1 hour of non-use for mobile nodes
    // Stationary routes expire after 24 hours
    uint32_t expireTime = (ownStatus == NODE_STATIONARY) ? 86400000 : 3600000;
    return (millis() - entry.lastUsed) > expireTime;
}

void RealMeshRouter::printRoutingTable() {
    Serial.printf("[ROUTER] Routing Table (%d entries):\n", routingTable.size());
    for (const auto& pair : routingTable) {
        const RoutingEntry& entry = pair.second;
        Serial.printf("  %s -> %s (hops: %d, rel: %d%%, rssi: %ddBm)\n",
                     entry.destination.getFullAddress().c_str(),
                     entry.nextHop.getFullAddress().c_str(),
                     entry.hopCount,
                     entry.reliability,
                     entry.signalStrength);
    }
}

void RealMeshRouter::printSubdomainInfo() {
    Serial.printf("[ROUTER] Subdomain Information (%d subdomains):\n", subdomains.size());
    for (const auto& pair : subdomains) {
        const SubdomainInfo& info = pair.second;
        Serial.printf("  %s: %d nodes, %d hubs, %s\n",
                     info.subdomainName.c_str(),
                     info.knownNodes.size(),
                     info.stationaryHubs.size(),
                     info.isLocal ? "LOCAL" : "REMOTE");
    }
}

void RealMeshRouter::printIntermediaryMemory() {
    Serial.printf("[ROUTER] Intermediary Memory (%d bridges):\n", intermediaryMemory.size());
    for (const auto& entry : intermediaryMemory) {
        if (entry.isActive) {
            Serial.printf("  %s <-> %s (bridges: %d)\n",
                         entry.nodeA.getFullAddress().c_str(),
                         entry.nodeB.getFullAddress().c_str(),
                         entry.bridgeCount);
        }
    }
}

void RealMeshRouter::printNetworkStats() {
    Serial.println("=== NETWORK STATISTICS ===");
    Serial.printf("Messages Sent: %d\n", stats.messagesSent);
    Serial.printf("Messages Received: %d\n", stats.messagesReceived);
    Serial.printf("Messages Forwarded: %d\n", stats.messagesForwarded);
    Serial.printf("Messages Dropped: %d\n", stats.messagesDropped);
    Serial.printf("Routing Table Size: %d\n", stats.routingTableSize);
    Serial.printf("Average RSSI: %.1f dBm\n", stats.avgRSSI);
    Serial.printf("Network Load: %d%%\n", stats.networkLoad);
    Serial.printf("Last Heartbeat: %d ms ago\n", millis() - stats.lastHeartbeat);
}

// Missing method implementations
bool RealMeshRouter::handleNameConflictMessage(const MessagePacket& packet, int16_t rssi) {
    Serial.printf("[ROUTER] Name conflict message from %s\n", packet.source.getFullAddress().c_str());
    
    // Parse name conflict resolution data from payload
    // In a real implementation, this would:
    // 1. Check if this affects our node
    // 2. Participate in name resolution protocol
    // 3. Update routing tables if names change
    
    Serial.println("[ROUTER] Name conflict message processed");
    return true;
}

bool RealMeshRouter::handleControlMessage(const MessagePacket& packet, int16_t rssi) {
    Serial.printf("[ROUTER] Control message from %s (RSSI: %d)\n", 
                  packet.source.getFullAddress().c_str(), rssi);
    
    // Process control messages like:
    // - Route announcements
    // - Network topology updates
    // - Configuration changes
    
    // For now, just acknowledge receipt
    Serial.println("[ROUTER] Control message processed");
    return true;
}

bool RealMeshRouter::handleHeartbeatMessage(const MessagePacket& packet, int16_t rssi) {
    Serial.printf("[ROUTER] Heartbeat from %s (RSSI: %d)\n", 
                  packet.source.getFullAddress().c_str(), rssi);
    
    // Update node information and routing tables
    NodeAddress source = packet.source;
    
    // Add or update routing entry for direct neighbor
    addRoute(source, source, 1);
    
    // Parse heartbeat payload for additional network info
    // (battery level, node type, neighboring nodes, etc.)
    
    Serial.println("[ROUTER] Heartbeat processed");
    return true;
}

bool RealMeshRouter::handleAckMessage(const MessagePacket& packet, int16_t rssi) {
    Serial.printf("[ROUTER] ACK message from %s (RSSI: %d)\n", 
                  packet.source.getFullAddress().c_str(), rssi);
    
    // Process acknowledgments:
    // 1. Mark messages as successfully delivered
    // 2. Update reliability statistics
    // 3. Clear retry queues
    
    // Extract message ID from payload and mark as acknowledged
    uint32_t ackedMessageId = packet.header.messageId;
    Serial.printf("[ROUTER] Message %u acknowledged\n", ackedMessageId);
    return true;
}
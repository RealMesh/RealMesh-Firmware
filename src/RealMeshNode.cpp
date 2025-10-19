#include "RealMeshNode.h"
#include "RealMeshConfig.h"
#include <esp_random.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <vector>
#include <algorithm>

// ============================================================================
// Node Identity and Management Implementation
// ============================================================================

// Storage keys
const char* RealMeshNode::STORAGE_NAMESPACE = "rm";
const char* RealMeshNode::KEY_NODE_ID = "node_id";
const char* RealMeshNode::KEY_SUBDOMAIN = "subdomain";
const char* RealMeshNode::KEY_UUID = "uuid";
const char* RealMeshNode::KEY_FIRST_BOOT = "first_boot";
const char* RealMeshNode::KEY_BOOT_COUNT = "boot_count";
const char* RealMeshNode::KEY_TOTAL_UPTIME = "total_uptime";

RealMeshNode::RealMeshNode() :
    radio(nullptr),
    router(nullptr),
    currentState(STATE_INITIALIZING),
    hasValidIdentity(false),
    nameConflictStartTime(0),
    nameConflictRetries(0),
    lastDiscoveryBroadcast(0),
    discoveryStartTime(0),
    discoveryComplete(false),
    lastHeartbeat(0),
    lastMaintenanceRun(0),
    nodeStartTime(0),
    autoHeartbeat(true),
    verboseLogging(false),
    messageReceivedCallback(nullptr),
    networkEventCallback(nullptr),
    stateChangedCallback(nullptr) {
    
    nodeStartTime = millis();
}

bool RealMeshNode::begin(const String& desiredNodeId, const String& desiredSubdomain) {
    Serial.println("[NODE] Starting RealMesh node...");
    
    // Initialize preferences
    if (!preferences.begin(STORAGE_NAMESPACE, false)) {
        Serial.println("[NODE] Failed to initialize storage");
        changeState(STATE_ERROR);
        return false;
    }
    
    // Update boot counter
    uint32_t bootCount = preferences.getUInt(KEY_BOOT_COUNT, 0) + 1;
    preferences.putUInt(KEY_BOOT_COUNT, bootCount);
    Serial.printf("[NODE] Boot count: %d\n", bootCount);
    
    // Set desired name if provided
    if (!desiredNodeId.isEmpty()) {
        this->desiredNodeId = desiredNodeId;
    }
    if (!desiredSubdomain.isEmpty()) {
        this->desiredSubdomain = desiredSubdomain;
    }
    
    // Try to load existing identity, create new if needed
    if (!loadStoredIdentity()) {
        Serial.println("[NODE] No stored identity found, creating new identity");
        if (!createNewIdentity()) {
            Serial.println("[NODE] Failed to create new identity");
            changeState(STATE_ERROR);
            return false;
        }
    }
    
    Serial.printf("[NODE] Node identity: %s (UUID: %s)\n", 
                 ownAddress.getFullAddress().c_str(),
                 ownAddress.uuid.toString().c_str());
    
    // Initialize radio
    radio = new RealMeshRadio();
    if (!radio->begin()) {
        Serial.println("[NODE] Failed to initialize radio");
        changeState(STATE_ERROR);
        return false;
    }
    
    // Set radio callbacks
    radio->setOnMessageReceived([this](const MessagePacket& packet, int16_t rssi, float snr) {
        this->onRadioMessageReceived(packet, rssi, snr);
    });
    radio->setOnTransmitComplete([this](bool success, const String& error) {
        this->onRadioTransmitComplete(success, error);
    });
    
    // Initialize router
    router = new RealMeshRouter(ownAddress);
    if (!router->begin()) {
        Serial.println("[NODE] Failed to initialize router");
        changeState(STATE_ERROR);
        return false;
    }
    
    // Set router callbacks
    router->setCallbacks(
        [this](const MessagePacket& packet) -> bool {
            return radio->sendPacket(packet);
        },
        [this](const MessagePacket& packet) {
            this->onRouterMessageForUs(packet);
        },
        [this](const String& update) {
            this->onRouteUpdate(update);
        }
    );
    
    // Start network discovery
    startNetworkDiscovery();
    
    Serial.println("[NODE] RealMesh node started successfully");
    return true;
}

void RealMeshNode::loop() {
    if (currentState == STATE_ERROR) return;
    
    // Process radio
    if (radio) {
        radio->processIncoming();
    }
    
    // Handle state-specific processing
    switch (currentState) {
        case STATE_NAME_CONFLICT:
            handleNameConflictTimeout();
            break;
            
        case STATE_DISCOVERING:
            handleDiscoveryTimeout();
            // Fall through to operational tasks
            
        case STATE_OPERATIONAL:
            // Send periodic heartbeats
            if (autoHeartbeat && router) {
                router->sendHeartbeat();
            }
            break;
            
        default:
            break;
    }
    
    // Run periodic maintenance
    if (millis() - lastMaintenanceRun > 60000) { // Every minute
        runPeriodicMaintenance();
        lastMaintenanceRun = millis();
    }
}

void RealMeshNode::shutdown() {
    Serial.println("[NODE] Shutting down RealMesh node...");
    
    // Store final statistics
    if (preferences.begin(STORAGE_NAMESPACE, false)) {
        uint32_t totalUptime = preferences.getUInt(KEY_TOTAL_UPTIME, 0);
        totalUptime += (millis() - nodeStartTime) / 1000;
        preferences.putUInt(KEY_TOTAL_UPTIME, totalUptime);
        preferences.end();
    }
    
    // Cleanup components
    if (router) {
        delete router;
        router = nullptr;
    }
    
    if (radio) {
        radio->end();
        delete radio;
        radio = nullptr;
    }
    
    Serial.println("[NODE] Shutdown complete");
}

bool RealMeshNode::sendMessage(const String& targetAddress, const String& message) {
    if (currentState != STATE_OPERATIONAL || !router) {
        logEvent("ERROR", "Cannot send message - node not operational");
        return false;
    }
    
    NodeAddress target = parseAddress(targetAddress);
    if (!target.isValid()) {
        logEvent("ERROR", "Invalid target address: " + targetAddress);
        return false;
    }
    
    return router->sendDirectMessage(target, message);
}

bool RealMeshNode::sendPublicMessage(const String& message) {
    if (currentState != STATE_OPERATIONAL || !router) {
        logEvent("ERROR", "Cannot send public message - node not operational");
        return false;
    }
    
    return router->sendPublicMessage(message);
}

bool RealMeshNode::sendEmergencyMessage(const String& message) {
    if (!router) {
        logEvent("ERROR", "Cannot send emergency message - router not initialized");
        return false;
    }
    
    // Emergency messages can be sent in any state except ERROR
    if (currentState == STATE_ERROR) {
        return false;
    }
    
    return router->sendEmergencyMessage(message);
}

void RealMeshNode::setStationary(bool stationary) {
    if (router) {
        router->setOwnStatus(stationary ? NODE_STATIONARY : NODE_MOBILE);
        
        if (networkEventCallback) {
            networkEventCallback("STATUS_CHANGE", stationary ? "STATIONARY" : "MOBILE");
        }
        
        logEvent("INFO", String("Node status changed to ") + (stationary ? "STATIONARY" : "MOBILE"));
    }
}

NetworkStats RealMeshNode::getNetworkStats() {
    if (router) {
        return router->getNetworkStats();
    }
    return {};
}

void RealMeshNode::printNodeInfo() {
    Serial.println("========================================");
    Serial.println("           NODE INFORMATION");
    Serial.println("========================================");
    Serial.printf("Address: %s\n", ownAddress.getFullAddress().c_str());
    Serial.printf("Internal: %s\n", ownAddress.getInternalAddress().c_str());
    Serial.printf("UUID: %s\n", ownAddress.uuid.toString().c_str());
    Serial.printf("State: %d\n", currentState);
    Serial.printf("Status: %s\n", isStationary() ? "STATIONARY" : "MOBILE");
    Serial.printf("Uptime: %d seconds\n", (millis() - nodeStartTime) / 1000);
    
    if (preferences.begin(STORAGE_NAMESPACE, true)) {
        Serial.printf("Boot count: %d\n", preferences.getUInt(KEY_BOOT_COUNT, 0));
        Serial.printf("Total uptime: %d seconds\n", preferences.getUInt(KEY_TOTAL_UPTIME, 0));
        preferences.end();
    }
    
    if (radio) {
        Serial.printf("Messages sent: %d\n", radio->getMessagesSent());
        Serial.printf("Messages received: %d\n", radio->getMessagesReceived());
        Serial.printf("Current RSSI: %.1f dBm\n", radio->getCurrentRSSI());
    }
    
    if (router) {
        Serial.printf("Routing entries: %d\n", router->getRoutingTableSize());
        Serial.printf("Known subdomains: %d\n", router->getSubdomainCount());
    }
    
    Serial.println("========================================");
}

// Private implementation

bool RealMeshNode::loadStoredIdentity() {
    // Use NVS directly
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }
    
    // Try to load node ID
    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, KEY_NODE_ID, NULL, &required_size);
    if (err != ESP_OK || required_size == 0) {
        nvs_close(nvs_handle);
        return false;
    }
    
    char* nodeId = (char*)malloc(required_size);
    err = nvs_get_str(nvs_handle, KEY_NODE_ID, nodeId, &required_size);
    if (err != ESP_OK) {
        free(nodeId);
        nvs_close(nvs_handle);
        return false;
    }
    
    // Try to load subdomain
    required_size = 0;
    err = nvs_get_str(nvs_handle, KEY_SUBDOMAIN, NULL, &required_size);
    if (err != ESP_OK || required_size == 0) {
        free(nodeId);
        nvs_close(nvs_handle);
        return false;
    }
    
    char* subdomain = (char*)malloc(required_size);
    err = nvs_get_str(nvs_handle, KEY_SUBDOMAIN, subdomain, &required_size);
    if (err != ESP_OK) {
        free(nodeId);
        free(subdomain);
        nvs_close(nvs_handle);
        return false;
    }
    
    // Load UUID
    required_size = RM_UUID_LENGTH;
    err = nvs_get_blob(nvs_handle, KEY_UUID, ownAddress.uuid.bytes, &required_size);
    if (err != ESP_OK || required_size != RM_UUID_LENGTH) {
        free(nodeId);
        free(subdomain);
        nvs_close(nvs_handle);
        return false;
    }
    
    nvs_close(nvs_handle);
    
    // Set address
    ownAddress.nodeId = String(nodeId);
    ownAddress.subdomain = String(subdomain);
    
    free(nodeId);
    free(subdomain);
    
    // Validate
    if (!validateStoredIdentity()) {
        return false;
    }
    
    hasValidIdentity = true;
    logEvent("INFO", "Loaded stored identity: " + ownAddress.getFullAddress());
    return true;
}

bool RealMeshNode::createNewIdentity() {
    // Use desired names if provided, otherwise generate defaults
    if (desiredNodeId.isEmpty()) {
        desiredNodeId = "node" + String(esp_random() % 9999);
    }
    if (desiredSubdomain.isEmpty()) {
        desiredSubdomain = "mesh" + String(esp_random() % 99);
    }
    
    // Validate names
    Serial.printf("[NODE] Validating nodeId='%s' (len=%d), subdomain='%s' (len=%d)\n", 
                  desiredNodeId.c_str(), desiredNodeId.length(),
                  desiredSubdomain.c_str(), desiredSubdomain.length());
    
    bool nodeIdValid = isValidNodeId(desiredNodeId);
    bool subdomainValid = isValidSubdomain(desiredSubdomain);
    
    Serial.printf("[NODE] nodeIdValid=%s, subdomainValid=%s\n", 
                  nodeIdValid ? "true" : "false", 
                  subdomainValid ? "true" : "false");
    
    if (!nodeIdValid || !subdomainValid) {
        logEvent("ERROR", "Invalid node ID or subdomain");
        return false;
    }
    
    // Generate UUID
    ownAddress.uuid = generateUUID();
    Serial.printf("[NODE] Generated UUID: %02x%02x%02x%02x-%02x%02x%02x%02x\n",
                  ownAddress.uuid.bytes[0], ownAddress.uuid.bytes[1], ownAddress.uuid.bytes[2], ownAddress.uuid.bytes[3],
                  ownAddress.uuid.bytes[4], ownAddress.uuid.bytes[5], ownAddress.uuid.bytes[6], ownAddress.uuid.bytes[7]);
    
    ownAddress.nodeId = desiredNodeId;
    ownAddress.subdomain = desiredSubdomain;
    
    // Store identity
    if (!storeIdentity()) {
        return false;
    }
    
    hasValidIdentity = true;
    logEvent("INFO", "Created new identity: " + ownAddress.getFullAddress());
    return true;
}

bool RealMeshNode::storeIdentity() {
    Serial.println("[NODE] Attempting to store identity...");
    
    // Try to initialize NVS first
    Serial.println("[NODE] Initializing NVS...");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("[NODE] NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    Serial.printf("[NODE] NVS init result: %s\n", esp_err_to_name(err));
    
    // Use NVS directly instead of Preferences
    nvs_handle_t nvs_handle;
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        Serial.printf("[NODE] ERROR: Failed to open NVS namespace '%s': %s\n", STORAGE_NAMESPACE, esp_err_to_name(err));
        return false;
    }
    
    Serial.printf("[NODE] Storing nodeId: %s\n", ownAddress.nodeId.c_str());
    Serial.printf("[NODE] Storing subdomain: %s\n", ownAddress.subdomain.c_str());
    Serial.printf("[NODE] Storing UUID: %02x%02x%02x%02x...\n", 
                  ownAddress.uuid.bytes[0], ownAddress.uuid.bytes[1], 
                  ownAddress.uuid.bytes[2], ownAddress.uuid.bytes[3]);
    
    // Store using direct NVS calls
    err = nvs_set_str(nvs_handle, KEY_NODE_ID, ownAddress.nodeId.c_str());
    if (err != ESP_OK) {
        Serial.printf("[NODE] ERROR storing nodeId: %s\n", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_set_str(nvs_handle, KEY_SUBDOMAIN, ownAddress.subdomain.c_str());
    if (err != ESP_OK) {
        Serial.printf("[NODE] ERROR storing subdomain: %s\n", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_set_blob(nvs_handle, KEY_UUID, ownAddress.uuid.bytes, RM_UUID_LENGTH);
    if (err != ESP_OK) {
        Serial.printf("[NODE] ERROR storing UUID: %s\n", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    // Mark first boot time if not set
    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, KEY_FIRST_BOOT, NULL, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        uint32_t first_boot = millis() / 1000;
        nvs_set_u32(nvs_handle, KEY_FIRST_BOOT, first_boot);
    }
    
    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        Serial.printf("[NODE] ERROR committing NVS: %s\n", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    nvs_close(nvs_handle);
    Serial.println("[NODE] Identity stored successfully");
    return true;
}

NodeUUID RealMeshNode::generateUUID() {
    NodeUUID uuid;
    esp_fill_random(uuid.bytes, RM_UUID_LENGTH);
    return uuid;
}

bool RealMeshNode::validateStoredIdentity() {
    return isValidNodeId(ownAddress.nodeId) && 
           isValidSubdomain(ownAddress.subdomain) &&
           ownAddress.uuid.bytes[0] != 0; // UUID should not be all zeros
}

void RealMeshNode::startNetworkDiscovery() {
    changeState(STATE_DISCOVERING);
    discoveryStartTime = millis();
    discoveryComplete = false;
    
    logEvent("INFO", "Starting network discovery");
    broadcastPresence();
}

void RealMeshNode::broadcastPresence() {
    if (!router) return;
    
    // Send heartbeat to announce our presence
    router->sendHeartbeat();
    lastDiscoveryBroadcast = millis();
    
    logEvent("INFO", "Broadcasted presence announcement");
}

void RealMeshNode::handleDiscoveryTimeout() {
    // Broadcast presence every 10 seconds during discovery
    if (millis() - lastDiscoveryBroadcast > 10000) {
        broadcastPresence();
    }
    
    // Complete discovery after 30 seconds
    if (millis() - discoveryStartTime > RM_NETWORK_JOIN_TIMEOUT) {
        discoveryComplete = true;
        changeState(STATE_OPERATIONAL);
        logEvent("INFO", "Network discovery completed");
    }
}

void RealMeshNode::changeState(NodeState newState) {
    if (currentState != newState) {
        NodeState oldState = currentState;
        currentState = newState;
        
        handleStateTransition(oldState, newState);
        
        if (stateChangedCallback) {
            stateChangedCallback(oldState, newState);
        }
        
        logEvent("INFO", "State changed from " + String(oldState) + " to " + String(newState));
    }
}

void RealMeshNode::handleStateTransition(NodeState oldState, NodeState newState) {
    switch (newState) {
        case STATE_OPERATIONAL:
            if (networkEventCallback) {
                networkEventCallback("NODE_READY", "Node is now operational");
            }
            break;
            
        case STATE_ERROR:
            if (networkEventCallback) {
                networkEventCallback("NODE_ERROR", "Node encountered an error");
            }
            break;
            
        default:
            break;
    }
}

void RealMeshNode::onRadioMessageReceived(const MessagePacket& packet, int16_t rssi, float snr) {
    if (verboseLogging) {
        logEvent("DEBUG", "Radio received: " + RealMeshPacket::packetToString(packet));
    }
    
    // Handle name conflict messages directly
    if (packet.header.messageType == MSG_NAME_CONFLICT && 
        packet.destination.getFullAddress() == ownAddress.getFullAddress()) {
        
        logEvent("WARNING", "Name conflict detected from " + packet.source.getFullAddress());
        startNameConflictResolution();
        return;
    }
    
    // Pass to router for processing
    if (router) {
        router->processIncomingPacket(packet, rssi, snr);
    }
}

void RealMeshNode::onRouterMessageForUs(const MessagePacket& packet) {
    if (packet.header.messageType == MSG_DATA && messageReceivedCallback) {
        String message((char*)packet.payload, packet.header.payloadLength);
        messageReceivedCallback(packet.source.getFullAddress(), message);
    }
}

void RealMeshNode::onRouteUpdate(const String& update) {
    if (networkEventCallback) {
        networkEventCallback("ROUTE_UPDATE", update);
    }
}

void RealMeshNode::runPeriodicMaintenance() {
    updateNodeStatistics();
    cleanupOldData();
    
    if (verboseLogging) {
        logEvent("DEBUG", "Periodic maintenance completed");
    }
}

bool RealMeshNode::isValidNodeId(const String& nodeId) {
    if (nodeId.length() < 3 || nodeId.length() > 20) return false;
    
    for (int i = 0; i < nodeId.length(); i++) {
        char c = nodeId.charAt(i);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return false;
        }
    }
    
    return true;
}

bool RealMeshNode::isValidSubdomain(const String& subdomain) {
    return isValidNodeId(subdomain); // Same rules apply
}

NodeAddress RealMeshNode::parseAddress(const String& addressString) {
    NodeAddress addr;
    
    int atIndex = addressString.indexOf('@');
    if (atIndex == -1) return addr; // Invalid format
    
    addr.nodeId = addressString.substring(0, atIndex);
    addr.subdomain = addressString.substring(atIndex + 1);
    
    return addr;
}

void RealMeshNode::logEvent(const String& level, const String& message) {
    Serial.printf("[%s] %s\n", level.c_str(), message.c_str());
    
    if (networkEventCallback) {
        networkEventCallback("LOG_" + level, message);
    }
}

void RealMeshNode::printNetworkInfo() {
    Serial.println("========================================");
    Serial.println("         NETWORK INFORMATION");
    Serial.println("========================================");
    
    if (router) {
        Serial.printf("Routing entries: %d\n", router->getRoutingTableSize());
        Serial.printf("Known subdomains: %d\n", router->getSubdomainCount());
        Serial.printf("Intermediary bridges: %d\n", router->getIntermediaryCount());
        
        // Print routing table
        router->printRoutingTable();
        
        // Print subdomain info
        router->printSubdomainInfo();
    } else {
        Serial.println("Router not initialized");
    }
    
    Serial.println("========================================");
}

void RealMeshNode::runDiagnostics() {
    Serial.println("========================================");
    Serial.println("         SYSTEM DIAGNOSTICS");
    Serial.println("========================================");
    
    // Node diagnostics
    printNodeInfo();
    
    // Radio diagnostics
    if (radio) {
        radio->runRadioTest();
    } else {
        Serial.println("[DIAG] Radio not initialized");
    }
    
    // Network diagnostics
    printNetworkInfo();
    
    // Memory diagnostics
    Serial.printf("[DIAG] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[DIAG] Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("[DIAG] Total PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("[DIAG] Free PSRAM: %d bytes\n", ESP.getFreePsram());
    
    Serial.println("========================================");
}

void RealMeshNode::factoryReset() {
    Serial.println("[RESET] Performing factory reset...");
    
    // Clear all stored preferences
    if (preferences.begin(STORAGE_NAMESPACE, false)) {
        preferences.clear();
        preferences.end();
        Serial.println("[RESET] Storage cleared");
    }
    
    // Reset network state
    hasValidIdentity = false;
    
    Serial.println("[RESET] Factory reset complete - device will restart");
}

// Missing method implementations
void RealMeshNode::onRadioTransmitComplete(bool success, const String& error) {
    if (success) {
        Serial.println("[TX] Transmission completed successfully");
    } else {
        Serial.print("[TX] Transmission failed: ");
        Serial.println(error);
    }
}

void RealMeshNode::startNameConflictResolution() {
    Serial.println("[NAME] Starting name conflict resolution");
    // Set a flag for name conflict resolution
    nameConflictActive = true;
    nameConflictStartTime = millis();
    
    // Generate a new random suffix
    String newNodeId = baseNodeId + "_" + String(random(100, 999));
    Serial.print("[NAME] Proposing new name: ");
    Serial.println(newNodeId);
    
    // Update current node ID temporarily
    ownAddress.nodeId = newNodeId;
}

void RealMeshNode::updateNodeStatistics() {
    // Update various node statistics
    nodeStats.uptimeSeconds = millis() / 1000;
    nodeStats.messagesReceived++;
    nodeStats.lastHeartbeat = millis();
    
    // Log stats periodically (every 10 minutes)
    static unsigned long lastStatsLog = 0;
    if (millis() - lastStatsLog > 600000) {
        Serial.println("[STATS] Node statistics updated");
        lastStatsLog = millis();
    }
}

void RealMeshNode::cleanupOldData() {
    // Clean up old routing entries, message cache, etc.
    Serial.println("[CLEANUP] Cleaning up old data");
    
    // This would typically clean up:
    // - Old routing entries
    // - Message cache
    // - Expired node information
    // Implementation depends on data structures used
}

void RealMeshNode::handleNameConflictTimeout() {
    if (nameConflictActive && (millis() - nameConflictStartTime > RM_NAME_TIMEOUT_MS)) {
        Serial.println("[NAME] Name conflict timeout - accepting new name");
        
        // Accept the new name and store it
        baseNodeId = ownAddress.nodeId;
        nameConflictActive = false;
        
        // Save the new identity
        storeIdentity();
        
        Serial.print("[NAME] New identity established: ");
        Serial.println(getOwnAddress().getFullAddress());
    }
}

// Network information methods
size_t RealMeshNode::getKnownNodesCount() {
    if (!router) {
        return 0;
    }
    return router->getRoutingTableSize();
}

std::vector<String> RealMeshNode::getKnownNodes() {
    std::vector<String> nodes;
    if (!router) {
        return nodes;
    }
    
    // This would typically iterate through the routing table
    // For now, return a simple placeholder until routing table access is implemented
    nodes.push_back(getOwnAddress().getFullAddress());
    
    return nodes;
}
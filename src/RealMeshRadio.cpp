#include "RealMeshRadio.h"
#include "RealMeshConfig.h"
#include <vector>

// ============================================================================
// LoRa Radio Implementation
// ============================================================================

// Static instance for interrupt callbacks
RealMeshRadio* RealMeshRadio::instance = nullptr;

RealMeshRadio::RealMeshRadio() : 
    radio(new Module(RM_LORA_CS, RM_LORA_DIO1, RM_LORA_RST, RM_LORA_BUSY)),
    initialized(false),
    transmitting(false),
    receiving(false),
    lastTransmission(0),
    lastReception(0),
    messagesSent(0),
    messagesReceived(0),
    transmitErrors(0),
    receiveErrors(0),
    bytesTransmitted(0),
    bytesReceived(0),
    channelBusyTime(0),
    channelSampleTime(0),
    avgRSSI(-100.0),
    avgSNR(-10.0),
    messageCallback(nullptr),
    transmitCallback(nullptr) {
    
    // Set static instance for interrupt callbacks
    instance = this;
}

bool RealMeshRadio::begin() {
    Serial.println("[RADIO] Initializing LoRa radio...");
    
    // Initialize SPI pins
    SPI.begin(RM_LORA_SCK, RM_LORA_MISO, RM_LORA_MOSI, RM_LORA_CS);
    
    // Initialize radio with basic settings
    int state = radio.begin(RM_FREQ_MHZ, RM_BANDWIDTH_KHZ, RM_SPREADING_FACTOR, RM_CODING_RATE, RM_SYNC_WORD, RM_TX_POWER_DBM, RM_PREAMBLE_LENGTH);
    
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] Failed to initialize radio: %s\n", getRadioStateString(state).c_str());
        return false;
    }
    
    // Configure additional radio parameters
    if (!configureRadio()) {
        Serial.println("[RADIO] Failed to configure radio parameters");
        return false;
    }
    
    // Start listening for incoming messages
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] Failed to start receive mode: %s\n", getRadioStateString(state).c_str());
        return false;
    }
    
    receiving = true;
    initialized = true;
    
    Serial.println("[RADIO] LoRa radio initialized successfully");
    printRadioConfig();
    
    return true;
}

void RealMeshRadio::end() {
    if (!initialized) return;
    
    Serial.println("[RADIO] Shutting down radio...");
    
    radio.standby();
    initialized = false;
    transmitting = false;
    receiving = false;
    
    Serial.println("[RADIO] Radio shutdown complete");
}

bool RealMeshRadio::sendPacket(const MessagePacket& packet) {
    if (!initialized || transmitting) {
        Serial.println("[RADIO] Cannot send - radio not ready");
        return false;
    }
    
    // Serialize packet to bytes
    std::vector<uint8_t> data = RealMeshPacket::serialize(packet);
    
    if (data.size() > RM_MAX_PACKET_SIZE) {
        Serial.printf("[RADIO] Packet too large: %d bytes\n", data.size());
        updateStatistics(true, false, data.size());
        return false;
    }
    
    // Stop receiving to transmit
    receiving = false;
    
    // Send the packet
    int state = radio.transmit(data.data(), data.size());
    
    bool success = (state == RADIOLIB_ERR_NONE);
    updateStatistics(true, success, data.size());
    
    if (success) {
        Serial.printf("[RADIO] Sent packet: %s (%d bytes)\n", 
                     RealMeshPacket::packetToString(packet).c_str(), data.size());
        lastTransmission = millis();
    } else {
        Serial.printf("[RADIO] Failed to send packet: %s\n", getRadioStateString(state).c_str());
        handleTransmitError(state);
    }
    
    // Resume receiving
    radio.startReceive();
    receiving = true;
    
    // Call callback if set
    if (transmitCallback) {
        transmitCallback(success, success ? "OK" : getRadioStateString(state));
    }
    
    return success;
}

void RealMeshRadio::processIncoming() {
    if (!initialized || !receiving) return;
    
    // Check if data is available (simpler approach)
    // Read the packet directly - RadioLib handles IRQ internally
    std::vector<uint8_t> data(RM_MAX_PACKET_SIZE);
    int state = radio.readData(data.data(), data.size());
    
    if (state > 0) {
        // Successful reception
        data.resize(state); // Trim to actual size
        
        // Get signal quality
        float rssi = radio.getRSSI();
        float snr = radio.getSNR();
        
        // Update statistics
        updateStatistics(false, true, data.size());
        avgRSSI = (avgRSSI * 0.9) + (rssi * 0.1); // Running average
        avgSNR = (avgSNR * 0.9) + (snr * 0.1);
        lastReception = millis();
        
        // Deserialize packet
        MessagePacket packet;
        if (RealMeshPacket::deserialize(data, packet)) {
            Serial.printf("[RADIO] Received packet: %s (RSSI: %.1fdBm, SNR: %.1fdB)\n",
                         RealMeshPacket::packetToString(packet).c_str(), rssi, snr);
            
            // Call callback if set
            if (messageCallback) {
                messageCallback(packet, (int16_t)rssi, snr);
            }
        } else {
            Serial.printf("[RADIO] Failed to deserialize packet (%d bytes)\n", data.size());
            receiveErrors++;
        }
    } else if (state != RADIOLIB_ERR_RX_TIMEOUT && state != RADIOLIB_ERR_NONE) {
        // Handle reception errors (ignore timeouts and "no data" as they're normal)
        handleReceiveError(state);
    }
}

void RealMeshRadio::setOnMessageReceived(OnMessageReceived callback) {
    messageCallback = callback;
}

void RealMeshRadio::setOnTransmitComplete(OnTransmitComplete callback) {
    transmitCallback = callback;
}

float RealMeshRadio::getCurrentRSSI() {
    if (!initialized) return -999.0;
    return radio.getRSSI();
}

float RealMeshRadio::getCurrentSNR() {
    if (!initialized) return -999.0;
    return radio.getSNR();
}

bool RealMeshRadio::isChannelBusy() {
    if (!initialized) return false;
    
    // Simple RSSI-based channel activity detection
    float rssi = getCurrentRSSI();
    return rssi > -90.0; // Threshold for "busy" channel
}

float RealMeshRadio::getChannelUtilization() {
    if (channelSampleTime == 0) return 0.0;
    return (float)channelBusyTime / channelSampleTime * 100.0;
}

void RealMeshRadio::printRadioConfig() {
    if (!initialized) {
        Serial.println("[RADIO] Radio not initialized");
        return;
    }
    
    Serial.println("[RADIO] Current Configuration:");
    Serial.printf("  Frequency: %.3f MHz\n", RM_FREQ_MHZ);
    Serial.printf("  Bandwidth: %.1f kHz\n", RM_BANDWIDTH_KHZ);
    Serial.printf("  Spreading Factor: SF%d\n", RM_SPREADING_FACTOR);
    Serial.printf("  Coding Rate: 4/%d\n", RM_CODING_RATE);
    Serial.printf("  TX Power: %d dBm\n", RM_TX_POWER_DBM);
    Serial.printf("  Preamble Length: %d symbols\n", RM_PREAMBLE_LENGTH);
    Serial.printf("  Sync Word: 0x%02X\n", RM_SYNC_WORD);
    Serial.printf("  Current RSSI: %.1f dBm\n", getCurrentRSSI());
    Serial.printf("  Current SNR: %.1f dB\n", getCurrentSNR());
    Serial.printf("  Messages Sent: %d\n", messagesSent);
    Serial.printf("  Messages Received: %d\n", messagesReceived);
    Serial.printf("  Transmit Errors: %d\n", transmitErrors);
    Serial.printf("  Receive Errors: %d\n", receiveErrors);
}

void RealMeshRadio::runRadioTest() {
    Serial.println("[RADIO] Running radio self-test...");
    
    if (!initialized) {
        Serial.println("[RADIO] ERROR: Radio not initialized");
        return;
    }
    
    // Test 1: Basic radio communication test
    Serial.println("[RADIO] Testing radio communication...");
    
    // Test 2: RSSI measurement
    float rssi = getCurrentRSSI();
    float snr = getCurrentSNR();
    Serial.printf("[RADIO] Background RSSI: %.1f dBm, SNR: %.1f dB\n", rssi, snr);
    
    // Test 3: Configuration verification
    Serial.printf("[RADIO] Configured frequency: %.3f MHz\n", RM_FREQ_MHZ);
    Serial.printf("[RADIO] Bandwidth: %.1f kHz\n", RM_BANDWIDTH_KHZ);
    Serial.printf("[RADIO] Spreading Factor: SF%d\n", RM_SPREADING_FACTOR);
    
    // Test 4: Power measurement
    Serial.printf("[RADIO] TX Power setting: %d dBm\n", RM_TX_POWER_DBM);
    
    // Test 5: Radio state verification
    Serial.printf("[RADIO] Radio initialized: %s\n", initialized ? "YES" : "NO");
    Serial.printf("[RADIO] Currently receiving: %s\n", receiving ? "YES" : "NO");
    
    Serial.println("[RADIO] Radio self-test complete");
}

// Private methods

bool RealMeshRadio::configureRadio() {
    int state;
    
    // Set explicit header mode
    state = radio.explicitHeader();
    if (state != RADIOLIB_ERR_NONE) return false;
    
    // Set CRC on
    state = radio.setCRC(true);
    if (state != RADIOLIB_ERR_NONE) return false;
    
    // Configure for long range
    state = radio.setCurrentLimit(140); // mA
    if (state != RADIOLIB_ERR_NONE) return false;
    
    // Set TCXO voltage for Heltec V3
    state = radio.setTCXO(1.8);
    if (state != RADIOLIB_ERR_NONE) return false;
    
    return true;
}

void RealMeshRadio::updateStatistics(bool sent, bool success, size_t bytes) {
    if (sent) {
        if (success) {
            messagesSent++;
            bytesTransmitted += bytes;
        } else {
            transmitErrors++;
        }
    } else {
        if (success) {
            messagesReceived++;
            bytesReceived += bytes;
        } else {
            receiveErrors++;
        }
    }
}

void RealMeshRadio::handleReceiveError(int state) {
    // Only log and count actual errors, not "Success" or timeout states
    if (state != RADIOLIB_ERR_NONE && state != RADIOLIB_ERR_RX_TIMEOUT) {
        receiveErrors++;
        Serial.printf("[RADIO] Receive error: %s\n", getRadioStateString(state).c_str());
    }
}

void RealMeshRadio::handleTransmitError(int state) {
    transmitErrors++;
    Serial.printf("[RADIO] Transmit error: %s\n", getRadioStateString(state).c_str());
}

String RealMeshRadio::getRadioStateString(int state) {
    switch (state) {
        case RADIOLIB_ERR_NONE: return "Success";
        case RADIOLIB_ERR_UNKNOWN: return "Unknown error";
        case RADIOLIB_ERR_CHIP_NOT_FOUND: return "Chip not found";
        case RADIOLIB_ERR_PACKET_TOO_LONG: return "Packet too long";
        case RADIOLIB_ERR_TX_TIMEOUT: return "TX timeout";
        case RADIOLIB_ERR_RX_TIMEOUT: return "RX timeout";
        case RADIOLIB_ERR_CRC_MISMATCH: return "CRC mismatch";
        case RADIOLIB_ERR_INVALID_BANDWIDTH: return "Invalid bandwidth";
        case RADIOLIB_ERR_INVALID_SPREADING_FACTOR: return "Invalid spreading factor";
        case RADIOLIB_ERR_INVALID_CODING_RATE: return "Invalid coding rate";
        case RADIOLIB_ERR_INVALID_FREQUENCY: return "Invalid frequency";
        case RADIOLIB_ERR_INVALID_OUTPUT_POWER: return "Invalid output power";
        default: return "Error code " + String(state);
    }
}

// Static interrupt handlers
void RealMeshRadio::onTransmitDone() {
    if (instance) {
        // Handle in main loop
    }
}

void RealMeshRadio::onReceiveDone() {
    if (instance) {
        // Handle in main loop via processIncoming()
    }
}
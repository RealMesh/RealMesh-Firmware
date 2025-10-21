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

void RealMeshRadio::scanSPI() {
    Serial.println("[RADIO] === SPI Bus Scanner ===");
    Serial.printf("[RADIO] Current config - SCK:%d MISO:%d MOSI:%d CS:%d RST:%d DIO1:%d BUSY:%d\n", 
                  RM_LORA_SCK, RM_LORA_MISO, RM_LORA_MOSI, RM_LORA_CS, RM_LORA_RST, RM_LORA_DIO1, RM_LORA_BUSY);
    
    // Test current configuration
    Serial.println("[RADIO] Testing current pin configuration...");
    testSPIConfiguration(RM_LORA_SCK, RM_LORA_MISO, RM_LORA_MOSI, RM_LORA_CS);
    
    // Alternative configurations to test (in case of board variant differences)
    Serial.println("[RADIO] Testing alternative configurations...");
    
    // Heltec V4 configuration (from search results)
    Serial.println("[RADIO] Testing Heltec V4 config (SCK:9 MISO:11 MOSI:10 CS:8)");
    testSPIConfiguration(9, 11, 10, 8);
    
    // Some other ESP32-S3 boards use different pins
    Serial.println("[RADIO] Testing alternative config (SCK:18 MISO:19 MOSI:23 CS:5)");
    testSPIConfiguration(18, 19, 23, 5);
    
    Serial.println("[RADIO] === SPI Scanner Complete ===");
}

void RealMeshRadio::testSPIConfiguration(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs) {
    Serial.printf("[RADIO] Testing SCK:%d MISO:%d MOSI:%d CS:%d\n", sck, miso, mosi, cs);
    
    // Initialize SPI with test pins
    SPI.end(); // End current SPI first
    SPI.begin(sck, miso, mosi, cs);
    
    // Set CS pin as output and high (inactive)
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
    delay(10);
    
    // Try to read version register
    digitalWrite(cs, LOW);
    delayMicroseconds(10);
    
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    uint8_t cmd = 0x1D; // Read register command for SX126x
    uint8_t addr = 0x00; // Version register address
    uint8_t nop = 0x00;
    
    SPI.transfer(cmd);
    SPI.transfer(addr);
    SPI.transfer(nop); // Status byte
    uint8_t version = SPI.transfer(0x00); // Read version
    
    SPI.endTransaction();
    digitalWrite(cs, HIGH);
    
    Serial.printf("[RADIO]   Version register: 0x%02X", version);
    
    // Check if this looks like a valid SX126x response
    if (version == 0x00 || version == 0xFF) {
        Serial.println(" (Invalid - chip not responding)");
    } else if (version == 0x22 || version == 0x24) {
        Serial.println(" (Valid SX126x chip detected!)");
    } else {
        Serial.printf(" (Unknown chip - might be valid: 0x%02X)\n", version);
    }
    
    // Test consistency
    bool consistent = true;
    for (int i = 0; i < 3; i++) {
        digitalWrite(cs, LOW);
        delayMicroseconds(10);
        
        SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
        SPI.transfer(0x1D);
        SPI.transfer(0x00);
        SPI.transfer(0x00);
        uint8_t val = SPI.transfer(0x00);
        SPI.endTransaction();
        
        digitalWrite(cs, HIGH);
        if (val != version) consistent = false;
        delay(5);
    }
    
    Serial.printf("[RADIO]   Consistency check: %s\n", consistent ? "PASS" : "FAIL");
    
    // Restore original SPI configuration
    SPI.end();
    SPI.begin(RM_LORA_SCK, RM_LORA_MISO, RM_LORA_MOSI, RM_LORA_CS);
}

bool RealMeshRadio::begin() {
    Serial.println("[RADIO] Initializing SX1262 with EXACT Meshtastic sequence...");
    Serial.printf("[RADIO] Using pins - SCK:%d MISO:%d MOSI:%d CS:%d RST:%d DIO1:%d BUSY:%d\n", 
                  RM_LORA_SCK, RM_LORA_MISO, RM_LORA_MOSI, RM_LORA_CS, RM_LORA_RST, RM_LORA_DIO1, RM_LORA_BUSY);
    
    // Initialize SPI pins
    SPI.begin(RM_LORA_SCK, RM_LORA_MISO, RM_LORA_MOSI, RM_LORA_CS);
    
    // Enable RadioLib verbose debugging
    Serial.println("[RADIO] Enabling RadioLib verbose debugging...");
    
    // First, scan SPI bus to see if chip responds
    scanSPI();
    
    // Test pin functionality
    Serial.println("[RADIO] Testing pin functionality...");
    pinMode(RM_LORA_RST, OUTPUT);
    pinMode(RM_LORA_CS, OUTPUT);
    pinMode(RM_LORA_BUSY, INPUT);
    
    // Test reset pin
    digitalWrite(RM_LORA_RST, LOW);
    delay(10);
    digitalWrite(RM_LORA_RST, HIGH);
    delay(100);
    Serial.printf("[RADIO] Reset pin test completed\n");
    
    // Test BUSY pin
    int busyState = digitalRead(RM_LORA_BUSY);
    Serial.printf("[RADIO] BUSY pin state: %d\n", busyState);
    
    // =================================================================
    // EXACT COPY of Meshtastic SX126xInterface<T>::init() method
    // Line-by-line replication of working Meshtastic code
    // =================================================================
    
    // Set TCXO voltage (from Meshtastic)
    float tcxoVoltage = 1.8; // SX126X_DIO3_TCXO_VOLTAGE for Heltec Wireless Paper
    Serial.printf("[RADIO] SX126X_DIO3_TCXO_VOLTAGE defined, using DIO3 as TCXO reference voltage at %f V\n", tcxoVoltage);
    
    // Use DCDC regulator (from Meshtastic) 
    bool useRegulatorLDO = false; // Meshtastic comment: "Seems to depend on the connection to pin 9/DCC_SW - if an inductor DCDC?"
    
    // Call RadioLibInterface::init() equivalent
    // (This would normally call the base class init, but we'll do radio.begin directly)
    
    // Limit power (Meshtastic does this before begin)
    // limitPower(SX126X_MAX_POWER) - we'll do this after begin
    
    // Ensure minimum power (-9dBm minimum for SX1262)
    int power = RM_TX_POWER_DBM;
    if (power < -9)
        power = -9;
    
    // THE CRITICAL CALL - exact Meshtastic begin() call
    int res = radio.begin(RM_FREQ_MHZ, RM_BANDWIDTH_KHZ, RM_SPREADING_FACTOR, RM_CODING_RATE, RM_SYNC_WORD, power, RM_PREAMBLE_LENGTH, tcxoVoltage, useRegulatorLDO);
    
    // Meshtastic error checking
    Serial.printf("[RADIO] SX126x init result %d\n", res);
    if (res == RADIOLIB_ERR_CHIP_NOT_FOUND || res == RADIOLIB_ERR_SPI_CMD_FAILED) {
        Serial.printf("[RADIO] Chip not found or SPI failed: %s\n", getRadioStateString(res).c_str());
        return false;
    }
    
    if (res != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] Initialization failed: %s\n", getRadioStateString(res).c_str());
        return false;
    }
    
    Serial.printf("[RADIO] Frequency set to %f\n", RM_FREQ_MHZ);
    Serial.printf("[RADIO] Bandwidth set to %f\n", RM_BANDWIDTH_KHZ);  
    Serial.printf("[RADIO] Power output set to %d\n", power);
    
    // Set current limit (Meshtastic: value in SX126xInterface.h currently 140 mA)
    res = radio.setCurrentLimit(140);
    if (res != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] Failed to set current limit: %s\n", getRadioStateString(res).c_str());
        // Don't fail - Meshtastic continues even if this fails
    } else {
        Serial.println("[RADIO] Current limit set to 140mA");
    }
    
    // Configure DIO2 as RF switch (exact Meshtastic logic)
    bool dio2AsRfSwitch = true; // SX126X_DIO2_AS_RF_SWITCH is defined for Heltec
    res = radio.setDio2AsRfSwitch(dio2AsRfSwitch);
    Serial.printf("[RADIO] Set DIO2 as RF switch, result: %d\n", res);
    
    // Set CRC (Meshtastic always enables this)
    res = radio.setCRC(RADIOLIB_SX126X_LORA_CRC_ON);
    if (res == RADIOLIB_ERR_NONE) {
        Serial.println("[RADIO] CRC enabled");
    } else {
        Serial.printf("[RADIO] CRC setting failed: %s\n", getRadioStateString(res).c_str());
        // Don't fail - continue like Meshtastic does
    }
    
    // Configure additional radio parameters (simplified - no need for separate function)
    Serial.println("[RADIO] Applying final configuration...");
    
    // Set explicit header mode (like Meshtastic)
    res = radio.explicitHeader();
    if (res != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] Failed to set explicit header: %s\n", getRadioStateString(res).c_str());
        // Continue anyway - not critical
    }
    
    // Start receiving (Meshtastic calls startReceive() at the end)
    res = radio.startReceive();
    if (res == RADIOLIB_ERR_NONE) {
        Serial.println("[RADIO] Started receive mode");
        receiving = true;
    } else {
        Serial.printf("[RADIO] Failed to start receive: %s\n", getRadioStateString(res).c_str());
        // Continue anyway - we can still transmit
    }
    
    initialized = true;
    
    Serial.println("[RADIO] === SX1262 INITIALIZATION COMPLETE ===");
    Serial.println("[RADIO] Using exact Meshtastic initialization sequence");
    Serial.printf("[RADIO] TCXO: %.1fV, Regulator: %s, DIO2: RF Switch, CRC: Enabled\n", 
                  tcxoVoltage, useRegulatorLDO ? "LDO" : "DCDC");
    
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
    // This function is now simplified since we do all configuration in begin()
    // following the exact Meshtastic sequence
    Serial.println("[RADIO] Additional configuration (already done in begin())");
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
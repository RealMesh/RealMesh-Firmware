#ifndef REALMESH_RADIO_H
#define REALMESH_RADIO_H

#include <RadioLib.h>
#include "RealMeshTypes.h"
#include "RealMeshPacket.h"
#include <functional>
#include <vector>

// ============================================================================
// LoRa Radio Abstraction Layer
// ============================================================================

class RealMeshRadio {
public:
    // Callback types for radio events
    typedef std::function<void(const MessagePacket&, int16_t rssi, float snr)> OnMessageReceived;
    typedef std::function<void(bool success, const String& error)> OnTransmitComplete;
    
    // Constructor
    RealMeshRadio();
    
    // Initialize radio with configured parameters
    bool begin();
    
    // Shutdown radio
    void end();
    
    // Send a message packet
    bool sendPacket(const MessagePacket& packet);
    
    // Check for incoming messages (call regularly in loop)
    void processIncoming();
    
    // Set callback functions
    void setOnMessageReceived(OnMessageReceived callback);
    void setOnTransmitComplete(OnTransmitComplete callback);
    
    // Radio status and statistics
    bool isInitialized() const { return initialized; }
    float getCurrentRSSI();
    float getCurrentSNR();
    uint32_t getMessagesSent() const { return messagesSent; }
    uint32_t getMessagesReceived() const { return messagesReceived; }
    uint32_t getTransmitErrors() const { return transmitErrors; }
    uint32_t getReceiveErrors() const { return receiveErrors; }
    
    // Channel management
    bool setFrequency(float freq);
    bool setBandwidth(float bw);
    bool setSpreadingFactor(uint8_t sf);
    bool setCodingRate(uint8_t cr);
    bool setTxPower(int8_t power);
    
    // Network analysis
    bool isChannelBusy();
    float getChannelUtilization();
    void startChannelScan(uint32_t duration = 5000);
    
    // Debug and testing
    void scanSPI();
    void testSPIConfiguration(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs);
    void printRadioConfig();
    void runRadioTest();
    
private:
    // RadioLib instances
    SX1262 radio;
    
    // Radio state
    bool initialized;
    bool transmitting;
    bool receiving;
    uint32_t lastTransmission;
    uint32_t lastReception;
    
    // Statistics
    uint32_t messagesSent;
    uint32_t messagesReceived;
    uint32_t transmitErrors;
    uint32_t receiveErrors;
    uint32_t bytesTransmitted;
    uint32_t bytesReceived;
    
    // Channel monitoring
    uint32_t channelBusyTime;
    uint32_t channelSampleTime;
    float avgRSSI;
    float avgSNR;
    
    // Callbacks
    OnMessageReceived messageCallback;
    OnTransmitComplete transmitCallback;
    
    // Internal methods
    bool configureRadio();
    void updateStatistics(bool sent, bool success, size_t bytes);
    void handleReceiveError(int state);
    void handleTransmitError(int state);
    String getRadioStateString(int state);
    
    // Interrupt handlers (static)
    static void onTransmitDone();
    static void onReceiveDone();
    static RealMeshRadio* instance; // For interrupt callbacks
};

#endif // REALMESH_RADIO_H
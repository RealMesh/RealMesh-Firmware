#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <epd/GxEPD2_213_FC1.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// ============================================================================
// Hardware Pin Definitions for Heltec Wireless Paper
// ============================================================================

// Display pins (using Meshtastic configuration)
#define EINK_CS     4  
#define EINK_DC     5
#define EINK_RES    6
#define EINK_BUSY   7
#define EINK_SCLK   3
#define EINK_MOSI   2

// Power control pin
#define PIN_VEXT_ENABLE  45  // Power enable pin (active LOW)

// Hardware control pins
#define LED_PIN         18    // Built-in LED on most Heltec devices
#define USR_BUTTON_PIN  0     // USR button (GPIO0) - standard on Heltec devices
#define PRG_BUTTON_PIN  35    // PRG button (GPIO35) - typically for programming

// Battery monitoring
// Battery pin is defined in RealMeshConfig.h
#define BATTERY_FACTOR  2.0   // Voltage divider factor

// ============================================================================
// Display System Configuration
// ============================================================================

#define DISPLAY_UPDATE_INTERVAL 5000  // Minimum 5 seconds between e-ink updates

// Screen definitions
enum DisplayScreen {
    SCREEN_HOME = 0,
    SCREEN_MESSAGES,
    SCREEN_NODE_INFO,
    SCREEN_COUNT
};

// Display message type definitions
enum DisplayMessageType {
    DISPLAY_MSG_INFO,
    DISPLAY_MSG_WARNING,
    DISPLAY_MSG_ERROR,
    DISPLAY_MSG_SUCCESS
};

// Screen layout constants
#define SCREEN_WIDTH    250
#define SCREEN_HEIGHT   122
#define HEADER_HEIGHT   20
#define FOOTER_HEIGHT   15
#define CONTENT_HEIGHT  (SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT)
#define MARGIN          5
#define DOT_SIZE        4
#define DOT_SPACING     8

// ============================================================================
// Display Manager Class
// ============================================================================

class RealMeshDisplayManager {
public:
    RealMeshDisplayManager();
    
    // Initialization
    bool begin();
    void end();
    
    // Screen management
    void nextScreen();
    void previousScreen();
    void setCurrentScreen(DisplayScreen screen);
    DisplayScreen getCurrentScreen() const { return currentScreen; }
    
    // Content updates
    void updateContent();
    void showTemporaryMessage(const String& title, const String& message, DisplayMessageType type = DISPLAY_MSG_INFO, uint32_t durationMs = 5000);
    void clearTemporaryMessage();
    
    // Message management
    void addMessage(const String& from, const String& content, bool isNew = false);
    void markAllMessagesAsRead();
    bool hasUnreadMessages() const { return unreadMessageCount > 0; }
    uint8_t getUnreadCount() const { return unreadMessageCount; }
    
    // Node information
    void setNodeName(const String& name) { 
        if (nodeName != name) { nodeName = name; needsUpdate = true; }
    }
    void setNodeAddress(const String& address) { 
        if (nodeAddress != address) { nodeAddress = address; needsUpdate = true; }
    }
    void setNodeType(const String& type) { 
        if (nodeType != type) { nodeType = type; needsUpdate = true; }
    }
    void setNetworkInfo(uint8_t nodeCount, const String& uptime) { 
        if (knownNodes != nodeCount) {
            knownNodes = nodeCount;
            needsUpdate = true;  // Only refresh on actual node count changes
        }
        networkUptime = uptime;  // Update uptime but don't trigger refresh for it
    }
    
    // Bluetooth information
    void setBluetoothInfo(const String& deviceName, bool isConnected) { 
        if (bleDeviceName != deviceName || bleConnected != isConnected) {
            bleDeviceName = deviceName; 
            bleConnected = isConnected; 
            needsUpdate = true;
        }
    }
    void setWiFiInfo(const String& ssid, const String& ip) { 
        if (wifiSSID != ssid || wifiIP != ip) {
            wifiSSID = ssid; 
            wifiIP = ip; 
            needsUpdate = true;
        }
    }
    
    // Battery information
    void updateBatteryLevel();
    uint8_t getBatteryPercentage() const { return batteryPercentage; }
    float getBatteryVoltage() const { return batteryVoltage; }
    
    // Auto-refresh control
    void setAutoRefresh(bool enabled) { autoRefreshEnabled = enabled; }
    void refresh();
    
private:
    // Display hardware (now uses EInkDisplay with framebuffer)
    bool displayInitialized;
    
    // Screen state
    DisplayScreen currentScreen;
    bool needsUpdate;
    bool autoRefreshEnabled;
    uint32_t lastUpdate;
    
    // Temporary message system
    bool tempMessageActive;
    String tempTitle;
    String tempMessage;
    DisplayMessageType tempType;
    uint32_t tempMessageTimeout;
    
    // Message storage
    struct StoredMessage {
        String from;
        String content;
        uint32_t timestamp;
        bool isRead;
    };
    static const uint8_t MAX_STORED_MESSAGES = 10;
    StoredMessage messages[MAX_STORED_MESSAGES];
    uint8_t messageCount;
    uint8_t unreadMessageCount;
    uint8_t currentMessageIndex;
    
    // Node information
    String nodeName;
    String nodeAddress;
    String nodeType;
    uint8_t knownNodes;
    String networkUptime;
    
    // Connectivity information
    String bleDeviceName;
    bool bleConnected;
    String wifiSSID;
    String wifiIP;
    
    // Battery monitoring
    uint8_t batteryPercentage;
    float batteryVoltage;
    uint32_t lastBatteryUpdate;
    
    // Drawing methods
    void drawHeader();
    void drawFooter();
    void drawScreenIndicators();
    void drawContent();
    
    // Screen-specific drawing
    void drawHomeScreen();
    void drawMessagesScreen();
    void drawNewMessageScreen();
    void drawNodeInfoScreen();
    void drawBluetoothInfoScreen();
    
    // Utility methods
    String formatTime(uint32_t timestamp);
    void addMessageInternal(const String& from, const String& content, bool isNew);
    void removeOldestMessage();
    uint16_t getTextWidth(const String& text, const GFXfont* font = nullptr);
    void drawCenteredText(const String& text, int16_t y, const GFXfont* font = nullptr);
    void drawRightAlignedText(const String& text, int16_t x, int16_t y, const GFXfont* font = nullptr);
    void drawWrappedText(const String& text, int16_t x, int16_t y, uint16_t maxWidth, const GFXfont* font = nullptr);
};

// ============================================================================
// LED Manager Class
// ============================================================================

class RealMeshLEDManager {
public:
    RealMeshLEDManager();
    
    // Initialization
    void begin();
    void end();
    
    // LED control
    void setHeartbeatEnabled(bool enabled);
    bool isHeartbeatEnabled() const { return heartbeatEnabled; }
    
    void setLED(bool on);
    void toggleLED();
    bool getLEDState() const { return ledState; }
    
    // Heartbeat patterns
    void setHeartbeatInterval(uint32_t intervalMs) { heartbeatInterval = intervalMs; }
    uint32_t getHeartbeatInterval() const { return heartbeatInterval; }
    
    // Status indication
    void showStatus(const String& pattern, uint32_t durationMs = 0);
    void flashError(uint8_t count = 3);
    void flashSuccess(uint8_t count = 2);
    void flashWarning(uint8_t count = 4);
    
    // Processing loop
    void loop();
    
private:
    bool ledState;
    bool heartbeatEnabled;
    uint32_t heartbeatInterval;
    uint32_t lastHeartbeat;
    
    // Status indication
    bool statusPatternActive;
    String currentPattern;
    uint32_t patternStartTime;
    uint32_t patternDuration;
    uint8_t patternIndex;
    
    void processHeartbeat();
    void processStatusPattern();
    void setLEDInternal(bool on);
};

// ============================================================================
// Button Manager Class
// ============================================================================

class RealMeshButtonManager {
public:
    RealMeshButtonManager();
    
    // Initialization
    void begin();
    void end();
    
    // Button state
    bool isUSRPressed() const { return usrPressed; }
    bool isPRGPressed() const { return prgPressed; }
    
    // Event callbacks
    typedef std::function<void()> ButtonCallback;
    void setUSRPressCallback(ButtonCallback callback) { usrPressCallback = callback; }
    void setPRGPressCallback(ButtonCallback callback) { prgPressCallback = callback; }
    void setUSRLongPressCallback(ButtonCallback callback) { usrLongPressCallback = callback; }
    void setPRGLongPressCallback(ButtonCallback callback) { prgLongPressCallback = callback; }
    
    // Processing loop
    void loop();
    
private:
    // Button states
    bool usrPressed;
    bool prgPressed;
    bool usrLastState;
    bool prgLastState;
    uint32_t usrPressTime;
    uint32_t prgPressTime;
    uint32_t lastDebounceTime;
    
    // Configuration
    static const uint32_t DEBOUNCE_DELAY = 50;
    static const uint32_t LONG_PRESS_DELAY = 1000;
    
    // Callbacks
    ButtonCallback usrPressCallback;
    ButtonCallback prgPressCallback;
    ButtonCallback usrLongPressCallback;
    ButtonCallback prgLongPressCallback;
    
    void processButton(uint8_t pin, bool& currentState, bool& lastState, uint32_t& pressTime, 
                      ButtonCallback pressCallback, ButtonCallback longPressCallback);
};

// Global instances
extern RealMeshDisplayManager* displayManager;
extern RealMeshLEDManager* ledManager;
extern RealMeshButtonManager* buttonManager;

// Global functions for backward compatibility
bool initializeDisplay();
void updateDisplay(const String& title, const String& status, const String& info);
void showError(const String& error);
void showTemporaryMessage(const String& title, const String& status, const String& info, uint32_t timeoutMs = 10000);
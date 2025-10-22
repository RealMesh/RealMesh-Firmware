#include "RealMeshDisplay.h"
#include "RealMeshConfig.h"
#include "EInkDisplay.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <GxEPD2_BW.h>
#include <epd/GxEPD2_213_FC1.h>
#include <Adafruit_GFX.h>

// ============================================================================
// Global Display Variables
// ============================================================================

EInkDisplay* einkDisplay = nullptr;

// Global manager instances
RealMeshDisplayManager* displayManager = nullptr;
RealMeshLEDManager* ledManager = nullptr;
RealMeshButtonManager* buttonManager = nullptr;

// ============================================================================
// Display Manager Implementation
// ============================================================================

RealMeshDisplayManager::RealMeshDisplayManager() 
    : displayInitialized(false),
      currentScreen(SCREEN_HOME), needsUpdate(true), autoRefreshEnabled(false), lastUpdate(0),
      tempMessageActive(false), tempMessageTimeout(0),
      messageCount(0), unreadMessageCount(0), currentMessageIndex(0),
      batteryPercentage(100), batteryVoltage(3.7), lastBatteryUpdate(0) {
    
    // Initialize node information with realistic defaults
    nodeName = "";
    nodeAddress = "";  
    nodeType = "";
    knownNodes = 0;  // Start with 0 nodes found
    networkUptime = "0:00:00";
    
    // Initialize connectivity information
    bleDeviceName = "";
    bleConnected = false;
    wifiSSID = "";
    wifiIP = "";
    
    // Initialize message array
    for (uint8_t i = 0; i < MAX_STORED_MESSAGES; i++) {
        messages[i] = {"", "", 0, true};
    }
}

bool RealMeshDisplayManager::begin() {
    Serial.println("[DISPLAY] Initializing e-ink display...");
    
    // Create EInkDisplay instance
    einkDisplay = new EInkDisplay(GEOMETRY_RAWMODE);
    
    // init() does: BufferOffset setup, connect(), allocateBuffer(), sendInitCommands(), resetDisplay()
    if (!einkDisplay->init()) {
        Serial.println("[DISPLAY] Failed to initialize display!");
        return false;
    }
    
    Serial.println("[DISPLAY] Display initialized successfully");
    
    displayInitialized = true;
    currentScreen = SCREEN_HOME;
    needsUpdate = true;
    
    Serial.println("[DISPLAY] Display manager initialized successfully");
    return true;
}

void RealMeshDisplayManager::end() {
    if (einkDisplay) {
        delete einkDisplay;
        einkDisplay = nullptr;
    }
    
    displayInitialized = false;
}

void RealMeshDisplayManager::nextScreen() {
    currentScreen = (DisplayScreen)((currentScreen + 1) % SCREEN_COUNT);
    needsUpdate = true;
    updateContent();
}

void RealMeshDisplayManager::previousScreen() {
    currentScreen = (DisplayScreen)((currentScreen - 1 + SCREEN_COUNT) % SCREEN_COUNT);
    needsUpdate = true;
    updateContent();
}

void RealMeshDisplayManager::setCurrentScreen(DisplayScreen screen) {
    if (screen != currentScreen && screen < SCREEN_COUNT) {
        currentScreen = screen;
        needsUpdate = true;
        updateContent();
    }
}

void RealMeshDisplayManager::updateContent() {
    if (!displayInitialized || !einkDisplay) {
        Serial.println("[DISPLAY] Update skipped - not initialized");
        return;
    }
    
    Serial.printf("[DISPLAY] ========================================\n");
    Serial.printf("[DISPLAY] UPDATE CONTENT - Screen %d\n", currentScreen);
    Serial.printf("[DISPLAY] Node: %s, Address: %s\n", nodeName.c_str(), nodeAddress.c_str());
    Serial.printf("[DISPLAY] ========================================\n");
    
    // Clear buffer first
    uint8_t* buf = einkDisplay->getBuffer();
    if (!buf) {
        Serial.println("[DISPLAY] ERROR: getBuffer() returned NULL!");
        return;
    }
    
    // Draw directly to GxEPD2 and trigger refresh immediately
    // This bypasses the OLEDDisplay buffer entirely
    auto* gxDisplay = einkDisplay->getGxEPD2();
    if (!gxDisplay) {
        Serial.println("[DISPLAY] ERROR: GxEPD2 is NULL!");
        return;
    }
    
    Serial.println("[DISPLAY] Drawing directly to GxEPD2...");
    
    // Use GxEPD2 native rendering
    gxDisplay->setFullWindow();
    gxDisplay->firstPage();
    do {
        gxDisplay->fillScreen(GxEPD_WHITE);
        gxDisplay->setTextColor(GxEPD_BLACK);
        gxDisplay->setTextWrap(false);
        
        // Draw header
        gxDisplay->setFont(&FreeMono9pt7b);
        gxDisplay->setCursor(5, 10);
        gxDisplay->print(nodeName.length() > 0 ? nodeName : "RealMesh");
        
        gxDisplay->setCursor(195, 10);
        gxDisplay->print(String(batteryPercentage) + "%");
        
        gxDisplay->drawLine(0, 12, 249, 12, GxEPD_BLACK);
        
        // Draw content based on current screen
        switch (currentScreen) {
            case SCREEN_HOME:
                // Node identity screen
                gxDisplay->setFont(&FreeMonoBold12pt7b);
                gxDisplay->setCursor(10, 40);
                if (nodeAddress.length() > 0) {
                    gxDisplay->print(nodeAddress);
                } else {
                    gxDisplay->print("RealMesh");
                }
                
                // Draw status
                gxDisplay->setFont(&FreeMono9pt7b);
                gxDisplay->setCursor(60, 70);
                if (knownNodes == 0) {
                    gxDisplay->print("No nodes found");
                } else if (knownNodes == 1) {
                    gxDisplay->print("1 node online");
                } else {
                    gxDisplay->print(String(knownNodes) + " nodes online");
                }
                break;
                
            case SCREEN_MESSAGES:
                // Messages screen
                gxDisplay->setFont(&FreeMono9pt7b);
                gxDisplay->setCursor(5, 25);
                gxDisplay->print("MESSAGES");
                
                if (messageCount == 0) {
                    gxDisplay->setCursor(30, 60);
                    gxDisplay->print("No messages");
                } else {
                    int y = 40;
                    for (int i = max(0, messageCount - 3); i < messageCount && y < 100; i++) {
                        gxDisplay->setCursor(5, y);
                        String msg = messages[i].from + ": " + messages[i].content.substring(0, 18);
                        gxDisplay->print(msg);
                        y += 20;
                    }
                }
                break;
                
            case SCREEN_NODE_INFO:
                // Node info screen
                gxDisplay->setFont(&FreeMono9pt7b);
                gxDisplay->setCursor(5, 25);
                gxDisplay->print("NODE INFO");
                
                gxDisplay->setCursor(5, 45);
                gxDisplay->print("Type: " + nodeType);
                
                gxDisplay->setCursor(5, 65);
                gxDisplay->print("Uptime: " + networkUptime);
                
                gxDisplay->setCursor(5, 85);
                gxDisplay->print("Battery: " + String(batteryVoltage, 2) + "V");
                break;
        }
        
        // Draw footer
        gxDisplay->drawLine(0, 107, 249, 107, GxEPD_BLACK);
        
        // Draw screen indicators
        for (int i = 0; i < SCREEN_COUNT; i++) {
            int dotX = (250 / 2) - ((SCREEN_COUNT * DOT_SPACING) / 2) + (i * DOT_SPACING);
            int dotY = 110;
            
            if (i == currentScreen) {
                gxDisplay->fillRect(dotX, dotY, DOT_SIZE, DOT_SIZE, GxEPD_BLACK);
            } else {
                gxDisplay->drawRect(dotX, dotY, DOT_SIZE, DOT_SIZE, GxEPD_BLACK);
            }
        }
        
    } while (gxDisplay->nextPage());
    
    // Hibernate the display
    gxDisplay->hibernate();
    
    Serial.println("[DISPLAY] GxEPD2 direct rendering complete (bypassed OLEDDisplay buffer)");
    

    
    /* ORIGINAL CONTENT - COMMENTED OUT FOR TEST
    // Draw to the framebuffer using OLEDDisplay methods
    einkDisplay->setColor(BLACK);
    einkDisplay->setTextAlignment(TEXT_ALIGN_LEFT);
    
    // Draw header
    einkDisplay->setFont(ArialMT_Plain_10);
    einkDisplay->drawString(5, 0, nodeName.length() > 0 ? nodeName : "RealMesh");
    einkDisplay->drawString(200, 0, String(batteryPercentage) + "%");
    einkDisplay->drawLine(0, 12, 250, 12);
    */
    
    /* ORIGINAL CONTENT - COMMENTED OUT FOR TEST
    // Draw main content
    einkDisplay->setFont(ArialMT_Plain_16);
    if (nodeAddress.length() > 0) {
        einkDisplay->drawString(5, 30, nodeAddress);
    } else {
        einkDisplay->drawString(5, 30, "RealMesh");
    }
    
    einkDisplay->setFont(ArialMT_Plain_10);
    if (knownNodes == 0) {
        einkDisplay->drawString(50, 60, "Searching...");
    } else {
        einkDisplay->drawString(30, 60, String(knownNodes) + " Nodes Found");
    }
    
    // Draw footer
    einkDisplay->drawLine(0, 107, 250, 107);
    */
    
    // No need to call forceDisplay() - we already refreshed with GxEPD2 directly
    Serial.println("[DISPLAY] ======== UPDATE COMPLETE ========");
    lastUpdate = millis();
    needsUpdate = false;
}

void RealMeshDisplayManager::showTemporaryMessage(const String& title, const String& message, DisplayMessageType type, uint32_t durationMs) {
    tempTitle = title;
    tempMessage = message;
    tempType = type;
    tempMessageActive = true;
    tempMessageTimeout = millis() + durationMs;
    
    // Force immediate update by resetting lastUpdate
    lastUpdate = 0;
    needsUpdate = true;
    updateContent();
}

void RealMeshDisplayManager::clearTemporaryMessage() {
    if (tempMessageActive) {
        tempMessageActive = false;
        updateContent();
    }
}

void RealMeshDisplayManager::addMessage(const String& from, const String& content, bool isNew) {
    addMessageInternal(from, content, isNew);
    
    if (isNew) {
        // Switch to home screen to show new message notification immediately
        currentScreen = SCREEN_HOME;
        needsUpdate = true;
        
        // Also show temporary detailed message
        showTemporaryMessage("New Message", "From: " + from + "\n" + content.substring(0, 30) + (content.length() > 30 ? "..." : ""), DISPLAY_MSG_INFO, 8000);
    }
    
    needsUpdate = true;
}

void RealMeshDisplayManager::addMessageInternal(const String& from, const String& content, bool isNew) {
    // If array is full, remove oldest message
    if (messageCount >= MAX_STORED_MESSAGES) {
        removeOldestMessage();
    }
    
    // Add new message
    messages[messageCount] = {from, content, millis(), !isNew};
    messageCount++;
    
    if (isNew) {
        unreadMessageCount++;
    }
}

void RealMeshDisplayManager::removeOldestMessage() {
    if (messageCount == 0) return;
    
    // Check if oldest message was unread
    if (!messages[0].isRead) {
        unreadMessageCount--;
    }
    
    // Shift all messages down
    for (uint8_t i = 1; i < messageCount; i++) {
        messages[i-1] = messages[i];
    }
    
    messageCount--;
}

void RealMeshDisplayManager::markAllMessagesAsRead() {
    for (uint8_t i = 0; i < messageCount; i++) {
        messages[i].isRead = true;
    }
    unreadMessageCount = 0;
}

void RealMeshDisplayManager::updateBatteryLevel() {
    uint32_t now = millis();
    if (now - lastBatteryUpdate < 30000) return; // Update every 30 seconds
    
    // Read battery voltage
    uint16_t adcValue = analogRead(BATTERY_PIN);
    batteryVoltage = (adcValue / 4095.0) * 3.3 * BATTERY_FACTOR;
    
    // Convert to percentage (assuming Li-ion battery: 3.0V = 0%, 4.2V = 100%)
    if (batteryVoltage >= 4.2) {
        batteryPercentage = 100;
    } else if (batteryVoltage <= 3.0) {
        batteryPercentage = 0;
    } else {
        batteryPercentage = (uint8_t)((batteryVoltage - 3.0) / (4.2 - 3.0) * 100);
    }
    
    lastBatteryUpdate = now;
    needsUpdate = true;
}

void RealMeshDisplayManager::refresh() {
    // Smart refresh logic - only update when there's actually new content
    uint32_t now = millis();
    
    // Only refresh if explicitly needed, or for battery updates every 5 minutes
    if (needsUpdate || (autoRefreshEnabled && (now - lastUpdate > 300000))) { // 5 minutes instead of 10 seconds
        Serial.println("[DISPLAY] Refreshing display (new content or battery update)...");
        updateContent();
    }
    // No unnecessary refreshes - save power and reduce noise
}

void RealMeshDisplayManager::drawHeader() {
    einkDisplay->setFont(ArialMT_Plain_10);
    
    // Node name (left side)
    einkDisplay->setTextAlignment(TEXT_ALIGN_LEFT);
    einkDisplay->drawString(MARGIN, 10, nodeName.length() > 0 ? nodeName : "RealMesh");
    
    // Battery percentage (right side)  
    String batteryText = String(batteryPercentage) + "%";
    einkDisplay->setTextAlignment(TEXT_ALIGN_RIGHT);
    einkDisplay->drawString(SCREEN_WIDTH - MARGIN, 10, batteryText);
    
    // Draw line under header
    einkDisplay->drawHorizontalLine(0, HEADER_HEIGHT, SCREEN_WIDTH);
}

void RealMeshDisplayManager::drawFooter() {
    // Draw line above footer
    int footerY = SCREEN_HEIGHT - FOOTER_HEIGHT;
    einkDisplay->drawHorizontalLine(0, footerY, SCREEN_WIDTH);
    
    drawScreenIndicators();
}

void RealMeshDisplayManager::drawScreenIndicators() {
    // Calculate position for centered dots
    int totalWidth = (SCREEN_COUNT * DOT_SIZE) + ((SCREEN_COUNT - 1) * DOT_SPACING);
    int startX = (SCREEN_WIDTH - totalWidth) / 2;
    int dotY = SCREEN_HEIGHT - FOOTER_HEIGHT + ((FOOTER_HEIGHT - DOT_SIZE) / 2);
    
    for (int i = 0; i < SCREEN_COUNT; i++) {
        int dotX = startX + (i * (DOT_SIZE + DOT_SPACING));
        
        if (i == currentScreen) {
            // Filled dot for current screen
            einkDisplay->fillCircle(dotX + DOT_SIZE/2, dotY + DOT_SIZE/2, DOT_SIZE/2);
        } else {
            // Empty dot for other screens
            einkDisplay->drawCircle(dotX + DOT_SIZE/2, dotY + DOT_SIZE/2, DOT_SIZE/2);
        }
    }
}

void RealMeshDisplayManager::drawContent() {
    Serial.printf("[DISPLAY] Drawing content for screen %d\n", currentScreen);
    switch (currentScreen) {
        case SCREEN_HOME:
            drawHomeScreen();
            break;
        case SCREEN_MESSAGES:
            drawMessagesScreen();
            break;
        case SCREEN_NODE_INFO:
            drawNodeInfoScreen();
            break;
        case SCREEN_COUNT:
            // Not a real screen, just marks the count
            break;
    }
}

void RealMeshDisplayManager::drawHomeScreen() {
    int contentY = HEADER_HEIGHT + 10;
    
    // Node identity - show the actual node address (e.g., "dale@dale")
    einkDisplay->setFont(ArialMT_Plain_16);
    einkDisplay->setTextAlignment(TEXT_ALIGN_LEFT);
    if (nodeAddress.length() > 0) {
        einkDisplay->drawString(5, contentY + 15, nodeAddress); // Show dale@dale
    } else {
        einkDisplay->drawString(5, contentY + 15, "RealMesh"); // Fallback if no identity set yet
    }
    
    // Node count in the center
    einkDisplay->setFont(ArialMT_Plain_10);
    int nodeCountY = contentY + 40;
    
    // Show network status
    if (knownNodes == 0) {
        einkDisplay->drawString(50, nodeCountY, "Searching...");
    } else if (knownNodes == 1) {
        einkDisplay->drawString(45, nodeCountY, "1 Node Found");
    } else {
        einkDisplay->drawString(30, nodeCountY, String(knownNodes) + " Nodes Found");
    }
    
    // Message indicator if there are unread messages
    if (unreadMessageCount > 0) {
        String msgText = String(unreadMessageCount) + " new message" + (unreadMessageCount != 1 ? "s" : "");
        einkDisplay->drawString(20, contentY + 60, msgText);
        
        // Notification icon
        einkDisplay->fillCircle(15, contentY + 55, 3);
    }
}

void RealMeshDisplayManager::drawMessagesScreen() {
    // TODO: Convert to OLEDDisplay API
    einkDisplay->setFont(ArialMT_Plain_16);
    einkDisplay->setTextAlignment(TEXT_ALIGN_LEFT);
    einkDisplay->drawString(MARGIN, HEADER_HEIGHT + 20, "Messages Screen");
}

void RealMeshDisplayManager::drawNewMessageScreen() {
    // TODO: Convert to OLEDDisplay API
    einkDisplay->setFont(ArialMT_Plain_16);
    einkDisplay->setTextAlignment(TEXT_ALIGN_LEFT);
    einkDisplay->drawString(MARGIN, HEADER_HEIGHT + 20, "New Message Screen");
}

void RealMeshDisplayManager::drawNodeInfoScreen() {
    // TODO: Convert to OLEDDisplay API
    einkDisplay->setFont(ArialMT_Plain_16);
    einkDisplay->setTextAlignment(TEXT_ALIGN_LEFT);
    einkDisplay->drawString(MARGIN, HEADER_HEIGHT + 20, "Node Info Screen");
}

void RealMeshDisplayManager::drawBluetoothInfoScreen() {
    // TODO: Convert to OLEDDisplay API
    einkDisplay->setFont(ArialMT_Plain_16);
    einkDisplay->setTextAlignment(TEXT_ALIGN_LEFT);
    einkDisplay->drawString(MARGIN, HEADER_HEIGHT + 20, "BT Info Screen");
}

String RealMeshDisplayManager::formatTime(uint32_t timestamp) {
    uint32_t seconds = (millis() - timestamp) / 1000;
    if (seconds < 60) return String(seconds) + "s ago";
    if (seconds < 3600) return String(seconds / 60) + "m ago";
    return String(seconds / 3600) + "h ago";
}

uint16_t RealMeshDisplayManager::getTextWidth(const String& text, const GFXfont* font) {
    // Simple approximation - each character is about 6 pixels wide
    return text.length() * 6;
}

void RealMeshDisplayManager::drawCenteredText(const String& text, int16_t y, const GFXfont* font) {
    // TODO: Convert to OLEDDisplay API
    einkDisplay->setFont(ArialMT_Plain_10);
    einkDisplay->setTextAlignment(TEXT_ALIGN_CENTER);
    einkDisplay->drawString(SCREEN_WIDTH / 2, y, text);
}

void RealMeshDisplayManager::drawRightAlignedText(const String& text, int16_t x, int16_t y, const GFXfont* font) {
    // TODO: Convert to OLEDDisplay API
    einkDisplay->setFont(ArialMT_Plain_10);
    einkDisplay->setTextAlignment(TEXT_ALIGN_RIGHT);
    einkDisplay->drawString(x, y, text);
}

void RealMeshDisplayManager::drawWrappedText(const String& text, int16_t x, int16_t y, uint16_t maxWidth, const GFXfont* font) {
    // TODO: Convert to OLEDDisplay API
    einkDisplay->setFont(ArialMT_Plain_10);
    einkDisplay->setTextAlignment(TEXT_ALIGN_LEFT);
    einkDisplay->drawString(x, y, text);
}

// ============================================================================
// LED Manager Implementation  
// ============================================================================

RealMeshLEDManager::RealMeshLEDManager()
    : ledState(false), heartbeatEnabled(true), heartbeatInterval(1000), lastHeartbeat(0),
      statusPatternActive(false), patternStartTime(0), patternDuration(0), patternIndex(0) {
}

void RealMeshLEDManager::begin() {
    pinMode(LED_PIN, OUTPUT);
    setLEDInternal(false);
    Serial.println("LED manager initialized");
}

void RealMeshLEDManager::end() {
    setLEDInternal(false);
}

void RealMeshLEDManager::setHeartbeatEnabled(bool enabled) {
    heartbeatEnabled = enabled;
    if (!enabled) {
        setLEDInternal(false);
    }
}

void RealMeshLEDManager::setLED(bool on) {
    if (!statusPatternActive) {
        setLEDInternal(on);
        ledState = on;
    }
}

void RealMeshLEDManager::toggleLED() {
    setLED(!ledState);
}

void RealMeshLEDManager::showStatus(const String& pattern, uint32_t durationMs) {
    currentPattern = pattern;
    statusPatternActive = true;
    patternStartTime = millis();
    patternDuration = durationMs;
    patternIndex = 0;
}

void RealMeshLEDManager::flashError(uint8_t count) {
    String pattern = "";
    for (uint8_t i = 0; i < count; i++) {
        pattern += "1010"; // Fast blink
    }
    pattern += "0000000000"; // Long pause
    showStatus(pattern, 3000);
}

void RealMeshLEDManager::flashSuccess(uint8_t count) {
    String pattern = "";
    for (uint8_t i = 0; i < count; i++) {
        pattern += "1100"; // Slower blink
    }
    pattern += "000000"; // Pause
    showStatus(pattern, 2000);
}

void RealMeshLEDManager::flashWarning(uint8_t count) {
    String pattern = "";
    for (uint8_t i = 0; i < count; i++) {
        pattern += "101010"; // Medium blink
    }
    showStatus(pattern, 2500);
}

void RealMeshLEDManager::loop() {
    if (statusPatternActive) {
        processStatusPattern();
    } else if (heartbeatEnabled) {
        processHeartbeat();
    }
}

void RealMeshLEDManager::processHeartbeat() {
    uint32_t now = millis();
    if (now - lastHeartbeat >= heartbeatInterval) {
        setLEDInternal(!ledState);
        ledState = !ledState;
        lastHeartbeat = now;
    }
}

void RealMeshLEDManager::processStatusPattern() {
    uint32_t now = millis();
    uint32_t elapsed = now - patternStartTime;
    
    // Check if pattern duration expired
    if (patternDuration > 0 && elapsed >= patternDuration) {
        statusPatternActive = false;
        setLEDInternal(false);
        return;
    }
    
    // Process pattern (100ms per character)
    uint32_t patternPos = (elapsed / 100) % currentPattern.length();
    if (patternPos != patternIndex) {
        patternIndex = patternPos;
        char c = currentPattern.charAt(patternIndex);
        setLEDInternal(c == '1');
    }
}

void RealMeshLEDManager::setLEDInternal(bool on) {
    digitalWrite(LED_PIN, on ? HIGH : LOW);
}

// ============================================================================
// Button Manager Implementation
// ============================================================================

RealMeshButtonManager::RealMeshButtonManager()
    : usrPressed(false), prgPressed(false), usrLastState(false), prgLastState(false),
      usrPressTime(0), prgPressTime(0), lastDebounceTime(0) {
}

void RealMeshButtonManager::begin() {
    pinMode(USR_BUTTON_PIN, INPUT_PULLUP);
    pinMode(PRG_BUTTON_PIN, INPUT_PULLUP);
    
    usrLastState = digitalRead(USR_BUTTON_PIN);
    prgLastState = digitalRead(PRG_BUTTON_PIN);
    
    Serial.println("Button manager initialized");
}

void RealMeshButtonManager::end() {
    // Nothing to clean up
}

void RealMeshButtonManager::loop() {
    processButton(USR_BUTTON_PIN, usrPressed, usrLastState, usrPressTime, 
                 usrPressCallback, usrLongPressCallback);
    processButton(PRG_BUTTON_PIN, prgPressed, prgLastState, prgPressTime,
                 prgPressCallback, prgLongPressCallback);
}

void RealMeshButtonManager::processButton(uint8_t pin, bool& currentState, bool& lastState, uint32_t& pressTime,
                                        ButtonCallback pressCallback, ButtonCallback longPressCallback) {
    bool reading = digitalRead(pin) == LOW; // Active low
    uint32_t now = millis();
    
    // Debounce
    if (reading != lastState) {
        lastDebounceTime = now;
    }
    
    if ((now - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != currentState) {
            currentState = reading;
            
            if (currentState) {
                // Button pressed
                pressTime = now;
            } else {
                // Button released
                uint32_t pressDuration = now - pressTime;
                
                if (pressDuration >= LONG_PRESS_DELAY && longPressCallback) {
                    longPressCallback();
                } else if (pressDuration >= 50 && pressCallback) { // Minimum press duration
                    pressCallback();
                }
            }
        }
    }
    
    lastState = reading;
}

// ============================================================================
// Backward Compatibility Functions
// ============================================================================

bool initializeDisplay() {
    if (!displayManager) {
        displayManager = new RealMeshDisplayManager();
    }
    return displayManager->begin();
}

void updateDisplay(const String& title, const String& status, const String& info) {
    if (displayManager) {
        String message = status;
        if (info.length() > 0) {
            message += " - " + info;
        }
        displayManager->showTemporaryMessage(title, message, DISPLAY_MSG_INFO, 5000);
    }
}

void showError(const String& error) {
    if (displayManager) {
        displayManager->showTemporaryMessage("Error", error, DISPLAY_MSG_ERROR, 10000);
    }
}

void showTemporaryMessage(const String& title, const String& status, const String& info, uint32_t timeoutMs) {
    if (displayManager) {
        String message = status;
        if (info.length() > 0) {
            message += " - " + info;
        }
        displayManager->showTemporaryMessage(title, message, DISPLAY_MSG_INFO, timeoutMs);
    }
}
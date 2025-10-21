#include "RealMeshDisplay.h"
#include "RealMeshConfig.h"

// ============================================================================
// Global Display Variables
// ============================================================================

SPIClass* hspi = nullptr;
GxEPD2_BW<GxEPD2_213_FC1, GxEPD2_213_FC1::HEIGHT>* display = nullptr;

// Global manager instances
RealMeshDisplayManager* displayManager = nullptr;
RealMeshLEDManager* ledManager = nullptr;
RealMeshButtonManager* buttonManager = nullptr;

// ============================================================================
// Display Manager Implementation
// ============================================================================

RealMeshDisplayManager::RealMeshDisplayManager() 
    : display(nullptr), displayInitialized(false),
      currentScreen(SCREEN_MESSAGES), needsUpdate(true), autoRefreshEnabled(false), lastUpdate(0),
      tempMessageActive(false), tempMessageTimeout(0),
      messageCount(0), unreadMessageCount(0), currentMessageIndex(0),
      batteryPercentage(100), batteryVoltage(3.7), lastBatteryUpdate(0) {
    
    // Initialize message array
    for (uint8_t i = 0; i < MAX_STORED_MESSAGES; i++) {
        messages[i] = {"", "", 0, true};
    }
}

bool RealMeshDisplayManager::begin() {
    Serial.println("Initializing enhanced display manager...");
    
    // Initialize display using the correct GxEPD2 constructor
    display = new GxEPD2_BW<GxEPD2_213_FC1, GxEPD2_213_FC1::HEIGHT>(
        GxEPD2_213_FC1(EINK_CS, EINK_DC, EINK_RES, EINK_BUSY, SPI)
    );
    
    display->init(115200);
    display->setRotation(0);
    display->setTextWrap(false);
    
    displayInitialized = true;
    
    // Show initial screen
    showTemporaryMessage("RealMesh", "Starting up...", DISPLAY_MSG_INFO, 3000);
    
    Serial.println("Display manager initialized");
    return true;
}

void RealMeshDisplayManager::end() {
    if (display) {
        display->hibernate();
        delete display;
        display = nullptr;
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
    if (!displayInitialized || !display) return;
    
    display->setFullWindow();
    display->firstPage();
    
    do {
        display->fillScreen(GxEPD_WHITE);
        display->setTextColor(GxEPD_BLACK);
        
        // Draw header with node name and battery
        drawHeader();
        
        // Draw main content based on current screen
        if (tempMessageActive && millis() < tempMessageTimeout) {
            // Show temporary message
            display->setFont(&FreeMonoBold12pt7b);
            drawCenteredText(tempTitle, 40);
            
            display->setFont(&FreeMono9pt7b);
            drawCenteredText(tempMessage, 65);
            
            // Show message type indicator
            String typeStr = (tempType == DISPLAY_MSG_ERROR) ? "ERROR" : 
                           (tempType == DISPLAY_MSG_WARNING) ? "WARN" : 
                           (tempType == DISPLAY_MSG_SUCCESS) ? "OK" : "INFO";
            drawCenteredText(typeStr, 85);
        } else {
            // Clear temporary message if expired
            if (tempMessageActive && millis() >= tempMessageTimeout) {
                tempMessageActive = false;
            }
            
            drawContent();
        }
        
        // Draw footer with screen indicators
        drawFooter();
        
    } while (display->nextPage());
    
    lastUpdate = millis();
    needsUpdate = false;
}

void RealMeshDisplayManager::showTemporaryMessage(const String& title, const String& message, DisplayMessageType type, uint32_t durationMs) {
    tempTitle = title;
    tempMessage = message;
    tempType = type;
    tempMessageActive = true;
    tempMessageTimeout = millis() + durationMs;
    
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
    
    if (isNew && currentScreen != SCREEN_NEW_MESSAGE) {
        // Show new message notification
        showTemporaryMessage("New Message", "From: " + from, DISPLAY_MSG_INFO, 5000);
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
    // Only update if needed and enough time has passed (throttling for e-ink)
    uint32_t now = millis();
    if ((autoRefreshEnabled || needsUpdate) && (now - lastUpdate > DISPLAY_UPDATE_INTERVAL)) {
        updateContent();
        lastUpdate = now;
        needsUpdate = false;
    }
}

void RealMeshDisplayManager::drawHeader() {
    display->setFont(&FreeMonoBold9pt7b);
    
    // Node name (left side)
    display->setCursor(MARGIN, 15);
    display->print(nodeName.length() > 0 ? nodeName : "RealMesh");
    
    // Battery percentage (right side)  
    String batteryText = String(batteryPercentage) + "%";
    uint16_t batteryWidth = getTextWidth(batteryText);
    display->setCursor(SCREEN_WIDTH - batteryWidth - MARGIN, 15);
    display->print(batteryText);
    
    // Draw line under header
    display->drawLine(0, HEADER_HEIGHT, SCREEN_WIDTH, HEADER_HEIGHT, GxEPD_BLACK);
}

void RealMeshDisplayManager::drawFooter() {
    // Draw line above footer
    int footerY = SCREEN_HEIGHT - FOOTER_HEIGHT;
    display->drawLine(0, footerY, SCREEN_WIDTH, footerY, GxEPD_BLACK);
    
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
            display->fillCircle(dotX + DOT_SIZE/2, dotY + DOT_SIZE/2, DOT_SIZE/2, GxEPD_BLACK);
        } else {
            // Empty dot for other screens
            display->drawCircle(dotX + DOT_SIZE/2, dotY + DOT_SIZE/2, DOT_SIZE/2, GxEPD_BLACK);
        }
    }
}

void RealMeshDisplayManager::drawContent() {
    switch (currentScreen) {
        case SCREEN_MESSAGES:
            drawMessagesScreen();
            break;
        case SCREEN_NEW_MESSAGE:
            drawNewMessageScreen();
            break;
        case SCREEN_NODE_INFO:
            drawNodeInfoScreen();
            break;
        case SCREEN_BLUETOOTH_INFO:
            drawBluetoothInfoScreen();
            break;
    }
}

void RealMeshDisplayManager::drawMessagesScreen() {
    int contentY = HEADER_HEIGHT + 5;
    
    display->setFont(&FreeMonoBold12pt7b);
    display->setCursor(MARGIN, contentY + 15);
    display->print("Messages");
    
    if (unreadMessageCount > 0) {
        String unreadText = "(" + String(unreadMessageCount) + " new)";
        display->setFont(&FreeMono9pt7b);
        display->setCursor(MARGIN + 80, contentY + 15);
        display->print(unreadText);
    }
    
    // Show recent messages
    display->setFont(&FreeMono9pt7b);
    int msgY = contentY + 35;
    int showCount = min((int)messageCount, 3);
    
    if (showCount == 0) {
        display->setCursor(MARGIN, msgY);
        display->print("No messages");
    } else {
        for (int i = messageCount - showCount; i < messageCount; i++) {
            // Show unread indicator
            if (!messages[i].isRead) {
                display->fillCircle(MARGIN + 2, msgY - 5, 2, GxEPD_BLACK);
            }
            
            // From field
            display->setCursor(MARGIN + 8, msgY);
            String fromStr = messages[i].from;
            if (fromStr.length() > 12) fromStr = fromStr.substring(0, 12) + "...";
            display->print(fromStr + ":");
            
            // Message content
            msgY += 12;
            display->setCursor(MARGIN + 8, msgY);
            String contentStr = messages[i].content;
            if (contentStr.length() > 25) contentStr = contentStr.substring(0, 25) + "...";
            display->print(contentStr);
            
            msgY += 18;
        }
    }
}

void RealMeshDisplayManager::drawNewMessageScreen() {
    int contentY = HEADER_HEIGHT + 5;
    
    display->setFont(&FreeMonoBold12pt7b);
    display->setCursor(MARGIN, contentY + 15);
    display->print("Latest Message");
    
    display->setFont(&FreeMono9pt7b);
    
    if (messageCount > 0) {
        StoredMessage& msg = messages[messageCount - 1];
        
        // From
        display->setCursor(MARGIN, contentY + 35);
        display->print("From: " + msg.from);
        
        // Time
        display->setCursor(MARGIN, contentY + 50);
        display->print("Time: " + formatTime(msg.timestamp));
        
        // Message content (wrapped)
        drawWrappedText(msg.content, MARGIN, contentY + 65, SCREEN_WIDTH - 2*MARGIN);
        
        // Mark as read if currently viewing
        if (!msg.isRead) {
            msg.isRead = true;
            unreadMessageCount--;
        }
    } else {
        display->setCursor(MARGIN, contentY + 35);
        display->print("No messages received");
    }
}

void RealMeshDisplayManager::drawNodeInfoScreen() {
    int contentY = HEADER_HEIGHT + 5;
    
    display->setFont(&FreeMonoBold12pt7b);
    display->setCursor(MARGIN, contentY + 15);
    display->print("Node Info");
    
    display->setFont(&FreeMono9pt7b);
    
    // Node address
    display->setCursor(MARGIN, contentY + 35);
    display->print("Addr: " + nodeAddress);
    
    // Node type
    display->setCursor(MARGIN, contentY + 50);
    display->print("Type: " + nodeType);
    
    // Network info
    display->setCursor(MARGIN, contentY + 65);
    display->print("Nodes: " + String(knownNodes));
    
    // Uptime
    display->setCursor(MARGIN, contentY + 80);
    display->print("Up: " + networkUptime);
}

void RealMeshDisplayManager::drawBluetoothInfoScreen() {
    int contentY = HEADER_HEIGHT + 5;
    
    display->setFont(&FreeMonoBold12pt7b);
    display->setCursor(MARGIN, contentY + 15);
    display->print("Connectivity");
    
    display->setFont(&FreeMono9pt7b);
    
    // Bluetooth status
    display->setCursor(MARGIN, contentY + 35);
    display->print("BLE: " + bleDeviceName);
    
    display->setCursor(MARGIN, contentY + 50);
    display->print("Status: " + String(bleConnected ? "Connected" : "Ready"));
    
    // WiFi status
    display->setCursor(MARGIN, contentY + 65);
    if (wifiSSID.length() > 0) {
        display->print("WiFi: " + wifiSSID);
        if (wifiIP.length() > 0) {
            display->setCursor(MARGIN, contentY + 80);
            display->print("IP: " + wifiIP);
        }
    } else {
        display->print("WiFi: Off");
    }
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
    if (font) display->setFont(font);
    
    uint16_t textWidth = getTextWidth(text, font);
    int16_t x = (SCREEN_WIDTH - textWidth) / 2;
    display->setCursor(x, y);
    display->print(text);
}

void RealMeshDisplayManager::drawRightAlignedText(const String& text, int16_t x, int16_t y, const GFXfont* font) {
    if (font) display->setFont(font);
    
    uint16_t textWidth = getTextWidth(text, font);
    display->setCursor(x - textWidth, y);
    display->print(text);
}

void RealMeshDisplayManager::drawWrappedText(const String& text, int16_t x, int16_t y, uint16_t maxWidth, const GFXfont* font) {
    if (font) display->setFont(font);
    
    // Simple word wrapping implementation
    String line = "";
    int currentY = y;
    
    for (int i = 0; i < text.length(); i++) {
        char c = text.charAt(i);
        
        if (c == ' ' || i == text.length() - 1) {
            String testLine = line + c;
            if (getTextWidth(testLine) > maxWidth && line.length() > 0) {
                display->setCursor(x, currentY);
                display->print(line);
                currentY += 12;
                line = String(c);
            } else {
                line = testLine;
            }
        } else {
            line += c;
        }
    }
    
    if (line.length() > 0) {
        display->setCursor(x, currentY);
        display->print(line);
    }
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
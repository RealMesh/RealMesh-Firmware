#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "RealMeshTypes.h"
#include "RealMeshNode.h"
#include "RealMeshEinkDisplay.h"
#include "RealMeshMobileAPI.h"

// ============================================================================
// Display Configuration (Heltec Wireless Paper - Meshtastic pins)
// ============================================================================

// Using Meshtastic pin configuration for Heltec Wireless Paper
#define EINK_CS     4  
#define EINK_DC     5
#define EINK_RES    6  // Meshtastic uses pin 6 for RST
#define EINK_BUSY   7  // Meshtastic uses pin 7 for BUSY
#define EINK_SCLK   3
#define EINK_MOSI   2

// External display variables (defined in RealMeshEinkDisplay.cpp)
extern SPIClass* hspi;
extern GxEPD2_Display* display;

// ============================================================================
// Global Variables
// ============================================================================

RealMeshNode* meshNode;
RealMeshAPI* mobileAPI;
String inputBuffer = "";
bool cliActive = false;

// Temporary message display
String tempMessage = "";
String tempStatus = "";
String tempInfo = "";
bool tempMessageActive = false;
unsigned long tempMessageTimeout = 0;

// Display carousel
enum DisplayScreen {
  SCREEN_BLE_PAIRING = 0,
  SCREEN_NETWORK_STATUS = 1,
  SCREEN_MESSAGE_STATS = 2,
  SCREEN_COUNT = 3
};

int currentDisplayScreen = SCREEN_BLE_PAIRING;
unsigned long lastScreenChange = 0;
const unsigned long SCREEN_DISPLAY_TIME = 15000; // 15 seconds per screen

// ============================================================================
// Function Declarations
// ============================================================================

void processCLI();
void processCommand(const String& command);
void showHelp();
void showStatus();
void changeName(const String& args);
void changeType(const String& args);
void controlWiFi(const String& args);
void controlBluetooth(const String& args);
void sendMessage(const String& args);
void scanNetwork();
void rebootDevice();
void showPrompt();
void updateDisplay(const String& title, const String& status, const String& info);
void updateCarouselDisplay();
void showBLEPairingScreen();
void showNetworkStatusScreen();
void showMessageStatsScreen();
void showTemporaryMessage(const String& title, const String& status, const String& info, uint32_t timeoutMs = 10000);
void showError(const String& error);
String formatUptime(uint32_t seconds);

// ============================================================================
// Setup Function
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("=== RealMesh Node Starting ===");
  
  // Initialize eInk display
  if (!initializeEinkDisplay()) {
    Serial.println("ERROR: Failed to initialize display");
    return;
  }
  
  showStartupScreen();
  
  // Initialize mesh node
  Serial.println("Initializing mesh node...");
  
  meshNode = new RealMeshNode();
  if (!meshNode->begin("node1", "local")) {
    Serial.println("ERROR: Failed to initialize mesh node");
    showError("Node Init Failed");
    return;
  }
  
  // Set up mesh event callbacks
  meshNode->setOnMessageReceived([](const String& from, const String& message) {
    Serial.println("📨 Message from " + from + ": " + message);
    showTemporaryMessage("Message Received", "From: " + from, message.substring(0, 20), 15000);
  });
  
  meshNode->setOnNetworkEvent([](const String& event, const String& details) {
    Serial.println("🌐 Network: " + event + " - " + details);
  });
  
  meshNode->setOnStateChanged([](RealMeshNode::NodeState oldState, RealMeshNode::NodeState newState) {
    Serial.println("🔄 State changed from " + String(oldState) + " to " + String(newState));
  });
  
  // Update display with ready status
  updateDisplay("RealMesh Ready", "CLI Active", "Type 'help' for commands");
  
  // Initialize mobile API
  Serial.println("Initializing mobile API...");
  mobileAPI = new RealMeshAPI(meshNode);
  
  // Start BLE for mobile API by default
  String deviceName = "RealMesh-" + String(ESP.getEfuseMac(), HEX).substring(8);
  
  if (mobileAPI->beginBLE(deviceName)) {
    Serial.println("✅ BLE API ready");
    Serial.printf("   Device: %s\n", deviceName.c_str());
    
    // Start with BLE pairing screen - no "tap to pair" since this isn't touchscreen
    showBLEPairingScreen();
  } else {
    Serial.println("❌ BLE API failed");
    showError("BLE Failed");
  }
  
  Serial.println("=== RealMesh Node Ready ===");
  Serial.println("CLI Commands available:");
  Serial.println("  help      - Show available commands");
  Serial.println("  status    - Show node status");
  Serial.println("  send <addr> <msg> - Send message");
  Serial.println("  scan      - Scan for nodes");
  Serial.println("  reboot    - Restart device");
  Serial.println("");
  Serial.println("Mobile API:");
  Serial.printf("  BLE: %s (ready for pairing)\n", deviceName.c_str());
  Serial.println("  WiFi: OFF (use 'wifi on' to enable)");
  Serial.println("");
  showPrompt();
  
  cliActive = true;
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
  // Process CLI input
  if (cliActive) {
    processCLI();
  }
  
  // Process mesh node operations
  if (meshNode) {
    meshNode->loop();
  }
  
  // Process mobile API
  if (mobileAPI) {
    mobileAPI->loop();
  }
  
  // Update display periodically (carousel through different screens)
  if (!tempMessageActive && (millis() - lastScreenChange > SCREEN_DISPLAY_TIME)) {
    currentDisplayScreen = (currentDisplayScreen + 1) % SCREEN_COUNT;
    updateCarouselDisplay();
    lastScreenChange = millis();
  }
  
  // Check if temporary message timeout has expired
  if (tempMessageActive && millis() > tempMessageTimeout) {
    tempMessageActive = false;
    // Reset carousel and show current screen
    updateCarouselDisplay();
    lastScreenChange = millis();
  }
  
  delay(10);
}

// ============================================================================
// CLI Functions
// ============================================================================

void processCLI() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c >= 32 && c <= 126) {
      Serial.write(c);
    }
    
    if (c == '\r' || c == '\n') {
      if (inputBuffer.length() > 0) {
        Serial.println();
        processCommand(inputBuffer);
        inputBuffer = "";
        showPrompt();
      } else {
        Serial.println();
        showPrompt();
      }
    } else if (c == '\b' || c == 127) { // Backspace
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        Serial.write('\b');
        Serial.write(' ');
        Serial.write('\b');
      }
    } else if (c >= 32 && c <= 126) { // Printable characters
      inputBuffer += c;
    }
  }
}

void processCommand(const String& command) {
  if (command.length() == 0) return;
  
  String cmd = command;
  String args = "";
  
  int spaceIndex = command.indexOf(' ');
  if (spaceIndex > 0) {
    cmd = command.substring(0, spaceIndex);
    args = command.substring(spaceIndex + 1);
    args.trim();
  }
  
  cmd.toLowerCase();
  
  if (cmd == "help") {
    showHelp();
  } else if (cmd == "status") {
    showStatus();
  } else if (cmd == "name") {
    changeName(args);
  } else if (cmd == "type") {
    changeType(args);
  } else if (cmd == "wifi") {
    controlWiFi(args);
  } else if (cmd == "ble") {
    controlBluetooth(args);
  } else if (cmd == "send") {
    sendMessage(args);
  } else if (cmd == "scan") {
    scanNetwork();
  } else if (cmd == "reboot") {
    rebootDevice();
  } else {
    Serial.println("Unknown command: " + cmd + ". Type 'help' for available commands.");
  }
}

void showHelp() {
  Serial.println("RealMesh CLI Commands:");
  Serial.println("");
  Serial.println("System:");
  Serial.println("  help              - Show this help");
  Serial.println("  status            - Show node status");
  Serial.println("  reboot            - Restart device");
  Serial.println("");
  Serial.println("Configuration:");
  Serial.println("  name <id> <domain> - Change node name and domain");
  Serial.println("  type <mobile|stationary> - Change node type");
  Serial.println("");
  Serial.println("Connectivity:");
  Serial.println("  wifi on           - Enable WiFi AP");
  Serial.println("  wifi off          - Disable WiFi AP");
  Serial.println("  ble on            - Enable BLE");
  Serial.println("  ble off           - Disable BLE");
  Serial.println("");
  Serial.println("Messaging:");
  Serial.println("  send <addr> <msg> - Send message to address");
  Serial.println("");
  Serial.println("Network:");
  Serial.println("  scan              - Scan for nearby nodes");
  Serial.println("");
  Serial.println("📱 Mobile Connection:");
  Serial.println("   For BLE: Use a BLE scanner app like 'nRF Connect'");
  Serial.println("   Regular Bluetooth settings won't show BLE devices!");
  Serial.println("");
}

void showStatus() {
  Serial.println("=== Node Status ===");
  if (meshNode) {
    // Use the actual mesh node methods
    meshNode->printNodeInfo();
    Serial.println();
    meshNode->printNetworkInfo();
    Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    
    // Show mobile API status
    if (mobileAPI) {
      Serial.println();
      Serial.println("=== Mobile API Status ===");
      Serial.printf("BLE Enabled: %s\n", mobileAPI->isBLEEnabled() ? "YES" : "NO");
      if (mobileAPI->isBLEEnabled()) {
        Serial.printf("BLE Device Name: %s\n", mobileAPI->getBLEDeviceName().c_str());
        Serial.println("BLE Status: Advertising and ready for connections");
      }
      Serial.printf("WiFi Enabled: %s\n", mobileAPI->isWiFiEnabled() ? "YES" : "NO");
    }
  } else {
    Serial.println("ERROR: Node not initialized");
  }
}

void sendMessage(const String& args) {
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex <= 0) {
    Serial.println("Usage: send <address> <message>");
    return;
  }
  
  String address = args.substring(0, spaceIndex);
  String message = args.substring(spaceIndex + 1);
  
  if (meshNode) {
    bool success = meshNode->sendMessage(address, message);
    if (success) {
      Serial.println("Message sent to " + address);
      showTemporaryMessage("Message Sent", "To: " + address, message.substring(0, 20), 5000);
    } else {
      Serial.println("Failed to send message");
      showTemporaryMessage("Send Failed", "To: " + address, "Message not delivered", 5000);
    }
  } else {
    Serial.println("ERROR: Node not initialized");
  }
}

void scanNetwork() {
  Serial.println("Scanning for nearby nodes...");
  if (meshNode) {
    // Show current known nodes
    auto knownNodes = meshNode->getKnownNodes();
    Serial.println("Known nodes (" + String(meshNode->getKnownNodesCount()) + "):");
    
    if (knownNodes.empty()) {
      Serial.println("  No nodes discovered yet");
    } else {
      for (const String& node : knownNodes) {
        Serial.println("  - " + node);
      }
    }
    
    // Show network statistics
    Serial.println();
    meshNode->printNetworkInfo();
  } else {
    Serial.println("ERROR: Node not initialized");
  }
}

void changeName(const String& args) {
  if (args.isEmpty()) {
    Serial.println("Usage: name <nodeId> <domain>");
    Serial.println("Example: name mynode home");
    return;
  }
  
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex <= 0) {
    Serial.println("Error: Both nodeId and domain required");
    Serial.println("Usage: name <nodeId> <domain>");
    return;
  }
  
  String nodeId = args.substring(0, spaceIndex);
  String domain = args.substring(spaceIndex + 1);
  nodeId.trim();
  domain.trim();
  
  if (nodeId.isEmpty() || domain.isEmpty()) {
    Serial.println("Error: Both nodeId and domain must be non-empty");
    return;
  }
  
  if (meshNode) {
    Serial.printf("Changing name from %s to %s@%s\n", 
                  meshNode->getOwnAddress().getFullAddress().c_str(),
                  nodeId.c_str(), domain.c_str());
    
    meshNode->setDesiredName(nodeId, domain);
    Serial.println("Name change initiated. Reboot required to apply changes.");
    Serial.println("Use 'reboot' command to restart with new identity.");
    
    showTemporaryMessage("Name Changed", "New: " + nodeId + "@" + domain, "Reboot to apply", 5000);
  } else {
    Serial.println("ERROR: Node not initialized");
  }
}

void changeType(const String& args) {
  if (args.isEmpty()) {
    Serial.println("Usage: type <mobile|stationary>");
    Serial.println("  mobile     - Node moves frequently");
    Serial.println("  stationary - Node stays in fixed location");
    return;
  }
  
  String type = args;
  type.trim();
  type.toLowerCase();
  
  if (meshNode) {
    bool currentStationary = meshNode->isStationary();
    String currentType = currentStationary ? "stationary" : "mobile";
    
    if (type == "mobile") {
      if (!currentStationary) {
        Serial.println("Node is already mobile");
        return;
      }
      meshNode->setStationary(false);
      Serial.println("Changed node type from stationary to mobile");
      showTemporaryMessage("Type Changed", "Now: Mobile", "Moves frequently", 3000);
    } else if (type == "stationary") {
      if (currentStationary) {
        Serial.println("Node is already stationary");
        return;
      }
      meshNode->setStationary(true);
      Serial.println("Changed node type from mobile to stationary");
      showTemporaryMessage("Type Changed", "Now: Stationary", "Fixed location", 3000);
    } else {
      Serial.println("Error: Invalid type '" + type + "'");
      Serial.println("Valid types: mobile, stationary");
      return;
    }
    
    Serial.println("Type change applied immediately");
  } else {
    Serial.println("ERROR: Node not initialized");
  }
}

void controlWiFi(const String& args) {
  if (args.isEmpty()) {
    Serial.println("Usage: wifi <on|off>");
    return;
  }
  
  String action = args;
  action.trim();
  action.toLowerCase();
  
  if (!mobileAPI) {
    Serial.println("ERROR: Mobile API not initialized");
    return;
  }
  
  if (action == "on") {
    if (mobileAPI->isWiFiEnabled()) {
      Serial.println("WiFi is already enabled");
      return;
    }
    
    String wifiSSID = "RealMesh-" + String(ESP.getEfuseMac(), HEX);
    String wifiPassword = "realmesh123";
    
    if (mobileAPI->beginWiFi(wifiSSID, wifiPassword, 8080)) {
      Serial.println("✅ WiFi AP enabled");
      Serial.printf("   SSID: %s\n", wifiSSID.c_str());
      Serial.printf("   Password: %s\n", wifiPassword.c_str());
      Serial.println("   Port: 8080");
      
      showTemporaryMessage("WiFi Enabled", "SSID: " + wifiSSID, "Port: 8080", 5000);
    } else {
      Serial.println("❌ Failed to enable WiFi AP");
      showError("WiFi Failed");
    }
  } else if (action == "off") {
    if (!mobileAPI->isWiFiEnabled()) {
      Serial.println("WiFi is already disabled");
      return;
    }
    
    mobileAPI->stopWiFi();
    Serial.println("WiFi AP disabled");
    showTemporaryMessage("WiFi Disabled", "", "AP stopped", 3000);
  } else {
    Serial.println("Error: Invalid action '" + action + "'");
    Serial.println("Valid actions: on, off");
  }
}

void controlBluetooth(const String& args) {
  if (args.isEmpty()) {
    Serial.println("Usage: ble <on|off>");
    return;
  }
  
  String action = args;
  action.trim();
  action.toLowerCase();
  
  if (!mobileAPI) {
    Serial.println("ERROR: Mobile API not initialized");
    return;
  }
  
  if (action == "on") {
    if (mobileAPI->isBLEEnabled()) {
      Serial.println("BLE is already enabled");
      return;
    }
    
    String deviceName = "RealMesh-" + String(ESP.getEfuseMac(), HEX).substring(8);
    
    if (mobileAPI->beginBLE(deviceName)) {
      Serial.println("✅ BLE enabled");
      Serial.printf("   Device: %s\n", deviceName.c_str());
      
      showTemporaryMessage("BLE Enabled", "Device: " + deviceName, "Ready to pair", 5000);
      
      // Update main display
      updateDisplay("RealMesh Ready", "BLE: " + deviceName, "Tap to pair");
    } else {
      Serial.println("❌ Failed to enable BLE");
      showError("BLE Failed");
    }
  } else if (action == "off") {
    if (!mobileAPI->isBLEEnabled()) {
      Serial.println("BLE is already disabled");
      return;
    }
    
    mobileAPI->stopBLE();
    Serial.println("BLE disabled");
    showTemporaryMessage("BLE Disabled", "", "Disconnected", 3000);
    
    // Update main display
    String nodeAddr = meshNode ? meshNode->getOwnAddress().getFullAddress() : "Unknown";
    updateDisplay("RealMesh Node", nodeAddr, "BLE: OFF");
  } else {
    Serial.println("Error: Invalid action '" + action + "'");
    Serial.println("Valid actions: on, off");
  }
}

void rebootDevice() {
  Serial.println("Rebooting in 3 seconds...");
  updateDisplay("Rebooting...", "", "");
  delay(3000);
  ESP.restart();
}

void showPrompt() {
  Serial.print("realmesh> ");
}

// ============================================================================
// Display Helper Functions
// ============================================================================

void updateDisplay(const String& title, const String& status, const String& info) {
  if (!display) {
    Serial.println("Display not available");
    return;
  }
  
  display->setFullWindow();
  display->firstPage();
  do {
    display->fillScreen(GxEPD_WHITE);
    
    // Set basic text color
    display->setTextColor(GxEPD_BLACK);
    
    // Title
    display->setCursor(10, 25);
    display->println(title);
    
    // Status
    if (status.length() > 0) {
      display->setCursor(10, 50);
      display->println(status);
    }
    
    // Info
    if (info.length() > 0) {
      display->setCursor(10, 75);
      display->println(info);
    }
    
  } while (display->nextPage());
}

void showError(const String& error) {
  if (!display) {
    Serial.println("Display not available for error: " + error);
    return;
  }
  
  display->setFullWindow();
  display->firstPage();
  do {
    display->fillScreen(GxEPD_WHITE);
    display->setTextColor(GxEPD_BLACK);
    display->setCursor(10, 30);
    display->println("ERROR:");
    display->setCursor(10, 60);
    display->println(error);
  } while (display->nextPage());
}

void showTemporaryMessage(const String& title, const String& status, const String& info, uint32_t timeoutMs) {
  updateDisplay(title, status, info);
  tempMessageTimeout = millis() + timeoutMs;
  tempMessageActive = true;
}

// ============================================================================
// Utility Functions
// ============================================================================

String formatUptime(uint32_t seconds) {
  uint32_t hours = seconds / 3600;
  uint32_t mins = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;
  
  return String(hours) + ":" + 
         (mins < 10 ? "0" : "") + String(mins) + ":" +
         (secs < 10 ? "0" : "") + String(secs);
}

// ============================================================================
// Display Carousel Functions
// ============================================================================

void updateCarouselDisplay() {
  switch (currentDisplayScreen) {
    case SCREEN_BLE_PAIRING:
      showBLEPairingScreen();
      break;
    case SCREEN_NETWORK_STATUS:
      showNetworkStatusScreen();
      break;
    case SCREEN_MESSAGE_STATS:
      showMessageStatsScreen();
      break;
    default:
      currentDisplayScreen = SCREEN_BLE_PAIRING;
      showBLEPairingScreen();
      break;
  }
}

void showBLEPairingScreen() {
  if (!mobileAPI) {
    updateDisplay("RealMesh", "Starting...", "");
    return;
  }
  
  String deviceName = mobileAPI->getBLEDeviceName();
  if (deviceName.length() == 0) {
    deviceName = "RealMesh-" + String(ESP.getEfuseMac(), HEX).substring(8);
  }
  
  updateDisplay("Bluetooth Pairing", 
               "Device: " + deviceName,
               "Ready for mobile apps to connect");
}

void showNetworkStatusScreen() {
  if (!meshNode) {
    updateDisplay("RealMesh Network", "Initializing...", "");
    return;
  }
  
  String nodeCount = String(meshNode->getKnownNodesCount());
  String uptime = formatUptime(millis() / 1000);
  
  updateDisplay("Network Status", 
               "Nodes: " + nodeCount + " | Uptime: " + uptime,
               "LoRa Mesh Network Active");
}

void showMessageStatsScreen() {
  if (!meshNode) {
    updateDisplay("Message Stats", "No data available", "");
    return;
  }
  
  auto stats = meshNode->getNetworkStats();
  
  updateDisplay("Message Statistics",
               "Sent: " + String(stats.messagesSent) + " | Received: " + String(stats.messagesReceived),
               "Forwarded: " + String(stats.messagesForwarded) + " | Dropped: " + String(stats.messagesDropped));
}
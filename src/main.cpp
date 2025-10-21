#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "RealMeshTypes.h"
#include "RealMeshNode.h"
#include "RealMeshDisplay.h"
#include "RealMeshMobileAPI.h"

// Display is managed through displayManager

// ============================================================================
// Global Variables
// ============================================================================

RealMeshNode* meshNode;
RealMeshAPI* mobileAPI;
String inputBuffer = "";
bool cliActive = false;

// Enhanced managers (declared in RealMeshDisplay.cpp)
extern RealMeshDisplayManager* displayManager;
extern RealMeshLEDManager* ledManager;  
extern RealMeshButtonManager* buttonManager;

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
void controlLED(const String& args);
void controlScreen(const String& args);
void sendMessage(const String& args);
void scanNetwork();
void rebootDevice();
void showPrompt();
String formatUptime(uint32_t seconds);

// ============================================================================
// Setup Function
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("=== RealMesh Node Starting ===");
  
  // Initialize enhanced hardware managers
  displayManager = new RealMeshDisplayManager();
  ledManager = new RealMeshLEDManager();
  buttonManager = new RealMeshButtonManager();
  
  // Initialize display system
  if (!displayManager->begin()) {
    Serial.println("ERROR: Failed to initialize display manager");
    return;
  }
  
  // Initialize LED system
  ledManager->begin();
  ledManager->setHeartbeatEnabled(true);
  ledManager->flashSuccess(2); // Startup success indication
  
  // Initialize button system with callbacks
  buttonManager->begin();
  buttonManager->setUSRPressCallback([]() {
    Serial.println("USR button pressed - next screen");
    if (displayManager) {
      displayManager->nextScreen();
    }
  });
  
  buttonManager->setUSRLongPressCallback([]() {
    Serial.println("USR button long press - toggle LED heartbeat");
    if (ledManager) {
      ledManager->setHeartbeatEnabled(!ledManager->isHeartbeatEnabled());
      Serial.println("LED heartbeat: " + String(ledManager->isHeartbeatEnabled() ? "ON" : "OFF"));
    }
  });
  
  // Initialize mesh node
  Serial.println("Initializing mesh node...");
  
  meshNode = new RealMeshNode();
  
  if (!meshNode->begin("node1", "local")) {
    Serial.println("ERROR: Failed to initialize mesh node");
    if (displayManager) {
      displayManager->showTemporaryMessage("Error", "Node Init Failed", DISPLAY_MSG_ERROR, 10000);
    }
    if (ledManager) {
      ledManager->flashError(5);
    }
    return;
  }
  
  // Set up mesh event callbacks
  meshNode->setOnMessageReceived([](const String& from, const String& message) {
    Serial.println("📨 Message from " + from + ": " + message);
    
    // Add message to display manager and show notification
    if (displayManager) {
      displayManager->addMessage(from, message, true);
    }
    
    // Flash LED for new message
    if (ledManager) {
      ledManager->flashSuccess(2);
    }
  });
  
  meshNode->setOnNetworkEvent([](const String& event, const String& details) {
    Serial.println("🌐 Network: " + event + " - " + details);
    
    // Update display with network information
    if (displayManager && meshNode) {
      String uptime = formatUptime(millis() / 1000);
      displayManager->setNetworkInfo(meshNode->getKnownNodesCount(), uptime);
    }
  });
  
  meshNode->setOnStateChanged([](RealMeshNode::NodeState oldState, RealMeshNode::NodeState newState) {
    Serial.println("🔄 State changed from " + String(oldState) + " to " + String(newState));
    
    // Update node info on display
    if (displayManager && meshNode) {
      displayManager->setNodeName(meshNode->getOwnAddress().nodeId);
      displayManager->setNodeAddress(meshNode->getOwnAddress().getFullAddress());
      displayManager->setNodeType(meshNode->isStationary() ? "Stationary" : "Mobile");
    }
  });
  
  // Initialize mobile API
  Serial.println("Initializing mobile API...");
  mobileAPI = new RealMeshAPI(meshNode);
  
  // Start BLE for mobile API by default
  String deviceName = "RealMesh-" + String(ESP.getEfuseMac(), HEX).substring(8);
  
  if (mobileAPI->beginBLE(deviceName)) {
    Serial.println("✅ BLE API ready");
    Serial.printf("   Device: %s\n", deviceName.c_str());
    
    // Update display with BLE info
    if (displayManager) {
      displayManager->setBluetoothInfo(deviceName, false);
      displayManager->showTemporaryMessage("BLE Ready", "Device: " + deviceName, DISPLAY_MSG_SUCCESS, 3000);
    }
    
    if (ledManager) {
      ledManager->flashSuccess(1);
    }
  } else {
    Serial.println("❌ BLE API failed");
    if (displayManager) {
      displayManager->showTemporaryMessage("Error", "BLE Failed", DISPLAY_MSG_ERROR, 5000);
    }
    if (ledManager) {
      ledManager->flashError(3);
    }
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
  // Process hardware managers
  if (ledManager) {
    ledManager->loop();
  }
  
  if (buttonManager) {
    buttonManager->loop();
  }
  
  // Update battery level periodically
  if (displayManager) {
    displayManager->updateBatteryLevel();
  }
  
  // Process CLI input
  if (cliActive) {
    processCLI();
  } else if (Serial.available()) {
    // If CLI not active due to setup failure, activate it manually
    Serial.println("\n=== RealMesh CLI Activated ===");
    showPrompt();
    cliActive = true;
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
  
  // Refresh display only when actually needed
  if (displayManager) {
    // Only refresh if there's new content or every 5 minutes for battery update
    static uint32_t lastDisplayCheck = 0;
    if (millis() - lastDisplayCheck > 5000) { // Check every 5 seconds instead of every 10ms
      displayManager->refresh();
      lastDisplayCheck = millis();
    }
  }
  
  delay(10);
}

// ============================================================================
// CLI Functions
// ============================================================================

void processCLI() {
  while (Serial.available()) {
    char c = Serial.read();
    
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
      Serial.write(c); // Echo character
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
  } else if (cmd == "led") {
    controlLED(args);
  } else if (cmd == "screen") {
    controlScreen(args);
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
  Serial.println("Hardware:");
  Serial.println("  led on|off|toggle - Control LED state");
  Serial.println("  led heartbeat on|off - Enable/disable heartbeat");
  Serial.println("  led interval <ms> - Set heartbeat interval");
  Serial.println("  screen next|prev  - Change display screen");
  Serial.println("  screen <0-3>      - Go to specific screen");
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
      if (displayManager) {
        displayManager->showTemporaryMessage("Message Sent", "To: " + address, DISPLAY_MSG_SUCCESS, 5000);
      }
      if (ledManager) {
        ledManager->flashSuccess(1);
      }
    } else {
      Serial.println("Failed to send message");
      if (displayManager) {
        displayManager->showTemporaryMessage("Send Failed", "To: " + address, DISPLAY_MSG_ERROR, 5000);
      }
      if (ledManager) {
        ledManager->flashError(2);
      }
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
    
    if (displayManager) {
      displayManager->showTemporaryMessage("Name Changed", "New: " + nodeId + "@" + domain, DISPLAY_MSG_INFO, 5000);
    }
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
      if (displayManager) {
        displayManager->showTemporaryMessage("Type Changed", "Now: Mobile", DISPLAY_MSG_INFO, 3000);
      }
    } else if (type == "stationary") {
      if (currentStationary) {
        Serial.println("Node is already stationary");
        return;
      }
      meshNode->setStationary(true);
      Serial.println("Changed node type from mobile to stationary");
      if (displayManager) {
        displayManager->showTemporaryMessage("Type Changed", "Now: Stationary", DISPLAY_MSG_INFO, 3000);
      }
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
      
      if (displayManager) {
        displayManager->setWiFiInfo(wifiSSID, "192.168.4.1");
        displayManager->showTemporaryMessage("WiFi Enabled", "SSID: " + wifiSSID, DISPLAY_MSG_SUCCESS, 5000);
      }
    } else {
      Serial.println("❌ Failed to enable WiFi AP");
      if (displayManager) {
        displayManager->showTemporaryMessage("Error", "WiFi Failed", DISPLAY_MSG_ERROR, 5000);
      }
    }
  } else if (action == "off") {
    if (!mobileAPI->isWiFiEnabled()) {
      Serial.println("WiFi is already disabled");
      return;
    }
    
    mobileAPI->stopWiFi();
    Serial.println("WiFi AP disabled");
    if (displayManager) {
      displayManager->setWiFiInfo("", "");
      displayManager->showTemporaryMessage("WiFi Disabled", "AP stopped", DISPLAY_MSG_INFO, 3000);
    }
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
      
      if (displayManager) {
        displayManager->setBluetoothInfo(deviceName, false);
        displayManager->showTemporaryMessage("BLE Enabled", "Device: " + deviceName, DISPLAY_MSG_SUCCESS, 5000);
      }
    } else {
      Serial.println("❌ Failed to enable BLE");
      if (displayManager) {
        displayManager->showTemporaryMessage("Error", "BLE Failed", DISPLAY_MSG_ERROR, 5000);
      }
    }
  } else if (action == "off") {
    if (!mobileAPI->isBLEEnabled()) {
      Serial.println("BLE is already disabled");
      return;
    }
    
    mobileAPI->stopBLE();
    Serial.println("BLE disabled");
    if (displayManager) {
      displayManager->setBluetoothInfo("", false);
      displayManager->showTemporaryMessage("BLE Disabled", "Disconnected", DISPLAY_MSG_INFO, 3000);
    }
  } else {
    Serial.println("Error: Invalid action '" + action + "'");
    Serial.println("Valid actions: on, off");
  }
}

void controlLED(const String& args) {
  if (args.isEmpty()) {
    Serial.println("Usage: led <on|off|toggle|heartbeat|interval>");
    Serial.println("  on                - Turn LED on");
    Serial.println("  off               - Turn LED off");
    Serial.println("  toggle            - Toggle LED state");
    Serial.println("  heartbeat on|off  - Control heartbeat");
    Serial.println("  interval <ms>     - Set heartbeat interval");
    return;
  }
  
  if (!ledManager) {
    Serial.println("ERROR: LED manager not initialized");
    return;
  }
  
  String arg1, arg2;
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex > 0) {
    arg1 = args.substring(0, spaceIndex);
    arg2 = args.substring(spaceIndex + 1);
    arg2.trim();
  } else {
    arg1 = args;
  }
  arg1.trim();
  arg1.toLowerCase();
  
  if (arg1 == "on") {
    ledManager->setLED(true);
    Serial.println("LED turned on");
  } else if (arg1 == "off") {
    ledManager->setLED(false);
    Serial.println("LED turned off");
  } else if (arg1 == "toggle") {
    ledManager->toggleLED();
    Serial.println("LED toggled to " + String(ledManager->getLEDState() ? "ON" : "OFF"));
  } else if (arg1 == "heartbeat") {
    if (arg2.isEmpty()) {
      Serial.println("Current heartbeat: " + String(ledManager->isHeartbeatEnabled() ? "ON" : "OFF"));
      Serial.println("Interval: " + String(ledManager->getHeartbeatInterval()) + "ms");
      return;
    }
    
    arg2.toLowerCase();
    if (arg2 == "on") {
      ledManager->setHeartbeatEnabled(true);
      Serial.println("LED heartbeat enabled");
    } else if (arg2 == "off") {
      ledManager->setHeartbeatEnabled(false);
      Serial.println("LED heartbeat disabled");
    } else {
      Serial.println("Invalid heartbeat option. Use 'on' or 'off'");
    }
  } else if (arg1 == "interval") {
    if (arg2.isEmpty()) {
      Serial.println("Current interval: " + String(ledManager->getHeartbeatInterval()) + "ms");
      return;
    }
    
    int interval = arg2.toInt();
    if (interval >= 100 && interval <= 10000) {
      ledManager->setHeartbeatInterval(interval);
      Serial.println("Heartbeat interval set to " + String(interval) + "ms");
    } else {
      Serial.println("Invalid interval. Use 100-10000ms");
    }
  } else {
    Serial.println("Invalid LED command. Use 'help' for available options");
  }
}

void controlScreen(const String& args) {
  if (args.isEmpty()) {
    Serial.println("Usage: screen <next|prev|0|1|2>");
    Serial.println("  next    - Go to next screen");
    Serial.println("  prev    - Go to previous screen");
    Serial.println("  0       - Home screen");
    Serial.println("  1       - Messages screen");
    Serial.println("  2       - Node info screen");
    return;
  }
  
  if (!displayManager) {
    Serial.println("ERROR: Display manager not initialized");
    return;
  }
  
  String action = args;
  action.trim();
  action.toLowerCase();
  
  if (action == "next") {
    displayManager->nextScreen();
    Serial.println("Switched to next screen");
  } else if (action == "prev") {
    displayManager->previousScreen();
    Serial.println("Switched to previous screen");
  } else if (action.length() == 1 && action[0] >= '0' && action[0] <= '2') {
    int screenNum = action[0] - '0';
    displayManager->setCurrentScreen((DisplayScreen)screenNum);
    
    String screenNames[] = {"Home", "Messages", "Node Info"};
    Serial.println("Switched to " + screenNames[screenNum] + " screen");
  } else {
    Serial.println("Invalid screen command. Use 'help' for available options");
  }
}

void rebootDevice() {
  Serial.println("Rebooting in 3 seconds...");
  if (displayManager) {
    displayManager->showTemporaryMessage("Rebooting", "Please wait...", DISPLAY_MSG_INFO, 3000);
  }
  if (ledManager) {
    ledManager->flashWarning(5);
  }
  delay(3000);
  ESP.restart();
}

void showPrompt() {
  Serial.print("realmesh> ");
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
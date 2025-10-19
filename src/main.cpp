#include <Arduino.h>
#include "RealMeshEinkDisplay.h"
#include "RealMeshConfig.h"

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("� RealMesh Framework Starting...");
    Serial.printf("Version: %s\n", RM_FIRMWARE_VERSION);
    
    // Initialize eInk display
    if (initializeEinkDisplay()) {
        Serial.println("✅ Display initialized successfully");
        
        // Show startup screen once
        showStartupScreen();
    } else {
        Serial.println("❌ Display initialization failed");
    }
}

void loop() {
    // Main loop - no periodic display updates
    // Add your RealMesh mesh networking logic here
    
    delay(1000);
}
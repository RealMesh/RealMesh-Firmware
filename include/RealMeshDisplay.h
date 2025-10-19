#pragma once

#ifdef HELTEC_WIRELESS_PAPER

#include <Arduino.h>
#include "RealMeshConfig.h"

// ============================================================================
// RealMesh E-ink Display Manager (Simplified Version)
// ============================================================================

class RealMeshDisplay {
public:
    RealMeshDisplay();
    
    // Display management
    bool begin();
    void clear();
    void update();
    void sleep();
    
    // Content display
    void showWelcomeScreen();
    void showStatusScreen(const String& nodeId, const String& subdomain, 
                         int batteryPercent, const String& bluetoothPin);
    void showMessageScreen(const String& from, const String& message);
    void showNetworkInfo(int nodeCount, int signalStrength);
    
    // Utility
    int getBatteryPercentage();
    String generateBluetoothPin();
    
private:
    bool initialized;
    String currentBluetoothPin;
    
    // For now, we'll just use Serial output until we get the right display driver
    void logDisplayUpdate(const String& content);
};

#endif // HELTEC_WIRELESS_PAPER
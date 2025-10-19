#include "RealMeshEinkDisplay.h"
#include "RealMeshConfig.h"

// Pin definitions for Heltec Wireless Paper
#define PIN_EINK_CS      4
#define PIN_EINK_DC      5
#define PIN_EINK_RES     6   // Meshtastic uses pin 6 for RST
#define PIN_EINK_BUSY    7   // Meshtastic uses pin 7 for BUSY
#define PIN_EINK_SCLK    3
#define PIN_EINK_MOSI    2
#define PIN_VEXT_ENABLE  45  // Power enable pin (active LOW)

// Global variables
SPIClass* hspi = nullptr;
GxEPD2_Display* display = nullptr;



bool initializeEinkDisplay() 
{
    Serial.println("🎨 RealMesh eInk Display Initialization");
    
    // Enable VEXT power (Active LOW)
    pinMode(PIN_VEXT_ENABLE, OUTPUT);
    digitalWrite(PIN_VEXT_ENABLE, LOW);  // Enable power
    delay(100);
    
    // Start HSPI
    Serial.println("🔧 Starting HSPI...");
    hspi = new SPIClass(HSPI);
    hspi->begin(PIN_EINK_SCLK, -1, PIN_EINK_MOSI, PIN_EINK_CS); // SCLK, MISO, MOSI, SS
    
    // Create GxEPD2 object for LCMEN213EFC1
    Serial.println("🎨 Creating GxEPD2 display object...");
    auto lowLevel = new GxEPD2_213_FC1(PIN_EINK_CS, PIN_EINK_DC, PIN_EINK_RES, PIN_EINK_BUSY, *hspi);
    display = new GxEPD2_Display(*lowLevel);

    // Initialize GxEPD2
    Serial.println("🔄 Initializing display...");
    display->init();
    display->setRotation(3);  // Landscape orientation
    
    Serial.printf("📐 Display ready: %dx%d\n", 250, 122);
    Serial.println("✅ RealMesh eInk display initialized!");
    
    return true;
}

void showStartupScreen() 
{
    if (!display) {
        Serial.println("❌ Display not initialized!");
        return;
    }
    
    Serial.println("🚀 Showing RealMesh startup screen...");
    
    display->setFullWindow();
    display->firstPage();
    do {
        // Clear to white background
        display->fillScreen(GxEPD_WHITE);
        
        // Set text properties for title
        display->setTextColor(GxEPD_BLACK);
        display->setFont(&FreeMonoBold12pt7b);
        
        // Title - RealMesh version (centered)
        String titleText = "RealMesh v" + String(RM_FIRMWARE_VERSION);
        int16_t tbx, tby; 
        uint16_t tbw, tbh;
        display->getTextBounds(titleText, 0, 0, &tbx, &tby, &tbw, &tbh);
        int titleX = (250 - tbw) / 2;
        display->setCursor(titleX, 30);
        display->print(titleText);
        
        // Status message
        display->setFont(&FreeMono9pt7b);
        display->setCursor(50, 80);
        display->print("Display Ready!");
        
    } while (display->nextPage());
    
    Serial.println("✅ Startup screen displayed: RealMesh v" + String(RM_FIRMWARE_VERSION));
    
    // Power down display
    display->hibernate();
    Serial.println("💤 Display hibernated");
}

void clearDisplay() 
{
    if (!display) {
        Serial.println("❌ Display not initialized!");
        return;
    }
    
    Serial.println("🧹 Clearing display...");
    
    display->setFullWindow();
    display->firstPage();
    do {
        display->fillScreen(GxEPD_WHITE);
    } while (display->nextPage());
    
    display->hibernate();
    Serial.println("✅ Display cleared");
}




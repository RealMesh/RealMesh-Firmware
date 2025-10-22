#include "EInkDisplay.h"
#include "RealMeshConfig.h"
#include <SPI.h>

// Pin definitions from RealMeshDisplay.h
#define EINK_CS     4  
#define EINK_DC     5
#define EINK_RES    6
#define EINK_BUSY   7
#define EINK_SCLK   3
#define EINK_MOSI   2
#define PIN_VEXT_ENABLE  45

#define EINK_WIDTH  250
#define EINK_HEIGHT 122

// Constructor - calls OLEDDisplay() constructor which initializes the buffer pointer
EInkDisplay::EInkDisplay(OLEDDISPLAY_GEOMETRY geometry) : OLEDDisplay() {
    // Set dimensions in OLEDDisplay base class
    this->geometry = GEOMETRY_RAWMODE;
    this->displayWidth = EINK_WIDTH;
    this->displayHeight = EINK_HEIGHT;
    
    // Round shortest side up to nearest byte
    uint16_t shortSide = min(EINK_WIDTH, EINK_HEIGHT);
    uint16_t longSide = max(EINK_WIDTH, EINK_HEIGHT);
    if (shortSide % 8 != 0)
        shortSide = (shortSide | 7) + 1;
    
    this->displayBufferSize = longSide * (shortSide / 8);
    
    Serial.printf("[EINK] EInkDisplay constructed: width=%u, height=%u, bufferSize=%u\n", 
                  displayWidth, displayHeight, displayBufferSize);
}

EInkDisplay::~EInkDisplay() {
    if (adafruitDisplay) {
        delete adafruitDisplay;
    }
    if (hspi) {
        hspi->end();
        delete hspi;
    }
}

/**
 * Force a display update - copies framebuffer pixel-by-pixel to GxEPD2
 * This is the KEY function that bypasses GxEPD2's buffer comparison
 */
bool EInkDisplay::forceDisplay(uint32_t msecLimit) {
    uint32_t now = millis();
    uint32_t sinceLast = now - lastDrawMsec;
    
    if (!adafruitDisplay) {
        Serial.println("[EINK] ERROR: adafruitDisplay is NULL! Aborting display update.");
        return false;
    }
    if (!buffer) {
        Serial.println("[EINK] ERROR: Framebuffer pointer is NULL! Aborting display update.");
        Serial.printf("[EINK] buffer=%p, displayBufferSize=%u\n", buffer, displayBufferSize);
        return false;
    }
    if (sinceLast < msecLimit && lastDrawMsec != 0) {
        return false;
    }
    
    // Add explicit serial flush to ensure message prints before crash
    Serial.printf("[EINK] Starting forceDisplay - buffer=%p, size=%ux%u\n", buffer, displayWidth, displayHeight);
    Serial.flush();
    
    lastDrawMsec = now;
    
    Serial.println("[EINK] Calling setFullWindow()...");
    Serial.flush();
    adafruitDisplay->setFullWindow();
    
    Serial.println("[EINK] Calling firstPage()...");
    Serial.flush();
    adafruitDisplay->firstPage();
    
    Serial.println("[EINK] Starting pixel copy loop...");
    Serial.flush();
    
    // DEBUG: Print first few buffer bytes to see what's in there
    Serial.print("[EINK] Buffer first 20 bytes: ");
    for (int i = 0; i < 20; i++) {
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();
    
    uint32_t startTime = millis();
    for (uint32_t y = 0; y < displayHeight; y++) {
        for (uint32_t x = 0; x < displayWidth; x++) {
            auto b = buffer[x + (y / 8) * displayWidth];
            auto isset = b & (1 << (y & 7));
            // TRY INVERTING: isset means BLACK in OLEDDisplay, should be BLACK in GxEPD2
            adafruitDisplay->drawPixel(x, y, isset ? GxEPD_BLACK : GxEPD_WHITE);
        }
    }
    uint32_t copyTime = millis() - startTime;
    Serial.printf("[EINK] Pixel copy took %lu ms\n", (unsigned long)copyTime);
    Serial.println("[EINK] Calling nextPage() to refresh...");
    startTime = millis();
    adafruitDisplay->nextPage();
    uint32_t refreshTime = millis() - startTime;
    Serial.printf("[EINK] nextPage() took %lu ms\n", (unsigned long)refreshTime);
    endUpdate();
    Serial.println("[EINK] Display update complete");
    return true;
}

// End update - hibernate the display
void EInkDisplay::endUpdate() {
    adafruitDisplay->hibernate();
}

// Write buffer to display (called periodically)
void EInkDisplay::display(void) {
    if (lastDrawMsec) {
        forceDisplay(slowUpdateMsec);
    }
}

// Send command (not used with GxEPD2)
void EInkDisplay::sendCommand(uint8_t com) {
    // Not needed
}

// Connect to the display
bool EInkDisplay::connect() {
    Serial.println("[EINK] Initializing E-Ink display...");
    
    // Initialize VEXT power control
    pinMode(PIN_VEXT_ENABLE, OUTPUT);
    digitalWrite(PIN_VEXT_ENABLE, LOW);  // Enable display power (active low)
    delay(200);
    
    // Start HSPI
    hspi = new SPIClass(HSPI);
    hspi->begin(EINK_SCLK, -1, EINK_MOSI, EINK_CS);
    
    // Create GxEPD2 object
    auto lowLevel = new GxEPD2_213_FC1(EINK_CS, EINK_DC, EINK_RES, EINK_BUSY, *hspi);
    adafruitDisplay = new GxEPD2_BW<GxEPD2_213_FC1, GxEPD2_213_FC1::HEIGHT>(*lowLevel);
    
    // Init GxEPD2
    adafruitDisplay->init();
    adafruitDisplay->setRotation(3);
    
    Serial.println("[EINK] E-Ink display initialized");
    return true;
}

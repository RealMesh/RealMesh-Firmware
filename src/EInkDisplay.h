#pragma once

#include <Arduino.h>
#include <OLEDDisplay.h>
#include <GxEPD2_BW.h>
#include <epd/GxEPD2_213_FC1.h>

/**
 * Adapter class based on Meshtastic's EInkDisplay2
 * Allows drawing to an internal framebuffer, then copying pixel-by-pixel to GxEPD2
 * This bypasses GxEPD2's problematic buffer comparison
 */
class EInkDisplay : public OLEDDisplay {
public:
    EInkDisplay(OLEDDISPLAY_GEOMETRY geometry);
    virtual ~EInkDisplay();
    
    /**
     * Force a display update - copies framebuffer to GxEPD2 pixel-by-pixel
     */
    bool forceDisplay(uint32_t msecLimit = 1000);
    
    /**
     * Write buffer to display (called periodically)
     */
    virtual void display(void) override;
    
    /**
     * Get direct access to framebuffer for debugging
     */
    uint8_t* getBuffer() { return buffer; }
    
    /**
     * Get direct access to GxEPD2 for advanced drawing
     */
    GxEPD2_BW<GxEPD2_213_FC1, GxEPD2_213_FC1::HEIGHT>* getGxEPD2() { return adafruitDisplay; }
    
    // Public access needed for initialization
    virtual bool connect() override;
    
protected:
    virtual int getBufferOffset(void) override { return 0; }
    virtual void sendCommand(uint8_t com) override;
    
    void endUpdate();
    
    // GxEPD2 display object
    GxEPD2_BW<GxEPD2_213_FC1, GxEPD2_213_FC1::HEIGHT>* adafruitDisplay = nullptr;
    SPIClass* hspi = nullptr;
    
private:
    uint32_t lastDrawMsec = 0;
    uint32_t slowUpdateMsec = 5 * 60 * 1000; // 5 minutes between updates
};

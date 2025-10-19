#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

// Include the specific driver for LCMEN213EFC1 - Fitipower JD79656
#include <epd/GxEPD2_213_FC1.h>

// Pin definitions - Heltec Wireless Paper
#define PIN_EINK_CS     4
#define PIN_EINK_DC     5  
#define PIN_EINK_RES    6
#define PIN_EINK_BUSY   7
#define PIN_EINK_SCLK   3
#define PIN_EINK_MOSI   2
#define PIN_VEXT_ENABLE 45

// Display dimensions
#define EINK_WIDTH  250
#define EINK_HEIGHT 122

// Use the specific driver for our controller
typedef GxEPD2_BW<GxEPD2_213_FC1, GxEPD2_213_FC1::HEIGHT> GxEPD2_Display;

// Global variables
extern SPIClass* hspi;
extern GxEPD2_Display* display;

// Function declarations
bool initializeEinkDisplay();
void showStartupScreen();
void clearDisplay();
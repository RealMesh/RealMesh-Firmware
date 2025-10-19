#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_Display.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_213_FC1.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// Global display variables
extern SPIClass* hspi;
extern GxEPD2_Display* display;

// Display functions
bool initializeEinkDisplay();
void showStartupScreen();
void clearDisplay();
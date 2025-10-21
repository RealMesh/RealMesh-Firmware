#pragma once

// Legacy compatibility header - redirects to new RealMeshDisplay system
#include "RealMeshDisplay.h"

// Legacy function redirects
#define initializeEinkDisplay() initializeDisplay()
#define showStartupScreen() displayManager ? displayManager->showTemporaryMessage("RealMesh", "Starting...", MSG_INFO, 3000) : false
#define clearDisplay() displayManager ? displayManager->refresh() : void()
# RealMesh - ESP32 Implementation

This directory contains the ESP32/Arduino implementation of RealMesh for Heltec V3 devices.

## Quick Start

1. Install PlatformIO
2. Open this project in VS Code with PlatformIO extension
3. Build and upload to Heltec V3 device

## Project Structure

```
src/
├── main.cpp              # Main application entry point
├── RealMeshRadio.cpp     # LoRa radio abstraction layer
├── RealMeshRouter.cpp    # Routing engine implementation
├── RealMeshNode.cpp      # Node identity and management
└── RealMeshQueue.cpp     # Message queuing system

include/
├── RealMeshConfig.h      # Configuration constants
├── RealMeshTypes.h       # Data structures and types
├── RealMeshRadio.h       # Radio interface
├── RealMeshRouter.h      # Routing interface
├── RealMeshNode.h        # Node interface
└── RealMeshQueue.h       # Queue interface

lib/
└── (external libraries)
```

## Configuration

Edit `include/RealMeshConfig.h` to modify:
- Radio parameters (frequency, power, etc.)
- Timing values (timeouts, intervals)
- Queue sizes and limits
- Debug options

## Development Status

- [x] Project structure setup
- [x] Core data types defined
- [x] Configuration system
- [ ] LoRa radio wrapper
- [ ] Message packet handling
- [ ] Routing engine
- [ ] Node identity system
- [ ] Message queuing
- [ ] Network protocols

## Hardware Support

- **Target**: Heltec WiFi LoRa 32 V3
- **Frequency**: 868MHz (EU)
- **Platform**: ESP32-S3
# RealMesh Firmware - ESP32 Implementation

ESP32-S3 firmware for RealMesh mesh networking nodes with LoRa radio and e-ink display.

## Hardware Support

- **Primary**: Heltec Wireless Paper (ESP32-S3 + SX1262 + 2.13" e-ink)
- **Radio**: SX1262 LoRa @ 868MHz (EU)
- **Display**: GxEPD2_213_FC1 (250x122px e-ink)
- **Platform**: ESP32-S3 with Arduino framework

## Features

### 📡 LoRa Mesh Networking
- **Frequency**: 868 MHz (EU ISM band)
- **Spreading Factor**: SF12 (maximum range)
- **Bandwidth**: 125 kHz
- **TX Power**: 20 dBm
- **Coding Rate**: 4/5
- **Range**: Several kilometers line-of-sight

### 🌐 Mesh Routing
- Flood routing for public messages
- Direct routing for peer-to-peer
- Automatic node discovery
- Heartbeat presence announcements
- Network topology tracking

### 📟 E-ink Display
- **Screen 1 (Home)**: Node identity, online status
- **Screen 2 (Messages)**: Last 3 received messages
- **Screen 3 (Node Info)**: Type, uptime, battery voltage
- Smart refresh (only on content changes)
- PRG button cycles screens
- Low power consumption

### 💬 Messaging
- Public channel "svet" (world/everyone) - broadcast to all nodes
- Direct messaging between nodes (node@domain format)
- Message queue and delivery tracking
- CLI and BLE API interfaces

### 🔷 Bluetooth LE API
- Mobile app integration
- Device name: `RealMesh-XXXX` (last 4 MAC chars)
- Service UUID: `12345678-1234-1234-1234-123456789abc`
- Real-time notifications for incoming messages
- JSON-based command/response protocol

### 🖥️ Serial CLI
- Interactive command-line interface
- Commands: `status`, `send`, `broadcast`, `scan`, `name`, `reboot`
- Real-time logging and debugging
- Node configuration

## Quick Start

### 1. Install PlatformIO

```bash
# Via pip
pip install platformio

# Or use VS Code extension
```

### 2. Build and Upload

```bash
# Navigate to firmware directory
cd RealMesh-Firmware

# Build for Heltec Wireless Paper
pio run -e heltec_wireless_paper --target upload

# Monitor serial output
pio device monitor

# Combined upload + monitor
pio run -e heltec_wireless_paper --target upload && pio device monitor
```

### 3. First Boot

Device will:
1. Initialize e-ink display
2. Generate unique node identity (or load stored)
3. Start LoRa radio
4. Begin network discovery
5. Display node address on screen
6. Start BLE advertising

## CLI Commands

Once connected via serial (`pio device monitor` @ 115200 baud):

```bash
# View node status
status

# Send direct message
send dale@dale Hello from node!

# Broadcast to public channel
broadcast Hello everyone!

# Change node name (requires reboot)
name mynodename mydomain

# Scan for nearby nodes
scan

# Reboot device
reboot

# Show help
help
```

## BLE API

### Connection
- Service UUID: `12345678-1234-1234-1234-123456789abc`
- Characteristic UUID: `87654321-4321-4321-4321-cba987654321`
- Device name format: `RealMesh-XXXX` (last 4 MAC chars)

### Commands (JSON)

**Get Status:**
```json
{"command": "status"}
```

**Send Message:**
```json
{
  "command": "send",
  "address": "node@domain",
  "message": "Hello!"
}
```

**Broadcast (use "svet" or "@"):**
```json
{
  "command": "send",
  "address": "svet",
  "message": "Hello everyone!"
}
```

### Notifications

Incoming messages trigger BLE notifications:
```json
{
  "type": "message",
  "from": "sender@domain",
  "message": "Hello!",
  "timestamp": 1234567890
}
```

## Project Structure

```
src/
├── main.cpp                    # Main application + CLI
├── RealMeshRadio.cpp          # SX1262 LoRa radio driver
├── RealMeshRouter.cpp         # Mesh routing engine
├── RealMeshNode.cpp           # Node identity & management
├── RealMeshPacket.cpp         # Packet serialization
├── RealMeshDisplay.cpp        # Display manager
├── RealMeshEinkDisplay.cpp    # E-ink display adapter
└── RealMeshMobileAPI.cpp      # BLE API implementation

include/
├── RealMeshConfig.h           # Radio/network configuration
├── RealMeshTypes.h            # Core data structures
├── RealMeshRadio.h            # Radio interface
├── RealMeshRouter.h           # Routing interface
├── RealMeshNode.h             # Node interface
├── RealMeshPacket.h           # Packet definitions
├── RealMeshDisplay.h          # Display interface
└── RealMeshMobileAPI.h        # BLE API interface
```

## Configuration

Edit `include/RealMeshConfig.h` to modify:
- **Radio**: Frequency, power, spreading factor, bandwidth
- **Network**: Heartbeat interval, discovery timeout, TTL
- **Display**: Refresh timing, screen layout
- **BLE**: Service/characteristic UUIDs
- **Debug**: Logging levels

## Node Identity

Nodes are identified by `nodeId@subdomain` format (like email):
- **nodeId**: Unique node name (e.g., "dale", "node1")
- **subdomain**: Domain/group name (e.g., "dale", "mesh", "local")
- **Full address**: `dale@dale`, `node1@mesh`

Identity is stored in NVS (non-volatile storage) and persists across reboots.

### Changing Node Name

```bash
# Via CLI
name mynodename mydomain
reboot

# Via BLE API
{"command": "name", "nodeId": "mynodename", "subdomain": "mydomain"}
```

## Development Status

- [x] LoRa radio driver (SX1262)
- [x] Packet serialization/deserialization
- [x] Flood routing for broadcasts
- [x] Direct routing for peer-to-peer
- [x] Node discovery and heartbeats
- [x] E-ink display with multi-screen support
- [x] BLE API with notifications
- [x] Serial CLI interface
- [x] Message queue and delivery
- [x] Node identity storage
- [x] Public channel "svet" support
- [x] Smart display refresh (content-aware)
- [ ] Mesh routing (multi-hop)
- [ ] ACK/NACK for reliable delivery
- [ ] Encryption
- [ ] Time synchronization

## Troubleshooting

### Display not updating
- Check if `needsUpdate` flag is being set on content changes
- PRG button should cycle screens (watch serial logs)
- Display refresh takes ~3.7s (e-ink is slow)

### Can't connect via BLE
- Check device name in serial output: `RealMesh-XXXX`
- Verify Bluetooth permissions on mobile device
- Try scanning with nRF Connect app first

### No LoRa communication
- Verify antenna is connected (critical!)
- Check frequency matches region (868 MHz EU)
- Monitor RSSI: should be > -120 dBm when receiving
- Use `scan` command to check for nearby nodes

### Node name not changing
- Must reboot after `name` command
- Check serial logs for "Pending name change detected"
- Identity stored in NVS partition
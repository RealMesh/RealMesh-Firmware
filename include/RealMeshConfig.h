#ifndef REALMESH_CONFIG_H
#define REALMESH_CONFIG_H

// ============================================================================
// RealMesh Configuration
// ============================================================================

// Radio Configuration (Maximum settings for PoC)
#define RM_FREQ_MHZ                 868.0
#define RM_BANDWIDTH_KHZ           125.0
#define RM_SPREADING_FACTOR        12
#define RM_CODING_RATE             5
#define RM_TX_POWER_DBM            20
#define RM_PREAMBLE_LENGTH         8
#define RM_SYNC_WORD               0x12

// Pin Configuration - Board Specific
#ifdef HELTEC_WIRELESS_PAPER
    // Heltec Wireless Paper Pin Configuration (ESP32-S3 + SX1262)
    // ✅ MESHTASTIC PROVEN CONFIGURATION - used by thousands of devices
    #define RM_LORA_SCK                9
    #define RM_LORA_MISO               11  
    #define RM_LORA_MOSI               10
    #define RM_LORA_CS                 8
    #define RM_LORA_RST                12
    #define RM_LORA_DIO1               14
    #define RM_LORA_BUSY               13
    
    // Display (E-Ink) - Using RealMeshEinkDisplay.h pin definitions
    // Pins are defined in RealMeshEinkDisplay.h - working configuration
    /*
    #define PIN_EINK_CS 4      // ✅ Arduino Forum confirmed
    #define PIN_EINK_BUSY 20   // ✅ DISCOVERED: Hardware variant (not GPIO 7)
    #define PIN_EINK_DC 5      // ✅ Arduino Forum confirmed  
    #define PIN_EINK_RES 19    // ✅ DISCOVERED: Hardware variant (not GPIO 6)
    #define PIN_EINK_SCLK 3    // ✅ Arduino Forum confirmed
    #define PIN_EINK_MOSI 2    // ✅ Arduino Forum confirmed
    */
    
    // Aliases for compatibility - commented out to avoid conflicts
    /*
    #define EPAPER_CS    PIN_EINK_CS
    #define EPAPER_BUSY  PIN_EINK_BUSY  
    #define EPAPER_DC    PIN_EINK_DC
    #define EPAPER_RST   PIN_EINK_RES
    #define EPAPER_SCLK  PIN_EINK_SCLK
    #define EPAPER_MOSI  PIN_EINK_MOSI
    */
    
    // Power control (critical ESP32-S3 pin!)
    #define VEXT_ENABLE                45  // Active low, powers the E-Ink display
    #define VEXT_ON_VALUE              LOW
    
    // Battery monitoring
    #define BATTERY_PIN                20  // ADC pin for battery voltage
    #define ADC_CTRL                   19
    #define ADC_MULTIPLIER             2   // Voltage divider roughly 1:1
    
    // LED and Button
    #define LED_PIN                    18
    #define BUTTON_PIN                 0
#else 
    // Heltec V3 Pin Configuration (Default)
    #define RM_LORA_SCK                9
    #define RM_LORA_MISO               11
    #define RM_LORA_MOSI               10
    #define RM_LORA_CS                 8
    #define RM_LORA_RST                12
    #define RM_LORA_DIO0               14
    #define RM_LORA_DIO1               13
    #define RM_LORA_BUSY               13
#endif

// Message Configuration
#define RM_MAX_PACKET_SIZE         255
#define RM_HEADER_SIZE             32
#define RM_MAX_PAYLOAD_SIZE        (RM_MAX_PACKET_SIZE - RM_HEADER_SIZE)
#define RM_MAX_HOP_COUNT           10
#define RM_PATH_HISTORY_SIZE       3

// Timing Configuration (milliseconds)
#define RM_ACK_TIMEOUT_DIRECT      10000    // 10 seconds
#define RM_ACK_TIMEOUT_FLOOD       30000    // 30 seconds
#define RM_RETRY_INTERVAL_BASE     5000     // 5 seconds
#define RM_RETRY_INTERVAL_MAX      45000    // 45 seconds
#define RM_HEARTBEAT_STATIONARY    15000    // 15 seconds (was 5 minutes - too slow!)
#define RM_HEARTBEAT_MOBILE        30000    // 30 seconds (was 15 minutes - too slow!)
#define RM_MESSAGE_MAX_AGE         600000   // 10 minutes
#define RM_NAME_CONFLICT_TIMEOUT   259200000 // 72 hours
#define RM_NETWORK_JOIN_TIMEOUT    30000    // 30 seconds

// Queue Configuration
#define RM_QUEUE_EMERGENCY_SIZE    20
#define RM_QUEUE_DIRECT_SIZE       10
#define RM_QUEUE_PUBLIC_SIZE       5
#define RM_QUEUE_CONTROL_SIZE      15

// Routing Table Configuration
#define RM_MAX_ROUTING_ENTRIES     1000
#define RM_MAX_SUBDOMAIN_NODES     200
#define RM_MAX_INTERMEDIARY_MEMORY 500

// Network Configuration
#define RM_NETWORK_JOIN_RETRIES    3
#define RM_MAX_RETRY_ATTEMPTS      3
#define RM_CONGESTION_THRESHOLD    80       // Percentage
#define RM_UUID_LENGTH             8        // bytes
#define RM_NAME_TIMEOUT_MS         30000    // Name conflict timeout (30 seconds)

// Debug Configuration
#define RM_DEBUG_ENABLED           1
#define RM_DEBUG_ROUTING           1
#define RM_DEBUG_RADIO             1
#define RM_DEBUG_MESSAGES          1

// Version Information
#define RM_PROTOCOL_VERSION        1
#define RM_FIRMWARE_VERSION        "0.1.0"

#endif // REALMESH_CONFIG_H
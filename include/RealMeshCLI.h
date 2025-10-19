#ifndef REALMESH_CLI_H
#define REALMESH_CLI_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>
#include <algorithm>
#include "RealMeshTypes.h"

// ============================================================================
// Command Line Interface for RealMesh
// ============================================================================

class RealMeshCLI {
public:
    // Constructor
    explicit RealMeshCLI(RealMeshAPI* api);
    
    // Initialize CLI
    bool begin();
    
    // Main CLI processing loop - call regularly
    void loop();
    
    // Process a complete command string
    bool processCommand(const String& command);
    
    // Enable/disable CLI features
    void setEcho(bool enabled) { echoEnabled = enabled; }
    void setPrompt(const String& newPrompt) { prompt = newPrompt; }
    void setVerbose(bool enabled) { verboseOutput = enabled; }
    
private:
    // Command structure
    struct Command {
        String name;
        String description;
        String usage;
        std::function<void(const std::vector<String>&)> handler;
        uint8_t minArgs;
        uint8_t maxArgs;
    };
    
    RealMeshAPI* api;
    String inputBuffer;
    String prompt;
    bool echoEnabled;
    bool verboseOutput;
    std::vector<Command> commands;
    
    // Command parsing
    std::vector<String> parseCommandLine(const String& input);
    String unquote(const String& str);
    
    // Output helpers
    void println(const String& message);
    void print(const String& message);
    void printError(const String& error);
    void printSuccess(const String& message);
    void printTable(const std::vector<std::vector<String>>& data);
    void showPrompt();
    
    // Command handlers
    void registerCommands();
    
    // System commands
    void cmd_help(const std::vector<String>& args);
    void cmd_status(const std::vector<String>& args);
    void cmd_reboot(const std::vector<String>& args);
    void cmd_factory_reset(const std::vector<String>& args);
    void cmd_debug(const std::vector<String>& args);
    
    // Node configuration
    void cmd_set_name(const std::vector<String>& args);
    void cmd_set_subdomain(const std::vector<String>& args);
    void cmd_set_type(const std::vector<String>& args);
    void cmd_get_config(const std::vector<String>& args);
    void cmd_save_config(const std::vector<String>& args);
    
    // Messaging
    void cmd_send(const std::vector<String>& args);
    void cmd_send_public(const std::vector<String>& args);
    void cmd_send_emergency(const std::vector<String>& args);
    void cmd_messages(const std::vector<String>& args);
    void cmd_clear_messages(const std::vector<String>& args);
    
    // Network discovery and routing
    void cmd_scan(const std::vector<String>& args);
    void cmd_nodes(const std::vector<String>& args);
    void cmd_routes(const std::vector<String>& args);
    void cmd_ping(const std::vector<String>& args);
    void cmd_traceroute(const std::vector<String>& args);
    void cmd_who_hears_me(const std::vector<String>& args);
    
    // Statistics and monitoring
    void cmd_stats(const std::vector<String>& args);
    void cmd_network_stats(const std::vector<String>& args);
    void cmd_signal_stats(const std::vector<String>& args);
    void cmd_log(const std::vector<String>& args);
    
    // Radio configuration
    void cmd_radio_config(const std::vector<String>& args);
    void cmd_set_power(const std::vector<String>& args);
    void cmd_set_frequency(const std::vector<String>& args);
    void cmd_test_radio(const std::vector<String>& args);
    
    // Advanced features
    void cmd_diagnostics(const std::vector<String>& args);
    void cmd_export_config(const std::vector<String>& args);
    void cmd_import_config(const std::vector<String>& args);
    void cmd_firmware_info(const std::vector<String>& args);
    
    // Interactive modes
    void cmd_chat_mode(const std::vector<String>& args);
    void cmd_monitor_mode(const std::vector<String>& args);
    
    // Utility functions
    String formatUptime(uint32_t seconds);
    String formatSize(size_t bytes);
    String formatSignalStrength(int16_t rssi);
    String boolToString(bool value);
    bool stringToBool(const String& str);
};

#endif // REALMESH_CLI_H
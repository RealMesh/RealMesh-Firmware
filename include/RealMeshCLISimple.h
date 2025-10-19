#ifndef REALMESH_CLI_SIMPLE_H
#define REALMESH_CLI_SIMPLE_H

#include <Arduino.h>
#include "RealMeshAPISimple.h"

class RealMeshCLI {
public:
    explicit RealMeshCLI(RealMeshAPI* api);
    bool begin();
    void loop();
    
private:
    RealMeshAPI* api;
    String inputBuffer;
    
    void processCommand(const String& command);
    void showHelp();
    void showPrompt();
    
    // Basic commands
    void cmd_help();
    void cmd_status();
    void cmd_send(const String& args);
    void cmd_public(const String& args);
    void cmd_scan();
    void cmd_set_name(const String& args);
    void cmd_reboot();
};

#endif // REALMESH_CLI_SIMPLE_H
#ifndef SERIAL_COMMAND_HANDLER_H
#define SERIAL_COMMAND_HANDLER_H

#include <Arduino.h>

class SerialCommandHandler {
public:
    void handleCommand(const String& command);

private:
    // Helper functions to parse arguments from the command string
    String getWord(const String& text, int index);
    String getRestOfCommand(const String& text, int startIndex);
    
    // Command-specific handler methods
    void handleListEffects();
    void handleGetStatus();
    void handleGetConfig();
    void handleSaveConfig();
    void handleSetLedCount(const String& args);
    void handleGetLedCount();
    void handleListSegments();
    void handleClearSegments();
    void handleAddSegment(const String& args);
    void handleSetEffect(const String& args);
    void handleGetEffectInfo(const String& args);
    void handleSetParameter(const String& args);
    void handleBatchConfig(const String& json);

    // <<-- ADD THIS LINE -->>
    void handleGetAllSegmentConfigsSerial(); 
};

#endif // SERIAL_COMMAND_HANDLER_H
// In src/SerialCommandHandler.h

#ifndef SERIAL_COMMAND_HANDLER_H
#define SERIAL_COMMAND_HANDLER_H

#include <Arduino.h>

/**
 * @class SerialCommandHandler
 * @brief Handles text-based commands from the serial port for testing and debugging.
 */
class SerialCommandHandler
{
public:
    void handleCommand(char* command);

private:
    // Command-specific handler methods
    void handleListEffects();
    void handleGetStatus();
    void handleGetSavedConfig();
    void handleGetCurrConfig();
    void handleSaveConfig();
    void handleSetLedCount(const char* args);
    void handleGetLedCount();
    void handleListSegments();
    void handleClearSegments();
    void handleAddSegment(char* args);
    void handleSetEffect(char* args);
    void handleGetEffectInfo(char* args);
    void handleSetParameter(char* args);
    void handleBatchConfig(const char* json);
    void handleBleReset();
    void handleBleStatus();
    void handleHelp();
    void handleGetParameters(const char* args);
    
    // Serial-only test functions that call the binary handler
    void handleGetAllSegmentConfigsSerial();
    void handleGetAllEffectsSerial();
    // void handleSetAllSegmentConfigsSerial(); // <-- FIX: This function is obsolete and has been removed.
    void handleSetSingleSegmentJson(const char* json);
};

#endif // SERIAL_COMMAND_HANDLER_H
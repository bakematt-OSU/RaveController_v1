/**
 * @file SerialCommandHandler.h
 * @brief Defines a handler for processing text-based commands from the serial port.
 * @version 0.4
 * @date 2025-07-16
 * @copyright Copyright (c) 2025
 */

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
    /**
     * @brief Main entry point for processing a command string from the Serial Monitor.
     * @param command A mutable C-style string containing the raw command.
     */
    void handleCommand(char* command);

private:
    // Command-specific handler methods
    void handleListEffects();
    void handleGetStatus();
    void handleGetConfig();
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

    void handleGetAllSegmentConfigsSerial();
    void handleGetAllEffectsSerial();
    void handleSetAllSegmentConfigsSerial();

    void handleGetParameters(const char* args);
    void handleSetSingleSegmentJson(const char* json);
};

#endif // SERIAL_COMMAND_HANDLER_H
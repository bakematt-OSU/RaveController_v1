/**
 * @file SerialCommandHandler.h
 * @author Matthew Baker
 * @brief Defines a handler for processing text-based commands from the serial port.
 * @version 0.4
 * @date 2025-07-16
 * @copyright Copyright (c) 2025
 *
 * @details This file contains the definition for the SerialCommandHandler class, which
 * is responsible for parsing and executing human-readable commands entered via
 * the Serial Monitor. This is primarily used for debugging and testing.
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
     * @param command The raw command string read from the serial input.
     */
    void handleCommand(const String &command);

private:
    // --- Helper functions to parse command arguments ---

    /**
     * @brief Extracts a specific word from a string, delimited by spaces.
     * @param text The input string.
     * @param index The zero-based index of the word to extract.
     * @return The extracted word as a String.
     */
    String getWord(const String &text, int index);

    /**
     * @brief Gets the remainder of a command string after a certain word index.
     * @param text The input string.
     * @param startIndex The starting word index from which to capture the rest of the string.
     * @return The rest of the command string.
     */
    String getRestOfCommand(const String &text, int startIndex);

    // --- Command-specific handler methods ---

    /** @brief Handles the 'list_effects' command. */
    void handleListEffects();
    /** @brief Handles the 'get_status' command. */
    void handleGetStatus();
    /** @brief Handles the 'get_config' command. */
    void handleGetConfig();
    /** @brief Handles the 'save_config' command. */
    void handleSaveConfig();
    /** @brief Handles the 'set_led_count' command with its arguments. */
    void handleSetLedCount(const String &args);
    /** @brief Handles the 'get_led_count' command. */
    void handleGetLedCount();
    /** @brief Handles the 'list_segments' command. */
    void handleListSegments();
    /** @brief Handles the 'clear_segments' command. */
    void handleClearSegments();
    /** @brief Handles the 'add_segment' command with its arguments. */
    void handleAddSegment(const String &args);
    /** @brief Handles the 'set_effect' command with its arguments. */
    void handleSetEffect(const String &args);
    /** @brief Handles the 'get_effect_info' command with its arguments. */
    void handleGetEffectInfo(const String &args);
    /** @brief Handles the 'set_parameter' command with its arguments. */
    void handleSetParameter(const String &args);
    /** @brief Handles a batch configuration update from a JSON string. */
    void handleBatchConfig(const String &json);

    /** @brief Handles the serial command to get all segment configurations. */
    void handleGetAllSegmentConfigsSerial();

    /** @brief Handles the serial command to get all effect configurations. */
    void handleGetAllEffectsSerial();

    /** @brief Handles the serial command to set all segment configurations. */
    void handleSetAllSegmentConfigsSerial();

    /** @brief Handles the serial command to get the parameters of an active effect on a segment. */
    void handleGetParameters(const String &args);

    void handleSetSingleSegmentJson(const String& json);
};

#endif // SERIAL_COMMAND_HANDLER_H

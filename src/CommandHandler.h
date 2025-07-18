/**
 * @file CommandHandler.h
 * @author Matthew Baker
 * @brief Defines a handler for processing text-based commands from BLE.
 * @version 0.3
 * @date 2025-07-16
 * @copyright Copyright (c) 2025
 *
 * @details This file contains the CommandHandler class, which is responsible for
 * parsing and executing string-based commands received over BLE. It interacts
 * with a BLEManager to send responses back to the client.
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>

// Forward declare BLEManager to break the circular include dependency.
class BLEManager;

/**
 * @class CommandHandler
 * @brief Processes commands received from BLE, sending responses back over
 * BLE and printing debug information to the Serial Monitor.
 */
class CommandHandler
{
public:
    /**
     * @brief Constructor for CommandHandler.
     * @param bleManager A pointer to the global BLEManager instance for sending responses.
     */
    CommandHandler(BLEManager *bleManager);

    /**
     * @brief The main entry point for processing a command string from BLE.
     *
     * @details It parses the incoming command and routes it to the appropriate
     * handler function, which will then send a response over BLE and
     * print debug info to the Serial monitor.
     *
     * @param command The raw command string received from BLE.
     */
    void handleCommand(const String &command);

private:
    /// A pointer to the BLEManager instance, used for sending responses back to the client.
    BLEManager *bleManager;

    // --- Helper functions to parse command arguments ---

    /**
     * @brief Extracts a specific word from a string.
     * @param text The input string.
     * @param index The zero-based index of the word to extract.
     * @return The extracted word as a String.
     */
    String getWord(const String &text, int index);

    /**
     * @brief Gets the remainder of a command string after a certain word index.
     * @param text The input string.
     * @param startIndex The starting word index.
     * @return The rest of the string.
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
};

#endif // COMMAND_HANDLER_H
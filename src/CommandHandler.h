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
class CommandHandler {
public:
    /**
     * @brief Constructor for CommandHandler.
     * @param bleManager A pointer to the global BLEManager instance for sending responses.
     */
    CommandHandler(BLEManager* bleManager);

    /**
     * @brief The main entry point for processing a command string from BLE.
     *
     * It parses the incoming command and routes it to the appropriate
     * handler function, which will then send a response over BLE and
     * print debug info to the Serial monitor.
     *
     * @param command The raw command string received from BLE.
     */
    void handleCommand(const String& command);

private:
    /// A pointer to the BLEManager instance, used for sending responses back to the client.
    BLEManager* bleManager;

    // --- Helper functions to parse command arguments ---
    String getWord(const String& text, int index);
    String getRestOfCommand(const String& text, int startIndex);

    // --- Command-specific handler methods ---
    void handleListEffects();
    void handleGetStatus();
    void handleGetConfig();
    void handleSaveConfig();
    void handleGetLedCount();
    void handleListSegments();
    void handleClearSegments();
    void handleAddSegment(const String& args);
    void handleSetEffect(const String& args);
    void handleGetEffectInfo(const String& args);
    void handleSetParameter(const String& args);
};

#endif // COMMAND_HANDLER_H
#ifndef BINARY_COMMAND_HANDLER_H
#define BINARY_COMMAND_HANDLER_H

#include <Arduino.h>

// Enum for binary command codes.
// These values MUST match the constants in the Android app's LedControllerCommands.kt
enum BleCommand : uint8_t {
    CMD_SET_COLOR       = 0x01,
    CMD_SET_EFFECT      = 0x02,
    CMD_SET_BRIGHTNESS  = 0x03,
    CMD_SET_SEG_BRIGHT  = 0x04,
    CMD_SELECT_SEGMENT  = 0x05,
    CMD_CLEAR_SEGMENTS  = 0x06,
    CMD_SET_SEG_RANGE   = 0x07,
    CMD_GET_STATUS      = 0x08,
    CMD_BATCH_CONFIG    = 0x09,
    // 0x0A is unused
    CMD_GET_EFFECT_INFO = 0x0B,
    CMD_SET_LED_COUNT   = 0x0C,
    CMD_GET_LED_COUNT   = 0x0D,
    
    // Response codes
    CMD_ACK             = 0xA0,
};

class BinaryCommandHandler {
public:
    /**
     * @brief The main entry point for processing a binary command.
     * @param data A pointer to the incoming byte array.
     * @param len The length of the data array.
     */
    void handleCommand(const uint8_t* data, size_t len);

private:
    // --- Command-specific handler methods ---
    void handleSetColor(const uint8_t* payload, size_t len);
    void handleSetEffect(const uint8_t* payload, size_t len);
    void handleSetBrightness(const uint8_t* payload, size_t len);
    void handleSetSegmentBrightness(const uint8_t* payload, size_t len);
    void handleSelectSegment(const uint8_t* payload, size_t len);
    void handleClearSegments();
    void handleSetSegmentRange(const uint8_t* payload, size_t len);
    void handleSetLedCount(const uint8_t* payload, size_t len);
    
    // --- Handlers for commands that send back data ---
    void handleGetStatus();
    void handleGetLedCount();
    void handleBatchConfig(const uint8_t* payload, size_t len);
    void handleGetEffectInfo(const uint8_t* payload, size_t len);

    // --- Handler for acknowledgements from the app ---
    void handleAck();
};

#endif // BINARY_COMMAND_HANDLER_H

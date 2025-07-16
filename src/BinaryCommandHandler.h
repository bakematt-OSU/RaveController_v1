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
    CMD_SET_EFFECT_PARAMETER = 0x0A, // New command for setting effect parameters
    CMD_GET_EFFECT_INFO = 0x0B,
    CMD_SET_LED_COUNT   = 0x0C,
    CMD_GET_LED_COUNT   = 0x0D,
    CMD_GET_ALL_SEGMENT_CONFIGS = 0x0E,
    CMD_SET_ALL_SEGMENT_CONFIGS = 0x0F, // New command code

    // Response codes
    CMD_ACK             = 0xA0,
};

// State machine for incoming multi-part commands (like batch config or set all segments)
enum class IncomingBatchState {
    IDLE,
    EXPECTING_BATCH_CONFIG_JSON, // For CMD_BATCH_CONFIG
    EXPECTING_ALL_SEGMENTS_COUNT, // For CMD_SET_ALL_SEGMENT_CONFIGS
    EXPECTING_ALL_SEGMENTS_JSON // For CMD_SET_ALL_SEGMENT_CONFIGS
};

class BinaryCommandHandler {
public:
    BinaryCommandHandler(); // Explicit declaration of the constructor

    /**
     * @brief The main entry point for processing a binary command.
     * @param data A pointer to the incoming byte array.
     * @param len The length of the data array.
     */
    void handleCommand(const uint8_t* data, size_t len);

    /**
     * @brief Handles the request to get all segment configurations.
     * @param viaSerial If true, output is sent to Serial. If false, output is sent via BLE.
     */
    void handleGetAllSegmentConfigs(bool viaSerial);

    void handleSetAllSegmentConfigsCommand(); // Now public so it can be called externally

    /**
     * @brief Get the current incoming batch state.
     * @return The current IncomingBatchState.
     */
    IncomingBatchState getIncomingBatchState() const { return _incomingBatchState; } // ADD THIS LINE

private:
    // --- State for handling chunked data ---
    String _incomingJsonBuffer; // Re-purposed for generic incoming JSON
    IncomingBatchState _incomingBatchState; // State for multi-part incoming commands

    // For ACK mechanism
    volatile bool _ackReceived;
    unsigned long _ackTimeoutStart;
    const unsigned long ACK_WAIT_TIMEOUT_MS = 1000;

    // For CMD_SET_ALL_SEGMENT_CONFIGS
    uint16_t _expectedSegmentsToReceive; // Total count of segments expected in the batch
    uint16_t _segmentsReceivedInBatch;   // Counter for segments received so far

    // --- Command-specific handler methods (these remain private) ---
    void handleSetColor(const uint8_t* payload, size_t len);
    void handleSetEffect(const uint8_t* payload, size_t len);
    void handleSetBrightness(const uint8_t* payload, size_t len);
    void handleSetSegmentBrightness(const uint8_t* payload, size_t len);
    void handleSelectSegment(const uint8_t* payload, size_t len);
    void handleClearSegments();
    void handleSetSegmentRange(const uint8_t* payload, size_t len);
    void handleSetLedCount(const uint8_t* payload, size_t len);
    void handleSetEffectParameter(const uint8_t* payload, size_t len); 

    // --- Handlers for commands that send back data (these remain private) ---
    void handleGetStatus();
    void handleGetLedCount();
    void handleBatchConfig(const uint8_t* payload, size_t len); // Original batch config
    void handleGetEffectInfo(const uint8_t* payload, size_t len);
    
    // Helper for multi-part incoming data processing (private as it's called internally)
    void processIncomingAllSegmentsData(const uint8_t* data, size_t len); 

    // --- Handler for acknowledgements from the app (this remains private) ---
    void handleAck();
};

#endif // BINARY_COMMAND_HANDLER_H
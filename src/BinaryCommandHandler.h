#ifndef BINARY_COMMAND_HANDLER_H
#define BINARY_COMMAND_HANDLER_H

#include <Arduino.h>

/**
 * @brief Enumerates the various binary commands that can be sent or received via BLE.
 *
 * These commands define the protocol for communication between the Rave Controller
 * firmware and a connected application (e.g., mobile app).
 */
enum BleCommand : uint8_t
{
    //GETTERS
    CMD_GET_LED_COUNT = 0x0D,           ///< Requests the current total number of LEDs.
    CMD_GET_ALL_SEGMENT_CONFIGS = 0x0E, ///< Requests the full configuration of all segments as JSON.
    CMD_SET_ALL_SEGMENT_CONFIGS = 0x0F, ///< Initiates receiving multiple segment configurations in a batch.
    CMD_GET_ALL_EFFECTS = 0x10,         ///< Requests detailed information for all available effects.
    CMD_SAVE_CONFIG = 0x12,             ///< Saves the current configuration to persistent storage.
    CMD_CLEAR_SEGMENTS = 0x06,          ///< Clears all segment configurations.

    CMD_ACK_GENERIC = 0xA0,             ///< Generic acknowledgment for a received command.

    CMD_READY = 0xD0,                   ///< Indicates the device is ready.
};

/**
 * @brief Enumerates the states for handling incoming multi-part binary commands.
 *
 * This helps manage the state machine for commands that require multiple BLE packets,
 * such as batch configuration updates or streaming effect information.
 */
enum class IncomingBatchState
{
    IDLE,                               ///< No multi-part command in progress.
    // Removed EXPECTING_BATCH_CONFIG_JSON as it's no longer used.
    EXPECTING_ALL_SEGMENTS_COUNT,       ///< Expecting the total count of segments for a batch update.
    EXPECTING_ALL_SEGMENTS_JSON,        ///< Expecting individual segment JSON payloads.
    EXPECTING_EFFECT_ACK,               ///< Waiting for an ACK before sending the next effect's info.
    EXPECTING_SEGMENT_ACK               ///< Waiting for an ACK before sending the next segment's info.
};

/**
 * @class BinaryCommandHandler
 * @brief Manages the parsing and execution of binary commands received over BLE.
 *
 * This class acts as the central router for binary protocol messages. It handles
 * single-packet commands directly and manages the state for multi-part commands,
 * ensuring proper assembly and processing of complex data structures like JSON.
 */
class BinaryCommandHandler
{
public:
    /**
     * @brief Constructs a new Binary Command Handler object.
     */
    BinaryCommandHandler();

    /**
     * @brief Processes an incoming raw binary command.
     * @param data A pointer to the raw byte array of the command.
     * @param len The length of the command data.
     */
    void handleCommand(const uint8_t *data, size_t len);

    /**
     * @brief Initiates the process of sending all segment configurations.
     * @param viaSerial If true, sends output to Serial; otherwise, sends via BLE.
     */
    void handleGetAllSegmentConfigs(bool viaSerial);
    

    /**
     * @brief Initiates the process of receiving all segment configurations in a batch.
     * @param viaSerial If true, expects input from Serial; otherwise, expects from BLE.
     */
    void handleSetAllSegmentConfigsCommand(bool viaSerial);

    /**
     * @brief Initiates the process of sending information for all available effects.
     * @param viaSerial If true, sends output to Serial; otherwise, sends via BLE.
     */
    void handleGetAllEffectsCommand(bool viaSerial);

    /**
     * @brief Gets the current state of the incoming batch processing.
     * @return The current IncomingBatchState.
     */
    IncomingBatchState getIncomingBatchState() const;

    /**
     * @brief Checks if a serial batch operation is currently active.
     * @return True if a serial batch is active, false otherwise.
     */
    bool isSerialBatchActive() const;

    /**
     * @brief Processes a single segment configuration provided as a JSON string.
     * @param jsonString The JSON string containing the segment configuration.
     */
    void processSingleSegmentJson(const char* jsonString);

    /**
     * @brief Updates the command handler's state, checking for timeouts.
     * This method should be called periodically in the main loop().
     */
    void update();

private:
    char _incomingJsonBuffer[1024];     ///< Buffer to accumulate incoming JSON data for multi-part commands.
    size_t _jsonBufferIndex;            ///< Current index/length of data in `_incomingJsonBuffer`.
    IncomingBatchState _incomingBatchState; ///< Current state of the incoming multi-part command handler.
    bool _isSerialEffectsTest;          ///< Flag to indicate if effects info is being sent via Serial for testing.
    bool _isSerialBatch;                ///< Flag to indicate if the current batch operation is via Serial.
    volatile bool _ackReceived;         ///< Flag set when an ACK is received (used for handshake).
    unsigned long _ackTimeoutStart;     ///< @brief Timestamp when ACK waiting started.
    const unsigned long ACK_WAIT_TIMEOUT_MS = 5000; ///< @brief Timeout duration in milliseconds for waiting for an ACK.
    uint16_t _expectedSegmentsToReceive; ///< Expected number of segments in an incoming batch.
    uint16_t _segmentsReceivedInBatch;   ///< Number of segments received so far in a batch.
    uint16_t _expectedEffectsToSend;     ///< Total number of effects to send.
    uint16_t _effectsSentInBatch;        ///< Number of effects sent so far.

    // NEW variables for outgoing segment batch
    uint16_t _expectedSegmentsToSend_Out; ///< Total number of segments to send in an outgoing batch.
    uint16_t _segmentsSentInBatch_Out;   ///< Number of segments sent so far in an outgoing batch.

    // --- Helper Functions ---
    // Removed sendAck and sendNack as they are no longer used in the new protocol.
    
    // --- Command Handlers (Private implementations for specific commands) ---
    // Removed all individual command handlers that are not part of the simplified binary protocol.
    // The only remaining ones are those directly related to the new binary commands.
    
    /**
     * @brief Handles the Save Config command.
     */
    void handleSaveConfig();
    
    /**
     * @brief Handles the Get LED Count command.
     */
    void handleGetLedCount();
    
    /**
     * @brief Builds a JSON string containing information about a specific effect.
     * @param effectIndex The index of the effect to build info for.
     * @return A String containing the JSON representation of the effect's info.
     */
    String buildEffectInfoJson(uint8_t effectIndex);

    /**
     * @brief Builds a JSON string containing information about a specific segment.
     * @param segmentIndex The index of the segment to build info for.
     * @return A String containing the JSON representation of the segment's info.
     */
    String buildSegmentInfoJson(uint8_t segmentIndex);

    /**
     * @brief Processes incoming data for multi-part commands (batch config, all segments).
     * @param data Pointer to the incoming data.
     * @param len Length of the data.
     */
    void processIncomingAllSegmentsData(const uint8_t *data, size_t len);

    /**
     * @brief Handles the reception of an ACK.
     */
    void handleAck();

    void handleClearSegments();
};

#endif // BINARY_COMMAND_HANDLER_H

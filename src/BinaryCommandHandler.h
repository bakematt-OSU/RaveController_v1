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
    // OBSOLETE commands are commented out to indicate they are no longer in use.
    //OBSOLETE:CMD_SET_COLOR = 0x01,
    //OBSOLETE:CMD_SET_EFFECT = 0x02,
    //OBSOLETE: CMD_SET_BRIGHTNESS = 0x03,
    //OBSOLETE:CMD_SET_SEG_BRIGHT = 0x04,
    CMD_SELECT_SEGMENT = 0x05,          ///< Selects a segment (no-op on current firmware, for app internal use).
    CMD_CLEAR_SEGMENTS = 0x06,          ///< Clears all user-defined segments.
    //OBSOLETE: CMD_SET_SEG_RANGE = 0x07,
    //OBSOLETE: CMD_GET_STATUS = 0x08,
    CMD_BATCH_CONFIG = 0x09,            ///< Initiates a batch configuration update using a JSON payload.
    CMD_SET_EFFECT_PARAMETER = 0x0A,    ///< Sets a parameter for an active effect on a segment.
    CMD_GET_EFFECT_INFO = 0x0B,         ///< Requests information about a specific effect, including its parameters.
    CMD_SET_LED_COUNT = 0x0C,           ///< Sets the total number of LEDs and triggers a restart.
    CMD_GET_LED_COUNT = 0x0D,           ///< Requests the current total number of LEDs.
    //GETTERS
    CMD_GET_ALL_SEGMENT_CONFIGS = 0x0E, ///< Requests the full configuration of all segments as JSON.
    CMD_SET_ALL_SEGMENT_CONFIGS = 0x0F, ///< Initiates receiving multiple segment configurations in a batch.
    CMD_GET_ALL_EFFECTS = 0x10,         ///< Requests detailed information for all available effects.
    CMD_SET_SINGLE_SEGMENT_JSON = 0x11, ///< Configures a single segment using a JSON string.
    CMD_SAVE_CONFIG = 0x12,             ///< Saves the current configuration to persistent storage.

    CMD_ACK_GENERIC = 0xA0,             ///< Generic acknowledgment for a received command.
    CMD_ACK_EFFECT_SET = 0xA1,          ///< Acknowledgment for an effect setting command.
    CMD_ACK_PARAM_SET = 0xA2,           ///< Acknowledgment for a parameter setting command.
    CMD_ACK_CONFIG_SAVED = 0xA3,        ///< Acknowledgment that configuration has been saved.
    CMD_ACK_RESTARTING = 0xA4,          ///< Acknowledgment that the device is restarting.

    CMD_READY = 0xD0,                   ///< Indicates the device is ready.

    CMD_NACK_UNKNOWN_CMD = 0xE0,        ///< Negative acknowledgment: Unknown command.
    CMD_NACK_INVALID_PAYLOAD = 0xE1,    ///< Negative acknowledgment: Invalid command payload.
    CMD_NACK_INVALID_SEGMENT = 0xE2,    ///< Negative acknowledgment: Invalid segment ID.
    CMD_NACK_NO_EFFECT = 0xE3,          ///< Negative acknowledgment: No active effect on segment.
    CMD_NACK_UNKNOWN_EFFECT = 0xE4,     ///< Negative acknowledgment: Unknown effect name/ID.
    CMD_NACK_UNKNOWN_PARAMETER = 0xE5,  ///< Negative acknowledgment: Unknown parameter name.
    CMD_NACK_JSON_ERROR = 0xE6,         ///< Negative acknowledgment: JSON parsing error.
    CMD_NACK_FS_ERROR = 0xE7,           ///< Negative acknowledgment: Filesystem error.
    CMD_NACK_BUFFER_OVERFLOW = 0xE8,    ///< Negative acknowledgment: Buffer overflow during data reception.
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
    EXPECTING_BATCH_CONFIG_JSON,        ///< Expecting the full JSON payload for a batch config.
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
    /**
     * @brief Sends an acknowledgment (ACK) command.
     * @param ackCode The specific ACK code to send.
     */
    void sendAck(BleCommand ackCode);
    /**
     * @brief Sends a negative acknowledgment (NACK) command.
     * @param nackCode The specific NACK code to send.
     */
    void sendNack(BleCommand nackCode);
    
    // --- Command Handlers (Private implementations for specific commands) ---
    /**
     * @brief Handles the Set Color command (OBSOLETE).
     * @param payload Pointer to the command payload.
     * @param len Length of the payload.
     */
    void handleSetColor(const uint8_t *payload, size_t len);
    /**
     * @brief Handles the Set Effect command (OBSOLETE).
     * @param payload Pointer to the command payload.
     * @param len Length of the payload.
     */
    void handleSetEffect(const uint8_t *payload, size_t len);
    /**
     * @brief Handles the Set Brightness (global) command (OBSOLETE).
     * @param payload Pointer to the command payload.
     * @param len Length of the payload.
     */
    void handleSetBrightness(const uint8_t *payload, size_t len);
    /**
     * @brief Handles the Set Segment Brightness command (OBSOLETE).
     * @param payload Pointer to the command payload.
     * @param len Length of the payload.
     */
    void handleSetSegmentBrightness(const uint8_t *payload, size_t len);
    /**
     * @brief Handles the Select Segment command.
     * @param payload Pointer to the command payload.
     * @param len Length of the payload.
     */
    void handleSelectSegment(const uint8_t *payload, size_t len);
    /**
     * @brief Handles the Clear Segments command.
     */
    void handleClearSegments();
    /**
     * @brief Handles the Set Segment Range command (OBSOLETE).
     * @param payload Pointer to the command payload.
     * @param len Length of the payload.
     */
    void handleSetSegmentRange(const uint8_t *payload, size_t len);
    /**
     * @brief Handles the Set LED Count command.
     * @param payload Pointer to the command payload (containing the new LED count).
     * @param len Length of the payload.
     */
    void handleSetLedCount(const uint8_t *payload, size_t len);
    /**
     * @brief Handles the Set Effect Parameter command.
     * @param payload Pointer to the command payload (segment ID, param type, name, value).
     * @param len Length of the payload.
     */
    void handleSetEffectParameter(const uint8_t *payload, size_t len);
    /**
     * @brief Handles the Save Config command.
     */
    void handleSaveConfig();
    /**
     * @brief Handles the Get Status command.
     */
    void handleGetStatus();
    /**
     * @brief Handles the Get LED Count command.
     */
    void handleGetLedCount();
    
    /**
     * @brief Processes a batch configuration provided as a JSON string.
     * @param json The JSON string to parse and apply.
     */
    void handleBatchConfigJson(const char* json);

    /**
     * @brief Handles the Get Effect Info command.
     * @param payload Pointer to the command payload (effect ID).
     * @param len Length of the payload.
     * @param viaSerial If true, sends output to Serial; otherwise, sends via BLE.
     */
    void handleGetEffectInfo(const uint8_t *payload, size_t len, bool viaSerial = false);

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
};

#endif // BINARY_COMMAND_HANDLER_H
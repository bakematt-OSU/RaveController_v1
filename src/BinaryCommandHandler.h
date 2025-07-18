/**
 * @file BinaryCommandHandler.h
 * @author Matthew Baker
 * @brief Defines the handler for processing binary commands for the LED controller.
 * @version 0.3
 * @date 2025-07-16
 * @copyright Copyright (c) 2025
 *
 * @details This file contains the definitions for handling a specific binary protocol,
 * including command codes and a state machine for processing multi-part messages.
 * It is designed to work with a corresponding client application (e.g., an Android app).
 */

#ifndef BINARY_COMMAND_HANDLER_H
#define BINARY_COMMAND_HANDLER_H

#include <Arduino.h>

/**
 * @enum BleCommand
 * @brief Defines the binary command codes for controlling the LED strip.
 * @note These values MUST match the constants in the client application.
 */
enum BleCommand : uint8_t
{
    CMD_SET_COLOR = 0x01,               ///< Set the color of the current segment.
    CMD_SET_EFFECT = 0x02,              ///< Set the active effect for the current segment.
    CMD_SET_BRIGHTNESS = 0x03,          ///< Set the global brightness.
    CMD_SET_SEG_BRIGHT = 0x04,          ///< Set the brightness for the current segment.
    CMD_SELECT_SEGMENT = 0x05,          ///< Select the active segment for subsequent commands.
    CMD_CLEAR_SEGMENTS = 0x06,          ///< Clear all segment configurations.
    CMD_SET_SEG_RANGE = 0x07,           ///< Define the start and end LEDs for a segment.
    CMD_GET_STATUS = 0x08,              ///< Request the current status of the controller.
    CMD_BATCH_CONFIG = 0x09,            ///< Initiate a batch configuration update (e.g., from JSON).
    CMD_SET_EFFECT_PARAMETER = 0x0A,    ///< Set a specific parameter for the current effect.
    CMD_GET_EFFECT_INFO = 0x0B,         ///< Request detailed information about a specific effect.
    CMD_SET_LED_COUNT = 0x0C,           ///< Set the total number of LEDs in the strip.
    CMD_GET_LED_COUNT = 0x0D,           ///< Request the total number of LEDs.
    CMD_GET_ALL_SEGMENT_CONFIGS = 0x0E, ///< Request the configuration of all segments.
    CMD_SET_ALL_SEGMENT_CONFIGS = 0x0F, ///< Set the configuration for all segments from a data stream.
    CMD_GET_ALL_EFFECTS = 0x10,         ///< Request a list of all available effects.
    CMD_SET_SINGLE_SEGMENT_JSON = 0x11, ///< Set the configuration for a single segment from a JSON string.

    // Response codes
    CMD_ACK = 0xA0, ///< Acknowledgment response code.
};

/**
 * @enum IncomingBatchState
 * @brief State machine for handling multi-part or large incoming commands.
 */
enum class IncomingBatchState
{
    IDLE,                         ///< Waiting for a new command.
    EXPECTING_BATCH_CONFIG_JSON,  ///< Waiting for a JSON string for batch configuration.
    EXPECTING_ALL_SEGMENTS_COUNT, ///< Waiting for the total count of segments to be received.
    EXPECTING_ALL_SEGMENTS_JSON,  ///< Waiting for a JSON string containing all segment data.
    EXPECTING_EFFECT_ACK          ///< Waiting for an ACK after sending effect data.
};

/**
 * @class BinaryCommandHandler
 * @brief Processes binary commands from BLE or other sources.
 *
 * @details This class is responsible for parsing and dispatching commands based on the
 * BleCommand enum. It manages state for multi-part data transfers, such as receiving
 * large JSON configurations.
 */
class BinaryCommandHandler
{
public:
    /**
     * @brief Construct a new Binary Command Handler object.
     */
    BinaryCommandHandler();

    /**
     * @brief Main entry point for processing an incoming command packet.
     * @param data Pointer to the buffer containing the command data.
     * @param len The length of the data in the buffer.
     */
    void handleCommand(const uint8_t *data, size_t len);

    /**
     * @brief Initiates the process of sending all segment configurations.
     * @param viaSerial True if the request came from Serial, false if from BLE.
     */
    void handleGetAllSegmentConfigs(bool viaSerial);

    /**
     * @brief Initiates the process of receiving all segment configurations.
     * @param viaSerial True if the request was initiated via Serial.
     */
    void handleSetAllSegmentConfigsCommand(bool viaSerial);

    /**
     * @brief Initiates the process of sending all available effect information.
     * @param viaSerial True if the request came from Serial, false if from BLE.
     */
    void handleGetAllEffectsCommand(bool viaSerial);

    /**
     * @brief Gets the current state of the incoming batch process.
     * @return The current IncomingBatchState.
     */
    IncomingBatchState getIncomingBatchState() const { return _incomingBatchState; }

    /**
     * @brief Checks if a multi-part command was initiated via the Serial port.
     * @return True if a serial-based batch command is active.
     */
    bool isSerialBatchActive() const { return _isSerialBatch; }

    /**
     * @brief Processes a JSON string to configure a single segment.
     * @param jsonString The JSON configuration for the segment.
     */
    void processSingleSegmentJson(const char* jsonString); // MODIFIED: Accepts const char*

private:
    char _incomingJsonBuffer[1024];         // MODIFIED: Changed from String to fixed-size char array
    size_t _jsonBufferIndex;                // MODIFIED: Added to track current position in the buffer
    IncomingBatchState _incomingBatchState; ///< Current state of the batch processing state machine.
    bool _isSerialEffectsTest;              ///< Flag to track if the effects test is initiated via serial.
    bool _isSerialBatch;                    ///< Flag to track if a batch command was initiated via serial.

    volatile bool _ackReceived;                     ///< Flag to indicate if an ACK has been received.
    unsigned long _ackTimeoutStart;                 ///< Timestamp for starting ACK wait timeout.
    const unsigned long ACK_WAIT_TIMEOUT_MS = 1000; ///< ACK wait timeout in milliseconds.

    uint16_t _expectedSegmentsToReceive; ///< Total number of segments expected in the current batch.
    uint16_t _segmentsReceivedInBatch;   ///< Counter for segments received so far in the batch.

    uint16_t _expectedEffectsToSend; ///< Total number of effects to send.
    uint16_t _effectsSentInBatch;    ///< Counter for effects sent so far.

    // Private handler methods for specific commands
    void handleSetColor(const uint8_t *payload, size_t len);
    void handleSetEffect(const uint8_t *payload, size_t len);
    void handleSetBrightness(const uint8_t *payload, size_t len);
    void handleSetSegmentBrightness(const uint8_t *payload, size_t len);
    void handleSelectSegment(const uint8_t *payload, size_t len);
    void handleClearSegments();
    void handleSetSegmentRange(const uint8_t *payload, size_t len);
    void handleSetLedCount(const uint8_t *payload, size_t len);
    void handleSetEffectParameter(const uint8_t *payload, size_t len);

    void handleGetStatus();
    void handleGetLedCount();
    void handleBatchConfig(const uint8_t *payload, size_t len);

    void handleGetEffectInfo(const uint8_t *payload, size_t len, bool viaSerial = false);
    String buildEffectInfoJson(uint8_t effectIndex);

    void processIncomingAllSegmentsData(const uint8_t *data, size_t len);
    void handleAck();
};

#endif // BINARY_COMMAND_HANDLER_H
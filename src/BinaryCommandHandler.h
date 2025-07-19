/**
 * @file BinaryCommandHandler.h
 * @brief Defines the handler for processing binary commands for the LED controller.
 * @version 0.7 (Linker Fix)
 * @date 2025-07-18
 * @copyright Copyright (c) 2025
 */

#ifndef BINARY_COMMAND_HANDLER_H
#define BINARY_COMMAND_HANDLER_H

#include <Arduino.h>

// (Enum definitions remain the same)
enum BleCommand : uint8_t
{
    //OBSOLETE:CMD_SET_COLOR = 0x01,
    //OBSOLETE:CMD_SET_EFFECT = 0x02,
    //OBSOLETE: CMD_SET_BRIGHTNESS = 0x03,
    //OBSOLETE:CMD_SET_SEG_BRIGHT = 0x04,
    CMD_SELECT_SEGMENT = 0x05,
    CMD_CLEAR_SEGMENTS = 0x06,
    //OBSOLETE: CMD_SET_SEG_RANGE = 0x07,
    //OBSOLETE: CMD_GET_STATUS = 0x08,
    CMD_BATCH_CONFIG = 0x09,
    CMD_SET_EFFECT_PARAMETER = 0x0A,
    CMD_GET_EFFECT_INFO = 0x0B,
    CMD_SET_LED_COUNT = 0x0C,
    CMD_GET_LED_COUNT = 0x0D,
    CMD_GET_ALL_SEGMENT_CONFIGS = 0x0E,
    CMD_SET_ALL_SEGMENT_CONFIGS = 0x0F,
    CMD_GET_ALL_EFFECTS = 0x10,
    CMD_SET_SINGLE_SEGMENT_JSON = 0x11,
    CMD_SAVE_CONFIG = 0x12,
    CMD_ACK_GENERIC = 0xA0,
    CMD_ACK_EFFECT_SET = 0xA1,
    CMD_ACK_PARAM_SET = 0xA2,
    CMD_ACK_CONFIG_SAVED = 0xA3,
    CMD_ACK_RESTARTING = 0xA4,
    CMD_READY = 0xD0,
    CMD_NACK_UNKNOWN_CMD = 0xE0,
    CMD_NACK_INVALID_PAYLOAD = 0xE1,
    CMD_NACK_INVALID_SEGMENT = 0xE2,
    CMD_NACK_NO_EFFECT = 0xE3,
    CMD_NACK_UNKNOWN_EFFECT = 0xE4,
    CMD_NACK_UNKNOWN_PARAMETER = 0xE5,
    CMD_NACK_JSON_ERROR = 0xE6,
    CMD_NACK_FS_ERROR = 0xE7,
    CMD_NACK_BUFFER_OVERFLOW = 0xE8,
};

enum class IncomingBatchState
{
    IDLE,
    EXPECTING_BATCH_CONFIG_JSON,
    EXPECTING_ALL_SEGMENTS_COUNT,
    EXPECTING_ALL_SEGMENTS_JSON,
    EXPECTING_EFFECT_ACK
};

class BinaryCommandHandler
{
public:
    BinaryCommandHandler();
    void handleCommand(const uint8_t *data, size_t len);
    void handleGetAllSegmentConfigs(bool viaSerial);
    void handleSetAllSegmentConfigsCommand(bool viaSerial);
    void handleGetAllEffectsCommand(bool viaSerial);
    IncomingBatchState getIncomingBatchState() const;
    bool isSerialBatchActive() const;
    void processSingleSegmentJson(const char* jsonString);

private:
    char _incomingJsonBuffer[1024];
    size_t _jsonBufferIndex;
    IncomingBatchState _incomingBatchState;
    bool _isSerialEffectsTest;
    bool _isSerialBatch;
    volatile bool _ackReceived;
    unsigned long _ackTimeoutStart;
    const unsigned long ACK_WAIT_TIMEOUT_MS = 1000;
    uint16_t _expectedSegmentsToReceive;
    uint16_t _segmentsReceivedInBatch;
    uint16_t _expectedEffectsToSend;
    uint16_t _effectsSentInBatch;

    // --- Helper Functions ---
    void sendAck(BleCommand ackCode);
    void sendNack(BleCommand nackCode);
    
    // --- Command Handlers ---
    void handleSetColor(const uint8_t *payload, size_t len);
    void handleSetEffect(const uint8_t *payload, size_t len);
    void handleSetBrightness(const uint8_t *payload, size_t len);
    void handleSetSegmentBrightness(const uint8_t *payload, size_t len);
    void handleSelectSegment(const uint8_t *payload, size_t len);
    void handleClearSegments();
    void handleSetSegmentRange(const uint8_t *payload, size_t len);
    void handleSetLedCount(const uint8_t *payload, size_t len);
    void handleSetEffectParameter(const uint8_t *payload, size_t len);
    void handleSaveConfig();
    void handleGetStatus();
    void handleGetLedCount();
    
    // *** ADDED DECLARATION ***
    void handleBatchConfigJson(const char* json);

    void handleGetEffectInfo(const uint8_t *payload, size_t len, bool viaSerial = false);
    String buildEffectInfoJson(uint8_t effectIndex);
    void processIncomingAllSegmentsData(const uint8_t *data, size_t len);
    void handleAck();
};

#endif // BINARY_COMMAND_HANDLER_H
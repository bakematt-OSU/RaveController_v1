#ifndef BINARY_COMMAND_HANDLER_H
#define BINARY_COMMAND_HANDLER_H

#include <Arduino.h>

// Enum for binary command codes.
// These values MUST match the constants in the Android app's LedControllerCommands.kt
enum BleCommand : uint8_t
{
    CMD_SET_COLOR = 0x01,
    CMD_SET_EFFECT = 0x02,
    CMD_SET_BRIGHTNESS = 0x03,
    CMD_SET_SEG_BRIGHT = 0x04,
    CMD_SELECT_SEGMENT = 0x05,
    CMD_CLEAR_SEGMENTS = 0x06,
    CMD_SET_SEG_RANGE = 0x07,
    CMD_GET_STATUS = 0x08,
    CMD_BATCH_CONFIG = 0x09,
    CMD_SET_EFFECT_PARAMETER = 0x0A,
    CMD_GET_EFFECT_INFO = 0x0B,
    CMD_SET_LED_COUNT = 0x0C,
    CMD_GET_LED_COUNT = 0x0D,
    CMD_GET_ALL_SEGMENT_CONFIGS = 0x0E,
    CMD_SET_ALL_SEGMENT_CONFIGS = 0x0F,
    CMD_GET_ALL_EFFECTS = 0x10,

    // Response codes
    CMD_ACK = 0xA0,
};

// State machine for incoming multi-part commands
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
    void handleSetAllSegmentConfigsCommand();
    void handleGetAllEffectsCommand(bool viaSerial); // Modified to accept context

    IncomingBatchState getIncomingBatchState() const { return _incomingBatchState; }

private:
    String _incomingJsonBuffer;
    IncomingBatchState _incomingBatchState;
    bool _isSerialEffectsTest; // Flag to track if the effects test is via serial

    volatile bool _ackReceived;
    unsigned long _ackTimeoutStart;
    const unsigned long ACK_WAIT_TIMEOUT_MS = 1000;

    uint16_t _expectedSegmentsToReceive;
    uint16_t _segmentsReceivedInBatch;

    uint16_t _expectedEffectsToSend;
    uint16_t _effectsSentInBatch;

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

    // Modified to accept context
    void handleGetEffectInfo(const uint8_t *payload, size_t len, bool viaSerial = false);
    String buildEffectInfoJson(uint8_t effectIndex); // Helper function

    void processIncomingAllSegmentsData(const uint8_t *data, size_t len);
    void handleAck();
};

#endif // BINARY_COMMAND_HANDLER_H

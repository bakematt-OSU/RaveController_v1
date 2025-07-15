#ifndef BINARY_COMMAND_HANDLER_H
#define BINARY_COMMAND_HANDLER_H

#include <Arduino.h>
#include "PixelStrip.h"

// Enum for binary command codes
enum BleCommand : uint8_t {
    CMD_SET_COLOR = 0x01,
    CMD_SET_EFFECT = 0x02,
    CMD_SET_BRIGHTNESS = 0x03,
    CMD_SET_SEG_BRIGHT = 0x04,
    CMD_SELECT_SEGMENT = 0x05,
    CMD_CLEAR_SEGMENTS = 0x06,
    CMD_SET_SEG_RANGE = 0x07,
    CMD_GET_STATUS = 0x08,
    CMD_BATCH_CONFIG = 0x09,
    CMD_NUM_PIXELS = 0x0A,
    CMD_GET_EFFECT_INFO = 0x0B,
    CMD_ACK = 0xA0,
    CMD_SET_LED_COUNT = 0x0C,
    CMD_GET_LED_COUNT = 0x0D
};

class BinaryCommandHandler {
public:
    void handleCommand(const uint8_t* data, size_t len);

private:
    void handleSetColor(const uint8_t* payload, size_t len);
    void handleSetEffect(const uint8_t* payload, size_t len);
    void handleSetBrightness(const uint8_t* payload, size_t len);
    void handleSetSegmentBrightness(const uint8_t* payload, size_t len);
    void handleSelectSegment(const uint8_t* payload, size_t len);
    // FIX: This command has no payload, so it takes no arguments.
    void handleClearSegments();
    void handleSetSegmentRange(const uint8_t* payload, size_t len);
    void handleGetStatus();
    void handleBatchConfig(const uint8_t* payload, size_t len);
    // FIX: This command's payload determines the segment, so it needs arguments.
    void handleGetEffectInfo(const uint8_t* payload, size_t len);
    void handleAck();
    void handleSetLedCount(const uint8_t* payload, size_t len);
    void handleGetLedCount();
};

#endif // BINARY_COMMAND_HANDLER_H

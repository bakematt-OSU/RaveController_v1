#ifndef BINARY_COMMAND_HANDLER_H
#define BINARY_COMMAND_HANDLER_H

#include <Arduino.h>
#include <vector>

// ... (Packet Protocol Definitions remain the same) ...
#define PACKET_PAYLOAD_SIZE 118
const uint8_t FLAG_START_OF_MESSAGE = 0x01;
const uint8_t FLAG_END_OF_MESSAGE   = 0x02;
const uint8_t FLAG_ACK              = 0x04;

struct BlePacket {
    uint8_t sequence;
    uint8_t flags;
    uint8_t payload[PACKET_PAYLOAD_SIZE];
    size_t  payloadSize;
};

enum BleCommand : uint8_t {
    CMD_GET_LED_COUNT = 0x0D,
    CMD_GET_ALL_SEGMENT_CONFIGS = 0x0E,
    CMD_SET_ALL_SEGMENT_CONFIGS = 0x0F,
    CMD_GET_ALL_EFFECTS = 0x10,
    CMD_SAVE_CONFIG = 0x12,
    CMD_CLEAR_SEGMENTS = 0x06,
    CMD_ACK_GENERIC = 0xA0,
    CMD_READY = 0xD0,
};

class BinaryCommandHandler
{
public:
    BinaryCommandHandler();

    void handleCommand(const uint8_t *data, size_t len);
    void sendReliableMessage(const uint8_t* data, size_t len);
    void update();

    // --- FIX: Add a public reset method ---
    void reset();

    // Public handlers for serial debugging
    void handleGetAllSegmentConfigs(bool viaSerial);
    void handleGetAllEffectsCommand(bool viaSerial);
    void processSingleSegmentJson(const char *jsonString);

private:
    char _incomingJsonBuffer[4096];
    size_t _jsonBufferIndex;

    std::vector<BlePacket> _outgoingPacketQueue;
    uint8_t _outgoingSequence;
    uint8_t _lastAckedSequence;
    uint8_t _expectedIncomingSequence;
    bool _isWaitingForAck;
    unsigned long _ackTimeoutStart;
    const unsigned long ACK_WAIT_TIMEOUT_MS = 5000;

    void handleSaveConfig();
    void handleGetLedCount();
    void handleClearSegments();

    String buildEffectInfoJson(uint8_t effectIndex);
    String buildSegmentInfoJson(uint8_t segmentIndex);
};

#endif // BINARY_COMMAND_HANDLER_H
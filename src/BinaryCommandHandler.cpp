#include "BinaryCommandHandler.h"
#include "globals.h"
#include "EffectLookup.h"
#include <Arduino.h>
#include "ConfigManager.h"

extern PixelStrip* strip;
extern uint16_t LED_COUNT;

void BinaryCommandHandler::handleCommand(const uint8_t* data, size_t len) {
    if (len < 1) return;

    BleCommand cmd = (BleCommand)data[0];
    const uint8_t* payload = data + 1;
    size_t payloadLen = len - 1;

    switch (cmd) {
        case CMD_SET_COLOR:
            handleSetColor(payload, payloadLen);
            break;
        case CMD_SET_EFFECT:
            handleSetEffect(payload, payloadLen);
            break;
        case CMD_SET_BRIGHTNESS:
            handleSetBrightness(payload, payloadLen);
            break;
        case CMD_SET_SEG_BRIGHT:
            handleSetSegmentBrightness(payload, payloadLen);
            break;
        case CMD_SELECT_SEGMENT:
            handleSelectSegment(payload, payloadLen);
            break;
        case CMD_CLEAR_SEGMENTS:
            handleClearSegments(payload, payloadLen);
            break;
        case CMD_SET_SEG_RANGE:
            handleSetSegmentRange(payload, payloadLen);
            break;
        case CMD_GET_STATUS:
            handleGetStatus();
            break;
        case CMD_BATCH_CONFIG:
            handleBatchConfig(payload, payloadLen);
            break;
        case CMD_NUM_PIXELS:
            handleGetNumPixels();
            break;
        case CMD_GET_EFFECT_INFO:
            handleGetEffectInfo();
            break;
        case CMD_ACK:
            handleAck();
            break;
        case CMD_SET_LED_COUNT:
            handleSetLedCount(payload, payloadLen);
            break;
        case CMD_GET_LED_COUNT:
            handleGetLedCount();
            break;
        default:
            Serial.println("Unknown binary command");
            break;
    }
}

void BinaryCommandHandler::handleSetColor(const uint8_t* payload, size_t len) {
    if (len < 3 || !strip) return;
    uint8_t r = payload[0];
    uint8_t g = payload[1];
    uint8_t b = payload[2];
    for (auto* s : strip->getSegments()) {
        s->setColor(r, g, b);
    }
    Serial.println("Binary CMD: Set color");
}

void BinaryCommandHandler::handleSetEffect(const uint8_t* payload, size_t len) {
    if (len < 1 || !strip) return;
    uint8_t effectId = payload[0];
    const char* effectName = getEffectNameFromId(effectId);
    if (effectName) {
        PixelStrip::Segment* seg = strip->getSegments()[0];
        BaseEffect* newEffect = createEffectByName(effectName, seg);
        if (newEffect) {
            if (seg->activeEffect) {
                delete seg->activeEffect;
            }
            seg->activeEffect = newEffect;
            // --- FIX: Provide detailed confirmation ---
            Serial.print("Binary CMD: Set effect to ");
            Serial.println(effectName);
        }
    } else {
         Serial.println("Binary CMD: Set effect failed - unknown ID");
    }
}

void BinaryCommandHandler::handleSetBrightness(const uint8_t* payload, size_t len) {
    if (len < 1 || !strip) return;
    uint8_t brightness = payload[0];
    strip->getSegments()[0]->setBrightness(brightness);
    // --- FIX: Provide detailed confirmation ---
    Serial.print("Binary CMD: Set Brightness to ");
    Serial.println(brightness);
}

void BinaryCommandHandler::handleSetSegmentBrightness(const uint8_t* payload, size_t len) {
    if (len < 2 || !strip) return;
    uint8_t segId = payload[0];
    uint8_t brightness = payload[1];
    if (segId < strip->getSegments().size()) {
        strip->getSegments()[segId]->setBrightness(brightness);
    }
    Serial.println("Binary CMD: Set segment brightness");
}

void BinaryCommandHandler::handleSelectSegment(const uint8_t* payload, size_t len) {
    Serial.println("Binary CMD: Select segment");
}

void BinaryCommandHandler::handleClearSegments(const uint8_t* payload, size_t len) {
    if (strip) {
        strip->clearUserSegments();
    }
    Serial.println("Binary CMD: Clear segments");
}

void BinaryCommandHandler::handleSetSegmentRange(const uint8_t* payload, size_t len) {
    if (len < 5 || !strip) return;
    uint8_t segId = payload[0];
    uint16_t start = (payload[1] << 8) | payload[2];
    uint16_t end = (payload[3] << 8) | payload[4];
    if (segId < strip->getSegments().size()) {
        strip->getSegments()[segId]->setRange(start, end);
    }
    Serial.println("Binary CMD: Set segment range");
}

void BinaryCommandHandler::handleGetStatus() {
    Serial.println("Binary CMD: Get status");
}

void BinaryCommandHandler::handleBatchConfig(const uint8_t* payload, size_t len) {
    Serial.println("Binary CMD: Batch config");
}

void BinaryCommandHandler::handleGetNumPixels() {
    Serial.println("Binary CMD: Get num pixels");
}

void BinaryCommandHandler::handleGetEffectInfo() {
    Serial.println("Binary CMD: Get effect info");
}

void BinaryCommandHandler::handleAck() {
    Serial.println("Binary CMD: ACK");
}

void BinaryCommandHandler::handleSetLedCount(const uint8_t* payload, size_t len) {
    if (len < 2) return;
    uint16_t count = (payload[0] << 8) | payload[1];
    setLedCount(count);
    // This part is not reached due to device restart, which is expected.
}

void BinaryCommandHandler::handleGetLedCount() {
    Serial.print("LED_COUNT: ");
    Serial.println(LED_COUNT);
}
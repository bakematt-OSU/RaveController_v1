#include "BinaryCommandHandler.h"
#include "globals.h"
#include "EffectLookup.h"
#include <Arduino.h>
#include "ConfigManager.h"
#include <ArduinoJson.h>

extern PixelStrip* strip;
extern uint16_t LED_COUNT;

// --- Main Handler ---
void BinaryCommandHandler::handleCommand(const uint8_t* data, size_t len) {
    if (len < 1) return;
    BleCommand cmd = (BleCommand)data[0];
    const uint8_t* payload = data + 1;
    size_t payloadLen = len - 1;

    switch (cmd) {
        case CMD_SET_COLOR: handleSetColor(payload, payloadLen); break;
        case CMD_SET_EFFECT: handleSetEffect(payload, payloadLen); break;
        case CMD_SET_BRIGHTNESS: handleSetBrightness(payload, payloadLen); break;
        case CMD_SET_SEG_BRIGHT: handleSetSegmentBrightness(payload, payloadLen); break;
        case CMD_SELECT_SEGMENT: handleSelectSegment(payload, payloadLen); break;
        // FIX: Call with no arguments to match the corrected declaration.
        case CMD_CLEAR_SEGMENTS: handleClearSegments(); break;
        case CMD_SET_SEG_RANGE: handleSetSegmentRange(payload, payloadLen); break;
        case CMD_GET_STATUS: handleGetStatus(); break;
        case CMD_BATCH_CONFIG: handleBatchConfig(payload, payloadLen); break;
        case CMD_NUM_PIXELS: handleGetLedCount(); break;
        // FIX: Call with arguments to match the corrected declaration.
        case CMD_GET_EFFECT_INFO: handleGetEffectInfo(payload, payloadLen); break;
        case CMD_ACK: handleAck(); break;
        case CMD_SET_LED_COUNT: handleSetLedCount(payload, payloadLen); break;
        case CMD_GET_LED_COUNT: handleGetLedCount(); break;
        default:
            Serial.println("ERR: Unknown binary command");
            break;
    }
}

// --- Command Implementations with Corrected Definitions ---

void BinaryCommandHandler::handleSetColor(const uint8_t* payload, size_t len) {
    if (len < 3 || !strip) return;
    strip->getSegments()[0]->setColor(payload[0], payload[1], payload[2]);
    Serial.println("OK: Color set");
}

void BinaryCommandHandler::handleSetEffect(const uint8_t* payload, size_t len) {
    if (len < 1 || !strip) return;
    const char* effectName = getEffectNameFromId(payload[0]);
    if (effectName) {
        PixelStrip::Segment* seg = strip->getSegments()[0];
        if (seg->activeEffect) delete seg->activeEffect;
        seg->activeEffect = createEffectByName(effectName, seg);
        Serial.print("OK: Effect set to ");
        Serial.println(effectName);
    } else {
        Serial.println("ERR: Unknown effect ID");
    }
}

void BinaryCommandHandler::handleSetBrightness(const uint8_t* payload, size_t len) {
    if (len < 1 || !strip) return;
    strip->getSegments()[0]->setBrightness(payload[0]);
    Serial.print("OK: Brightness set to ");
    Serial.println(payload[0]);
}

void BinaryCommandHandler::handleSetSegmentBrightness(const uint8_t* payload, size_t len) {
    if (len < 2 || !strip) return;
    uint8_t segId = payload[0];
    if (segId < strip->getSegments().size()) {
        strip->getSegments()[segId]->setBrightness(payload[1]);
        Serial.print("OK: Segment ");
        Serial.print(segId);
        Serial.print(" brightness set to ");
        Serial.println(payload[1]);
    } else {
        Serial.println("ERR: Invalid segment ID");
    }
}

void BinaryCommandHandler::handleSelectSegment(const uint8_t* payload, size_t len) {
    Serial.println("OK: Segment selected (no-op on firmware)");
}

// FIX: Update function definition to take no arguments.
void BinaryCommandHandler::handleClearSegments() {
    if (strip) {
        strip->clearUserSegments();
        Serial.println("OK: User segments cleared");
    } else {
        Serial.println("ERR: Strip not initialized");
    }
}

void BinaryCommandHandler::handleSetSegmentRange(const uint8_t* payload, size_t len) {
    if (len < 5 || !strip) return;
    uint8_t segId = payload[0];
    uint16_t start = (payload[1] << 8) | payload[2];
    uint16_t end = (payload[3] << 8) | payload[4];
    if (segId < strip->getSegments().size()) {
        strip->getSegments()[segId]->setRange(start, end);
        Serial.print("OK: Segment ");
        Serial.print(segId);
        Serial.print(" range set to ");
        Serial.print(start);
        Serial.print("-");
        Serial.println(end);
    } else {
        Serial.println("ERR: Invalid segment ID");
    }
}

void BinaryCommandHandler::handleGetStatus() {
    StaticJsonDocument<1024> doc;
    doc["led_count"] = LED_COUNT;
    JsonArray segments = doc.createNestedArray("segments");
    if (strip) {
        for (auto* s : strip->getSegments()) {
            JsonObject segObj = segments.createNestedObject();
            segObj["id"] = s->getId();
            segObj["name"] = s->getName();
            segObj["startLed"] = s->startIndex();
            segObj["endLed"] = s->endIndex();
            segObj["brightness"] = s->getBrightness();
            segObj["effect"] = s->activeEffect ? s->activeEffect->getName() : "None";
        }
    }
    serializeJson(doc, Serial);
    Serial.println();
}

void BinaryCommandHandler::handleBatchConfig(const uint8_t* payload, size_t len) {
    Serial.println("OK: Batch config (not implemented)");
}

// FIX: Update function definition to take arguments.
void BinaryCommandHandler::handleGetEffectInfo(const uint8_t* payload, size_t len) {
    if (len < 1) {
        Serial.println("ERR: Missing segment ID");
        return;
    }
    uint8_t segIndex = payload[0];
    if (!strip || segIndex >= strip->getSegments().size()) {
        Serial.println("ERR: Invalid segment index");
        return;
    }
    PixelStrip::Segment* seg = strip->getSegments()[segIndex];
    if (!seg->activeEffect) {
        Serial.println("ERR: No active effect");
        return;
    }
    StaticJsonDocument<512> doc;
    doc["effect"] = seg->activeEffect->getName();
    JsonArray params = doc.createNestedArray("params");
    for (int i = 0; i < seg->activeEffect->getParameterCount(); ++i) {
        EffectParameter* p = seg->activeEffect->getParameter(i);
        JsonObject p_obj = params.createNestedObject();
        p_obj["name"] = p->name;
        p_obj["type"] = (int)p->type;
        // This could be expanded to include the value, min, and max
    }
    serializeJson(doc, Serial);
    Serial.println();
}

void BinaryCommandHandler::handleAck() {
    Serial.println("OK: ACK");
}

void BinaryCommandHandler::handleSetLedCount(const uint8_t* payload, size_t len) {
    if (len < 2) return;
    setLedCount((payload[0] << 8) | payload[1]);
}

void BinaryCommandHandler::handleGetLedCount() {
    Serial.print("LED_COUNT: ");
    Serial.println(LED_COUNT);
}

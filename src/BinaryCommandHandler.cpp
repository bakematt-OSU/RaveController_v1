/**
 * @file BinaryCommandHandler.cpp
 * @brief Implementation for handling binary commands from the app.
 *
 * This version contains the fully implemented 'handleGetEffectInfo' function
 * to correctly respond to the app's request for effect parameters.
 *
 * @version 2.1
 * @date 2025-07-15
 */
#include "BinaryCommandHandler.h"
#include "globals.h"
#include "EffectLookup.h"
#include "ConfigManager.h"
#include "BLEManager.h"
#include <ArduinoJson.h>

extern PixelStrip *strip;
extern uint16_t LED_COUNT;

// --- Main Command Router ---
void BinaryCommandHandler::handleCommand(const uint8_t *data, size_t len)
{
    if (len < 1)
        return;

    BleCommand cmd = (BleCommand)data[0];
    const uint8_t *payload = data + 1;
    size_t payloadLen = len - 1;
    bool sendGenericAck = true;

    switch (cmd)
    {
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
        handleClearSegments();
        break;
    case CMD_SET_SEG_RANGE:
        handleSetSegmentRange(payload, payloadLen);
        break;
    case CMD_SET_LED_COUNT:
        handleSetLedCount(payload, payloadLen);
        break;

    case CMD_GET_STATUS:
        handleGetStatus();
        sendGenericAck = false;
        break;
    case CMD_GET_LED_COUNT:
        handleGetLedCount();
        sendGenericAck = false;
        break;
    case CMD_GET_EFFECT_INFO:
        handleGetEffectInfo(payload, payloadLen);
        sendGenericAck = false;
        break;
    case CMD_BATCH_CONFIG:
        handleBatchConfig(payload, payloadLen);
        sendGenericAck = false;
        break;
    case CMD_ACK:
        handleAck();
        sendGenericAck = false;
        break;

    default:
        Serial.print("ERR: Unknown binary command: 0x");
        Serial.println(cmd, HEX);
        sendGenericAck = false;
        break;
    }

    if (sendGenericAck)
    {
        uint8_t ack_payload[] = {CMD_ACK};
        BLEManager::getInstance().sendMessage(ack_payload, 1);
        Serial.println("-> Sent ACK");
    }
}

// --- Command Implementations ---

void BinaryCommandHandler::handleSetColor(const uint8_t *payload, size_t len)
{
    if (len < 4 || !strip)
        return;
    uint8_t segId = payload[0];
    if (segId < strip->getSegments().size())
    {
        strip->getSegments()[segId]->setColor(payload[1], payload[2], payload[3]);
        Serial.println("OK: Color set");
    }
}

void BinaryCommandHandler::handleSetEffect(const uint8_t *payload, size_t len)
{
    if (len < 2 || !strip)
        return;
    uint8_t segId = payload[0];
    uint8_t effectId = payload[1];

    if (segId >= strip->getSegments().size())
    {
        Serial.println("ERR: Invalid segment ID");
        return;
    }

    const char *effectName = getEffectNameFromId(effectId);
    if (effectName)
    {
        PixelStrip::Segment *seg = strip->getSegments()[segId];
        if (seg->activeEffect)
            delete seg->activeEffect;
        seg->activeEffect = createEffectByName(effectName, seg);
        Serial.print("OK: Segment ");
        Serial.print(segId);
        Serial.print(" effect set to ");
        Serial.println(effectName);
    }
    else
    {
        Serial.println("ERR: Unknown effect ID");
    }
}

void BinaryCommandHandler::handleSetBrightness(const uint8_t *payload, size_t len)
{
    if (len < 1 || !strip)
        return;
    strip->getSegments()[0]->setBrightness(payload[0]);
    Serial.print("OK: Global Brightness set to ");
    Serial.println(payload[0]);
}

void BinaryCommandHandler::handleSetSegmentBrightness(const uint8_t *payload, size_t len)
{
    if (len < 2 || !strip)
        return;
    uint8_t segId = payload[0];
    if (segId < strip->getSegments().size())
    {
        strip->getSegments()[segId]->setBrightness(payload[1]);
        Serial.print("OK: Segment ");
        Serial.print(segId);
        Serial.print(" brightness set to ");
        Serial.println(payload[1]);
    }
    else
    {
        Serial.println("ERR: Invalid segment ID");
    }
}

void BinaryCommandHandler::handleSelectSegment(const uint8_t *payload, size_t len)
{
    Serial.println("OK: Select segment command received (no-op on firmware)");
}

void BinaryCommandHandler::handleClearSegments()
{
    if (strip)
    {
        strip->clearUserSegments();
        Serial.println("OK: User segments cleared");
    }
    else
    {
        Serial.println("ERR: Strip not initialized");
    }
}

void BinaryCommandHandler::handleSetSegmentRange(const uint8_t *payload, size_t len)
{
    if (len < 5 || !strip)
        return;
    uint8_t segId = payload[0];
    uint16_t start = (payload[1] << 8) | payload[2];
    uint16_t end = (payload[3] << 8) | payload[4];

    if (segId < strip->getSegments().size())
    {
        strip->getSegments()[segId]->setRange(start, end);
        Serial.print("OK: Segment ");
        Serial.print(segId);
        Serial.print(" range set to ");
        Serial.print(start);
        Serial.print("-");
        Serial.println(end);
    }
    else
    {
        Serial.println("ERR: Invalid segment ID");
    }
}

void BinaryCommandHandler::handleSetLedCount(const uint8_t *payload, size_t len)
{
    if (len < 2)
        return;
    uint16_t count = (payload[0] << 8) | payload[1];
    setLedCount(count);
}

void BinaryCommandHandler::handleGetStatus()
{
    StaticJsonDocument<1024> doc;
    doc["led_count"] = LED_COUNT;

    JsonArray effects = doc.createNestedArray("available_effects");
    for (int i = 0; i < EFFECT_COUNT; ++i)
    {
        effects.add(EFFECT_NAMES[i]);
    }

    JsonArray segments = doc.createNestedArray("segments");
    if (strip)
    {
        for (auto *s : strip->getSegments())
        {
            JsonObject segObj = segments.createNestedObject();
            segObj["id"] = s->getId();
            segObj["name"] = s->getName();
            segObj["startLed"] = s->startIndex();
            segObj["endLed"] = s->endIndex();
            segObj["brightness"] = s->getBrightness();
            segObj["effect"] = s->activeEffect ? s->activeEffect->getName() : "None";
        }
    }

    String response;
    serializeJson(doc, response);
    BLEManager::getInstance().sendMessage(response);
    Serial.println("-> Sent Status JSON");
}

void BinaryCommandHandler::handleGetLedCount()
{
    uint8_t response[3];
    response[0] = CMD_GET_LED_COUNT;
    response[1] = (LED_COUNT >> 8) & 0xFF;
    response[2] = LED_COUNT & 0xFF;
    BLEManager::getInstance().sendMessage(response, 3);
    Serial.print("-> Sent LED Count: ");
    Serial.println(LED_COUNT);
}

void BinaryCommandHandler::handleBatchConfig(const uint8_t *payload, size_t len)
{
    String jsonPayload;
    jsonPayload.reserve(len);
    for (size_t i = 0; i < len; ++i)
    {
        jsonPayload += (char)payload[i];
    }

    Serial.println("Received batch config. Applying...");
    handleBatchConfigJson(jsonPayload);
}

/**
 * **[THIS IS THE FIX]**
 * This function now correctly handles a request for an effect's details.
 * It uses the incoming effect index to create a temporary instance of that effect,
 * serializes its parameters into a JSON object, and sends it back to the app.
 */
void BinaryCommandHandler::handleGetEffectInfo(const uint8_t *payload, size_t len)
{
    if (len < 1)
    {
        Serial.println("ERR: Missing effect ID for GET_EFFECT_INFO");
        return;
    }
    uint8_t effectIndex = payload[0];
    const char *effectName = getEffectNameFromId(effectIndex);

    if (!effectName || !strip)
    {
        Serial.print("ERR: Invalid effect index or strip not initialized. Index: ");
        Serial.println(effectIndex);
        return;
    }

    // Create a temporary "dummy" segment to pass to the effect constructor.
    // The effect needs a segment to access helper functions like ColorHSV, but it won't
    // actually draw anything.
    PixelStrip::Segment *dummySegment = strip->getSegments()[0];
    BaseEffect *tempEffect = createEffectByName(effectName, dummySegment);

    if (!tempEffect)
    {
        Serial.print("ERR: Failed to create temporary effect for ");
        Serial.println(effectName);
        return;
    }

    // Create the JSON response
    StaticJsonDocument<512> doc;
    doc["effect"] = tempEffect->getName();
    JsonArray params = doc.createNestedArray("params");
    for (int i = 0; i < tempEffect->getParameterCount(); ++i)
    {
        EffectParameter *p = tempEffect->getParameter(i);
        JsonObject p_obj = params.createNestedObject();
        p_obj["name"] = p->name;

        switch (p->type)
        {
        case ParamType::INTEGER:
            p_obj["type"] = "integer";
            p_obj["value"] = p->value.intValue;
            break;
        case ParamType::FLOAT:
            p_obj["type"] = "float";
            p_obj["value"] = p->value.floatValue;
            break;
        case ParamType::COLOR:
            p_obj["type"] = "color";
            p_obj["value"] = p->value.colorValue;
            break;
        case ParamType::BOOLEAN:
            p_obj["type"] = "boolean";
            p_obj["value"] = p->value.boolValue;
            break;
        }
        p_obj["min_val"] = p->min_val;
        p_obj["max_val"] = p->max_val;
    }

    // Send the JSON response over BLE
    String response;
    serializeJson(doc, response);
    BLEManager::getInstance().sendMessage(response);
    Serial.print("-> Sent Effect Info for: ");
    Serial.println(effectName);

    // Clean up the temporary effect instance
    delete tempEffect;
}

void BinaryCommandHandler::handleAck()
{
    Serial.println("Received ACK from app.");
}

/**
 * @file BinaryCommandHandler.cpp
 * @brief Implementation for handling binary commands from the app.
 *
 * This version contains the fully implemented 'handleGetEffectInfo' function,
 * the corrected 'handleBatchConfig' to send a success response, and enhanced
 * Serial debugging output for easier troubleshooting.
 *
 * @version 2.4
 * @date 2025-07-15
 */
#include "BinaryCommandHandler.h"
#include "globals.h"
#include "EffectLookup.h"
#include "ConfigManager.h"
#include "BLEManager.h"
#include <ArduinoJson.h>

// Forward declaration for a function you likely have in another file.
void handleBatchConfigJson(const String &jsonPayload);

extern PixelStrip *strip;
extern uint16_t LED_COUNT;

// --- Main Command Router ---
void BinaryCommandHandler::handleCommand(const uint8_t *data, size_t len)
{
    if (len < 1)
    {
        Serial.println("ERR: Received empty command.");
        return;
    }

    // If we are in the middle of receiving a batch config, buffer the data
    if (isReceivingBatchConfig)
    {
        for (size_t i = 0; i < len; i++)
        {
            batchConfigBuffer += (char)data[i];
        }

        // Check if the received data completes the JSON object
        // A simple but effective check is looking for the closing sequence.
        if (batchConfigBuffer.endsWith("}]}"))
        {
            Serial.println("Batch config fully received. Parsing...");
            Serial.println(batchConfigBuffer);

            // Now that we have the full JSON, process it.
            handleBatchConfigJson(batchConfigBuffer);

            // Reset the state machine
            isReceivingBatchConfig = false;
            batchConfigBuffer = "";
        }
        // Don't process this chunk as a new command, just wait for more data.
        return;
    }

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
    case CMD_SET_EFFECT_PARAMETER: // Handle the new command
        handleSetEffectParameter(payload, payloadLen); //
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
        // This is the start of a new batch configuration
        handleBatchConfig(payload, payloadLen);
        sendGenericAck = false; // The function will handle its own ACKs/responses
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
        Serial.println("-> Sent Generic ACK");
    }
}

// --- Command Implementations (existing) ---
// ... (handleBatchConfig, handleSetColor, handleSetEffect, etc. remain the same)
void BinaryCommandHandler::handleBatchConfig(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Batch Config STARTED");
    isReceivingBatchConfig = true;
    batchConfigBuffer = ""; // Clear buffer
    for (size_t i = 0; i < len; i++)
    {
        batchConfigBuffer += (char)payload[i];
    }
    // It's possible the whole message came in one chunk if it's small.
    if (batchConfigBuffer.endsWith("}]}"))
    {
        Serial.println("Batch config fully received (single chunk). Parsing...");
        handleBatchConfigJson(batchConfigBuffer);
        isReceivingBatchConfig = false;
        batchConfigBuffer = "";
    }
    // Otherwise, we just wait for the next chunk to arrive, which will be handled
    // by the `if (isReceivingBatchConfig)` block at the top of `handleCommand`.
}

void BinaryCommandHandler::handleSetColor(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Set Color");
    if (len < 4)
    {
        Serial.println("ERR: Payload too short for Set Color");
        return;
    }
    if (!strip)
    {
        Serial.println("ERR: Strip not initialized!");
        return;
    }

    uint8_t segId = payload[0];
    if (segId < strip->getSegments().size())
    {
        strip->getSegments()[segId]->setColor(payload[1], payload[2], payload[3]);
        Serial.print("OK: Seg ");
        Serial.print(segId);
        Serial.print(" color set to R:");
        Serial.print(payload[1]);
        Serial.print(" G:");
        Serial.print(payload[2]);
        Serial.print(" B:");
        Serial.println(payload[3]);
    }
    else
    {
        Serial.print("ERR: Invalid segment ID: ");
        Serial.println(segId);
    }
}

void BinaryCommandHandler::handleSetEffect(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Set Effect");
    if (len < 2)
    {
        Serial.println("ERR: Payload too short for Set Effect");
        return;
    }
    if (!strip)
    {
        Serial.println("ERR: Strip not initialized!");
        return;
    }

    uint8_t segId = payload[0];
    uint8_t effectId = payload[1];

    if (segId >= strip->getSegments().size())
    {
        Serial.print("ERR: Invalid segment ID: ");
        Serial.println(segId);
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
        Serial.print("ERR: Unknown effect ID: ");
        Serial.println(effectId);
    }
}

void BinaryCommandHandler::handleSetBrightness(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Set Brightness (Global)");
    if (len < 1)
    {
        Serial.println("ERR: Payload too short for Set Brightness");
        return;
    }
    if (!strip)
    {
        Serial.println("ERR: Strip not initialized!");
        return;
    }
    strip->getSegments()[0]->setBrightness(payload[0]);
    Serial.print("OK: Global Brightness set to ");
    Serial.println(payload[0]);
}

void BinaryCommandHandler::handleSetSegmentBrightness(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Set Segment Brightness");
    if (len < 2)
    {
        Serial.println("ERR: Payload too short for Set Seg Brightness");
        return;
    }
    if (!strip)
    {
        Serial.println("ERR: Strip not initialized!");
        return;
    }
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
        Serial.print("ERR: Invalid segment ID: ");
        Serial.println(segId);
    }
}

void BinaryCommandHandler::handleSelectSegment(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Select segment (no-op on firmware)");
}

void BinaryCommandHandler::handleClearSegments()
{
    Serial.println("CMD: Clear Segments");
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
    Serial.println("CMD: Set Segment Range");
    if (len < 5)
    {
        Serial.println("ERR: Payload too short for Set Seg Range");
        return;
    }
    if (!strip)
    {
        Serial.println("ERR: Strip not initialized!");
        return;
    }
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
        Serial.print("ERR: Invalid segment ID: ");
        Serial.println(segId);
    }
}

void BinaryCommandHandler::handleSetLedCount(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Set LED Count");
    if (len < 2)
    {
        Serial.println("ERR: Payload too short for Set LED Count");
        return;
    }
    uint16_t count = (payload[0] << 8) | payload[1];
    Serial.print("OK: Setting LED count to ");
    Serial.println(count);
    setLedCount(count);
}

void BinaryCommandHandler::handleGetStatus()
{
    Serial.println("CMD: Get Status");
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
    Serial.print("-> Sending Status JSON (");
    Serial.print(response.length());
    Serial.println(" bytes)");
    BLEManager::getInstance().sendMessage(response);
}

void BinaryCommandHandler::handleGetLedCount()
{
    Serial.println("CMD: Get LED Count");
    uint8_t response[3];
    response[0] = CMD_GET_LED_COUNT;
    response[1] = (LED_COUNT >> 8) & 0xFF;
    response[2] = LED_COUNT & 0xFF;
    BLEManager::getInstance().sendMessage(response, 3);
    Serial.print("-> Sent LED Count: ");
    Serial.println(LED_COUNT);
}

void BinaryCommandHandler::handleGetEffectInfo(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Get Effect Info");
    if (len < 2)
    {
        Serial.println("ERR: Missing segment ID or effect ID for GET_EFFECT_INFO");
        return;
    }
    uint8_t effectIndex = payload[1];

    Serial.print("Requested info for effect index: ");
    Serial.println(effectIndex);

    const char *effectName = getEffectNameFromId(effectIndex);

    if (!effectName || !strip || strip->getSegments().empty())
    {
        Serial.print("ERR: Invalid effect index, strip not initialized, or no segments. Index: ");
        Serial.println(effectIndex);
        return;
    }

    PixelStrip::Segment *dummySegment = strip->getSegments()[0];
    BaseEffect *tempEffect = createEffectByName(effectName, dummySegment);

    if (!tempEffect)
    {
        Serial.print("ERR: Failed to create temporary effect for ");
        Serial.println(effectName);
        return;
    }

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

    String response;
    serializeJson(doc, response);
    Serial.print("-> Sending Effect Info for '");
    Serial.print(effectName);
    Serial.print("' (");
    Serial.print(response.length());
    Serial.println(" bytes)");
    BLEManager::getInstance().sendMessage(response);

    delete tempEffect;
}

void BinaryCommandHandler::handleAck()
{
    Serial.println("<- Received ACK from app.");
}

// New implementation for setting effect parameters
void BinaryCommandHandler::handleSetEffectParameter(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Set Effect Parameter");
    if (len < 4) // Minimum payload: segId (1) + paramType (1) + nameLen (1) + value (1 for bool minimum)
    {
        Serial.println("ERR: Payload too short for Set Effect Parameter");
        BLEManager::getInstance().sendMessage("{\"error\":\"Payload too short\"}");
        return;
    }
    if (!strip)
    {
        Serial.println("ERR: Strip not initialized!");
        BLEManager::getInstance().sendMessage("{\"error\":\"Strip not initialized\"}");
        return;
    }

    uint8_t segId = payload[0];
    uint8_t paramTypeRaw = payload[1];
    uint8_t nameLen = payload[2];

    if (segId >= strip->getSegments().size())
    {
        Serial.print("ERR: Invalid segment ID: ");
        Serial.println(segId);
        BLEManager::getInstance().sendMessage("{\"error\":\"Invalid segment ID\"}");
        return;
    }

    if ((size_t)3 + nameLen >= len) // Fix: Cast 3 to size_t for comparison with len
    {
        Serial.println("ERR: Invalid parameter name length or missing value data.");
        BLEManager::getInstance().sendMessage("{\"error\":\"Invalid parameter data\"}");
        return;
    }

    String paramName;
    // Read parameter name (bytes 3 to 3 + nameLen - 1)
    for (int i = 0; i < nameLen; ++i)
    {
        paramName += (char)payload[3 + i];
    }

    PixelStrip::Segment *seg = strip->getSegments()[segId];
    if (!seg->activeEffect)
    {
        Serial.println("ERR: No active effect on segment to set parameter.");
        BLEManager::getInstance().sendMessage("{\"error\":\"No active effect\"}");
        return;
    }

    // Get a pointer to the value bytes
    const uint8_t *valueBytes = payload + 3 + nameLen;
    size_t valueLen = len - (3 + nameLen);

    ParamType paramType = (ParamType)paramTypeRaw;

    // Dispatch based on parameter type
    switch (paramType)
    {
    case ParamType::INTEGER:
    {
        if (valueLen < 4)
        {
            Serial.println("ERR: Integer value too short.");
            BLEManager::getInstance().sendMessage("{\"error\":\"Invalid integer value\"}");
            return;
        }
        int32_t intValue = (int32_t)valueBytes[0] << 24 |
                           (int32_t)valueBytes[1] << 16 |
                           (int32_t)valueBytes[2] << 8 |
                           (int32_t)valueBytes[3];
        // Fix: Explicitly cast intValue to int to resolve ambiguity
        seg->activeEffect->setParameter(paramName.c_str(), (int)intValue); //
        Serial.print("OK: Set param '");
        Serial.print(paramName);
        Serial.print("' to int ");
        Serial.println(intValue);
        break;
    }
    case ParamType::FLOAT:
    {
        if (valueLen < 4)
        {
            Serial.println("ERR: Float value too short.");
            BLEManager::getInstance().sendMessage("{\"error\":\"Invalid float value\"}");
            return;
        }
        float floatValue;
        memcpy(&floatValue, valueBytes, 4); // Copy bytes directly into float
        seg->activeEffect->setParameter(paramName.c_str(), floatValue);
        Serial.print("OK: Set param '");
        Serial.print(paramName);
        Serial.print("' to float ");
        Serial.println(floatValue);
        break;
    }
    case ParamType::COLOR:
    {
        if (valueLen < 4)
        {
            Serial.println("ERR: Color value too short.");
            BLEManager::getInstance().sendMessage("{\"error\":\"Invalid color value\"}");
            return;
        }
        // Assuming valueBytes[0] is typically 0x00 for RGB or alpha.
        // For standard 0xRRGGBB, use bytes 1, 2, 3:
        uint32_t colorValue = (uint32_t)valueBytes[1] << 16 |
                              (uint32_t)valueBytes[2] << 8 |
                              (uint32_t)valueBytes[3];
        seg->activeEffect->setParameter(paramName.c_str(), colorValue);
        Serial.print("OK: Set param '");
        Serial.print(paramName);
        Serial.print("' to color 0x");
        Serial.println(colorValue, HEX);
        break;
    }
    case ParamType::BOOLEAN:
    {
        if (valueLen < 1)
        {
            Serial.println("ERR: Bool value too short.");
            BLEManager::getInstance().sendMessage("{\"error\":\"Invalid boolean value\"}");
            return;
        }
        bool boolValue = (valueBytes[0] != 0);
        seg->activeEffect->setParameter(paramName.c_str(), boolValue);
        Serial.print("OK: Set param '");
        Serial.print(paramName);
        Serial.print("' to bool ");
        Serial.println(boolValue ? "true" : "false");
        break;
    }
    default:
        Serial.print("ERR: Unknown ParamType: ");
        Serial.println((int)paramTypeRaw);
        BLEManager::getInstance().sendMessage("{\"error\":\"Unknown param type\"}");
        return;
    }
}
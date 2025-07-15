/**
 * @file BinaryCommandHandler.cpp
 * @brief Implementation for handling binary commands from the app.
 *
 * This version contains the fully implemented 'handleGetEffectInfo' function,
 * the corrected 'handleBatchConfig' to send a success response, and enhanced
 * Serial debugging output for easier troubleshooting.
 *
 * @version 2.3
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
    if (len < 1) {
        Serial.println("ERR: Received empty command.");
        return;
    }

    // Print incoming raw data for debugging
    Serial.print("<- RX Command (len ");
    Serial.print(len);
    Serial.print("): ");
    for(size_t i=0; i<len; i++) {
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();


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
        Serial.println("-> Sent Generic ACK");
    }
}

// --- Command Implementations ---

void BinaryCommandHandler::handleSetColor(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Set Color");
    if (len < 4) {
        Serial.println("ERR: Payload too short for Set Color");
        return;
    }
    if (!strip) {
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
    } else {
        Serial.print("ERR: Invalid segment ID: ");
        Serial.println(segId);
    }
}

void BinaryCommandHandler::handleSetEffect(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Set Effect");
    if (len < 2) {
        Serial.println("ERR: Payload too short for Set Effect");
        return;
    }
     if (!strip) {
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
    if (len < 1) {
        Serial.println("ERR: Payload too short for Set Brightness");
        return;
    }
    if (!strip) {
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
    if (len < 2) {
        Serial.println("ERR: Payload too short for Set Seg Brightness");
        return;
    }
    if (!strip) {
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
    if (len < 5) {
        Serial.println("ERR: Payload too short for Set Seg Range");
        return;
    }
    if (!strip) {
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
    if (len < 2) {
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

void BinaryCommandHandler::handleBatchConfig(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Batch Config");
    String jsonPayload;
    jsonPayload.reserve(len + 1);
    for (size_t i = 0; i < len; ++i)
    {
        jsonPayload += (char)payload[i];
    }
    
    Serial.print("Received JSON payload (");
    Serial.print(jsonPayload.length());
    Serial.println(" bytes). Applying...");
    // Serial.println(jsonPayload); // Uncomment for full JSON dump

    handleBatchConfigJson(jsonPayload); // Assuming this function applies the config.

    // *** THE FIX IS HERE ***
    // After successfully applying the configuration, we MUST send a response.
    // This tells the Android app that the command was successful and it can
    // send the next command in its queue. Without this, the app freezes.
    BLEManager::getInstance().sendMessage("{\"status\":\"OK\"}");
    Serial.println("-> Sent Batch Config OK response");
}

void BinaryCommandHandler::handleGetEffectInfo(const uint8_t *payload, size_t len)
{
    // Serial.println("CMD: Get Effect Info");
    // if (len < 1)
    // {
    //     Serial.println("ERR: Missing effect ID for GET_EFFECT_INFO");
    //     return;
    // }
    // uint8_t effectIndex = payload[0];
    // Serial.print("Requested info for effect index: ");
    // Serial.println(effectIndex);

    // const char *effectName = getEffectNameFromId(effectIndex);

    // if (!effectName || !strip || strip->getSegments().empty())
    // {
    //     Serial.print("ERR: Invalid effect index, strip not initialized, or no segments. Index: ");
    //     Serial.println(effectIndex);
    //     return;
    // }
        Serial.println("CMD: Get Effect Info");
    if (len < 2) // Changed len check to 2, because we expect segment index and effect index
    {
        Serial.println("ERR: Missing segment ID or effect ID for GET_EFFECT_INFO");
        return;
    }
    // uint8_t segmentIndex = payload[0]; // If you need to use segmentIndex, you can extract it here
    uint8_t effectIndex = payload[1]; // CORRECTED: Get effect index from payload[1]

    Serial.print("Requested info for effect index: ");
    Serial.println(effectIndex);

    const char *effectName = getEffectNameFromId(effectIndex);

    if (!effectName || !strip || strip->getSegments().empty())
    {
        Serial.print("ERR: Invalid effect index, strip not initialized, or no segments. Index: ");
        Serial.println(effectIndex);
        return;
    }

    // Create a temporary "dummy" segment to pass to the effect constructor.
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
    Serial.print("-> Sending Effect Info for '");
    Serial.print(effectName);
    Serial.print("' (");
    Serial.print(response.length());
    Serial.println(" bytes)");
    BLEManager::getInstance().sendMessage(response);

    // Clean up the temporary effect instance
    delete tempEffect;
}

void BinaryCommandHandler::handleAck()
{
    Serial.println("<- Received ACK from app.");
}

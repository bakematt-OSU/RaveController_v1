#include "BinaryCommandHandler.h"
#include "globals.h"
#include "EffectLookup.h"
#include "ConfigManager.h"
#include "BLEManager.h"
#include <ArduinoJson.h>

extern PixelStrip *strip;
extern uint16_t LED_COUNT;

// Constructor
BinaryCommandHandler::BinaryCommandHandler()
    : _incomingJsonBuffer(""),
      _incomingBatchState(IncomingBatchState::IDLE),
      _isSerialEffectsTest(false),
      _ackReceived(false),
      _ackTimeoutStart(0),
      _expectedSegmentsToReceive(0),
      _segmentsReceivedInBatch(0),
      _expectedEffectsToSend(0),
      _effectsSentInBatch(0)
{
}

// Main Command Router
void BinaryCommandHandler::handleCommand(const uint8_t *data, size_t len)
{
    if (len < 1)
    {
        Serial.println("ERR: Received empty command.");
        return;
    }

    // Handle ACK for the get all effects command
    if (_incomingBatchState == IncomingBatchState::EXPECTING_EFFECT_ACK)
    {
        bool isAck = (data[0] == CMD_ACK) || (len >= 3 && data[0] == 'a' && data[1] == 'c' && data[2] == 'k');
        if (isAck)
        {
            handleAck();
            if (_effectsSentInBatch < _expectedEffectsToSend)
            {
                String effectJson = buildEffectInfoJson(_effectsSentInBatch);
                if (_isSerialEffectsTest)
                {
                    Serial.println(effectJson);
                }
                else
                {
                    BLEManager::getInstance().sendMessage(effectJson);
                }
                _effectsSentInBatch++;
                if (_effectsSentInBatch >= _expectedEffectsToSend)
                {
                    Serial.println("OK: All effects sent.");
                    _incomingBatchState = IncomingBatchState::IDLE;
                    _isSerialEffectsTest = false;
                }
                else
                {
                    Serial.print("Now waiting for ACK to send effect ");
                    Serial.print(_effectsSentInBatch);
                    Serial.println("...");
                }
            }
        }
        return;
    }

    // Handle other multi-part commands
    if (_incomingBatchState == IncomingBatchState::EXPECTING_BATCH_CONFIG_JSON ||
        _incomingBatchState == IncomingBatchState::EXPECTING_ALL_SEGMENTS_COUNT ||
        _incomingBatchState == IncomingBatchState::EXPECTING_ALL_SEGMENTS_JSON)
    {
        processIncomingAllSegmentsData(data, len);
        return;
    }

    // Process new commands
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
    case CMD_SET_EFFECT_PARAMETER:
        handleSetEffectParameter(payload, payloadLen);
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
        handleGetEffectInfo(payload, payloadLen, false);
        sendGenericAck = false;
        break;
    case CMD_BATCH_CONFIG:
        _incomingBatchState = IncomingBatchState::EXPECTING_BATCH_CONFIG_JSON;
        _incomingJsonBuffer = "";
        processIncomingAllSegmentsData(payload, payloadLen);
        sendGenericAck = false;
        break;
    case CMD_GET_ALL_SEGMENT_CONFIGS:
        handleGetAllSegmentConfigs(false);
        sendGenericAck = false;
        break;
    case CMD_SET_ALL_SEGMENT_CONFIGS:
        handleSetAllSegmentConfigsCommand();
        sendGenericAck = false;
        break;
    case CMD_GET_ALL_EFFECTS:
        handleGetAllEffectsCommand(false);
        sendGenericAck = false;
        break;
     case CMD_SET_SINGLE_SEGMENT_JSON:
    {
        String jsonPayload;
        for(size_t i = 0; i < payloadLen; i++) {
            jsonPayload += (char)payload[i];
        }
        processSingleSegmentJson(jsonPayload);
        break;
    }
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

void BinaryCommandHandler::handleAck()
{
    Serial.println("<- Received ACK from app.");
    _ackReceived = true;
}

void BinaryCommandHandler::handleSetAllSegmentConfigsCommand()
{
    Serial.println("CMD: Set All Segment Configurations - Initiated.");
    if (strip)
    {
        strip->clearUserSegments();
        Serial.println("OK: Cleared existing user segments.");
    }
    _incomingBatchState = IncomingBatchState::EXPECTING_ALL_SEGMENTS_COUNT;
    _incomingJsonBuffer = "";
    _expectedSegmentsToReceive = 0;
    _segmentsReceivedInBatch = 0;
    uint8_t ack_payload[] = {CMD_ACK};
    BLEManager::getInstance().sendMessage(ack_payload, 1);
    Serial.println("-> Sent ACK for CMD_SET_ALL_SEGMENT_CONFIGS initiation.");
}

// src/BinaryCommandHandler.cpp

void BinaryCommandHandler::handleGetAllEffectsCommand(bool viaSerial)
{
    if (!viaSerial)
    {
        Serial.println("CMD: Get All Effects - Initiated.");
    }
    _expectedEffectsToSend = EFFECT_COUNT;
    _effectsSentInBatch = 0;
    _isSerialEffectsTest = viaSerial;

    uint8_t count_payload[3];
    count_payload[0] = CMD_GET_ALL_EFFECTS;
    count_payload[1] = (_expectedEffectsToSend >> 8) & 0xFF;
    count_payload[2] = _expectedEffectsToSend & 0xFF;

    if (viaSerial)
    {
        Serial.write(count_payload, 3);
    }
    else
    {
        BLEManager::getInstance().sendMessage(count_payload, 3);
    }

    Serial.print("-> Sent effect count: ");
    Serial.println(_expectedEffectsToSend);
    Serial.println("Now waiting for ACK to send first effect...");
    _incomingBatchState = IncomingBatchState::EXPECTING_EFFECT_ACK;
}

void BinaryCommandHandler::processIncomingAllSegmentsData(const uint8_t *data, size_t len)
{
    if (_incomingBatchState == IncomingBatchState::EXPECTING_BATCH_CONFIG_JSON)
    {
        for (size_t i = 0; i < len; i++)
        {
            _incomingJsonBuffer += (char)data[i];
        }
        if (_incomingJsonBuffer.endsWith("}]}"))
        {
            Serial.println("Batch config fully received. Parsing...");
            Serial.println(_incomingJsonBuffer);
            handleBatchConfigJson(_incomingJsonBuffer);
            _incomingBatchState = IncomingBatchState::IDLE;
            _incomingJsonBuffer = "";
        }
    }
    else if (_incomingBatchState == IncomingBatchState::EXPECTING_ALL_SEGMENTS_COUNT)
    {
        if (len >= 2)
        {
            _expectedSegmentsToReceive = (data[0] << 8) | data[1];
            Serial.print("Expected segments to receive: ");
            Serial.println(_expectedSegmentsToReceive);
            _segmentsReceivedInBatch = 0;
            _incomingBatchState = IncomingBatchState::EXPECTING_ALL_SEGMENTS_JSON;
            _incomingJsonBuffer = "";
            uint8_t ack_payload[] = {CMD_ACK};
            BLEManager::getInstance().sendMessage(ack_payload, 1);
            Serial.println("-> Sent ACK for segment count.");
        }
        else
        {
            Serial.println("ERR: Payload too short for segment count.");
            _incomingBatchState = IncomingBatchState::IDLE;
            BLEManager::getInstance().sendMessage("{\"error\":\"Invalid segment count payload\"}");
        }
    }
    else if (_incomingBatchState == IncomingBatchState::EXPECTING_ALL_SEGMENTS_JSON)
    {
        for (size_t i = 0; i < len; i++)
        {
            _incomingJsonBuffer += (char)data[i];
        }
        int startIndex = _incomingJsonBuffer.indexOf('{');
        int endIndex = _incomingJsonBuffer.lastIndexOf('}');
        if (startIndex != -1 && endIndex != -1 && endIndex > startIndex)
        {
            String jsonString = _incomingJsonBuffer.substring(startIndex, endIndex + 1);
            Serial.print("Received segment JSON: ");
            Serial.println(jsonString);
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, jsonString);
            if (error)
            {
                Serial.print("ERR: JSON parse error for segment config: ");
                Serial.println(error.c_str());
                BLEManager::getInstance().sendMessage("{\"error\":\"JSON_PARSE_ERROR_SEGMENT\"}");
                _incomingBatchState = IncomingBatchState::IDLE;
                _incomingJsonBuffer = "";
                return;
            }
            String name = doc["name"] | "";
            uint16_t start = doc["startLed"] | 0;
            uint16_t end = doc["endLed"] | 0;
            uint8_t brightness = doc["brightness"] | 255;
            String effectNameStr = doc["effect"] | "SolidColor";
            uint8_t segmentId = doc["id"] | 0;
            PixelStrip::Segment *targetSeg = nullptr;
            if (name.equalsIgnoreCase("all"))
            {
                targetSeg = strip->getSegments()[0];
                targetSeg->setRange(start, end);
            }
            else
            {
                for (auto *s : strip->getSegments())
                {
                    if (s->getId() == segmentId)
                    {
                        targetSeg = s;
                        break;
                    }
                }
                if (!targetSeg)
                {
                    strip->addSection(start, end, name);
                    targetSeg = strip->getSegments().back();
                }
                targetSeg->setRange(start, end);
            }
            if (targetSeg)
            {
                targetSeg->setBrightness(brightness);
                if (targetSeg->activeEffect)
                {
                    if (!String(targetSeg->activeEffect->getName()).equalsIgnoreCase(effectNameStr))
                    {
                        delete targetSeg->activeEffect;
                        targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);
                    }
                }
                else
                {
                    targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);
                }
                if (targetSeg->activeEffect)
                {
                    for (int i = 0; i < targetSeg->activeEffect->getParameterCount(); ++i)
                    {
                        EffectParameter *p = targetSeg->activeEffect->getParameter(i);
                        if (doc.containsKey(p->name))
                        {
                            switch (p->type)
                            {
                            case ParamType::INTEGER:
                                targetSeg->activeEffect->setParameter(p->name, doc[p->name].as<int>());
                                break;
                            case ParamType::FLOAT:
                                targetSeg->activeEffect->setParameter(p->name, doc[p->name].as<float>());
                                break;
                            case ParamType::COLOR:
                                targetSeg->activeEffect->setParameter(p->name, doc[p->name].as<uint32_t>());
                                break;
                            case ParamType::BOOLEAN:
                                targetSeg->activeEffect->setParameter(p->name, doc[p->name].as<bool>());
                                break;
                            }
                        }
                    }
                }
                Serial.print("OK: Segment ID ");
                Serial.print(targetSeg->getId());
                Serial.print(" (");
                Serial.print(targetSeg->getName());
                Serial.println(") config applied.");
            }
            else
            {
                Serial.println("ERR: Failed to find or create segment.");
            }
            _segmentsReceivedInBatch++;
            _incomingJsonBuffer.remove(startIndex, endIndex + 1 - startIndex);
            uint8_t ack_payload[] = {CMD_ACK};
            BLEManager::getInstance().sendMessage(ack_payload, 1);
            Serial.print("-> Sent ACK for segment ");
            Serial.print(_segmentsReceivedInBatch);
            Serial.println(".");
            if (_segmentsReceivedInBatch >= _expectedSegmentsToReceive)
            {
                Serial.println("OK: All segment configurations received and applied.");
                _incomingBatchState = IncomingBatchState::IDLE;
                _incomingJsonBuffer = "";
                strip->show();
            }
        }
    }
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

String BinaryCommandHandler::buildEffectInfoJson(uint8_t effectIndex)
{
    const char *effectName = getEffectNameFromId(effectIndex);
    if (!effectName || !strip || strip->getSegments().empty())
    {
        return "{\"error\":\"Invalid effect index or strip not ready\"}";
    }
    PixelStrip::Segment *dummySegment = strip->getSegments()[0];
    BaseEffect *tempEffect = createEffectByName(effectName, dummySegment);
    if (!tempEffect)
    {
        return "{\"error\":\"Failed to create temporary effect\"}";
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
    delete tempEffect;
    return response;
}

void BinaryCommandHandler::handleGetEffectInfo(const uint8_t *payload, size_t len, bool viaSerial)
{
    Serial.println("CMD: Get Effect Info");
    if (len < 2)
    {
        Serial.println("ERR: Missing segment ID or effect ID for GET_EFFECT_INFO");
        return;
    }
    uint8_t effectIndex = payload[1];
    String response = buildEffectInfoJson(effectIndex);
    Serial.print("-> Sending Effect Info for index '");
    Serial.print(effectIndex);
    Serial.print("' (");
    Serial.print(response.length());
    Serial.println(" bytes)");
    if (viaSerial)
    {
        Serial.println(response);
    }
    else
    {
        BLEManager::getInstance().sendMessage(response);
    }
}

void BinaryCommandHandler::handleSetEffectParameter(const uint8_t *payload, size_t len)
{
    Serial.println("CMD: Set Effect Parameter");
    if (len < 4)
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
    if ((size_t)3 + nameLen >= len)
    {
        Serial.println("ERR: Invalid parameter name length or missing value data.");
        BLEManager::getInstance().sendMessage("{\"error\":\"Invalid parameter data\"}");
        return;
    }
    String paramName;
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
    const uint8_t *valueBytes = payload + 3 + nameLen;
    size_t valueLen = len - (3 + nameLen);
    ParamType paramType = (ParamType)paramTypeRaw;
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
        int32_t intValue = (int32_t)valueBytes[0] << 24 | (int32_t)valueBytes[1] << 16 | (int32_t)valueBytes[2] << 8 | (int32_t)valueBytes[3];
        seg->activeEffect->setParameter(paramName.c_str(), (int)intValue);
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
        memcpy(&floatValue, valueBytes, 4);
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
        uint32_t colorValue = (uint32_t)valueBytes[1] << 16 | (uint32_t)valueBytes[2] << 8 | (uint32_t)valueBytes[3];
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

void BinaryCommandHandler::handleGetAllSegmentConfigs(bool viaSerial)
{
    Serial.println("CMD: Get All Segment Configurations");
    StaticJsonDocument<2048> doc;
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
            if (s->activeEffect)
            {
                for (int i = 0; i < s->activeEffect->getParameterCount(); ++i)
                {
                    EffectParameter *p = s->activeEffect->getParameter(i);
                    switch (p->type)
                    {
                    case ParamType::INTEGER:
                        segObj[p->name] = p->value.intValue;
                        break;
                    case ParamType::FLOAT:
                        segObj[p->name] = p->value.floatValue;
                        break;
                    case ParamType::COLOR:
                        segObj[p->name] = p->value.colorValue;
                        break;
                    case ParamType::BOOLEAN:
                        segObj[p->name] = p->value.boolValue;
                        break;
                    }
                }
            }
        }
    }
    String response;
    serializeJson(doc, response);
    if (viaSerial)
    {
        Serial.print("-> Sending All Segment Configs JSON via Serial (");
        Serial.print(response.length());
        Serial.println(" bytes)");
        Serial.println(response);
    }
    else
    {
        Serial.print("-> Sending All Segment Configs JSON via BLE (");
        Serial.print(response.length());
        Serial.println(" bytes)");
        BLEManager::getInstance().sendMessage(response);
    }
}
void BinaryCommandHandler::processSingleSegmentJson(const String &jsonString) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error)
    {
        Serial.print("ERR: JSON parse error for segment config: ");
        Serial.println(error.c_str());
        BLEManager::getInstance().sendMessage("{\"error\":\"JSON_PARSE_ERROR_SEGMENT\"}");
        return;
    }
    String name = doc["name"] | "";
    uint16_t start = doc["startLed"] | 0;
    uint16_t end = doc["endLed"] | 0;
    uint8_t brightness = doc["brightness"] | 255;
    String effectNameStr = doc["effect"] | "SolidColor";
    uint8_t segmentId = doc["id"] | 0;
    PixelStrip::Segment *targetSeg = nullptr;

    if (name.equalsIgnoreCase("all"))
    {
        targetSeg = strip->getSegments()[0];
        targetSeg->setRange(start, end);
    }
    else
    {
        // Find existing segment by ID
        for (auto *s : strip->getSegments())
        {
            if (s->getId() == segmentId)
            {
                targetSeg = s;
                break;
            }
        }
        // If not found, create a new one
        if (!targetSeg)
        {
            strip->addSection(start, end, name);
            targetSeg = strip->getSegments().back();
        } else {
            // if found, update its range
            targetSeg->setRange(start,end);
        }
    }

    if (targetSeg)
    {
        targetSeg->setBrightness(brightness);
        if (targetSeg->activeEffect)
        {
            if (!String(targetSeg->activeEffect->getName()).equalsIgnoreCase(effectNameStr))
            {
                delete targetSeg->activeEffect;
                targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);
            }
        }
        else
        {
            targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);
        }

        if (targetSeg->activeEffect)
        {
            JsonArray params = doc["params"];
            for (JsonObject param_data : params)
            {
                const char* paramName = param_data["name"];
                if (paramName)
                {
                    // Find the parameter in the effect
                    for(int i = 0; i < targetSeg->activeEffect->getParameterCount(); i++)
                    {
                        EffectParameter* p = targetSeg->activeEffect->getParameter(i);
                        if (strcmp(p->name, paramName) == 0)
                        {
                             switch (p->type)
                            {
                            case ParamType::INTEGER:
                                targetSeg->activeEffect->setParameter(p->name, param_data["value"].as<int>());
                                break;
                            case ParamType::FLOAT:
                                targetSeg->activeEffect->setParameter(p->name, param_data["value"].as<float>());
                                break;
                            case ParamType::COLOR:
                                targetSeg->activeEffect->setParameter(p->name, param_data["value"].as<uint32_t>());
                                break;
                            case ParamType::BOOLEAN:
                                targetSeg->activeEffect->setParameter(p->name, param_data["value"].as<bool>());
                                break;
                            }
                        }
                    }
                }
            }
        }
        Serial.print("OK: Segment ID ");
        Serial.print(targetSeg->getId());
        Serial.print(" (");
        Serial.print(targetSeg->getName());
        Serial.println(") config applied.");
    }
    else
    {
        Serial.println("ERR: Failed to find or create segment.");
    }
    strip->show();
}
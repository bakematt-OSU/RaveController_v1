#include "BinaryCommandHandler.h"
#include "globals.h"
#include "EffectLookup.h"
#include "ConfigManager.h"
#include "BLEManager.h"
#include <ArduinoJson.h>

extern PixelStrip *strip;
extern uint16_t LED_COUNT;

// --- Constructor ---
BinaryCommandHandler::BinaryCommandHandler()
    : _jsonBufferIndex(0),
      _outgoingSequence(0),
      _lastAckedSequence(0),
      _expectedIncomingSequence(0),
      _isWaitingForAck(false)
{
    memset(_incomingJsonBuffer, 0, sizeof(_incomingJsonBuffer));
}


// --- Reliable Message Sending Logic ---
void BinaryCommandHandler::sendReliableMessage(const uint8_t *data, size_t len)
{
    size_t data_offset = 0;
    while (data_offset < len)
    {
        BlePacket packet;
        packet.sequence = _outgoingSequence++;
        packet.flags = 0;

        if (data_offset == 0)
        {
            packet.flags |= FLAG_START_OF_MESSAGE;
        }

        size_t chunkSize = min((size_t)PACKET_PAYLOAD_SIZE, len - data_offset);
        memcpy(packet.payload, data + data_offset, chunkSize);
        packet.payloadSize = chunkSize;
        data_offset += chunkSize;

        if (data_offset >= len)
        {
            packet.flags |= FLAG_END_OF_MESSAGE;
        }

        _outgoingPacketQueue.push_back(packet);
    }
}


// --- Main Command and Packet Handling Logic ---
void BinaryCommandHandler::handleCommand(const uint8_t *data, size_t len)
{
    if (len < 2)
    {
        // This can happen if an empty ACK is received, it's not an error.
        return;
    }

    uint8_t sequence = data[0];
    uint8_t flags = data[1];

    // Handle incoming ACKs for packets we've sent
    if (flags & FLAG_ACK)
    {
        if (_isWaitingForAck && !_outgoingPacketQueue.empty() && sequence == _outgoingPacketQueue.front().sequence)
        {
            _isWaitingForAck = false;
            _lastAckedSequence = sequence;
            _outgoingPacketQueue.erase(_outgoingPacketQueue.begin());
        }
        return;
    }

    // If it's a data packet, send an ACK back immediately
    uint8_t ack_payload[2] = {sequence, FLAG_ACK};
    BLEManager::getInstance().sendMessage(ack_payload, 2);

    // Ignore out-of-order packets
    if (sequence != _expectedIncomingSequence)
    {
        return;
    }
    _expectedIncomingSequence++;

    // If this is the first packet of a message, clear our assembly buffer
    if (flags & FLAG_START_OF_MESSAGE) {
        _jsonBufferIndex = 0;
        memset(_incomingJsonBuffer, 0, sizeof(_incomingJsonBuffer));
    }

    // Append the new data to our assembly buffer
    size_t payloadLen = len - 2;
    if (_jsonBufferIndex + payloadLen >= sizeof(_incomingJsonBuffer))
    {
        Serial.println("ERR: JSON buffer overflow! Message discarded.");
        _jsonBufferIndex = 0;
        return;
    }
    memcpy(_incomingJsonBuffer + _jsonBufferIndex, data + 2, payloadLen);
    _jsonBufferIndex += payloadLen;

    // If this is the last packet, process the fully assembled command
    if (flags & FLAG_END_OF_MESSAGE)
    {
        BleCommand cmd = (BleCommand)_incomingJsonBuffer[0];
        
        switch (cmd) {
            case CMD_SET_ALL_SEGMENT_CONFIGS:
                _incomingJsonBuffer[_jsonBufferIndex] = '\0';
                handleBatchConfigJson(_incomingJsonBuffer + 1);
                break;
            case CMD_GET_LED_COUNT:
                handleGetLedCount();
                break;
            case CMD_GET_ALL_EFFECTS:
                handleGetAllEffectsCommand(false);
                break;
            case CMD_GET_ALL_SEGMENT_CONFIGS:
                handleGetAllSegmentConfigs(false);
                break;
            case CMD_SAVE_CONFIG:
                handleSaveConfig();
                break;
            case CMD_CLEAR_SEGMENTS:
                handleClearSegments();
                break;
            default:
                Serial.println("ERR: Unknown command in packet.");
        }
        // Reset for the next message
        _jsonBufferIndex = 0;
    }
}


// --- Update loop (sends queued messages) ---
void BinaryCommandHandler::update()
{
    if (_isWaitingForAck && (millis() - _ackTimeoutStart > ACK_WAIT_TIMEOUT_MS))
    {
        _isWaitingForAck = false; // Allow the packet to be resent
    }

    if (!_isWaitingForAck && !_outgoingPacketQueue.empty())
    {
        BlePacket &packet = _outgoingPacketQueue.front();

        std::vector<uint8_t> raw_packet;
        raw_packet.push_back(packet.sequence);
        raw_packet.push_back(packet.flags);
        raw_packet.insert(raw_packet.end(), packet.payload, packet.payload + packet.payloadSize);

        BLEManager::getInstance().sendMessage(raw_packet.data(), raw_packet.size());

        _isWaitingForAck = true;
        _ackTimeoutStart = millis();
    }
}

// --- Reset handler (called on new BLE connection) ---
void BinaryCommandHandler::reset()
{
    Serial.println("INFO: Resetting BinaryCommandHandler sequence numbers.");
    _outgoingPacketQueue.clear();
    _outgoingSequence = 0;
    _expectedIncomingSequence = 0;
    _isWaitingForAck = false;
    _jsonBufferIndex = 0;
    memset(_incomingJsonBuffer, 0, sizeof(_incomingJsonBuffer));
}


// --- Command Handlers (The rest of the file is unchanged) ---

void BinaryCommandHandler::handleGetLedCount()
{
    StaticJsonDocument<32> doc;
    doc["led_count"] = LED_COUNT;
    String response;
    serializeJson(doc, response);
    sendReliableMessage((const uint8_t *)response.c_str(), response.length());
}

void BinaryCommandHandler::handleGetAllEffectsCommand(bool viaSerial)
{
    if (viaSerial) return;
    StaticJsonDocument<4096> doc;
    JsonArray effectsArray = doc.createNestedArray("effects");
    for (int i = 0; i < EFFECT_COUNT; ++i)
    {
        String effectJsonStr = buildEffectInfoJson(i);
        StaticJsonDocument<1024> effectDoc;
        deserializeJson(effectDoc, effectJsonStr);
        effectsArray.add(effectDoc.as<JsonObject>());
    }
    String response;
    serializeJson(doc, response);
    sendReliableMessage((const uint8_t *)response.c_str(), response.length());
}

void BinaryCommandHandler::handleGetAllSegmentConfigs(bool viaSerial)
{
    if (viaSerial) return;
    StaticJsonDocument<4096> doc;
    JsonArray segmentsArray = doc.createNestedArray("segments");
    if (strip)
    {
        for (size_t i = 0; i < strip->getSegments().size(); ++i)
        {
            String segmentJsonStr = buildSegmentInfoJson(i);
            StaticJsonDocument<1024> segmentDoc;
            deserializeJson(segmentDoc, segmentJsonStr);
            segmentsArray.add(segmentDoc.as<JsonObject>());
        }
    }
    String response;
    serializeJson(doc, response);
    sendReliableMessage((const uint8_t *)response.c_str(), response.length());
}

void BinaryCommandHandler::handleSaveConfig()
{
    String response = saveConfig() ? "{\"status\":\"OK\"}" : "{\"error\":\"Save failed\"}";
    sendReliableMessage((const uint8_t *)response.c_str(), response.length());
}

void BinaryCommandHandler::handleClearSegments()
{
    if (strip)
    {
        strip->clearUserSegments();
        sendReliableMessage((const uint8_t *)"{\"status\":\"OK\"}", 15);
    }
}

void BinaryCommandHandler::processSingleSegmentJson(const char *jsonString)
{
    handleBatchConfigJson(jsonString);
}


// --- Helper Functions (JSON builders) ---

String BinaryCommandHandler::buildEffectInfoJson(uint8_t effectIndex)
{
    const char *effectName = getEffectNameFromId(effectIndex);
    if (!effectName || !strip || strip->getSegments().empty()) return "{}";
    
    PixelStrip::Segment *dummySegment = new PixelStrip::Segment(*strip, 0, 0, "dummy", 255);
    BaseEffect *tempEffect = createEffectByName(effectName, dummySegment);
    if (!tempEffect) {
        delete dummySegment;
        return "{}";
    }

    StaticJsonDocument<1024> doc;
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
    delete dummySegment;
    return response;
}

String BinaryCommandHandler::buildSegmentInfoJson(uint8_t segmentIndex)
{
    if (!strip || segmentIndex >= strip->getSegments().size()) return "{}";
    
    PixelStrip::Segment *s = strip->getSegments()[segmentIndex];
    StaticJsonDocument<1024> doc;
    doc["id"] = s->getId();
    doc["name"] = s->getName();
    doc["startLed"] = s->startIndex();
    doc["endLed"] = s->endIndex();
    doc["brightness"] = s->getBrightness();
    if (s->activeEffect)
    {
        doc["effect"] = s->activeEffect->getName();
        for (int i = 0; i < s->activeEffect->getParameterCount(); ++i)
        {
            EffectParameter *p = s->activeEffect->getParameter(i);
            switch (p->type)
            {
            case ParamType::INTEGER:
                doc[p->name] = p->value.intValue;
                break;
            case ParamType::FLOAT:
                doc[p->name] = p->value.floatValue;
                break;
            case ParamType::COLOR:
                doc[p->name] = p->value.colorValue;
                break;
            case ParamType::BOOLEAN:
                doc[p->name] = p->value.boolValue;
                break;
            }
        }
    }
    else
    {
        doc["effect"] = "None";
    }
    String response;
    serializeJson(doc, response);
    return response;
}
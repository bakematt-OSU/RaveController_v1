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
    : _incomingBatchState(IncomingBatchState::IDLE),
      _jsonBufferIndex(0), // Initialize buffer index
      _isSerialEffectsTest(false),
      _isSerialBatch(false),
      _ackReceived(false),
      _ackTimeoutStart(0),
      _expectedSegmentsToReceive(0),
      _segmentsReceivedInBatch(0),
      _expectedEffectsToSend(0),
      _effectsSentInBatch(0),
      _expectedSegmentsToSend_Out(0), // NEW
      _segmentsSentInBatch_Out(0)     // NEW
{
    // Ensure the buffer is cleared on startup
    memset(_incomingJsonBuffer, 0, sizeof(_incomingJsonBuffer));
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
        // Use CMD_ACK_GENERIC
        bool isAck = (data[0] == (uint8_t)CMD_ACK_GENERIC);
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
                    // Add a small delay here to ensure the previous write completes fully
                    // This is a common workaround for some BLE stacks that might have internal buffering delays
                    delay(5); // Add this line
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
                    _ackReceived = false;        // Reset ACK for the next expected ACK
                    _ackTimeoutStart = millis(); // Restart timeout for the next ACK
                }
            }
        }
        return;
    }

    // NEW: Handle ACK for the get all segments command
    if (_incomingBatchState == IncomingBatchState::EXPECTING_SEGMENT_ACK)
    {
        bool isAck = (data[0] == (uint8_t)CMD_ACK_GENERIC);
        if (isAck)
        {
            handleAck(); // Call general ACK handler
            if (_segmentsSentInBatch_Out < _expectedSegmentsToSend_Out)
            {
                // Send next segment
                String segmentJson = buildSegmentInfoJson(_segmentsSentInBatch_Out);
                // Decide if serial or BLE, based on _isSerialBatch
                if (_isSerialBatch)
                {
                    Serial.println(segmentJson);
                }
                else
                {
                    BLEManager::getInstance().sendMessage(segmentJson);
                    delay(5); // Add this line for segment JSONs too
                }
                _segmentsSentInBatch_Out++;
                if (_segmentsSentInBatch_Out >= _expectedSegmentsToSend_Out)
                {
                    Serial.println("OK: All segments sent.");
                    _incomingBatchState = IncomingBatchState::IDLE;
                    _isSerialBatch = false; // Reset the flag
                }
                else
                {
                    Serial.print("Now waiting for ACK to send segment ");
                    Serial.print(_segmentsSentInBatch_Out);
                    Serial.println("...");
                    _ackReceived = false;        // Reset ACK for the next expected ACK
                    _ackTimeoutStart = millis(); // Restart timeout for the next ACK
                }
            }
        }
        return;
    }

    // Handle multi-part commands related to segment configuration
    if (_incomingBatchState == IncomingBatchState::EXPECTING_ALL_SEGMENTS_COUNT ||
        _incomingBatchState == IncomingBatchState::EXPECTING_ALL_SEGMENTS_JSON)
    {
        processIncomingAllSegmentsData(data, len);
        return;
    }

    // Process new commands
    BleCommand cmd = (BleCommand)data[0];
    const uint8_t *payload = data + 1;
    size_t payloadLen = len - 1;
    bool sendGenericAck = true; // Default to sending ACK unless specified otherwise

    switch (cmd)
    {
    case CMD_SAVE_CONFIG:
        handleSaveConfig();
        sendGenericAck = false; // Handled by saveConfig
        break;
    case CMD_GET_LED_COUNT:
        handleGetLedCount();
        sendGenericAck = false; // Handled by getLedCount
        break;
    case CMD_GET_ALL_SEGMENT_CONFIGS:
        handleGetAllSegmentConfigs(false); // False for BLE
        sendGenericAck = false; // Handled by getAllSegmentConfigs
        break;
    case CMD_SET_ALL_SEGMENT_CONFIGS:
        handleSetAllSegmentConfigsCommand(false); // False for BLE
        sendGenericAck = false; // Handled by setAllSegmentConfigsCommand
        break;
    case CMD_GET_ALL_EFFECTS:
        handleGetAllEffectsCommand(false); // False for BLE
        sendGenericAck = false; // Handled by getAllEffectsCommand
        break;
    case CMD_ACK_GENERIC: // Use CMD_ACK_GENERIC
        handleAck();
        sendGenericAck = false; // This is an ACK, not a command to ACK
        break;
    case CMD_READY: // CMD_READY is an indicator, not a command to be actively handled with a function call here.
        Serial.println("CMD: Device Ready received.");
        sendGenericAck = false; // No ACK needed for a READY signal
        break;
    default:
        Serial.print("ERR: Unknown binary command: 0x");
        Serial.println(cmd, HEX);
        sendGenericAck = false; // Unknown command, no ACK
        break;
    }

    if (sendGenericAck)
    {
        uint8_t ack_payload[] = {(uint8_t)CMD_ACK_GENERIC}; // Use CMD_ACK_GENERIC
        BLEManager::getInstance().sendMessage(ack_payload, 1);
        Serial.println("-> Sent Generic ACK");
    }
}

void BinaryCommandHandler::handleSaveConfig()
{
    Serial.println("CMD: Save Config");
    if (saveConfig())
    {
        Serial.println("-> OK: Config saved.");
        BLEManager::getInstance().sendMessage("{\"status\":\"OK\", \"message\":\"Config saved\"}");
    }
    else
    {
        Serial.println("-> ERR: Failed to save config.");
        BLEManager::getInstance().sendMessage("{\"error\":\"Failed to save config\"}");
    }
}

void BinaryCommandHandler::handleAck()
{
    Serial.println("<- Received ACK from app.");
    _ackReceived = true;
}

void BinaryCommandHandler::handleSetAllSegmentConfigsCommand(bool viaSerial)
{
    _isSerialBatch = viaSerial; // Set the flag
    Serial.println("CMD: Set All Segment Configurations - Initiated.");
    if (strip)
    {
        strip->clearUserSegments();
        Serial.println("OK: Cleared existing user segments.");
    }
    _incomingBatchState = IncomingBatchState::EXPECTING_ALL_SEGMENTS_COUNT;
    _jsonBufferIndex = 0;
    memset(_incomingJsonBuffer, 0, sizeof(_incomingJsonBuffer));
    _expectedSegmentsToReceive = 0;
    _segmentsReceivedInBatch = 0;
    uint8_t ack_payload[] = {(uint8_t)CMD_ACK_GENERIC}; // Use CMD_ACK_GENERIC
    BLEManager::getInstance().sendMessage(ack_payload, 1);
    Serial.println("-> Sent ACK for CMD_SET_ALL_SEGMENT_CONFIGS initiation.");
}

void BinaryCommandHandler::handleGetAllEffectsCommand(bool viaSerial)
{
    _isSerialBatch = viaSerial;
    if (!viaSerial)
    {
        Serial.println("CMD: Get All Effects - Initiated.");
    }
    _expectedEffectsToSend = EFFECT_COUNT;
    _effectsSentInBatch = 0;
    _isSerialEffectsTest = viaSerial;

    uint8_t count_payload[3];
    count_payload[0] = (uint8_t)CMD_GET_ALL_EFFECTS; // Cast to uint8_t
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
    _ackReceived = false;        // Set to false to wait for the first ACK
    _ackTimeoutStart = millis(); // Start timeout for the first ACK
}

void BinaryCommandHandler::processIncomingAllSegmentsData(const uint8_t *data, size_t len)
{
    // Append new data to the buffer, checking for overflow
    if (_jsonBufferIndex + len >= sizeof(_incomingJsonBuffer))
    {
        Serial.println("ERR: JSON buffer overflow!");
        _incomingBatchState = IncomingBatchState::IDLE;
        _jsonBufferIndex = 0;
        memset(_incomingJsonBuffer, 0, sizeof(_incomingJsonBuffer));
        return;
    }
    memcpy(_incomingJsonBuffer + _jsonBufferIndex, data, len);
    _jsonBufferIndex += len;
    _incomingJsonBuffer[_jsonBufferIndex] = '\0'; // Ensure null termination

    if (_incomingBatchState == IncomingBatchState::EXPECTING_ALL_SEGMENTS_COUNT)
    {
        // Expecting 2 bytes for count
        if (_jsonBufferIndex >= 2)
        {
            _expectedSegmentsToReceive = (_incomingJsonBuffer[0] << 8) | _incomingJsonBuffer[1];
            Serial.print("Expected segments to receive: ");
            Serial.println(_expectedSegmentsToReceive);
            _segmentsReceivedInBatch = 0;
            _incomingBatchState = IncomingBatchState::EXPECTING_ALL_SEGMENTS_JSON;
            _jsonBufferIndex = 0; // Clear buffer for incoming JSON
            memset(_incomingJsonBuffer, 0, sizeof(_incomingJsonBuffer));
            uint8_t ack_payload[] = {(uint8_t)CMD_ACK_GENERIC}; // Use CMD_ACK_GENERIC
            BLEManager::getInstance().sendMessage(ack_payload, 1);
            Serial.println("-> Sent ACK for segment count.");
        }
    }
    else if (_incomingBatchState == IncomingBatchState::EXPECTING_ALL_SEGMENTS_JSON)
    {
        // Look for a complete JSON object in the buffer
        char *start = strchr(_incomingJsonBuffer, '{');
        char *end = strrchr(_incomingJsonBuffer, '}');

        if (start && end && end > start)
        {
            // Temporarily null-terminate the JSON string for parsing
            char original_char_after_end = *(end + 1);
            *(end + 1) = '\0';

            processSingleSegmentJson(start);

            // Restore the original character
            *(end + 1) = original_char_after_end;

            // Shift remaining data in the buffer
            size_t processed_len = (end - _incomingJsonBuffer) + 1;
            size_t remaining_len = _jsonBufferIndex - processed_len;
            memmove(_incomingJsonBuffer, end + 1, remaining_len);
            _jsonBufferIndex = remaining_len;
            _incomingJsonBuffer[_jsonBufferIndex] = '\0'; // Ensure null termination

            _segmentsReceivedInBatch++;

            uint8_t ack_payload[] = {(uint8_t)CMD_ACK_GENERIC}; // Use CMD_ACK_GENERIC
            BLEManager::getInstance().sendMessage(ack_payload, 1);
            Serial.print("-> Sent ACK for segment ");
            Serial.print(_segmentsReceivedInBatch);
            Serial.println(".");

            if (_segmentsReceivedInBatch >= _expectedSegmentsToReceive)
            {
                Serial.println("OK: All segment configurations received and applied.");
                _incomingBatchState = IncomingBatchState::IDLE;
                _jsonBufferIndex = 0;
                memset(_incomingJsonBuffer, 0, sizeof(_incomingJsonBuffer));
                strip->show(); // Update strip after all segments are applied
            }
        }
    }
}

IncomingBatchState BinaryCommandHandler::getIncomingBatchState() const
{
    return _incomingBatchState;
}

bool BinaryCommandHandler::isSerialBatchActive() const
{
    return _isSerialBatch;
}

// Implementation of the update method to handle timeouts for ACKs
void BinaryCommandHandler::update()
{
    if (_incomingBatchState == IncomingBatchState::EXPECTING_EFFECT_ACK)
    {
        if (!_ackReceived && (millis() - _ackTimeoutStart > ACK_WAIT_TIMEOUT_MS))
        {
            Serial.println("WARN: ACK timeout reached while expecting effect ACK. Resetting batch state.");
            // Reset state after timeout
            _incomingBatchState = IncomingBatchState::IDLE;
            _effectsSentInBatch = 0;
            _expectedEffectsToSend = 0;
            _isSerialEffectsTest = false;
        }
    }
    else if (_incomingBatchState == IncomingBatchState::EXPECTING_SEGMENT_ACK)
    { // NEW BLOCK
        if (!_ackReceived && (millis() - _ackTimeoutStart > ACK_WAIT_TIMEOUT_MS))
        {
            Serial.println("WARN: ACK timeout reached while expecting segment ACK. Resetting batch state.");
            // Reset state after timeout for segments
            _incomingBatchState = IncomingBatchState::IDLE;
            _segmentsSentInBatch_Out = 0;
            _expectedSegmentsToSend_Out = 0;
            _isSerialBatch = false;
        }
    }
}

// NEW: buildSegmentInfoJson function
String BinaryCommandHandler::buildSegmentInfoJson(uint8_t segmentIndex)
{
    if (!strip || segmentIndex >= strip->getSegments().size())
    {
        return "{\"error\":\"Invalid segment index or strip not ready\"}";
    }

    PixelStrip::Segment *s = strip->getSegments()[segmentIndex];

    StaticJsonDocument<512> doc; // Adjust size if segments can be very large
    JsonObject segObj = doc.to<JsonObject>();
    segObj["id"] = s->getId();
    segObj["name"] = s->getName();
    segObj["startLed"] = s->startIndex();
    segObj["endLed"] = s->endIndex();
    segObj["brightness"] = s->getBrightness();

    if (s->activeEffect)
    {
        segObj["effect"] = s->activeEffect->getName();
        // Add parameters to the top level of the segment JSON
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
    else
    {
        segObj["effect"] = "None";
    }

    String response;
    serializeJson(doc, response);
    return response;
}

void BinaryCommandHandler::handleGetAllSegmentConfigs(bool viaSerial)
{
    _isSerialBatch = viaSerial;
    if (!viaSerial)
    {
        Serial.println("CMD: Get All Segment Configurations - Initiated.");
    }

    _expectedSegmentsToSend_Out = strip ? strip->getSegments().size() : 0;
    _segmentsSentInBatch_Out = 0;

    // Send count of segments first
    uint8_t count_payload[3];
    count_payload[0] = (uint8_t)CMD_GET_ALL_SEGMENT_CONFIGS; // Command for batch segment config
    count_payload[1] = (_expectedSegmentsToSend_Out >> 8) & 0xFF; // MSB
    count_payload[2] = _expectedSegmentsToSend_Out & 0xFF;       // LSB

    if (viaSerial)
    {
        Serial.write(count_payload, 3);
    }
    else
    {
        BLEManager::getInstance().sendMessage(count_payload, 3);
    }

    Serial.print("-> Sent segment count: ");
    Serial.println(_expectedSegmentsToSend_Out);

    if (_expectedSegmentsToSend_Out > 0)
    {
        Serial.println("Now waiting for ACK to send first segment...");
        _incomingBatchState = IncomingBatchState::EXPECTING_SEGMENT_ACK; // Transition to waiting for ACK
        _ackReceived = false;                                            // Set to false to wait for the first ACK
        _ackTimeoutStart = millis();                                     // Start timeout for the first ACK

        // Send the first segment immediately after sending the count, if any segments exist
        String segmentJson = buildSegmentInfoJson(_segmentsSentInBatch_Out);
        if (_isSerialBatch)
        {
            Serial.println(segmentJson);
        }
        else
        {
            BLEManager::getInstance().sendMessage(segmentJson);
        }
        _segmentsSentInBatch_Out++;
    }
    else
    {
        Serial.println("OK: No segments to send.");
        _incomingBatchState = IncomingBatchState::IDLE; // No segments, go back to IDLE
    }
}

void BinaryCommandHandler::handleGetLedCount()
{
    Serial.println("CMD: Get LED Count");
    uint8_t response[3];
    response[0] = (uint8_t)CMD_GET_LED_COUNT;
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
    // Create a dummy segment to instantiate the effect and get its parameters
    // This is a temporary object and will be deleted.
    PixelStrip::Segment *dummySegment = new PixelStrip::Segment(*strip, 0, 0, "dummy", 255); 
    BaseEffect *tempEffect = createEffectByName(effectName, dummySegment);
    if (!tempEffect)
    {
        delete dummySegment; // Clean up dummy segment
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
    delete tempEffect;    // Clean up temporary effect
    delete dummySegment; // Clean up dummy segment
    return response;
}

// processSingleSegmentJson is kept because it's a public method in the provided header
// and is called within the batch segment configuration flow.
void BinaryCommandHandler::processSingleSegmentJson(const char *jsonString)
{
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error)
    {
        Serial.print("ERR: JSON parse error for segment config: ");
        Serial.println(error.c_str());
        BLEManager::getInstance().sendMessage("{\"error\":\"JSON_PARSE_ERROR_SEGMENT\"}");
        return;
    }
    const char *name = doc["name"] | "";
    uint16_t start = doc["startLed"] | 0;
    uint16_t end = doc["endLed"] | 0;
    uint8_t brightness = doc["brightness"] | 255;
    const char *effectNameStr = doc["effect"] | "SolidColor";
    uint8_t segmentId = doc["id"] | 0;
    PixelStrip::Segment *targetSeg = nullptr;

    // Find existing segment by ID or create a new one
    bool segmentFound = false;
    if (strip) {
        for (auto *s : strip->getSegments()) {
            if (s->getId() == segmentId) {
                targetSeg = s;
                segmentFound = true;
                break;
            }
        }
    }

    if (!segmentFound) {
        // If segment not found by ID, add a new one
        if (strip) {
            strip->addSection(start, end, name);
            targetSeg = strip->getSegments().back();
            targetSeg->setRange(start, end); // Ensure range is set for new segment
        }
    } else {
        // If segment found, update its range
        targetSeg->setRange(start, end);
    }

    if (targetSeg)
    {
        targetSeg->setBrightness(brightness);
        if (targetSeg->activeEffect)
        {
            if (strcmp(targetSeg->activeEffect->getName(), effectNameStr) != 0)
            {
                delete targetSeg->activeEffect;
                targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);
            }
        }
        else
        {
            targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);
        }

        // This block now correctly parses parameters from the top level of the JSON object.
        if (targetSeg->activeEffect)
        {
            JsonObject docObj = doc.as<JsonObject>();
            for (int i = 0; i < targetSeg->activeEffect->getParameterCount(); ++i)
            {
                EffectParameter *p = targetSeg->activeEffect->getParameter(i);
                if (docObj.containsKey(p->name))
                {
                    switch (p->type)
                    {
                    case ParamType::INTEGER:
                        targetSeg->activeEffect->setParameter(p->name, docObj[p->name].as<int>());
                        break;
                    case ParamType::FLOAT:
                        targetSeg->activeEffect->setParameter(p->name, docObj[p->name].as<float>());
                        break;
                    case ParamType::COLOR:
                        targetSeg->activeEffect->setParameter(p->name, docObj[p->name].as<uint32_t>());
                        break;
                    case ParamType::BOOLEAN:
                        targetSeg->activeEffect->setParameter(p->name, docObj[p->name].as<bool>());
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
    // strip->show() is called after all segments are received in processIncomingAllSegmentsData
}

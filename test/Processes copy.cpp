#include "Processes.h"
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <math.h>
#include <PDM.h>
#include "Triggers.h"
#include <LittleFS_Mbed_RP2040.h>
#include <stdio.h>
#include "effects/Effects.h" // The critical include for the EFFECT_LIST macro

// --- External globals defined in main.cpp ---
extern volatile int16_t sampleBuffer[];
extern volatile int samplesRead;
extern float accelX, accelY, accelZ;
extern volatile bool triggerRipple;
extern unsigned long lastStepTime;
extern bool debugAccel;
extern PixelStrip strip;
extern PixelStrip::Segment *seg;
extern AudioTrigger<SAMPLES> audioTrigger;
extern HeartbeatColor hbColor;
extern unsigned long lastHbChange;
extern uint8_t activeR, activeG, activeB;
extern BLEService ledService;
extern BLECharacteristic cmdCharacteristic;

// --- Static variables ---
static BLEDevice connectedCentral;
static String jsonBuffer = "";
static bool isReceivingBatch = false;
static bool isSendingMultiPart = false;
static const unsigned long HEARTBEAT_INTERVAL = 1000;
static unsigned long lastHeartbeatTime = 0;

// --- Binary command IDs for BLE/Android control ---
static constexpr uint8_t CMD_SET_COLOR = 0x01;
static constexpr uint8_t CMD_SET_EFFECT = 0x02;
static constexpr uint8_t CMD_SET_BRIGHTNESS = 0x03;
static constexpr uint8_t CMD_SET_SEG_BRIGHT = 0x04;
static constexpr uint8_t CMD_SELECT_SEGMENT = 0x05;
static constexpr uint8_t CMD_CLEAR_SEGMENTS = 0x06;
static constexpr uint8_t CMD_SET_SEG_RANGE = 0x07;
static constexpr uint8_t CMD_GET_STATUS = 0x08;
static constexpr uint8_t CMD_BATCH_CONFIG = 0x09;
static constexpr uint8_t CMD_NUM_PIXELS = 0x0A;
static constexpr uint8_t CMD_GET_EFFECT_INFO = 0x0B;
static constexpr uint8_t CMD_ACK = 0xA0;

const char *getBLECmdName(uint8_t cmd)
{
    switch (cmd)
    {
    case CMD_SET_COLOR:
        return "CMD_SET_COLOR";
    case CMD_SET_EFFECT:
        return "CMD_SET_EFFECT";
    case CMD_SET_BRIGHTNESS:
        return "CMD_SET_BRIGHTNESS";
    case CMD_SET_SEG_BRIGHT:
        return "CMD_SET_SEG_BRIGHT";
    case CMD_SELECT_SEGMENT:
        return "CMD_SELECT_SEGMENT";
    case CMD_CLEAR_SEGMENTS:
        return "CMD_CLEAR_SEGMENTS";
    case CMD_SET_SEG_RANGE:
        return "CMD_SET_SEG_RANGE";
    case CMD_GET_STATUS:
        return "CMD_GET_STATUS";
    case CMD_BATCH_CONFIG:
        return "CMD_BATCH_CONFIG";
    case CMD_NUM_PIXELS:
        return "CMD_NUM_PIXELS";
    case CMD_GET_EFFECT_INFO:
        return "CMD_GET_EFFECT_INFO";
    case CMD_ACK:
        return "CMD_ACK";
    default:
        return "UNKNOWN_CMD";
    }
}

/**
 * @brief Creates an effect instance based on its string name using the EFFECT_LIST macro.
 */
BaseEffect *createEffectByName(const String &name, PixelStrip::Segment *seg)
{
#define CREATE_EFFECT_IF_MATCH(effectName, className) \
    if (name.equalsIgnoreCase(#effectName))           \
    {                                                 \
        return new className(seg);                    \
    }
    EFFECT_LIST(CREATE_EFFECT_IF_MATCH)
#undef CREATE_EFFECT_IF_MATCH
    return nullptr;
}

PixelStrip::Segment *findSegmentByIndex(const String &args, String &remainingArgs)
{
    int d = args.indexOf(' ');
    if (d < 0)
    {
        remainingArgs = "";
        return nullptr;
    }
    int idx = args.substring(0, d).toInt();
    remainingArgs = args.substring(d + 1);
    auto &segments = strip.getSegments();
    if (idx < 0 || (size_t)idx >= segments.size())
    {
        Serial.println("Invalid segment index");
        return nullptr;
    }
    return segments[idx];
}

void handleBatchConfigJson(const String &json)
{
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error)
    {
        Serial.print("BatchConfig: JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }
    strip.clearUserSegments();
    if (doc.containsKey("segments"))
    {
        PixelStrip::Segment *allSegment = strip.getSegments()[0];
        for (auto item : doc["segments"].as<JsonArray>())
        {
            String name = item["name"] | "";
            if (name.equalsIgnoreCase("all"))
            {
                if (item.containsKey("brightness"))
                    allSegment->setBrightness(item["brightness"]);
                if (item.containsKey("effect"))
                {
                    String effectName = item["effect"].as<const char *>();
                    if (allSegment->activeEffect)
                        delete allSegment->activeEffect;
                    allSegment->activeEffect = createEffectByName(effectName, allSegment);
                }
            }
            else
            {
                uint16_t start = item["startLed"];
                uint16_t end = item["endLed"];
                strip.addSection(start, end, name);
                PixelStrip::Segment *newSeg = strip.getSegments().back();
                if (item.containsKey("brightness"))
                    newSeg->setBrightness(item["brightness"]);
                if (item.containsKey("effect"))
                {
                    String effectName = item["effect"].as<const char *>();
                    newSeg->activeEffect = createEffectByName(effectName, newSeg);
                }
            }
        }
    }
    strip.show();
    Serial.println("Batch configuration applied");
}

/**
 * @brief Main command handler with hybrid JSON and plain-text parsing.
 */
void handleCommandLine(const String &line)
{
    String cmdLine = line;
    cmdLine.trim();
    if (cmdLine.isEmpty())
        return;

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, cmdLine);

    if (error == DeserializationError::Ok)
    {
        if (doc.containsKey("get_parameters"))
        {
            String effectName = doc["get_parameters"];
            BaseEffect *tempEffect = createEffectByName(effectName, nullptr);
            if (tempEffect)
            {
                StaticJsonDocument<512> outDoc;
                outDoc["effect"] = tempEffect->getName();
                JsonArray params = outDoc.createNestedArray("params");
                for (int i = 0; i < tempEffect->getParameterCount(); ++i)
                {
                    EffectParameter *p = tempEffect->getParameter(i);
                    JsonObject pj = params.createNestedObject();
                    pj["name"] = p->name;
                    switch (p->type)
                    {
                    case ParamType::INTEGER:
                        pj["type"] = "integer";
                        break;
                    case ParamType::FLOAT:
                        pj["type"] = "float";
                        break;
                    case ParamType::COLOR:
                        pj["type"] = "color";
                        break;
                    case ParamType::BOOLEAN:
                        pj["type"] = "boolean";
                        break;
                    }
                    switch (p->type)
                    {
                    case ParamType::INTEGER:
                        pj["value"] = p->value.intValue;
                        break;
                    case ParamType::FLOAT:
                        pj["value"] = p->value.floatValue;
                        break;
                    case ParamType::BOOLEAN:
                        pj["value"] = p->value.boolValue;
                        break;
                    case ParamType::COLOR:
                        pj["value"] = p->value.colorValue;
                        break;
                    }
                    pj["min"] = p->min_val;
                    pj["max"] = p->max_val;
                }
                String out;
                serializeJson(outDoc, out);
                Serial.println(out);
                delete tempEffect;
            }
            else
            {
                Serial.println("ERR: Unknown effect");
            }
            return;
        }

        if (doc.containsKey("set_parameter"))
        {
            JsonObject data = doc["set_parameter"];
            int segIdx = data["segment_id"];
            String effectName = data["effect"];
            String paramName = data["name"];
            auto &segments = strip.getSegments();
            if (segIdx >= 0 && (size_t)segIdx < segments.size())
            {
                BaseEffect *effect = segments[segIdx]->activeEffect;
                if (effect && effectName.equalsIgnoreCase(effect->getName()))
                {
                    JsonVariant val = data["value"];
                    if (val.is<int>())
                        effect->setParameter(paramName.c_str(), val.as<int>());
                    else if (val.is<float>())
                        effect->setParameter(paramName.c_str(), val.as<float>());
                    else if (val.is<bool>())
                        effect->setParameter(paramName.c_str(), val.as<bool>());
                    else if (val.is<const char *>())
                        effect->setParameter(paramName.c_str(), (uint32_t)strtoul(val.as<const char *>(), nullptr, 16));
                    Serial.println("OK");
                }
                else
                {
                    Serial.println("ERR: Effect not active");
                }
            }
            else
            {
                Serial.println("ERR: Invalid segment");
            }
            return;
        }
    }

    int sp = cmdLine.indexOf(' ');
    String cmd = sp > 0 ? cmdLine.substring(0, sp) : cmdLine;
    String args = sp > 0 ? cmdLine.substring(sp + 1) : String();
    cmd.toLowerCase();

    auto &segments = strip.getSegments();

    if (cmd.equalsIgnoreCase("listeffects"))
    {
        StaticJsonDocument<512> doc;
        JsonArray effects = doc.createNestedArray("effects");
#define ADD_EFFECT_TO_JSON(name, className) effects.add(#name);
        EFFECT_LIST(ADD_EFFECT_TO_JSON)
#undef ADD_EFFECT_TO_JSON
        String out;
        serializeJson(doc, out);
        Serial.println(out);
    }
    else if (cmd == "select")
    {
        int idx = args.toInt();
        if (idx >= 0 && (size_t)idx < segments.size())
        {
            seg = segments[idx];
            Serial.print("Active segment set to: ");
            Serial.println(idx);
        }
        else
        {
            Serial.println("Invalid segment index.");
        }
    }
    else if (cmd == "setcolor")
    {
        int r, g, b;
        if (sscanf(args.c_str(), "%d %d %d", &r, &g, &b) == 3)
        {
            seg->setColor(r, g, b);
            Serial.print("Active segment color set to: ");
            Serial.print(r);
            Serial.print(" ");
            Serial.print(g);
            Serial.print(" ");
            Serial.println(b);
            activeR = r;
            activeG = g;
            activeB = b;
        }
        else
        {
            Serial.println("Usage: setcolor <r> <g> <b>");
        }
    }
    else if (cmd == "seteffect")
    {
        int spaceIdx = args.indexOf(' ');
        if (spaceIdx < 0)
        {
            Serial.println("Usage: seteffect <seg> <effect>");
            return;
        }
        int segIdx = args.substring(0, spaceIdx).toInt();
        String effectName = args.substring(spaceIdx + 1);
        if (segIdx >= 0 && (size_t)segIdx < segments.size())
        {
            if (segments[segIdx]->activeEffect)
                delete segments[segIdx]->activeEffect;
            segments[segIdx]->activeEffect = createEffectByName(effectName, segments[segIdx]);
            Serial.println(segments[segIdx]->activeEffect ? "OK" : "ERR: effect not found");
        }
        else
        {
            Serial.println("ERR: Invalid segment");
        }
    }
    else if (cmd.equalsIgnoreCase("batchconfig"))
    {
        handleBatchConfigJson(args);
    }
    else if (cmd == "listsegments")
    {
        Serial.println("Segments:");
        for (size_t i = 0; i < segments.size(); ++i)
        {
            auto *s = segments[i];
            Serial.print("[");
            Serial.print(i);
            Serial.print("] ");
            Serial.print(s->getName().c_str());
            Serial.print(": ");
            Serial.print(s->startIndex());
            Serial.print("-");
            Serial.print(s->endIndex());
            Serial.print(", Brightness=");
            Serial.print(s->getBrightness());
            if (s->activeEffect)
            {
                Serial.print(", Effect=" + String(s->activeEffect->getName()));
            }
            Serial.println();
        }
    }
    else if (cmd.equalsIgnoreCase("getstatus"))
    {
        StaticJsonDocument<1024> statusDoc;
        JsonArray segs = statusDoc.createNestedArray("segments");
        for (auto *s : strip.getSegments())
        {
            JsonObject obj = segs.createNestedObject();
            obj["id"] = s->getId();
            obj["name"] = s->getName();
            obj["startLed"] = s->startIndex();
            obj["endLed"] = s->endIndex();
            obj["brightness"] = s->getBrightness();
            obj["effect"] = s->activeEffect ? s->activeEffect->getName() : "NONE";
        }
        String out;
        serializeJson(statusDoc, out);
        Serial.println(out);
    }
    else if (cmd == "geteffectinfo")
    {
        int segIdx = args.toInt();
        if (segIdx >= 0 && (size_t)segIdx < segments.size() && segments[segIdx]->activeEffect)
        {
            BaseEffect *effect = segments[segIdx]->activeEffect;
            StaticJsonDocument<512> infoDoc;
            infoDoc["effect"] = effect->getName();
            JsonArray params = infoDoc.createNestedArray("params");
            for (int i = 0; i < effect->getParameterCount(); ++i)
            {
                EffectParameter *p = effect->getParameter(i);
                JsonObject pj = params.createNestedObject();
                pj["name"] = p->name;
                pj["type"] = static_cast<int>(p->type);
                switch (p->type)
                {
                case ParamType::INTEGER:
                    pj["value"] = p->value.intValue;
                    break;
                case ParamType::FLOAT:
                    pj["value"] = p->value.floatValue;
                    break;
                case ParamType::BOOLEAN:
                    pj["value"] = p->value.boolValue;
                    break;
                case ParamType::COLOR:
                    pj["value"] = p->value.colorValue;
                    break;
                }
                pj["min"] = p->min_val;
                pj["max"] = p->max_val;
            }
            String out;
            serializeJson(infoDoc, out);
            Serial.println(out);
        }
        else
        {
            Serial.println("ERR: segment/effect not found");
        }
    }
    else
    {
        Serial.print("Unknown cmd: ");
        Serial.println(cmd);
    }
}

void processSerial()
{
    while (Serial.available())
    {
        String line = Serial.readStringUntil('\n');
        handleCommandLine(line);
    }
}

void handleBinarySerial(const uint8_t *data, size_t len)
{
    if (!len)
        return;
    uint8_t cmdId = data[0];
    if (cmdId == CMD_BATCH_CONFIG)
    {
        isReceivingBatch = true;
        jsonBuffer = "";
        jsonBuffer.concat((const char *)(data + 1), len - 1);
    }
    else if (isReceivingBatch)
    {
        jsonBuffer.concat((const char *)data, len);
        StaticJsonDocument<1024> doc;
        if (deserializeJson(doc, jsonBuffer) == DeserializationError::Ok)
        {
            Serial.println("Batch config JSON fully received and parsed.");
            handleBatchConfigJson(jsonBuffer);
            isReceivingBatch = false;
            jsonBuffer = "";
        }
    }
    else
    {
        switch (cmdId)
        {
        case CMD_GET_STATUS:
            handleCommandLine("getstatus");
            break;
        default:
            break;
        }
    }
}

void processBLE()
{
    if (!connectedCentral)
    {
        connectedCentral = BLE.central();
        if (connectedCentral)
        {
            Serial.print("[BLE] Connected: ");
            Serial.println(connectedCentral.address());
        }
    }

    if (connectedCentral && connectedCentral.connected())
    {
        if (cmdCharacteristic.written())
        {
            size_t len = cmdCharacteristic.valueLength();
            uint8_t buf[256];
            if (len > sizeof(buf))
                len = sizeof(buf);
            memcpy(buf, cmdCharacteristic.value(), len);

            // For now, assume BLE sends plain-text commands terminated by newline
            String bleCommand = String((char *)buf, len);
            bleCommand.trim();
            handleCommandLine(bleCommand);
        }
    }
    else if (connectedCentral)
    {
        Serial.print("[BLE] Disconnected from: ");
        Serial.println(connectedCentral.address());
        connectedCentral = BLEDevice();
        BLE.advertise();
        Serial.println("[BLE] Advertising restarted");
    }
}

void processAudio()
{
    if (samplesRead)
    {
        audioTrigger.update(sampleBuffer);
        samplesRead = 0;
    }
}

void processAccel()
{
    if (IMU.accelerationAvailable())
    {
        IMU.readAcceleration(accelX, accelY, accelZ);
    }
}

void saveConfig()
{
    StaticJsonDocument<1024> doc;
    JsonArray segments = doc.createNestedArray("segments");
    for (auto *s : strip.getSegments())
    {
        JsonObject segmentObject = segments.createNestedObject();
        segmentObject["name"] = s->getName();
        segmentObject["startLed"] = s->startIndex();
        segmentObject["endLed"] = s->endIndex();
        segmentObject["brightness"] = s->getBrightness();
        segmentObject["effect"] = (s->activeEffect) ? s->activeEffect->getName() : "NONE";
    }
    FILE *file = fopen(STATE_FILE, "w");
    if (file)
    {
        String output;
        serializeJson(doc, output);
        fputs(output.c_str(), file);
        fclose(file);
        Serial.println("Configuration saved.");
    }
    else
    {
        Serial.println("Failed to open state file for writing.");
    }
}

void loadConfig()
{
    FILE *file = fopen(STATE_FILE, "r");
    if (file)
    {
        char buf[1024];
        fread(buf, 1, sizeof(buf), file);
        fclose(file);
        String json(buf);
        handleBatchConfigJson(json);
        Serial.println("Configuration loaded.");
    }
    else
    {
        Serial.println("Could not find state file, using default configuration.");
    }
}
#include "Processes.h"
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <math.h>
#include <PDM.h>
#include "Triggers.h"
#include <LittleFS_Mbed_RP2040.h>
#include <stdio.h>

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

// BaseEffect *createEffectByName(const String &name, PixelStrip::Segment *seg)
// {
//     if (name.equalsIgnoreCase("Fire"))
//     {
//         return new FireEffect(seg);
//     }
//     if (name.equalsIgnoreCase("SolidColor"))
//     {
//         return new SolidColorEffect(seg);
//     }
//     if (name.equalsIgnoreCase("RainbowChase"))
//     {
//         return new RainbowChaseEffect(seg);
//     }
//     if (name.equalsIgnoreCase("RainbowCycle"))
//     {
//         return new RainbowCycleEffect(seg);
//     }
//     if (name.equalsIgnoreCase("AccelMeter"))
//     {
//         return new AccelMeterEffect(seg);
//     }
//     if (name.equalsIgnoreCase("Flare"))
//     {
//         return new FlareEffect(seg);
//     }
//     if (name.equalsIgnoreCase("FlashOnTrigger"))
//     {
//         return new FlashOnTriggerEffect(seg);
//     }
//     if (name.equalsIgnoreCase("KineticRipple"))
//     {
//         return new KineticRippleEffect(seg);
//     }
//     if (name.equalsIgnoreCase("TheaterChase"))
//     {
//         return new TheaterChaseEffect(seg);
//     }
//     if (name.equalsIgnoreCase("ColoredFire"))
//     {
//         return new ColoredFireEffect(seg);
//     }
//     return nullptr;
// }

/**
 * @brief Creates an effect instance based on its string name.
 * This function correctly uses the EFFECT_LIST macro to avoid hardcoding.
 */
BaseEffect* createEffectByName(const String &name, PixelStrip::Segment *seg) {
    #define CREATE_EFFECT_IF_MATCH(effectName, className) \
        if (name.equalsIgnoreCase(#effectName)) { return new className(seg); }
    
    EFFECT_LIST(CREATE_EFFECT_IF_MATCH)
    
    #undef CREATE_EFFECT_IF_MATCH
    return nullptr; // Return null if no effect name matched
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

    // Clear all user-defined segments, which preserves the primary "all" segment at index 0.
    strip.clearUserSegments();

    if (doc.containsKey("segments"))
    {
        // Get a direct pointer to the main "all" segment.
        PixelStrip::Segment* allSegment = strip.getSegments()[0];

        for (auto item : doc["segments"].as<JsonArray>())
        {
            String name = item["name"] | ""; // Use default "" if name is not present

            // Check if this JSON object is specifically for configuring the "all" segment.
            if (name.equalsIgnoreCase("all"))
            {
                // --- Configure the existing "all" segment (segment 0) ---
                if (item.containsKey("brightness"))
                {
                    allSegment->setBrightness(item["brightness"]);
                }

                if (item.containsKey("effect"))
                {
                    String effectName = item["effect"].as<const char *>();
                    if (allSegment->activeEffect) delete allSegment->activeEffect;
                    allSegment->activeEffect = createEffectByName(effectName, allSegment);
                }
            }
            else 
            {
                // --- Add a new user-defined segment ---
                uint16_t start = item["startLed"];
                uint16_t end = item["endLed"];
                strip.addSection(start, end, name);
                
                // Get the segment that was just added (it's the last one).
                auto &v = strip.getSegments();
                PixelStrip::Segment* newSeg = v.back();

                if (item.containsKey("brightness"))
                {
                    newSeg->setBrightness(item["brightness"]);
                }

                if (item.containsKey("effect"))
                {
                    String effectName = item["effect"].as<const char *>();
                    // The new segment doesn't have an active effect yet, so no need to delete.
                    newSeg->activeEffect = createEffectByName(effectName, newSeg);
                }
            }
        }
    }
    strip.show();
    Serial.println("Batch configuration applied");
}

void handleCommandLine(const String &line)
{
    String cmdLine = line;
    cmdLine.trim();
    if (cmdLine.isEmpty())
        return;
    int sp = cmdLine.indexOf(' ');
    String cmd = sp > 0 ? cmdLine.substring(0, sp) : cmdLine;
    String args = sp > 0 ? cmdLine.substring(sp + 1) : String();
    cmd.toLowerCase();

    auto &segments = strip.getSegments();

    if (cmd == "select")
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
        int num_parsed = sscanf(args.c_str(), "%d %d %d", &r, &g, &b);
        if (num_parsed == 3)
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
    else if (cmd == "setfirecolors")
    {
        int r1, g1, b1, r2, g2, b2, r3, g3, b3;
        int num_parsed = sscanf(args.c_str(), "%d %d %d %d %d %d %d %d %d",
                                &r1, &g1, &b1, &r2, &g2, &b2, &r3, &g3, &b3);

        if (num_parsed == 9)
        {
            if (seg->activeEffect && strcmp(seg->activeEffect->getName(), "ColoredFire") == 0)
            {
                seg->activeEffect->setParameter("color1", (uint32_t)strip.Color(r1, g1, b1));
                seg->activeEffect->setParameter("color2", (uint32_t)strip.Color(r2, g2, b2));
                seg->activeEffect->setParameter("color3", (uint32_t)strip.Color(r3, g3, b3));
                Serial.println("ColoredFire colors updated.");
            }
            else
            {
                Serial.println("ERR: 'ColoredFire' effect must be active.");
            }
        }
        else
        {
            Serial.println("Usage: setfirecolors <r1 g1 b1 r2 g2 b2 r3 g3 b3>");
        }
    }
    else if (cmd == "seteffect")
    {
        int spaceIdx = args.indexOf(' ');
        if (spaceIdx < 0)
        {
            Serial.println("Usage: seteffect <segment> <effect_name>");
            return;
        }
        int segIdx = args.substring(0, spaceIdx).toInt();
        String effectName = args.substring(spaceIdx + 1);

        if (segIdx < 0 || (size_t)segIdx >= segments.size())
        {
            Serial.println("ERR: segment");
            return;
        }

        if (segments[segIdx]->activeEffect)
        {
            delete segments[segIdx]->activeEffect;
            segments[segIdx]->activeEffect = nullptr;
        }
        segments[segIdx]->activeEffect = createEffectByName(effectName, segments[segIdx]);

        if (!segments[segIdx]->activeEffect)
        {
            Serial.println("ERR: effect not found");
        }
        else
        {
            Serial.println("OK");
        }
        return;
    }
    else if (cmd == "setparam")
    {
        int firstSpace = args.indexOf(' ');
        int secondSpace = args.indexOf(' ', firstSpace + 1);
        if (firstSpace < 0 || secondSpace < 0)
        {
            Serial.println("Usage: setparam <segment> <param_name> <value>");
            return;
        }
        int segIdx = args.substring(0, firstSpace).toInt();
        String pname = args.substring(firstSpace + 1, secondSpace);
        String valstr = args.substring(secondSpace + 1);

        if (segIdx < 0 || (size_t)segIdx >= segments.size())
        {
            Serial.println("ERR: segment");
            return;
        }

        BaseEffect *effect = segments[segIdx]->activeEffect;
        if (!effect)
        {
            Serial.println("ERR: no active effect on segment");
            return;
        }

        for (int i = 0; i < effect->getParameterCount(); ++i)
        {
            EffectParameter *p = effect->getParameter(i);
            if (pname.equalsIgnoreCase(p->name))
            {
                switch (p->type)
                {
                case ParamType::INTEGER:
                    effect->setParameter(p->name, (int)valstr.toInt());
                    break;
                case ParamType::FLOAT:
                    effect->setParameter(p->name, valstr.toFloat());
                    break;
                case ParamType::BOOLEAN:
                    effect->setParameter(p->name, (valstr.toInt() != 0));
                    break;
                case ParamType::COLOR:
                    effect->setParameter(p->name, (uint32_t)strtoul(valstr.c_str(), nullptr, 16));
                    break;
                }
                Serial.println("OK");
                return;
            }
        }
        Serial.println("ERR: param not found");
        return;
    }
    else if (cmd == "geteffectinfo")
    {
        int segIdx = args.toInt();
        if (segIdx < 0 || (size_t)segIdx >= segments.size() || !segments[segIdx]->activeEffect)
        {
            Serial.println("ERR: segment/effect");
            return;
        }
        BaseEffect *effect = segments[segIdx]->activeEffect;
        StaticJsonDocument<512> doc;
        doc["effect"] = effect->getName();
        JsonArray params = doc.createNestedArray("params");
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
        serializeJson(doc, out);
        Serial.println(out);
        return;
    }
    else if (cmd == "setsegbrightness")
    {
        String remainingArgs;
        PixelStrip::Segment *targetSeg = findSegmentByIndex(args, remainingArgs);
        if (targetSeg)
        {
            uint8_t b = constrain(remainingArgs.toInt(), 0, 255);
            targetSeg->setBrightness(b);
            targetSeg->update();
            strip.show();
            Serial.print("Segment ");
            Serial.print(targetSeg->getId());
            Serial.print(" brightness=");
            Serial.println(b);
        }
        else
        {
            Serial.println("Usage: setsegbrightness <idx> <0-255>");
        }
    }
    else if (cmd == "setsegrange")
    {
        String remainingArgs;
        PixelStrip::Segment *targetSeg = findSegmentByIndex(args, remainingArgs);
        if (targetSeg)
        {
            int p = remainingArgs.indexOf(' ');
            if (p > 0)
            {
                int start = remainingArgs.substring(0, p).toInt();
                int end = remainingArgs.substring(p + 1).toInt();
                if (end >= start)
                {
                    targetSeg->setRange(start, end);
                    Serial.print("Segment ");
                    Serial.print(targetSeg->getId());
                    Serial.print(" range=");
                    Serial.print(start);
                    Serial.print("-");
                    Serial.println(end);
                }
                else
                {
                    Serial.println("Error: end < start");
                }
            }
            else
            {
                Serial.println("Usage: setsegrange <idx> <start> <end>");
            }
        }
        else
        {
            Serial.println("Usage: setsegrange <idx> <start> <end>");
        }
    }
    else if (cmd == "clearsegments")
    {
        strip.clearUserSegments();
        seg = strip.getSegments()[0];
        if (seg->activeEffect)
        {
            delete seg->activeEffect;
        }
        seg->activeEffect = createEffectByName("SolidColor", seg);
        seg->update();
        strip.show();
        Serial.println("Segments cleared; active = 0");
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
                Serial.print(", Effect=" + String(s->activeEffect->getName()));
            Serial.println();
        }
    }
    else if (cmd == "addsegment")
    {
        int d = args.indexOf(' ');
        if (d < 0)
            Serial.println("Usage: addsegment <start> <end>");
        else
        {
            int start = args.substring(0, d).toInt();
            int end = args.substring(d + 1).toInt();
            if (end < start)
                Serial.println("Error: end<start");
            else
            {
                strip.addSection(start, end, "seg" + String(segments.size()));
                Serial.print("Added segment ");
                Serial.print(segments.size() - 1);
                Serial.print(" [");
                Serial.print(start);
                Serial.print("-");
                Serial.print(end);
                Serial.println("]");
            }
        }
    }
    else if (cmd.equalsIgnoreCase("getstatus"))
    {
        String response = "{";
        response += "\"segments\":[";
        auto &v = strip.getSegments();
        for (size_t i = 0; i < v.size(); ++i)
        {
            auto *s = v[i];
            response += "{\"id\":" + String(s->getId());
            response += ",\"name\":\"" + String(s->getName().c_str()) + "\"";
            response += ",\"startLed\":" + String(s->startIndex());
            response += ",\"endLed\":" + String(s->endIndex());
            response += ",\"brightness\":" + String(s->getBrightness());
            if (s->activeEffect)
                response += ",\"effect\":\"" + String(s->activeEffect->getName()) + "\"}";
            else
                response += ",\"effect\":\"NONE\"}";
            if (i + 1 < v.size())
                response += ',';
        }
        response += "]}";
        if (connectedCentral)
        {
            isSendingMultiPart = true;
            const int chunkSize = 20;
            for (unsigned int i = 0; i < response.length(); i += chunkSize)
            {
                String chunk = response.substring(i, i + chunkSize);
                cmdCharacteristic.writeValue((uint8_t *)chunk.c_str(), chunk.length());
                delay(10);
            }
            cmdCharacteristic.writeValue((uint8_t)'\n');
            isSendingMultiPart = false;
        }
        Serial.println(response);
    }
    else if (cmd.equalsIgnoreCase("batchconfig"))
    {
        handleBatchConfigJson(args);
    }
    else
    {
        Serial.print("Unknown cmd: ");
        Serial.println(cmd.c_str());
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
    if (isReceivingBatch)
    {
        Serial.println(jsonBuffer);
        StaticJsonDocument<1024> doc;
        if (deserializeJson(doc, jsonBuffer) == DeserializationError::Ok)
        {
            Serial.println("Batch config JSON fully received and parsed.");
            handleBatchConfigJson(jsonBuffer);
            isReceivingBatch = false;
            jsonBuffer = "";
        }
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

void processBLE()
{
    if (!connectedCentral)
    {
        BLEDevice central = BLE.central();
        if (central)
        {
            connectedCentral = central;
            Serial.print("[BLE] Connected: ");
            Serial.println(connectedCentral.address());
            lastHeartbeatTime = millis();
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

            if (!isReceivingBatch || buf[0] == CMD_BATCH_CONFIG)
            {
                Serial.print("[BLE] Cmd recv ID=0x");
                if (buf[0] < 0x10)
                    Serial.print('0');
                Serial.print(buf[0], HEX);
                Serial.print(" (");
                Serial.print(getBLECmdName(buf[0]));
                Serial.print("), len=");
                Serial.println(len);
            }
            handleBinarySerial(buf, len);
        }
    }
    else if (connectedCentral && !connectedCentral.connected())
    {
        Serial.print("[BLE] Disconnected from: ");
        Serial.println(connectedCentral.address());
        connectedCentral = BLEDevice();
        isReceivingBatch = false;
        jsonBuffer = "";
        BLE.advertise();
        Serial.println("[BLE] Advertising restarted");
    }
}

// **FIXED**: Add empty function definitions to satisfy the linker

void processAudio()
{
    // Your audio processing logic will go here.
    // For now, this stub prevents the linker error.
    if (samplesRead)
    {
        audioTrigger.update(sampleBuffer);
        samplesRead = 0;
    }
}

void processAccel()
{
    // Your accelerometer logic will go here.
    // For now, this stub prevents the linker error.
    if (IMU.accelerationAvailable())
    {
        IMU.readAcceleration(accelX, accelY, accelZ);
    }
}
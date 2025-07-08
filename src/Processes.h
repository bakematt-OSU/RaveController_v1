#pragma once

#include "config.h"
#include <Arduino.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <math.h>
#include <PDM.h>
#include "PixelStrip.h"
#include "Triggers.h"
#include "EffectLookup.h"
#include <LittleFS_Mbed_RP2040.h>
#include <stdio.h>

// --- Forward declarations for functions defined in this file ---
inline void handleBatchConfigJson(const String &json);
inline void saveConfig();
inline void loadConfig();
inline void handleBinarySerial(const uint8_t *data, size_t len);
inline void handleCommandLine(const String &line);

//-----------------------------------------------------------------------------
// Binary command IDs for quick BLE/Android control
//-----------------------------------------------------------------------------
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
static constexpr uint8_t CMD_ACK = 0xA0; // Acknowledge byte

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

// ——— External BLE state (define these in your main.cpp) ————————————
extern BLEService uartService;
extern BLECharacteristic cmdCharacteristic;

// --- BLE Connection State Management ---
static BLEDevice connectedCentral;
static String jsonBuffer = "";        // Buffer for reassembling JSON chunks
static bool isReceivingBatch = false; // --- NEW: State flag for batch transfer ---
static bool isSendingMultiPart = false;
static const unsigned long HEARTBEAT_INTERVAL = 1000;
static unsigned long lastHeartbeatTime = 0;

//-----------------------------------------------------------------------------
// Helper to map BLE command IDs to names
//-----------------------------------------------------------------------------
inline const char *getBLECmdName(uint8_t cmd)
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

// --- Configuration Functions (moved from Config.h) ---
inline void saveConfig()
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
        segmentObject["effect"] = EFFECT_NAMES[(int)s->activeEffect];
    }

    JsonArray effects = doc.createNestedArray("effects");
    for (uint8_t i = 0; i < EFFECT_COUNT; ++i)
    {
        effects.add(EFFECT_NAMES[i]);
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

inline void loadConfig()
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

//-----------------------------------------------------------------------------
// handleBatchConfigJson(): apply full configuration from JSON
//-----------------------------------------------------------------------------
inline void handleBatchConfigJson(const String &json)
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
        for (auto item : doc["segments"].as<JsonArray>())
        {
            uint16_t start = item["startLed"];
            uint16_t end = item["endLed"];
            String name = item["name"];
            strip.addSection(start, end, name);
            auto &v = strip.getSegments();
            uint8_t idx = v.size() - 1;
            if (item.containsKey("brightness"))
                v[idx]->setBrightness(item["brightness"]);
            if (item.containsKey("effect"))
            {
                EffectType e = effectFromString(item["effect"].as<const char *>());
                if (e != EffectType::UNKNOWN)
                {
                    applyEffectToSegment(v[idx], e);
                    v[idx]->activeEffect = static_cast<PixelStrip::Segment::SegmentEffect>(e);
                }
            }
        }
    }
    strip.show();
    Serial.println("Batch configuration applied");
}

//-----------------------------------------------------------------------------
// handleCommandLine(): execute ASCII commands from Serial or BLE
//-----------------------------------------------------------------------------
inline void handleCommandLine(const String &line)
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

    if (cmd == "clearsegments")
    {
        strip.clearUserSegments();
        seg = strip.getSegments()[0];
        applyEffectToSegment(seg, static_cast<EffectType>(seg->activeEffect));
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
            Serial.println(s->getBrightness());
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
    else if (cmd == "setsegrange")
    {
        int p1 = args.indexOf(' '), p2 = args.indexOf(' ', p1 + 1);
        if (p1 < 0 || p2 < 0)
            Serial.println("Usage: setsegrange <idx> <start> <end>");
        else
        {
            int idx = args.substring(0, p1).toInt();
            int start = args.substring(p1 + 1, p2).toInt();
            int end = args.substring(p2 + 1).toInt();
            if (idx < 0 || (size_t)idx >= segments.size())
                Serial.println("Invalid index");
            else if (end < start)
                Serial.println("Error: end<start");
            else
            {
                segments[idx]->setRange(start, end);
                Serial.print("Segment ");
                Serial.print(idx);
                Serial.print(" range=");
                Serial.print(start);
                Serial.print("-");
                Serial.println(end);
            }
        }
    }
    else if (cmd == "setsegeffect")
    {
        int d = args.indexOf(' ');
        if (d < 0)
            Serial.println("Usage: setsegeffect <idx> <effect>");
        else
        {
            int idx = args.substring(0, d).toInt();
            String eff = args.substring(d + 1);
            if (idx < 0 || (size_t)idx >= segments.size())
                Serial.println("Invalid index");
            else
            {
                EffectType e = effectFromString(eff);
                if (e == EffectType::UNKNOWN)
                    Serial.println("Invalid effect");
                else
                {
                    applyEffectToSegment(segments[idx], e);
                    segments[idx]->activeEffect = static_cast<PixelStrip::Segment::SegmentEffect>(e);
                    segments[idx]->update();
                    strip.show();
                    Serial.print("Segment ");
                    Serial.print(idx);
                    Serial.print(" effect=");
                    Serial.println(eff.c_str());
                }
            }
        }
    }
    else if (cmd == "setsegbrightness")
    {
        int d = args.indexOf(' ');
        if (d < 0)
            Serial.println("Usage: setsegbrightness <idx> <0-255>");
        else
        {
            int idx = args.substring(0, d).toInt();
            uint8_t b = constrain(args.substring(d + 1).toInt(), 0, 255);
            if (idx < 0 || (size_t)idx >= segments.size())
                Serial.println("Invalid segment index");
            else
            {
                segments[idx]->setBrightness(b);
                segments[idx]->update();
                strip.show();
                Serial.print("Segment ");
                Serial.print(idx);
                Serial.print(" brightness=");
                Serial.println(b);
            }
        }
    }
    else if (cmd.equalsIgnoreCase("getstatus"))
    {
        String response = "{";
        response += "\"effects\":[";
        for (uint8_t i = 0; i < EFFECT_COUNT; ++i)
        {
            response += "\"" + String(EFFECT_NAMES[i]) + "\"";
            if (i < EFFECT_COUNT - 1)
                response += ",";
        }
        response += "],";
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
            response += ",\"effect\":\"" + String(EFFECT_NAMES[(uint8_t)s->activeEffect]) + "\"}";
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

//-----------------------------------------------------------------------------
// handleBinarySerial(): execute binary commands over Serial (same as BLE)
//-----------------------------------------------------------------------------
inline void handleBinarySerial(const uint8_t *data, size_t len)
{
    if (!len)
        return;

    uint8_t cmdId = data[0];
    bool needsAck = true;

    // --- State Machine for Batch Transfer ---
    if (cmdId == CMD_BATCH_CONFIG)
    {
        isReceivingBatch = true;
        jsonBuffer = ""; // Start a new batch transfer
        jsonBuffer.concat((const char *)(data + 1), len - 1);
        Serial.println("Starting batch config reception...");
    }
    else if (isReceivingBatch)
    {
        // This is a continuation chunk. It does NOT have a command ID prefix.
        jsonBuffer.concat((const char *)data, len);
    }
    else
    {
        // This is a single, non-batch command.
        switch (cmdId)
        {
        case CMD_GET_STATUS:
            handleCommandLine("getstatus");
            needsAck = false;
            break;
        // Handle other single-shot commands here
        default:
            break;
        }
    }

    // After processing the chunk, check if a batch is complete
    if (isReceivingBatch)
    {
        StaticJsonDocument<1024> doc;
        if (deserializeJson(doc, jsonBuffer) == DeserializationError::Ok)
        {
            Serial.println("Batch config JSON fully received and parsed.");
            handleBatchConfigJson(jsonBuffer);
            isReceivingBatch = false; // End of batch
            jsonBuffer = "";          // Clear buffer
        }
        else
        {
            Serial.println("...Partial batch config received, waiting for more...");
        }
    }

    if (needsAck && connectedCentral)
    {
        uint8_t ack_byte = CMD_ACK;
        cmdCharacteristic.writeValue(&ack_byte, 1);
    }
}

//-----------------------------------------------------------------------------
// processSerial(): read and dispatch USB Serial commands (ASCII and binary)
//-----------------------------------------------------------------------------
inline void processSerial()
{
    while (Serial.available())
    {
        String line = Serial.readStringUntil('\n');
        handleCommandLine(line);
    }
}

//-----------------------------------------------------------------------------
// processAudio, processAccel, updateDigHeartbeat, sendBleHeartbeat, processBLE
//-----------------------------------------------------------------------------
inline void processAudio()
{
    if (samplesRead > 0)
    {
        audioTrigger.update((volatile int16_t *)sampleBuffer);
        samplesRead = 0;
    }
}

inline void processAccel()
{
    if (IMU.accelerationAvailable())
    {
        IMU.readAcceleration(accelX, accelY, accelZ);
        float m = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
        if (debugAccel && millis() - lastHbChange > 250)
            Serial.println(m);
        if (m > STEP_THRESHOLD && millis() - lastStepTime > STEP_COOLDOWN_MS)
        {
            triggerRipple = true;
            lastStepTime = millis();
        }
    }
}

inline void updateDigHeartbeat()
{
    static uint8_t st = 0;
    if (millis() - lastHbChange < HB_INTERVAL_MS)
        return;
    lastHbChange = millis();

    digitalWrite(LEDR_PIN, LOW);
    digitalWrite(LEDG_PIN, LOW);
    digitalWrite(LEDB_PIN, LOW);
    digitalWrite((st == 0 ? LEDR_PIN : (st == 1 ? LEDG_PIN : LEDB_PIN)), HIGH);
    st = (st + 1) % 3;
}

inline void sendBleHeartbeat()
{
    if (isSendingMultiPart)
        return;

    if (connectedCentral && millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL)
    {
        lastHeartbeatTime = millis();
        uint8_t beat = 0;
        if (cmdCharacteristic.canWrite())
        {
            cmdCharacteristic.writeValue(&beat, 1);
        }
    }
}

inline void processBLE()
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

            // Only print for the start of a new command, not for every data chunk
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
        sendBleHeartbeat();
    }
    else if (connectedCentral && !connectedCentral.connected())
    {
        Serial.print("[BLE] Disconnected from: ");
        Serial.println(connectedCentral.address());
        connectedCentral = BLEDevice();
        isReceivingBatch = false; // Reset batch state on disconnect
        jsonBuffer = "";
        BLE.advertise();
        Serial.println("[BLE] Advertising restarted");
    }
}
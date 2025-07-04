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
#include "Config.h"


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

// ——— External state (define these in your main.cpp) ————————————————
extern BLEService       uartService;
extern BLECharacteristic cmdCharacteristic;

// --- BLE Connection State Management (file-scoped) ---
static BLEDevice connectedCentral;
static unsigned long disconnectTime = 0;
static const unsigned long DISCONNECT_GRACE_PERIOD = 2000;
static const unsigned long HEARTBEAT_INTERVAL = 1000;
static unsigned long lastHeartbeatTime = 0;

//-----------------------------------------------------------------------------
// handleBatchConfigJson(): apply full configuration from JSON
//-----------------------------------------------------------------------------
inline void handleBatchConfigJson(const String &json)
{
    StaticJsonDocument<1024> doc;
    auto err = deserializeJson(doc, json);
    if (err)
    {
        Serial.println("BatchConfig: JSON parse error");
        return;
    }
    strip.clearUserSegments();
    if (doc.containsKey("segments"))
    {
        for (auto item : doc["segments"].as<JsonArray>())
        {
            uint16_t start = item["start"].as<uint16_t>();
            uint16_t end = item["end"].as<uint16_t>();
            String name = item["name"].as<String>();
            strip.addSection(start, end, name);
            uint8_t idx = strip.getSegments().size() - 1;
            if (item.containsKey("brightness"))
                strip.getSegments()[idx]->setBrightness(item["brightness"].as<uint8_t>());
            if (item.containsKey("effect"))
            {
                EffectType e = effectFromString(item["effect"].as<const char *>());
                if (e != EffectType::UNKNOWN)
                    applyEffectToSegment(strip.getSegments()[idx], e);
            }
        }
    }
    if (doc.containsKey("brightness"))
        strip.setActiveBrightness(doc["brightness"].as<uint8_t>());
    if (doc.containsKey("color"))
    {
        auto col = doc["color"].as<JsonArray>();
        if (col.size() == 3)
        {
            activeR = col[0];
            activeG = col[1];
            activeB = col[2];
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
    String cmd = (sp > 0 ? cmdLine.substring(0, sp) : cmdLine);
    String args = (sp > 0 ? cmdLine.substring(sp + 1) : String());
    cmd.toLowerCase();

    // Segment management
    if (cmd == "clearsegments")
    {
        strip.clearUserSegments();
        seg = strip.getSegments()[0];
        Serial.println("Segments cleared; active = 0");
    }
    else if (cmd == "listsegments")
    {
        Serial.println("Segments:");
        auto &v = strip.getSegments();
        for (size_t i = 0; i < v.size(); ++i)
        {
            auto *s = v[i];
            // FIX: Reverted from printf to print/println
            Serial.print("[");
            Serial.print((unsigned)i);
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
            int start = args.substring(0, d).toInt(), end = args.substring(d + 1).toInt();
            if (end < start)
                Serial.println("Error: end < start");
            else
            {
                strip.addSection(start, end, "seg" + String(strip.getSegments().size()));
                // FIX: Reverted from printf to print/println
                Serial.print("Added segment ");
                Serial.print(strip.getSegments().size() - 1);
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
            auto &v = strip.getSegments();
            // FIX: Cast to size_t to fix signed/unsigned comparison warning
            if (idx < 0 || (size_t)idx >= v.size())
                Serial.println("Invalid segment index");
            else
            {
                v[idx]->setRange(start, end);
                // FIX: Reverted from printf to print/println
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
            auto &v = strip.getSegments();
            // FIX: Cast to size_t to fix signed/unsigned comparison warning
            if (idx < 0 || (size_t)idx >= v.size())
                Serial.println("Invalid segment index");
            else
            {
                EffectType e = effectFromString(eff);
                if (e == EffectType::UNKNOWN)
                    Serial.println("Invalid effect");
                else
                {
                    applyEffectToSegment(v[idx], e);
                    // FIX: Reverted from printf to print/println
                    Serial.print("Segment ");
                    Serial.print(idx);
                    Serial.print(" effect=");
                    Serial.println(eff.c_str());
                }
            }
        }
    }
    else if (cmd == "setsegname")
    {
        int d = args.indexOf(' ');
        if (d < 0)
            Serial.println("Usage: setsegname <idx> <name>");
        else
        {
            int idx = args.substring(0, d).toInt();
            String name = args.substring(d + 1);
            auto &v = strip.getSegments();
            // FIX: Cast to size_t to fix signed/unsigned comparison warning
            if (idx < 0 || (size_t)idx >= v.size())
                Serial.println("Invalid segment index");
            else
            {
                // FIX: The compiler indicated 'setName' does not exist.
                // This function is disabled until a setName method is added to the PixelStrip::Segment class.
                // v[idx]->setName(name);
                Serial.print("Segment ");
                Serial.print(idx);
                Serial.print(" name set to: ");
                Serial.println(name.c_str());
                Serial.println("WARNING: 'setName' is not implemented. Name not changed.");
            }
        }
    }
    // Color & Effects
    else if (cmd == "setcolor")
    {
        int a = args.indexOf(' '), b = args.indexOf(' ', a + 1);
        if (a < 0 || b < 0)
            Serial.println("Usage: setcolor <r> <g> <b>");
        else
        {
            activeR = args.substring(0, a).toInt();
            activeG = args.substring(a + 1, b).toInt();
            activeB = args.substring(b + 1).toInt();
            strip.show();
            // FIX: Reverted from printf to print/println
            Serial.print("Color set R=");
            Serial.print(activeR);
            Serial.print(" G=");
            Serial.print(activeG);
            Serial.print(" B=");
            Serial.println(activeB);
        }
    }
    else if (cmd == "listeffects")
    {
        Serial.println("Effects:");
        for (uint8_t i = 0; i < EFFECT_COUNT; ++i)
        {
            // FIX: Reverted from printf to print/println
            Serial.print("[");
            Serial.print(i);
            Serial.print("] ");
            Serial.println(EFFECT_NAMES[i]);
        }
    }
    else if (cmd == "listeffectsjson")
    {
        Serial.print("{\"effects\":[");
        for (uint8_t i = 0; i < EFFECT_COUNT; ++i)
        {
            // FIX: Reverted from printf to print
            Serial.print("\"");
            Serial.print(EFFECT_NAMES[i]);
            Serial.print("\"");
            if (i + 1 < EFFECT_COUNT)
                Serial.print(',');
        }
        Serial.println("]}");
    }
    else if (cmd == "seteffect")
    {
        EffectType e = effectFromString(args);
        if (e == EffectType::UNKNOWN)
            Serial.println("Invalid effect");
        else
        {
            applyEffectToSegment(seg, e);
            // FIX: Reverted from printf to print/println
            Serial.print("Effect -> ");
            Serial.println(args.c_str());
        }
    }
    // Brightness
    else if (cmd == "setbrightness")
    {
        uint8_t b = constrain(args.toInt(), 0, 255);
        strip.setActiveBrightness(b);
        strip.show();
        // FIX: Reverted from printf to print/println
        Serial.print("Global brightness=");
        Serial.println(b);
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
            auto &v = strip.getSegments();
            // FIX: Cast to size_t to fix signed/unsigned comparison warning
            if (idx < 0 || (size_t)idx >= v.size())
                Serial.println("Invalid segment index");
            else
            {
                v[idx]->setBrightness(b);
                v[idx]->update();
                strip.show();
                // FIX: Reverted from printf to print/println
                Serial.print("Segment ");
                Serial.print(idx);
                Serial.print(" brightness=");
                Serial.println(b);
            }
        }
    }
    else if (cmd == "numpixels")
    {
        // Build JSON payload once
        String resp = String("{\"numpixels\":") + LED_COUNT + String("}");
        Serial.println(resp); // USB serial response
        if (connectedCentral) // BLE response (ASCII mode)
            cmdCharacteristic.writeValue(resp.c_str());
    }
    else if (cmd == "getstatus")
    {
        // FIX: Reverted from printf to print for more efficient JSON generation
        Serial.print("{\"effects\":[");
        for (uint8_t i = 0; i < EFFECT_COUNT; ++i)
        {
            Serial.print("\"");
            Serial.print(EFFECT_NAMES[i]);
            Serial.print("\"");
            if (i + 1 < EFFECT_COUNT)
                Serial.print(',');
        }
        Serial.print("],\"segments\":[");
        auto &v = strip.getSegments();
        for (size_t i = 0; i < v.size(); ++i)
        {
            auto *s = v[i];
            Serial.print("{\"id\":");
            Serial.print(s->getId());
            Serial.print(",\"name\":\"");
            Serial.print(s->getName().c_str());
            Serial.print("\",\"start\":");
            Serial.print(s->startIndex());
            Serial.print(",\"end\":");
            Serial.print(s->endIndex());
            Serial.print(",\"brightness\":");
            Serial.print(s->getBrightness());
            Serial.print(",\"effect\":\"");
            Serial.print(EFFECT_NAMES[(uint8_t)s->activeEffect]);
            Serial.print("\"}");
            if (i + 1 < v.size())
                Serial.print(',');
        }
        Serial.println("]}");
    }
    else if (cmd == "batchconfig")
        handleBatchConfigJson(args);
    else
    {
        // FIX: Reverted from printf to print/println
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
    switch (data[0])
    {
    case CMD_SET_COLOR:
        if (len >= 4)
        {
            activeR = data[1];
            activeG = data[2];
            activeB = data[3];
            strip.show();
        }
        break;
    case CMD_SET_EFFECT:
        if (len >= 2 && data[1] < EFFECT_COUNT)
            applyEffectToSegment(seg, (EffectType)data[1]);
        break;
    case CMD_SET_BRIGHTNESS:
        if (len >= 2)
        {
            strip.setActiveBrightness(data[1]);
            strip.show();
        }
        break;
    case CMD_SET_SEG_BRIGHT:
        if (len >= 3 && data[1] < strip.getSegments().size())
        {
            auto *s = strip.getSegments()[data[1]];
            s->setBrightness(data[2]);
            s->update();
            strip.show();
        }
        break;
    case CMD_SELECT_SEGMENT:
        if (len >= 2 && data[1] < strip.getSegments().size())
            seg = strip.getSegments()[data[1]];
        break;
    case CMD_CLEAR_SEGMENTS:
        strip.clearUserSegments();
        seg = strip.getSegments()[0];
        break;
    case CMD_SET_SEG_RANGE:
        if (len >= 6 && data[1] < strip.getSegments().size())
        {
            auto *s = strip.getSegments()[data[1]];
            s->setRange((data[2] << 8) | data[3], (data[4] << 8) | data[5]);
        }
        break;
    case CMD_GET_STATUS:
        handleCommandLine(String("getstatus"));
        break;
    case CMD_BATCH_CONFIG:
        if (len > 1)
        {
            String j((char *)(data + 1), len - 1);
            handleBatchConfigJson(j);
        }
        break;

    case CMD_NUM_PIXELS:
    {
        // Build a little JSON payload
        String resp = String("{\"numpixels\":") + LED_COUNT + String("}");
        cmdCharacteristic.writeValue(resp.c_str());
    }
    break;
    }
}

//-----------------------------------------------------------------------------
// processSerial(): read and dispatch USB Serial commands (ASCII and binary)
//-----------------------------------------------------------------------------
inline void processSerial()
{
    while (Serial.available())
    {
        int c = Serial.peek();
        if (c >= 0 && c < 0x20)
        {
            uint8_t buf[64];
            size_t n = Serial.readBytes(buf, min((size_t)Serial.available(), sizeof(buf)));
            handleBinarySerial(buf, n);
        }
        else
        {
            String line = Serial.readStringUntil('\n');
            handleCommandLine(line);
        }
    }
}

//-----------------------------------------------------------------------------
// processAudio(), processAccel(), updateDigHeartbeat(), processBLE()
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
        {
            Serial.println(m);
        }
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
//-----------------------------------------------------------------------------
// Send a notification every HEARTBEAT_INTERVAL ms to keep the BLE link alive
// This is now simplified to only send if the central is valid.
//-----------------------------------------------------------------------------
inline void sendBleHeartbeat() {
  if (connectedCentral && millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL)
  {
    lastHeartbeatTime = millis();
    // A single write is sufficient. The 'notify' parameter is now handled by the library.
    uint8_t beat = 0; // Sending a single dummy byte is common practice
    cmdCharacteristic.writeValue(beat);
  }
}

//-----------------------------------------------------------------------------
// processBLE: Rewritten for stability.
// It now has only two states: looking for a connection, or handling a connection.
// The problematic "grace period" has been removed.
//-----------------------------------------------------------------------------
inline void processBLE()
{
    // Step 1: Check for a new connection if we don't have one.
    if (!connectedCentral) {
        BLEDevice central = BLE.central();
        if (central) {
            connectedCentral = central;
            Serial.print("[BLE] Connected to central: ");
            Serial.println(connectedCentral.address());
            lastHeartbeatTime = millis();
        }
    }

    // Step 2: If we are connected, process data and heartbeats.
    // If not, the object will be invalid and this block is skipped.
    if (connectedCentral && connectedCentral.connected()) {
        // Handle incoming commands
        if (cmdCharacteristic.written()) {
            size_t len = cmdCharacteristic.valueLength();
            uint8_t buf[256];
            if (len > 0) {
                if (len > sizeof(buf)) len = sizeof(buf);
                memcpy(buf, cmdCharacteristic.value(), len);

                Serial.print("[BLE] Cmd recv: ");
                Serial.write(buf, len);
                Serial.println();

                handleBinarySerial(buf, len);
            }
        }

        // Always send a heartbeat to keep the connection alive.
        sendBleHeartbeat();

    }
    // Step 3: If the central device disconnected, invalidate the object.
    else if (connectedCentral && !connectedCentral.connected()) {
        Serial.print("[BLE] Disconnected from: ");
        Serial.println(connectedCentral.address());
        connectedCentral = BLEDevice(); // Invalidate the object. Ready for a new connection.
    }
}
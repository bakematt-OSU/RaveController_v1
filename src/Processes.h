// Processes.h
// Declarations and implementations for processing routines

#pragma once

#include "config.h"
#include <Arduino.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <ArduinoBLE.h> // ← needed for BLE.central(), BLE.setLocalName(), etc.
#include <math.h>
#include <PDM.h>
#include "PixelStrip.h"
#include "Triggers.h"
#include "EffectLookup.h"
#include <LittleFS_Mbed_RP2040.h> // brings in LittleFS + FS
#include <stdio.h>                // for fopen/fputs/fclose

// External globals defined in main.cpp
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

// BLE characteristic defined in main.cpp
extern BLECharacteristic cmdCharacteristic;

inline void handleCommandLine(const String &line)
{
    Serial.println("HANDLE COMMAND LINE: ");
    // — TRIM WHITESPACE AND SPLIT COMMAND/ARGUMENTS ——————————————
    String trimmed = line;
    trimmed.trim();

    int spaceIdx = trimmed.indexOf(' ');
    String cmd = (spaceIdx >= 0) ? trimmed.substring(0, spaceIdx) : trimmed;
    String args = (spaceIdx >= 0) ? trimmed.substring(spaceIdx + 1) : String();
    cmd.toLowerCase();
    Serial.print("Command: ");
    Serial.println(cmd);

    // CLEARSEGMENTS: remove all user‐added segments, reset to full strip
    if (cmd == "clearsegments")
    {
        Serial.println("Clearing user-defined segments.");
        strip.clearUserSegments();
        seg = strip.getSegments()[0];
        seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);
        Serial.println("Active segment reset to 0 (full strip).");
    }
    // ADDSEGMENT <start> <end>: carve out a new segment
    else if (cmd == "addsegment")
    {
        int delim = args.indexOf(' ');
        if (delim != -1)
        {
            int start = args.substring(0, delim).toInt();
            int end = args.substring(delim + 1).toInt();
            if (end >= start)
            {
                String name = "seg" + String(strip.getSegments().size());
                strip.addSection(start, end, name);
                Serial.print("Added segment #");
                Serial.print(strip.getSegments().size() - 1);
                Serial.print(" from ");
                Serial.print(start);
                Serial.print(" to ");
                Serial.println(end);
            }
            else
            {
                Serial.println("Error: end must be >= start.");
            }
        }
        else
        {
            Serial.println("Usage: addsegment <start> <end>");
        }
    }
    // SELECT <index>: switch which segment ‘seg’ points to
    else if (cmd == "select")
    {
        int idxArg = args.toInt();
        if (idxArg >= 0 && idxArg < (int)strip.getSegments().size())
        {
            seg = strip.getSegments()[idxArg];
            Serial.print("Selected segment ");
            Serial.println(idxArg);
        }
        else
        {
            Serial.println("Invalid segment index.");
        }
    }
    // SETCOLOR <r> <g> <b>: update the globals for your next effects
    else if (cmd == "setcolor")
    {
        int a = args.indexOf(' ');
        int b = args.indexOf(' ', a + 1);
        if (a > 0 && b > a)
        {
            activeR = args.substring(0, a).toInt();
            activeG = args.substring(a + 1, b).toInt();
            activeB = args.substring(b + 1).toInt();
            Serial.print("Color set to R=");
            Serial.print(activeR);
            Serial.print(" G=");
            Serial.print(activeG);
            Serial.print(" B=");
            Serial.println(activeB);
        }
        else
        {
            Serial.println("Usage: setcolor <r> <g> <b>");
        }
    }
    // SETEFFECT <name>: apply an effect
    else if (cmd == "seteffect")
    {
        EffectType effect = effectFromString(args);
        if (effect == EffectType::UNKNOWN)
        {
            Serial.println("Invalid effect name.");
        }
        else
        {
            applyEffectToSegment(seg, effect);
            Serial.print("Effect applied: ");
            Serial.println(args);
        }
    }
    // SETBTNAME <1–20 chars>: change the BLE Device Name
    else if (cmd == "setbtname")
    {
        args.trim();
        if (args.length() >= 1 && args.length() <= 20)
        {
            FILE *f = fopen(BT_NAME_FILE, "w");
            if (f)
            {
                fputs(args.c_str(), f);
                fputc('\n', f);
                fclose(f);
                // immediately bump the BLE name
                BLE.stopAdvertise();
                BLE.setLocalName(args.c_str());
                BLE.advertise();
                Serial.print("BT name set to “");
                Serial.print(args);
                Serial.println("”");
            }
            else
            {
                Serial.println("Error: cannot open file for write");
            }
        }
        else
        {
            Serial.println("Usage: setbtname <1–20 chars>");
        }
    }
    else if (cmd == "listeffects")
    {
        Serial.println("Available effects:");
        for (uint8_t i = 0; i < EFFECT_COUNT; ++i)
        {
            Serial.print("  ");
            Serial.println(EFFECT_NAMES[i]);
        }
    }
    // ─── catch-all for anything else ────────────────────────────────────────
    else
    {
        Serial.print("Unknown command: ");
        Serial.println(cmd);
    }
}

inline void processSerial()
{
    if (!Serial.available())
        return;

    // read one line (up to '\n')
    String line = Serial.readStringUntil('\n');
    line.trim(); // strip any stray \r or spaces

    // acknowledge receipt
    Serial.print(F("Received command: "));
    Serial.println(line);

    // dispatch it
    handleCommandLine(line);

    // final confirmation
    Serial.print(F("Command processed: "));
    Serial.print(line);
    Serial.println(F("→ OK"));
}

inline void processAudio()
{
    if (samplesRead <= 0)
        return;
    audioTrigger.update(sampleBuffer);
    samplesRead = 0;
}

inline void processAccel()
{
    if (!IMU.accelerationAvailable())
        return;
    IMU.readAcceleration(accelX, accelY, accelZ);
    float mag = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);

    if (debugAccel && (millis() - lastHbChange > 250))
    {
        Serial.print("Accel: ");
        Serial.println(mag);
        lastHbChange = millis();
    }

    if (mag > STEP_THRESHOLD && millis() - lastStepTime > STEP_COOLDOWN_MS)
    {
        triggerRipple = true;
        lastStepTime = millis();
    }
}

// inline void updateHeartbeat()
// {

//     if (millis() - lastHeartbeatColorChange < HB_INTERVAL_MS)
//     {
//         return;
//     }
//     lastHeartbeatColorChange = millis();

//     WiFiDrv::analogWrite(LEDR_PIN, 0);
//     WiFiDrv::analogWrite(LEDG_PIN, 0);
//     WiFiDrv::analogWrite(LEDB_PIN, 0);

//     switch (heartbeatColorState)
//     {
//     case HEARTBEAT_RED:
//         WiFiDrv::analogWrite(LEDR_PIN, 255);
//         heartbeatColorState = HEARTBEAT_GREEN;
//         break;
//     case HEARTBEAT_GREEN:
//         WiFiDrv::analogWrite(LEDG_PIN, 255);
//         heartbeatColorState = HEARTBEAT_BLUE;
//         break;
//     case HEARTBEAT_BLUE:
//         WiFiDrv::analogWrite(LEDB_PIN, 255);
//         heartbeatColorState = HEARTBEAT_RED;
//         break;
//     }
// }

inline void updateDigHeartbeat()
{
    // only advance once per interval
    if (millis() - lastHeartbeatColorChange < HB_INTERVAL_MS)
    {
        return;
    }
    lastHeartbeatColorChange = millis();

    // turn all channels off
    WiFiDrv::digitalWrite(LEDR_PIN, LOW);
    WiFiDrv::digitalWrite(LEDG_PIN, LOW);
    WiFiDrv::digitalWrite(LEDB_PIN, LOW);
    // Serial.print("[HB] Color change: ");
    // Serial.println(heartbeatColorState);
    // light only the next color
    switch (heartbeatColorState)
    {
    case HEARTBEAT_RED:
        WiFiDrv::digitalWrite(LEDR_PIN, HIGH);
        heartbeatColorState = HEARTBEAT_GREEN;
        break;

    case HEARTBEAT_GREEN:
        WiFiDrv::digitalWrite(LEDG_PIN, HIGH);
        heartbeatColorState = HEARTBEAT_BLUE;
        break;

    case HEARTBEAT_BLUE:
        WiFiDrv::digitalWrite(LEDB_PIN, HIGH);
        heartbeatColorState = HEARTBEAT_RED;
        break;
    }
}

// inline void processBLE()

// keep a handle to the connected central across calls
static BLEDevice connectedCentral;

inline void processBLE()
{
    // if not already connected, see if one just attached
    if (!connectedCentral)
    {
        connectedCentral = BLE.central();
        if (connectedCentral)
        {
            Serial.println("[BLE] Central connected");
        }
    }

    // if we are connected, poll once and handle any incoming writes
    if (connectedCentral && connectedCentral.connected())
    {
        BLE.poll();
        if (cmdCharacteristic.written())
        {
            String line((const char *)cmdCharacteristic.value(),
                        cmdCharacteristic.valueLength());
            line.trim();
            Serial.print("[BLE] ");
            Serial.println(line);
            handleCommandLine(line);
        }
    }
    else if (connectedCentral)
    {
        // central was connected but now it's gone
        Serial.println("[BLE] Central disconnected");
        connectedCentral = BLEDevice(); // reset handle
    }
}

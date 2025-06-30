#pragma once

#include "config.h"
#include <Arduino.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <math.h>
#include <PDM.h>
#include "PixelStrip.h"
#include "Triggers.h"
#include "EffectLookup.h"
#include <LittleFS_Mbed_RP2040.h> // LittleFS + FS
#include <FS.h>                   // File system interface
#include <ArduinoBLE.h>
#include <ArduinoJson.h> // For saveconfig serialization

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

inline void handleCommandLine(const String &line)
{
    // Trim and split
    String trimmed = line;
    trimmed.trim();
    int spaceIdx = trimmed.indexOf(' ');
    String cmd = (spaceIdx >= 0) ? trimmed.substring(0, spaceIdx) : trimmed;
    String args = (spaceIdx >= 0) ? trimmed.substring(spaceIdx + 1) : String();
    cmd.toLowerCase();

    if (cmd == "clearsegments")
    {
        Serial.println("Clearing user-defined segments.");
        strip.clearUserSegments();
        seg = strip.getSegments()[0];
        seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);
    }
    else if (cmd == "addsegment")
    {
        int d = args.indexOf(' ');
        if (d > 0)
        {
            int start = args.substring(0, d).toInt();
            int end = args.substring(d + 1).toInt();
            if (end >= start)
            {
                String name = "seg" + String(strip.getSegments().size());
                strip.addSection(start, end, name);
                Serial.print("Added segment ");
                Serial.println(name);
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
    else if (cmd == "select")
    {
        int idx = args.toInt();
        if (idx >= 0 && idx < (int)strip.getSegments().size())
        {
            seg = strip.getSegments()[idx];
            Serial.print("Selected segment ");
            Serial.println(idx);
        }
        else
        {
            Serial.println("Invalid segment index.");
        }
    }
    else if (cmd == "setcolor")
    {
        int d1 = args.indexOf(' ');
        int d2 = args.indexOf(' ', d1 + 1);
        if (d1 > 0 && d2 > d1)
        {
            activeR = args.substring(0, d1).toInt();
            activeG = args.substring(d1 + 1, d2).toInt();
            activeB = args.substring(d2 + 1).toInt();
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
    else if (cmd == "seteffect")
    {
        // Use EffectLookup to map name to enum and apply
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
    else if (cmd == "setbtname")
    {
        args.trim();
        if (args.length() >= 1 && args.length() <= 20)
        {
            File f = LittleFS.open(BT_NAME_FILE, "w");
            if (f)
            {
                f.println(args);
                f.close();
                BLE.stopAdvertise();
                BLE.setLocalName(args.c_str());
                BLE.advertise();
                Serial.print("BT name set to ");
                Serial.println(args);
            }
            else
            {
                Serial.println("Error opening BT name file.");
            }
        }
        else
        {
            Serial.println("Usage: setbtname <1-20 chars>");
        }
    }
    else if (cmd == "saveconfig")
    {
        StaticJsonDocument<1024> doc;
        auto arr = doc.createNestedArray("segments");
        for (auto *s : strip.getSegments())
        {
            JsonObject o = arr.createNestedObject();
            o["id"] = s->id();
            o["start"] = s->startIndex();
            o["end"] = s->endIndex();
            o["name"] = s->name().c_str();
            // record current effect
            auto e = s->currentEffect();
            if (e == PixelStrip::Segment::SegmentEffect::RAINBOW)
            {
                o["effect"] = "rainbow";
                o["speed"] = s->effectParam();
            }
            else if (e == PixelStrip::Segment::SegmentEffect::SOLID)
            {
                o["effect"] = "solid";
                auto c = s->primaryColor();
                auto carr = o.createNestedArray("color");
                carr.add(c.R);
                carr.add(c.G);
                carr.add(c.B);
            }
        }
        if (saveConfig(doc))
            Serial.println("Configuration saved");
        else
            Serial.println("Save failed");
    }
    else if (cmd == "clearconfig")
    {
        LittleFS.remove(STATE_FILE);
        Serial.println("Configuration cleared");
    }
    else
    {
        Serial.print("Unknown command: ");
        Serial.println(cmd);
    }
}

// Process serial input
inline void processSerial()
{
    if (!Serial.available())
        return;
    String line = Serial.readStringUntil('\n');
    handleCommandLine(line);
}

// Process audio-triggered updates
inline void processAudio()
{
    if (samplesRead <= 0)
        return;
    audioTrigger.update(sampleBuffer);
    samplesRead = 0;
}

// Process accelerometer data
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

// Heartbeat LED cycle
inline void updateHeartbeat()
{
    if (millis() - lastHbChange < HB_INTERVAL_MS)
        return;
    lastHbChange = millis();
    WiFiDrv::analogWrite(LEDR_PIN, 0);
    WiFiDrv::analogWrite(LEDG_PIN, 0);
    WiFiDrv::analogWrite(LEDB_PIN, 0);
    switch (hbColor)
    {
    case HeartbeatColor::RED:
        WiFiDrv::analogWrite(LEDR_PIN, 255);
        hbColor = HeartbeatColor::GREEN;
        break;
    case HeartbeatColor::GREEN:
        WiFiDrv::analogWrite(LEDG_PIN, 255);
        hbColor = HeartbeatColor::BLUE;
        break;
    case HeartbeatColor::BLUE:
        WiFiDrv::analogWrite(LEDB_PIN, 255);
        hbColor = HeartbeatColor::RED;
        break;
    }
}

// Process BLE commands
inline void processBLE()
{
    BLEDevice central = BLE.central();
    if (!central)
        return;
    while (central.connected())
    {
        BLE.poll();
        if (cmdCharacteristic.written())
        {
            String line((const char *)cmdCharacteristic.value(), cmdCharacteristic.valueLength());
            handleCommandLine(line);
        }
    }
}

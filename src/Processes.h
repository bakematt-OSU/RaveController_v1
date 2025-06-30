// Processes.h
// Declarations and implementations for processing routines

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

// ——————————————
// Process serial commands
// ——————————————
inline void processSerial()
{
    // WAIT FOR SERIAL INPUT
    if (!Serial.available())
        return;

    // READ THE COMMAND LINE
    String line = Serial.readStringUntil('\n');
    line.trim();

    // SPLIT INTO COMMAND AND ARGUMENTS
    int spaceIdx = line.indexOf(' ');
    String cmd = (spaceIdx >= 0) ? line.substring(0, spaceIdx) : line;
    String args = (spaceIdx >= 0) ? line.substring(spaceIdx + 1) : String();
    cmd.toLowerCase();

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
            extern uint8_t activeR, activeG, activeB;
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
    // UNKNOWN
    else
    {
        Serial.print("Unknown command: ");
        Serial.println(cmd);
    }
}

// Process audio-triggered updates
inline void processAudio()
{
    if (samplesRead <= 0)
        return;
    audioTrigger.update(sampleBuffer);
    samplesRead = 0;
}

// Process accelerometer data and detect steps
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

// Update RGB heartbeat LEDs
inline void updateHeartbeat()
{
    if (millis() - lastHbChange < HB_INTERVAL_MS)
        return;
    lastHbChange = millis();

    // Turn off all heartbeat LEDs
    WiFiDrv::analogWrite(LEDR_PIN, 0);
    WiFiDrv::analogWrite(LEDG_PIN, 0);
    WiFiDrv::analogWrite(LEDB_PIN, 0);

    // Cycle colors
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

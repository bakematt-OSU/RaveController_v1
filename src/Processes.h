#pragma once

#include "config.h"
#include <Arduino.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <math.h>
#include <PDM.h>
#include "PixelStrip.h"
#include "Triggers.h"
#include "EffectLookup.h"
#include <LittleFS_Mbed_RP2040.h>
#include <stdio.h>

//-----------------------------------------------------------------------------
// External globals defined in main.cpp
//----------------------------------------------------------------------------- 
extern volatile int16_t sampleBuffer[];    // Audio sample buffer
extern volatile int samplesRead;            // New audio samples flag/count
extern float accelX, accelY, accelZ;       // Latest accelerometer readings
extern volatile bool triggerRipple;         // Flag to start ripple on motion
extern unsigned long lastStepTime;          // Timestamp of last motion event
extern bool debugAccel;                     // Enable accelerometer debug prints
extern PixelStrip strip;                    // NeoPixel strip controller
extern PixelStrip::Segment *seg;            // Currently active segment
extern AudioTrigger<SAMPLES> audioTrigger;  // Audio trigger processor
extern HeartbeatColor hbColor;              // Heartbeat LED color state
extern unsigned long lastHbChange;          // Timestamp for heartbeat change
extern uint8_t activeR, activeG, activeB;   // Global RGB for next effect

// BLE characteristic declared in main.cpp
extern BLECharacteristic cmdCharacteristic;
static BLEDevice connectedCentral;          // Connected BLE central device

//-----------------------------------------------------------------------------
// handleCommandLine(): parse and execute a single command
//----------------------------------------------------------------------------- 
inline void handleCommandLine(const String &line) {
    // Trim and split command
    String cmdLine = line; cmdLine.trim();
    if (cmdLine.isEmpty()) return;
    int sp = cmdLine.indexOf(' ');
    String cmd = sp > 0 ? cmdLine.substring(0, sp) : cmdLine;
    String args = sp > 0 ? cmdLine.substring(sp + 1) : String();
    cmd.toLowerCase();  // case-insensitive

    // --- Segment management ---
    if (cmd == "clearsegments") {
        // Reset segments
        strip.clearUserSegments();
        seg = strip.getSegments()[0];
        seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);
        Serial.println("Segments cleared; active = 0");
    }
    else if (cmd == "addsegment") {
        // addsegment <start> <end>
        int d = args.indexOf(' ');
        if (d < 0) Serial.println("Usage: addsegment <start> <end>");
        else {
            int start = args.substring(0, d).toInt();
            int end = args.substring(d + 1).toInt();
            if (end < start) Serial.println("Error: end < start");
            else {
                strip.addSection(start, end, "seg" + String(strip.getSegments().size()));
                uint8_t idx = strip.getSegments().size() - 1;
                Serial.print("Added segment "); Serial.print(idx);
                Serial.print(" ["); Serial.print(start);
                Serial.print("-"); Serial.print(end);
                Serial.println("]");
            }
        }
    }
    else if (cmd == "select") {
        // select <index>
        int idx = args.toInt();
        auto &vec = strip.getSegments();
        if (idx < 0 || idx >= (int)vec.size()) Serial.println("Invalid segment index");
        else {
            seg = vec[idx];
            Serial.print("Active segment = "); Serial.println(idx);
        }
    }

    // --- Color & Effects ---
    else if (cmd == "setcolor") {
        // setcolor <r> <g> <b>
        int a = args.indexOf(' '), b = args.indexOf(' ', a + 1);
        if (a < 0 || b < 0) Serial.println("Usage: setcolor <r> <g> <b>");
        else {
            activeR = args.substring(0, a).toInt();
            activeG = args.substring(a + 1, b).toInt();
            activeB = args.substring(b + 1).toInt();
            Serial.print("Color = R="); Serial.print(activeR);
            Serial.print(" G="); Serial.print(activeG);
            Serial.print(" B="); Serial.println(activeB);
        }
    }
    else if (cmd == "seteffect") {
        // seteffect <name>
        EffectType e = effectFromString(args);
        if (e == EffectType::UNKNOWN) Serial.println("Invalid effect");
        else {
            applyEffectToSegment(seg, e);
            Serial.print("Effect → "); Serial.println(args);
        }
    }

    // --- Brightness ---
    else if (cmd == "setbrightness") {
        // setbrightness <0-255>
        uint8_t b = constrain(args.toInt(), 0, 255);
        strip.setActiveBrightness(b);
        strip.show();
        Serial.print("Global brightness = "); Serial.println(b);
    }
    else if (cmd == "setsegbrightness") {
        // setsegbrightness <idx> <0-255>
        int d = args.indexOf(' ');
        if (d < 0) Serial.println("Usage: setsegbrightness <idx> <0-255>");
        else {
            int idx = args.substring(0, d).toInt();
            uint8_t b = constrain(args.substring(d + 1).toInt(), 0, 255);
            auto &vec = strip.getSegments();
            if (idx < 0 || idx >= (int)vec.size()) Serial.println("Invalid segment index");
            else {
                vec[idx]->setBrightness(b);
                vec[idx]->update(); strip.show();
                Serial.print("Segment "); Serial.print(idx);
                Serial.print(" brightness = "); Serial.println(b);
            }
        }
    }

    // --- BLE Name Persistence ---
    else if (cmd == "setbtname") {
        // setbtname <1-20 chars>
        args.trim();
        if (args.length() < 1 || args.length() > 20) Serial.println("Usage: setbtname <1-20 chars>");
        else {
            FILE *f = fopen(BT_NAME_FILE, "w");
            if (!f) Serial.println("Error opening file");
            else {
                fputs(args.c_str(), f); fputc('\n', f); fclose(f);
                BLE.stopAdvertise(); BLE.setLocalName(args.c_str()); BLE.advertise();
                Serial.print("BT name = "); Serial.println(args);
            }
        }
    }

    // --- List Commands ---
    else if (cmd == "listeffects") {
        // List available effects
        Serial.println("Effects:");
        for (uint8_t i = 0; i < EFFECT_COUNT; ++i)
            Serial.println("  " + String(EFFECT_NAMES[i]));
    }
    else if (cmd == "listeffectsjson") {
        // JSON array of effects
        Serial.print("[");
        for (uint8_t i = 0; i < EFFECT_COUNT; ++i) {
            Serial.print("\""); Serial.print(EFFECT_NAMES[i]); Serial.print("\"");
            if (i + 1 < EFFECT_COUNT) Serial.print(",");
        }
        Serial.println("]");
    }
    else if (cmd == "listsegments") {
        // List configured segments
        Serial.println("Segments:");
        auto &v = strip.getSegments();
        for (auto *s : v) {
            Serial.print("  idx="); Serial.print(s->getId());
            Serial.print(" name="); Serial.print(s->getName());
            Serial.print(" range="); Serial.print(s->startIndex());
            Serial.print("-"); Serial.print(s->endIndex());
            Serial.print(" bright="); Serial.print(s->getBrightness());
            Serial.print(" effect="); Serial.println(EFFECT_NAMES[(uint8_t)s->activeEffect]);
        }
    }
    else if (cmd == "listsegmentsjson") {
        // JSON array of segment objects
        auto &v = strip.getSegments();
        Serial.print("[");
        for (size_t i = 0; i < v.size(); ++i) {
            auto *s = v[i];
            Serial.print("{\"id\":"); Serial.print(s->getId());
            Serial.print(",\"name\":\""); Serial.print(s->getName()); Serial.print("\"");
            Serial.print(",\"start\":"); Serial.print(s->startIndex());
            Serial.print(",\"end\":"); Serial.print(s->endIndex());
            Serial.print(",\"brightness\":"); Serial.print(s->getBrightness());
            Serial.print(",\"effect\":\""); Serial.print(EFFECT_NAMES[(uint8_t)s->activeEffect]); Serial.print("\"}");
            if (i + 1 < v.size()) Serial.print(",");
        }
        Serial.println("]");
    }
    else if (cmd == "listeffectattrs") {
        // List effect parameters (human)
        Serial.println("Effect parameters:");
        Serial.println("  Fire: fireSparking, fireCooling");
        Serial.println("  ColoredFire: fireColor1, fireColor2, fireColor3");
        Serial.println("  KineticRipple: rippleWidth, rippleSpeed");
        Serial.println("  (others: color, brightness only)");
    }
    else if (cmd == "listeffectattrsjson") {
        // JSON effect parameters
        Serial.println("[{\"effect\":\"Fire\",\"params\":[\"fireSparking\",\"fireCooling\"]},");
        Serial.println("{\"effect\":\"ColoredFire\",\"params\":[\"fireColor1\",\"fireColor2\",\"fireColor3\"]},");
        Serial.println("{\"effect\":\"KineticRipple\",\"params\":[\"rippleWidth\",\"rippleSpeed\"]}]");
    }
    
    // --- Unknown command ---
    else {
        Serial.print("Unknown command: "); Serial.println(cmd);
    }
}

//-----------------------------------------------------------------------------  
// processSerial(): read and dispatch Serial commands
//-----------------------------------------------------------------------------  
inline void processSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n'); line.trim();
    Serial.print("Received command: "); Serial.println(line);
    handleCommandLine(line);
    Serial.print("Command processed: "); Serial.println(line);
}

//-----------------------------------------------------------------------------  
// processAudio(): handle new audio samples
//-----------------------------------------------------------------------------  
inline void processAudio() {
    if (samplesRead <= 0) return;
    audioTrigger.update(sampleBuffer);
    samplesRead = 0;
}

//-----------------------------------------------------------------------------  
// processAccel(): read accelerometer and trigger ripple on motion
//-----------------------------------------------------------------------------  
inline void processAccel() {
    if (!IMU.accelerationAvailable()) return;
    IMU.readAcceleration(accelX, accelY, accelZ);
    float magnitude = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
    if (debugAccel && millis() - lastHbChange > 250) {
        Serial.print("Accel magnitude: "); Serial.println(magnitude);
        lastHbChange = millis();
    }
    if (magnitude > STEP_THRESHOLD && millis() - lastStepTime > STEP_COOLDOWN_MS) {
        triggerRipple = true;
        lastStepTime = millis();
    }
}

//-----------------------------------------------------------------------------  
// updateDigHeartbeat(): blink on-board LED as a heartbeat indicator
//-----------------------------------------------------------------------------  
inline void updateDigHeartbeat() {
    static uint8_t state = 0;
    if (millis() - lastHbChange < HB_INTERVAL_MS) return;
    lastHbChange = millis();
    // Cycle through R, G, B channels
    digitalWrite(LEDR_PIN, LOW);
    digitalWrite(LEDG_PIN, LOW);
    digitalWrite(LEDB_PIN, LOW);
    switch (state) {
        case 0: digitalWrite(LEDR_PIN, HIGH); break;
        case 1: digitalWrite(LEDG_PIN, HIGH); break;
        case 2: digitalWrite(LEDB_PIN, HIGH); break;
    }
    state = (state + 1) % 3;
}

//-----------------------------------------------------------------------------  
// processBLE(): handle BLE central connections and commands
//-----------------------------------------------------------------------------  
inline void processBLE() {
    // Accept new central
    if (!connectedCentral) {
        connectedCentral = BLE.central();
        if (connectedCentral) Serial.println("[BLE] Central connected");
    }
    // Handle connected central
    if (connectedCentral && connectedCentral.connected()) {
        BLE.poll();
        if (cmdCharacteristic.written()) {
            String cmd = String((char*)cmdCharacteristic.value(), cmdCharacteristic.valueLength());
            cmd.trim();
            Serial.print("[BLE] "); Serial.println(cmd);
            handleCommandLine(cmd);
        }
    }
    // Detect disconnect
    else if (connectedCentral) {
        Serial.println("[BLE] Central disconnected");
        connectedCentral = BLEDevice();
    }
}

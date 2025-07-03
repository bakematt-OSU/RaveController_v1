#pragma once

#include "config.h"
#include <Arduino.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>             // JSON parsing for batch config
#include <math.h>
#include <PDM.h>
#include "PixelStrip.h"
#include "Triggers.h"
#include "EffectLookup.h"
#include <LittleFS_Mbed_RP2040.h>
#include <stdio.h>

//-----------------------------------------------------------------------------  
// Binary command IDs for quick BLE/Android control
//-----------------------------------------------------------------------------  
static constexpr uint8_t CMD_SET_COLOR         = 0x01;
static constexpr uint8_t CMD_SET_EFFECT        = 0x02;
static constexpr uint8_t CMD_SET_BRIGHTNESS    = 0x03;
static constexpr uint8_t CMD_SET_SEG_BRIGHT    = 0x04;
static constexpr uint8_t CMD_SELECT_SEGMENT    = 0x05;
static constexpr uint8_t CMD_CLEAR_SEGMENTS    = 0x06;
static constexpr uint8_t CMD_SET_SEG_RANGE     = 0x07;
static constexpr uint8_t CMD_GET_STATUS        = 0x08;
static constexpr uint8_t CMD_BATCH_CONFIG      = 0x09;

//-----------------------------------------------------------------------------  
// External globals defined in main.cpp
//-----------------------------------------------------------------------------
extern volatile int16_t sampleBuffer[];    // Audio sample buffer
extern volatile int samplesRead;            // New audio samples flag/count
extern float accelX, accelY, accelZ;       // Accelerometer readings
extern volatile bool triggerRipple;         // Motion trigger flag
extern unsigned long lastStepTime;          // Last motion timestamp
extern bool debugAccel;                     // Enable accel debug prints
extern PixelStrip strip;                    // NeoPixel strip controller
extern PixelStrip::Segment *seg;            // Active segment pointer
extern AudioTrigger<SAMPLES> audioTrigger;  // Audio trigger handler
extern HeartbeatColor hbColor;              // Heartbeat LED color state
extern unsigned long lastHbChange;          // Heartbeat timestamp
extern uint8_t activeR, activeG, activeB;   // Next effect RGB values

extern BLECharacteristic cmdCharacteristic;  // BLE UART characteristic
static BLEDevice connectedCentral;            // Connected BLE central

//-----------------------------------------------------------------------------  
// handleBatchConfigJson(): apply full configuration from JSON
//-----------------------------------------------------------------------------
inline void handleBatchConfigJson(const String &json) {
    StaticJsonDocument<1024> doc;
    auto err = deserializeJson(doc, json);
    if (err) {
        Serial.println("BatchConfig: JSON parse error");
        return;
    }
    // Clear existing segments
    strip.clearUserSegments();
    // Configure segments
    if (doc.containsKey("segments")) {
        for (auto item : doc["segments"].as<JsonArray>()) {
            uint16_t start = item["start"].as<uint16_t>();
            uint16_t end   = item["end"].as<uint16_t>();
            String name    = item["name"].as<const char*>();
            strip.addSection(start, end, name);
            uint8_t idx = strip.getSegments().size() - 1;
            // per-segment brightness
            if (item.containsKey("brightness")) {
                strip.getSegments()[idx]->setBrightness(item["brightness"].as<uint8_t>());
            }
            // per-segment effect
            if (item.containsKey("effect")) {
                String eff = item["effect"].as<const char*>();
                EffectType e = effectFromString(eff);
                if (e != EffectType::UNKNOWN) applyEffectToSegment(strip.getSegments()[idx], e);
            }
        }
    }
    // Global brightness
    if (doc.containsKey("brightness")) {
        strip.setActiveBrightness(doc["brightness"].as<uint8_t>());
    }
    // Global color
    if (doc.containsKey("color")) {
        auto col = doc["color"].as<JsonArray>();
        if (col.size() == 3) {
            activeR = col[0]; activeG = col[1]; activeB = col[2];
        }
    }
    strip.show();
    Serial.println("Batch configuration applied");
}

//-----------------------------------------------------------------------------  
// handleCommandLine(): execute ASCII commands from Serial or BLE
//-----------------------------------------------------------------------------
inline void handleCommandLine(const String &line) {
    String cmdLine = line; cmdLine.trim();
    if (cmdLine.isEmpty()) return;
    int sp = cmdLine.indexOf(' ');
    String cmd = sp > 0 ? cmdLine.substring(0, sp) : cmdLine;
    String args = sp > 0 ? cmdLine.substring(sp + 1) : String();
    cmd.toLowerCase();

    // Segment management
    if (cmd == "clearsegments") {
        strip.clearUserSegments(); seg = strip.getSegments()[0];
        seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);
        Serial.println("Segments cleared; active = 0");
    }
    else if (cmd == "addsegment") {
        int d = args.indexOf(' ');
        if (d < 0) Serial.println("Usage: addsegment <start> <end>");
        else {
            int start = args.substring(0, d).toInt();
            int end   = args.substring(d + 1).toInt();
            if (end < start) Serial.println("Error: end < start");
            else {
                strip.addSection(start, end, "seg" + String(strip.getSegments().size()));
                Serial.printf("Added segment %u [%u-%u]\n", strip.getSegments().size()-1, start, end);
            }
        }
    }
    else if (cmd == "select") {
        int idx = args.toInt(); auto &v = strip.getSegments();
        if (idx < 0 || idx >= (int)v.size()) Serial.println("Invalid segment index");
        else { seg = v[idx]; Serial.printf("Active segment = %u\n", idx); }
    }
    else if (cmd == "setsegrange") {
        int p1 = args.indexOf(' '), p2 = args.indexOf(' ', p1+1);
        if (p1 < 0 || p2 < 0) Serial.println("Usage: setsegrange <idx> <start> <end>");
        else {
            int idx = args.substring(0,p1).toInt();
            int start = args.substring(p1+1,p2).toInt();
            int end   = args.substring(p2+1).toInt();
            auto &v = strip.getSegments();
            if (idx<0||idx>=(int)v.size()) Serial.println("Invalid segment index");
            else { v[idx]->setRange(start,end); Serial.printf("Segment %u range=%u-%u\n",idx,start,end); }
        }
    }
    // Color & effects
    else if (cmd == "setcolor") {
        int a = args.indexOf(' '), b = args.indexOf(' ', a+1);
        if (a<0||b<0) Serial.println("Usage: setcolor <r> <g> <b>");
        else {
            activeR=args.substring(0,a).toInt(); activeG=args.substring(a+1,b).toInt(); activeB=args.substring(b+1).toInt();
            Serial.printf("Color set R=%u G=%u B=%u\n",activeR,activeG,activeB);
        }
    }
    else if (cmd == "seteffect") {
        EffectType e = effectFromString(args);
        if (e==EffectType::UNKNOWN) Serial.println("Invalid effect");
        else { applyEffectToSegment(seg,e); Serial.printf("Effect -> %s\n", args.c_str()); }
    }
    // Brightness
    else if (cmd == "setbrightness") {
        uint8_t b=constrain(args.toInt(),0,255); strip.setActiveBrightness(b); strip.show();
        Serial.printf("Global brightness=%u\n",b);
    }
    else if (cmd == "setsegbrightness") {
        int d=args.indexOf(' ');
        if(d<0) Serial.println("Usage: setsegbrightness <idx> <0-255>");
        else {
            int idx=args.substring(0,d).toInt(); uint8_t b=constrain(args.substring(d+1).toInt(),0,255);
            auto &v=strip.getSegments();
            if(idx<0||idx>=(int)v.size()) Serial.println("Invalid segment index");
            else { v[idx]->setBrightness(b); v[idx]->update(); strip.show();
                   Serial.printf("Segment %u brightness=%u\n",idx,b); }
        }
    }
    // BLE name
    else if (cmd=="setbtname") {
        args.trim();
        if(args.length()<1||args.length()>20) Serial.println("Usage: setbtname <1-20 chars>");
        else {
            FILE*f=fopen(BT_NAME_FILE,"w"); if(!f) Serial.println("Error opening file");
            else { fputs(args.c_str(),f); fputc('\n',f); fclose(f);
                   BLE.stopAdvertise(); BLE.setLocalName(args.c_str()); BLE.advertise();
                   Serial.printf("BT name=%s\n",args.c_str()); }
        }
    }
    // Batchconfig via ASCII
    else if (cmd=="batchconfig") {
        handleBatchConfigJson(args);
    }
    // Unknown
    else {
        Serial.printf("Unknown cmd: %s\n",cmd.c_str());
    }
}

//-----------------------------------------------------------------------------  
// processSerial(): read and dispatch USB Serial commands
//-----------------------------------------------------------------------------
inline void processSerial() {
    if(!Serial.available()) return;
    String line=Serial.readStringUntil('\n'); line.trim();
    Serial.print("USB cmd: "); Serial.println(line);
    handleCommandLine(line);
}

//-----------------------------------------------------------------------------  
// processBLE(): handle BLE writes, binary or ASCII
//-----------------------------------------------------------------------------
inline void processBLE() {
    if(!connectedCentral) {
        connectedCentral=BLE.central(); if(connectedCentral) Serial.println("[BLE] Central connected");
    }
    if(connectedCentral && connectedCentral.connected()) {
        BLE.poll();
        if(cmdCharacteristic.written()) {
            uint8_t* data=(uint8_t*)cmdCharacteristic.value(); size_t len=cmdCharacteristic.valueLength();
            if(len>0 && data[0]<0x20) {
                switch(data[0]) {
                    case CMD_SET_COLOR:
                        if(len>=4) { activeR=data[1]; activeG=data[2]; activeB=data[3]; strip.show(); }
                        break;
                    case CMD_SET_EFFECT:
                        if(len>=2 && data[1]<EFFECT_COUNT) applyEffectToSegment(seg,(EffectType)data[1]);
                        break;
                    case CMD_SET_BRIGHTNESS:
                        if(len>=2) strip.setActiveBrightness(data[1]), strip.show();
                        break;
                    case CMD_SET_SEG_BRIGHT:
                        if(len>=3 && data[1]<strip.getSegments().size()) {
                            auto*s=strip.getSegments()[data[1]]; s->setBrightness(data[2]); s->update(); strip.show(); }
                        break;
                    case CMD_SELECT_SEGMENT:
                        if(len>=2 && data[1]<strip.getSegments().size()) seg=strip.getSegments()[data[1]];
                        break;
                    case CMD_CLEAR_SEGMENTS:
                        strip.clearUserSegments(); seg=strip.getSegments()[0];
                        break;
                    case CMD_SET_SEG_RANGE:
                        if(len>=6) {
                            uint8_t idx=data[1]; uint16_t s=(data[2]<<8)|data[3], e=(data[4]<<8)|data[5];
                            auto&v=strip.getSegments(); if(idx<v.size()) v[idx]->setRange(s,e);
                        }
                        break;
                    case CMD_GET_STATUS: {
                        // build status JSON
                        String resp="{";
                        // effects
                        resp+="\"effects\":[";
                        for(uint8_t i=0;i<EFFECT_COUNT;i++){ resp+='"'; resp+=EFFECT_NAMES[i]; resp+='"'; if(i+1<EFFECT_COUNT) resp+=','; }
                        resp+="] ,\"segments\":[";
                        auto&v=strip.getSegments(); for(size_t i=0;i<v.size();i++){
                            auto*s=v[i];
                            resp+="{\"id\":"+String(s->getId())+",";
                            resp+="name\":\""+s->getName()+"\",";
                            resp+="start\":"+String(s->startIndex())+",";
                            resp+="end\":"+String(s->endIndex())+",";
                            resp+="brightness\":"+String(s->getBrightness())+",";
                            resp+="effect\":\""+EFFECT_NAMES[(uint8_t)s->activeEffect]+"\"}";
                            if(i+1<v.size()) resp+=',';
                        }
                        resp+="]}";
                        cmdCharacteristic.writeValue(resp);
                        break;
                    }
                    case CMD_BATCH_CONFIG:
                        if(len>1) {
                            String payload((char*)&data[1],len-1);
                            handleBatchConfigJson(payload);
                        }
                        break;
                }
            } else if(len>0 && data[0]=='{') {
                // JSON command
                String payload((char*)data,len);
                handleBatchConfigJson(payload);
            } else {
                // ASCII fallback
                String txt((char*)data,len); txt.trim();
                Serial.print("[BLE] "); Serial.println(txt);
                handleCommandLine(txt);
            }
        }
    } else if(connectedCentral) {
        Serial.println("[BLE] Central disconnected");
        connectedCentral=BLEDevice();
    }
}

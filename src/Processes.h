// In src/Processes.h

#pragma once

#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "PixelStrip.h"
#include "EffectLookup.h"

// --- Function Declarations ---
void handleBatchConfigJson(const String &json);
void saveConfig();
void loadConfig();
void handleBinarySerial(const uint8_t *data, size_t len);
void handleCommandLine(const String &line);
void setSegmentEffect(PixelStrip::Segment *s, EffectType e, const char* effectName);
PixelStrip::Segment* findSegmentByIndex(const String& args, String& remainingArgs);
const char *getBLECmdName(uint8_t cmd);
void processSerial();
void processAudio();
void processAccel();
void updateDigHeartbeat();
void sendBleHeartbeat();
void processBLE();
#pragma once

#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "PixelStrip.h"
#include "effects/BaseEffect.h"

// CORRECTED: Only include the master effects header to prevent redefinitions.
#include "effects/Effects.h"

// --- Function Declarations ---
void handleBatchConfigJson(const String &json);
void saveConfig();
void loadConfig();
void handleBinarySerial(const uint8_t *data, size_t len);
void handleCommandLine(const String &line);
BaseEffect* createEffectByName(const String& name, PixelStrip::Segment *seg);
PixelStrip::Segment* findSegmentByIndex(const String& args, String& remainingArgs);
const char *getBLECmdName(uint8_t cmd);
void processSerial();
void processAudio();
void processAccel();
void updateDigHeartbeat();
void sendBleHeartbeat();
void processBLE();
#pragma once

#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "PixelStrip.h"
#include "effects/BaseEffect.h"
#include "effects/Effects.h"
#include "ConfigManager.h" // Use the declarations from the official manager

// --- Function Declarations ---
// All function declarations are now managed by their respective headers (e.g., ConfigManager.h)
// We only need to declare functions specific to this older "Processes" architecture if any remain.

// For legacy compatibility if still used elsewhere:
BaseEffect* createEffectByName(const String& name, PixelStrip::Segment *seg);
PixelStrip::Segment* findSegmentByIndex(const String& args, String& remainingArgs);
const char *getBLECmdName(uint8_t cmd);
void processSerial();
void processAudio();
void processAccel();
void updateDigHeartbeat();
void sendBleHeartbeat();
void processBLE();
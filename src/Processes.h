#pragma once

#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "PixelStrip.h"
#include "effects/BaseEffect.h"

// Add includes for all your effects:
#include "effects/Fire.h"
#include "effects/SolidColor.h"
#include "effects/RainbowChase.h"
#include "effects/RainbowCycle.h"
#include "effects/AccelMeter.h"
#include "effects/Flare.h"
#include "effects/FlashOnTrigger.h"
#include "effects/KineticRipple.h"
#include "effects/TheaterChase.h"
#include "effects/ColoredFire.h" // <-- ADDED THIS LINE

// --- Function Declarations ---
void handleBatchConfigJson(const String &json);
void saveConfig();
void loadConfig();
void handleBinarySerial(const uint8_t *data, size_t len);
void handleCommandLine(const String &line);

// Modernized: effect switch by name
BaseEffect* createEffectByName(const String& name, PixelStrip::Segment *seg);

PixelStrip::Segment* findSegmentByIndex(const String& args, String& remainingArgs);
const char *getBLECmdName(uint8_t cmd);
void processSerial();
void processAudio();
void processAccel();
void updateDigHeartbeat();
void sendBleHeartbeat();
void processBLE();
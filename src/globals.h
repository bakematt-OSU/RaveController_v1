#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include "config.h"
#include "Triggers.h"
#include "PixelStrip.h"
#include <LittleFS_Mbed_RP2040.h>

// Forward declarations for our main manager classes
class BLEManager;
class CommandHandler;

// --- Global Object Instances ---
extern BLEManager& bleManager;
extern CommandHandler commandHandler;

// --- LED Strip & Segments ---
extern PixelStrip* strip;
extern PixelStrip::Segment* seg;
extern uint16_t LED_COUNT;
extern const char* STATE_FILE;

// --- Audio Processing ---
extern AudioTrigger<SAMPLES> audioTrigger;
extern volatile int16_t sampleBuffer[SAMPLES];
extern volatile int samplesRead;

// --- Accelerometer & Motion ---
extern float accelX, accelY, accelZ;
extern volatile bool triggerRipple; // <-- FIX: Added extern declaration

#endif // GLOBALS_H

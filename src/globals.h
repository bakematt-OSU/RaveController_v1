#pragma once

#include <Arduino.h>
#include "config.h"
#include "Triggers.h"
#include "PixelStrip.h"
#include <LittleFS_Mbed_RP2040.h>

// This file contains the declarations for all global variables.
// They are defined in main.cpp to ensure there is a single source of truth.

// --- LED Strip & Segments ---
extern PixelStrip* strip;
extern PixelStrip::Segment *seg;
extern uint16_t LED_COUNT;

// --- Color & Effects ---
extern uint8_t activeR;
extern uint8_t activeG;
extern uint8_t activeB;

// --- Audio Processing ---
extern AudioTrigger<SAMPLES> audioTrigger;
extern volatile int16_t sampleBuffer[SAMPLES];
extern volatile int samplesRead;

// --- Accelerometer & Motion ---
extern float accelX, accelY, accelZ;
extern volatile bool triggerRipple;
extern unsigned long lastStepTime;
extern bool debugAccel;

// --- System & State ---
extern LittleFS_MBED myFS;
extern HeartbeatColor hbColor;
extern unsigned long lastHbChange;

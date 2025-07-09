#include <Arduino.h>
#include "config.h"
#include "PixelStrip.h"
#include "Triggers.h"
#include <PDM.h>
#include "Init.h"
#include "Processes.h"
#include "globals.h" // <-- Includes all global declarations

//================================================================
// GLOBAL VARIABLE DEFINITIONS
// This is the single source of truth for all global variables.
//================================================================

// --- Filesystem ---
LittleFS_MBED myFS;

// --- LED Strip & Segments ---
PixelStrip* strip = nullptr;
PixelStrip::Segment *seg = nullptr;
uint16_t LED_COUNT = 45; // Default value, will be overwritten by config

// --- Color & Effects ---
uint8_t activeR = 255;
uint8_t activeG = 0;
uint8_t activeB = 255;

// --- Audio Processing ---
AudioTrigger<SAMPLES> audioTrigger;
volatile int16_t sampleBuffer[SAMPLES];
volatile int samplesRead = 0;

// --- Accelerometer & Motion ---
float accelX, accelY, accelZ;
volatile bool triggerRipple = false;
unsigned long lastStepTime = 0;
bool debugAccel = false;

// --- System & State ---
HeartbeatColor hbColor = HeartbeatColor::RED;
volatile bool saveConfigRequested = false; // <-- DEFINE the new flag
unsigned long lastHbChange = 0;


//================================================================
// SETUP & LOOP
//================================================================

void setup()
{
    // Initialize core components first
    initSerial();
    initFS(); // Mount the global filesystem once

    // --- Staged Configuration Loading ---

    // 1. Load the configuration string from the filesystem
    String configJson = loadConfig();

    // 2. Pre-strip Initialization: Use config to set the LED_COUNT
    if (configJson.length() > 0) {
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, configJson);
        if (error) {
            Serial.print("Failed to parse config in setup: ");
            Serial.println(error.c_str());
        } else {
            // Set the global LED_COUNT before initializing the strip
            LED_COUNT = doc["led_count"] | 45;
        }
    }

    // 3. Initialize hardware that may depend on config values (like LED_COUNT)
    initIMU();
    initAudio();
    initLEDs(); // This function now uses the correct, loaded LED_COUNT

    // 4. Post-strip Initialization: Apply the rest of the config (segments, effects)
    if (configJson.length() > 0) {
        handleBatchConfigJson(configJson);
        Serial.println("Applied full configuration from file.");
    }

    // Initialize remaining services
    initBLE(); // Must be last to advertise correctly configured services


    Serial.println("\n--- Performing automatic save test at end of setup... ---");
    saveConfig(); // <-- THE TEST: Call saveConfig here.

    Serial.println("Setup complete. Entering main loop...");
}

void loop()
{
    // Process all ongoing tasks
    processSerial();
    processBLE();
    processAudio();
    processAccel();
    // updateDigHeartbeat();

    // Update all LED segments
    if (strip) {
        for (auto *s : strip->getSegments())
        {
            s->update();
        }

        // Show the final result on the strip
        strip->show();
    }
}
#include <Arduino.h>
#include "config.h"
#include "PixelStrip.h"
#include "Triggers.h"
#include <PDM.h>
#include "Init.h"
#include "Processes.h"
#include <LittleFS_Mbed_RP2040.h> // Include LittleFS header

//================================================================
// GLOBAL VARIABLE DEFINITIONS
//================================================================

// --- User-selected color globals (used by effects) ---
uint8_t activeR = 255;
uint8_t activeG = 0;
uint8_t activeB = 255;

// --- Hardware & Library Objects ---
LittleFS_MBED myFS; // Define the filesystem object globally
uint16_t LED_COUNT = 45; // Default value
PixelStrip* strip = nullptr; // Changed to a pointer
PixelStrip::Segment *seg; // Pointer to the currently active segment
AudioTrigger<SAMPLES> audioTrigger;

// --- Sensor & State Variables ---
float accelX, accelY, accelZ;
volatile bool triggerRipple = false;
volatile int16_t sampleBuffer[SAMPLES];
volatile int samplesRead = 0;
unsigned long lastStepTime = 0;
bool debugAccel = false;

// --- Heartbeat LED ---
HeartbeatColor hbColor = HeartbeatColor::RED;
unsigned long lastHbChange = 0;

//================================================================
// SETUP & LOOP
//================================================================

void setup()
{
    // Initialize core components
    initSerial();
    initFS(); // Must be before BLE init to load name
    initIMU();
    initAudio();
    initLEDs();
    initBLE(); // Must be last to advertise correctly configured services

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
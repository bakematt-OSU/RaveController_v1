#include <Arduino.h>
#include "config.h"
#include "PixelStrip.h"
#include "Triggers.h"
#include <PDM.h>
#include "Init.h"
#include "Processes.h"

//================================================================
// GLOBAL VARIABLE DEFINITIONS
//================================================================

// --- User-selected color globals (used by effects) ---
uint8_t activeR = 255;
uint8_t activeG = 0;
uint8_t activeB = 255;

// --- Hardware & Library Objects ---
PixelStrip strip(LED_PIN, LED_COUNT, BRIGHTNESS, SEGMENT_COUNT);
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

    // Serial.println("→ Blink test start");
    // WiFiDrv::pinMode(LEDR_PIN, OUTPUT);
    // WiFiDrv::pinMode(LEDG_PIN, OUTPUT);
    // WiFiDrv::pinMode(LEDB_PIN, OUTPUT);
    // for (int i = 0; i < 3; ++i) {
    //     WiFiDrv::digitalWrite(LEDR_PIN, HIGH); delay(50);
    //     WiFiDrv::digitalWrite(LEDR_PIN, LOW);  delay(50);
    //     WiFiDrv::digitalWrite(LEDG_PIN, HIGH); delay(50);
    //     WiFiDrv::digitalWrite(LEDG_PIN, LOW);  delay(50);
    //     WiFiDrv::digitalWrite(LEDB_PIN, HIGH); delay(50);
    //     WiFiDrv::digitalWrite(LEDB_PIN, LOW);  delay(50);
    // }
    // Serial.println("→ Blink test end");
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
    for (auto *s : strip.getSegments())
    {
        s->update();
    }

    // Show the final result on the strip
    strip.show();
}

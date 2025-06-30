#include <Arduino.h>
#include "config.h"
#include "PixelStrip.h"
#include "Triggers.h"
#include <PDM.h>
#include "Init.h"
#include "Processes.h"

// —— User-selected color globals (must match the externs in Processes.h) ——
uint8_t activeR = 255;
uint8_t activeG = 255;
uint8_t activeB = 255;

// —— Globals ——
PixelStrip strip(LED_PIN, LED_COUNT, BRIGHTNESS, SEGMENT_COUNT);
PixelStrip::Segment *seg;
AudioTrigger<SAMPLES> audioTrigger;

float accelX, accelY, accelZ;
volatile bool triggerRipple = false;
volatile int16_t sampleBuffer[SAMPLES];
volatile int samplesRead;
unsigned long lastStepTime = 0;
bool debugAccel = false;

HeartbeatColor hbColor = HeartbeatColor::RED;
unsigned long lastHbChange = 0;

// —— Setup & Loop ——
void setup()
{
    initSerial();
    initIMU();
    initAudio();
    initLEDs();
    initFS();
    initStateFS(); // mount again for state.json

    // if there’s a saved state, restore segments+effects
    StaticJsonDocument<1024> stateDoc;
    if (loadConfig(stateDoc))
    {
        applyConfig(stateDoc);
        Serial.println("✔️ Restored segment state");
    }

    initBLE();
}

void loop()
{
    processSerial();
    processBLE();
    processAudio();
    processAccel();
    updateHeartbeat();

    for (auto *s : strip.getSegments())
    {
        s->update();
    }
    strip.show();
}
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
    Serial.println(F("→ Blink test start"));
    WiFiDrv::pinMode(LEDR_PIN, OUTPUT);
    WiFiDrv::pinMode(LEDG_PIN, OUTPUT);
    WiFiDrv::pinMode(LEDB_PIN, OUTPUT);
    for (int i = 0; i < 3; ++i) {
    WiFiDrv::digitalWrite(LEDR_PIN, HIGH); delay(50);
    WiFiDrv::digitalWrite(LEDR_PIN, LOW);  delay(50);
    WiFiDrv::digitalWrite(LEDG_PIN, HIGH); delay(50);
    WiFiDrv::digitalWrite(LEDG_PIN, LOW);  delay(50);
    WiFiDrv::digitalWrite(LEDB_PIN, HIGH); delay(50);
    WiFiDrv::digitalWrite(LEDB_PIN, LOW);  delay(50);
    }
    Serial.println(F("→ Blink test end"));
    initBLE();

}

void loop()
{
    processSerial();
    processBLE();
    processAudio();
    processAccel();
    updateDigHeartbeat();
    for (auto *s : strip.getSegments())
    {
        s->update();
    }

    strip.show();
}

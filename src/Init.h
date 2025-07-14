#pragma once

#include "config.h"
#include <Arduino.h>
#include <PDM.h>
#include <Arduino_LSM6DSOX.h>
#include "PixelStrip.h"
#include "Triggers.h"
#include <LittleFS_Mbed_RP2040.h>
#include "EffectLookup.h" // For createEffectByName and EFFECT_LIST macro

// --- Externally defined globals (from main.cpp) ---
extern volatile int16_t sampleBuffer[];
extern volatile int samplesRead;
extern PixelStrip* strip;
extern AudioTrigger<SAMPLES> audioTrigger;
extern PixelStrip::Segment* seg;
extern LittleFS_MBED myFS;
extern uint16_t LED_COUNT;

// --- Initialization Functions ---

inline void initSerial() {
    Serial.begin(115200);
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime < 4000));
    Serial.println(F("Serial ready"));
}

inline void initIMU() {
    if (!IMU.begin()) {
        Serial.println("Failed to initialize IMU!");
        while (true);
    }
}

inline void onPDMdata() {
    int bytes = PDM.available();
    PDM.read((void*)sampleBuffer, bytes);
    samplesRead = bytes / 2;
}

inline void ledFlashCallback(bool active, uint8_t brightness) {
    if(strip) strip->propagateTriggerState(active, brightness);
}

inline void initAudio() {
    PDM.onReceive(onPDMdata);
    audioTrigger.onTrigger(ledFlashCallback);
    if (!PDM.begin(1, SAMPLING_FREQ)) {
        Serial.println("Failed to start PDM!");
        while (true);
    }
}

inline void initLEDs() {
    strip = new PixelStrip(LED_PIN, LED_COUNT, BRIGHTNESS, SEGMENT_COUNT);
    strip->begin();
    seg = strip->getSegments()[0];
    if (seg->activeEffect) {
        delete seg->activeEffect;
    }
    // createEffectByName is now available via EffectLookup.h
    seg->activeEffect = createEffectByName("SolidColor", seg);
    strip->show(); // Clear the strip on startup
}

inline void initFS() {
    if (!myFS.init()) {
        Serial.println("LittleFS mount failed on startup");
    } else {
        Serial.println("LittleFS mounted successfully");
    }
}

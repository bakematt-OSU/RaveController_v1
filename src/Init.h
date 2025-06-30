// Init.h
// Definitions for initialization routines and callbacks (all in-header implementation)

#pragma once

#include "config.h"
#include <Arduino.h>
#include <PDM.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include "PixelStrip.h"
#include "Triggers.h"
#include <ArduinoBLE.h>

// Externally defined globals (from main.cpp)
extern volatile int16_t       sampleBuffer[];
extern volatile int           samplesRead;
extern PixelStrip             strip;
extern AudioTrigger<SAMPLES>  audioTrigger;
extern PixelStrip::Segment*   seg;

// Initialize serial communication
inline void initSerial() {
    Serial.begin(115200);
    while (!Serial) { /* wait for port */ }
}

// Initialize IMU sensor
inline void initIMU() {
    if (!IMU.begin()) {
        Serial.println("Failed to initialize IMU!");
        while (true) { /* halt */ }
    }
}

// Handle incoming PDM audio data
inline void onPDMdata() {
    int bytes = PDM.available();
    // PDM.read expects a void*; sampleBuffer is volatile, so we cast it away here
    PDM.read((void*)sampleBuffer, bytes);
    samplesRead = bytes / 2;
}
// Callback when audio trigger fires
inline void ledFlashCallback(bool active, uint8_t brightness) {
    strip.propagateTriggerState(active, brightness);
}

// Initialize audio input and trigger callback
inline void initAudio() {
    PDM.onReceive(onPDMdata);
    audioTrigger.onTrigger(ledFlashCallback);
    if (!PDM.begin(1, SAMPLING_FREQ)) {
        Serial.println("Failed to start PDM!");
        while (true) { /* halt */ }
    }
}

// Initialize LED strip and heartbeat LEDs
inline void initLEDs() {
    // Turn off heartbeat LEDs
    WiFiDrv::analogWrite(LEDR_PIN, 0);
    WiFiDrv::analogWrite(LEDG_PIN, 0);
    WiFiDrv::analogWrite(LEDB_PIN, 0);

    // Check WiFi module
    if (WiFi.status() == WL_NO_MODULE) {
        Serial.println("WiFi module failed!");
        WiFiDrv::analogWrite(LEDR_PIN, 255);
        while (true) { /* halt */ }
    }

    // Initialize strip and first segment
    strip.begin();
    seg = strip.getSegments()[0];
    seg->begin();
    seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);
}


BLEService ledService("180A"); // Custom LED service
BLECharacteristic cmdCharacteristic("2A57", BLEWrite, 100); // Write-only command channel

inline void initBLE() {
    if (!BLE.begin()) {
        Serial.println("BLE init failed!");
        while (1);
    }
    BLE.setLocalName("RP2040-LED");
    BLE.setAdvertisedService(ledService);
    ledService.addCharacteristic(cmdCharacteristic);
    BLE.addService(ledService);
    BLE.advertise();
    Serial.println("BLE Ready and Advertising.");
}

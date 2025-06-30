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
#include <LittleFS_Mbed_RP2040.h>  
#include <stdio.h> 

// — where to keep the name on flash —
#define BT_NAME_FILE     "/btname.txt"
#define DEFAULT_BT_NAME  "RP2040-LED"

// Externally defined globals (from main.cpp)
extern volatile int16_t       sampleBuffer[];
extern volatile int           samplesRead;
extern PixelStrip             strip;
extern AudioTrigger<SAMPLES>  audioTrigger;
extern PixelStrip::Segment*   seg;

// Initialize serial communication
inline void initSerial() {
  Serial.begin(115200);
  unsigned long startTime = millis();
  const unsigned long timeoutMs = 5000;  // adjust as needed

  // wait up to timeoutMs for the host to open the Serial port
  while (!Serial && (millis() - startTime < timeoutMs)) {
    // you can blink an LED here or just spin
    delay(10);
  }

  if (!Serial) {
    // Serial never came up—optional fallback
    // e.g. blink an error LED or send via another interface
  } else {
    // Serial is ready
    Serial.println(F("Serial ready"));
  }
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
    // — configure the NINA LED pins as outputs —
    WiFiDrv::pinMode(LEDR_PIN, OUTPUT);
    WiFiDrv::pinMode(LEDG_PIN, OUTPUT);
    WiFiDrv::pinMode(LEDB_PIN, OUTPUT);

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

    // Initialize strip…
    strip.begin();
    seg = strip.getSegments()[0];
    seg->begin();
    seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);
    // strip.begin();
    // strip.clear();                 // clear any garbage
    // seg = strip.getSegments()[0];
    // seg->begin();
    // // — show your default static color at startup —
    // {
    //   uint32_t c = strip.Color(128, 0, 128);
    //   for (uint16_t i = seg->startIndex(); i <= seg->endIndex(); ++i)
    //     strip.setPixel(i, c);
    //   strip.show();
    // }
}
// inline void initLEDs() {
//     // Turn off heartbeat LEDs
//     WiFiDrv::analogWrite(LEDR_PIN, 0);
//     WiFiDrv::analogWrite(LEDG_PIN, 0);
//     WiFiDrv::analogWrite(LEDB_PIN, 0);

//     // Check WiFi module
//     if (WiFi.status() == WL_NO_MODULE) {
//         Serial.println("WiFi module failed!");
//         WiFiDrv::analogWrite(LEDR_PIN, 255);
//         while (true) { /* halt */ }
//     }

//     // Initialize strip and first segment
//     strip.begin();
//     seg = strip.getSegments()[0];
//     seg->begin();
//     seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);
// }


BLEService ledService("180A"); // Custom LED service
BLECharacteristic cmdCharacteristic("2A57", BLEWrite, 100); // Write-only command channel

// 1) Create a global pointer to the FS wrapper
static LittleFS_MBED *myFS = nullptr;

// 2) Mount (init) the file system
inline void initFS() {
  myFS = new LittleFS_MBED();        // allocate the wrapper
  if (! myFS->init()) {              // mount it
    Serial.println("⚠️ LittleFS mount failed");
  }
}

inline String loadBTName() {
    FILE *f = fopen(BT_NAME_FILE, "r");
    if (!f) return DEFAULT_BT_NAME;

    char buf[32] = {0};
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return DEFAULT_BT_NAME;
    }
    fclose(f);

    // ----- fix starts here -----
    String name(buf);   // construct the String
    name.trim();        // strip newline/whitespace
    // ----- fix ends here -----
    

    return name.length() ? name : DEFAULT_BT_NAME;
}

// 4) In your BLE init, pull that name in
inline void initBLE() {
  if (!BLE.begin()) {
    Serial.println("BLE init failed!");
    while (1);
  }

  String btName = loadBTName();
  BLE.setLocalName(btName.c_str());
  BLE.setAdvertisedService(ledService);
  ledService.addCharacteristic(cmdCharacteristic);
  BLE.addService(ledService);
  BLE.advertise();

  Serial.print("BLE Ready as “");
  Serial.print(btName);
  Serial.println("”");
}
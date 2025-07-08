// Init.h
// Definitions for initialization routines and callbacks

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
#include "Processes.h"


// --- Filesystem Constants ---
#define BT_NAME_FILE "/btname.txt"
#define DEFAULT_BT_NAME "RP2040-LED"

// --- Externally defined globals (from main.cpp) ---
extern volatile int16_t sampleBuffer[];
extern volatile int samplesRead;
extern PixelStrip strip;
extern AudioTrigger<SAMPLES> audioTrigger;
extern PixelStrip::Segment *seg;

// --- BLE Service and Characteristic Definitions ---
// These UUIDs MUST match the ones in your Android app's BluetoothService.kt
BLEService ledService("0000180A-0000-1000-8000-00805F9B34FB");
BLECharacteristic cmdCharacteristic("00002A57-0000-1000-8000-00805F9B34FB", BLERead | BLEWrite | BLENotify, 256);

// --- Initialization Functions ---

inline void initSerial()
{
  Serial.begin(115200);
  // Optional: Wait for serial monitor to open, with a timeout
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 4000))
  {
    delay(10);
  }
  Serial.println(F("Serial ready"));
}

inline void initIMU()
{
  if (!IMU.begin())
  {
    Serial.println("Failed to initialize IMU!");
    while (true)
      ; // Halt
  }
}

inline void onPDMdata()
{
  int bytes = PDM.available();
  PDM.read((void *)sampleBuffer, bytes);
  samplesRead = bytes / 2;
}

inline void ledFlashCallback(bool active, uint8_t brightness)
{
  strip.propagateTriggerState(active, brightness);
}

inline void initAudio()
{
  PDM.onReceive(onPDMdata);
  audioTrigger.onTrigger(ledFlashCallback);
  if (!PDM.begin(1, SAMPLING_FREQ))
  {
    Serial.println("Failed to start PDM!");
    while (true)
      ; // Halt
  }
}

inline void initLEDs()
{
  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("WiFi module failed!");
    while (true)
      ; // Halt
  }
  strip.begin();
  seg = strip.getSegments()[0];
  seg->begin();
  seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);
  strip.show(); // Clear the strip on startup
}

inline void initFS()
{
  static LittleFS_MBED myFS;
  if (!myFS.init())
  {
    Serial.println("⚠️ LittleFS mount failed");
  }
  else
  {
    // ADD THIS LINE to load the configuration
    loadConfig();
  } 
}

inline String loadBTName()
{
  FILE *f = fopen(BT_NAME_FILE, "r");
  if (!f)
    return DEFAULT_BT_NAME;

  char buf[32] = {0};
  if (!fgets(buf, sizeof(buf), f))
  {
    fclose(f);
    return DEFAULT_BT_NAME;
  }
  fclose(f);

  String name(buf);
  name.trim(); // Strip newline/whitespace
  return name.length() ? name : DEFAULT_BT_NAME;
}

inline void initBLE()
{
  if (!BLE.begin())
  {
    Serial.println("BLE init failed!");
    while (1)
      ;
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

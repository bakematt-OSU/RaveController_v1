/**
 * @file main.cpp
 * @brief Main application logic for the Rave Controller.
 *
 * This version is updated to use the new simplified BLEManager and a pure
 * binary command handler, removing the old text-based serial handler.
 *
 * @version 2.0
 * @date 2025-07-15
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_LSM6DSOX.h>
#include "globals.h"
#include "Init.h"
#include "BLEManager.h"
#include "BinaryCommandHandler.h"
#include "ConfigManager.h"

// --- Global Object Instances ---
BLEManager& bleManager = BLEManager::getInstance();
BinaryCommandHandler binaryCommandHandler;

// --- Global Variable Definitions ---
PixelStrip* strip = nullptr;
PixelStrip::Segment* seg = nullptr;
uint16_t LED_COUNT = 150; // Default value, will be overwritten by config
const char* STATE_FILE = "/littlefs/state.json";
LittleFS_MBED myFS;

AudioTrigger<SAMPLES> audioTrigger;
volatile int16_t sampleBuffer[SAMPLES];
volatile int samplesRead = 0;
float accelX, accelY, accelZ;
volatile bool triggerRipple = false;

// --- Forward declarations for hardware processing ---
void processAudio();
void processAccel();

/**
 * @brief Callback function for the BLEManager.
 * This function is called by the BLEManager whenever a command is received from the app.
 * It simply passes the data to the binary command handler.
 * @param data Pointer to the received byte array.
 * @param len Length of the byte array.
 */
void onBleCommandReceived(const uint8_t* data, size_t len) {
    binaryCommandHandler.handleCommand(data, len);
}

void setup() {
    initSerial();
    initFS();

    // Load the configuration from LittleFS
    String configJson = loadConfig();
    if (configJson.length() > 0) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, configJson);
        if (error == DeserializationError::Ok) {
            LED_COUNT = doc["led_count"] | 150; // Load LED count or use default
        }
    }

    // Initialize hardware and LED strip
    initIMU();
    initAudio();
    initLEDs();

    // If a full configuration was saved, apply it now
    if (configJson.length() > 0) {
        Serial.println("Restoring full configuration from saved state...");
        handleBatchConfigJson(configJson);
    }

    // Start the BLE manager with our command handler callback
    bleManager.begin("RaveControllerV2", onBleCommandReceived);

    Serial.println("Setup complete. Entering main loop...");
}

void loop() {
    // Poll for BLE events
    bleManager.update();

    // Process hardware inputs
    processAudio();
    processAccel();

    // Update all LED segments and show the result
    if (strip) {
        for (auto* s : strip->getSegments()) {
            s->update();
        }
        strip->show();
    }
}

// --- Hardware Processing Functions ---
void processAudio() {
    if (samplesRead > 0) {
        audioTrigger.update(sampleBuffer);
        samplesRead = 0;
    }
}

void processAccel() {
    if (IMU.accelerationAvailable()) {
        IMU.readAcceleration(accelX, accelY, accelZ);
    }
}

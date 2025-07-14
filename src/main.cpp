#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_LSM6DSOX.h>
#include "globals.h"
#include "Init.h"
#include "BLEManager.h"
#include "CommandHandler.h"
#include "SerialCommandHandler.h" // <-- Include the new header
#include "ConfigManager.h" // <-- Include the new manager

// --- Global Object Instances ---
BLEManager& bleManager = BLEManager::getInstance();
CommandHandler commandHandler(&bleManager);

// --- Global Variable Definitions ---
SerialCommandHandler serialCommandHandler; // <-- C
PixelStrip* strip = nullptr;
PixelStrip::Segment* seg = nullptr;
uint16_t LED_COUNT = 150;
const char* STATE_FILE = "/littlefs/state.json";
LittleFS_MBED myFS;

AudioTrigger<SAMPLES> audioTrigger;
volatile int16_t sampleBuffer[SAMPLES];
volatile int samplesRead = 0;
float accelX, accelY, accelZ;
volatile bool triggerRipple = false; // <-- FIX: Added definition

// --- Forward declarations ---
void processAudio();
void processAccel();

// --- Callback function for BLEManager ---
void onBleCommandReceived(const String& command) {
    // FIX: Call the correct command handler
    serialCommandHandler.handleCommand(command);
}
void setup() {
    initSerial();
    initFS();

    // Now calls the global function
    String configJson = loadConfig();
    if (configJson.length() > 0) {
        StaticJsonDocument<256> doc; // Just need a small doc to read led_count
        DeserializationError error = deserializeJson(doc, configJson);
        if (error == DeserializationError::Ok) {
            LED_COUNT = doc["led_count"] | 150;
        }
    }

    initIMU();
    initAudio();
    initLEDs(); // Initialize LEDs first

    // --- FIX: Restore the full configuration after LEDs are initialized ---
    if (configJson.length() > 0) {
        Serial.println("Restoring full configuration from saved state...");
        handleBatchConfigJson(configJson); // Reuse existing logic to apply the config
    }
    // --- End of Fix ---

    bleManager.begin("RaveControllerV2", onBleCommandReceived);

    Serial.println("Setup complete. Entering main loop...");
}
// void setup() {
//     initSerial();
//     initFS();

//      // Now calls the global function
//     String configJson = loadConfig();
//     if (configJson.length() > 0) {
//         StaticJsonDocument<256> doc;
//         if (deserializeJson(doc, configJson) == DeserializationError::Ok) {
//             LED_COUNT = doc["led_count"] | 150;
//         }
//     }

//     initIMU();
//     initAudio();
//     initLEDs();

//     bleManager.begin("RaveControllerV2", onBleCommandReceived);

//     Serial.println("Setup complete. Entering main loop...");
// }

void loop() {
        // --- ADD THIS BLOCK TO PROCESS SERIAL COMMANDS ---
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command.length() > 0) {
            serialCommandHandler.handleCommand(command);
        }
    }
    // --- END OF NEW BLOCK ---
    bleManager.update();
    processAudio();
    processAccel();

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
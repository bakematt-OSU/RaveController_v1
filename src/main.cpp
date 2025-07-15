#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_LSM6DSOX.h>
#include "globals.h"
#include "Init.h"
#include "BLEManager.h"
#include "CommandHandler.h"
#include "SerialCommandHandler.h" 
#include "ConfigManager.h" 
#include "BinaryCommandHandler.h" // Added in previous step

// --- Global Object Instances ---
BLEManager& bleManager = BLEManager::getInstance();
CommandHandler commandHandler(&bleManager);
BinaryCommandHandler binaryCommandHandler; // Added in previous step

// --- Global Variable Definitions ---
SerialCommandHandler serialCommandHandler; 
PixelStrip* strip = nullptr;
PixelStrip::Segment* seg = nullptr;
uint16_t LED_COUNT = 150;
const char* STATE_FILE = "/littlefs/state.json";
LittleFS_MBED myFS;

AudioTrigger<SAMPLES> audioTrigger;
volatile int16_t sampleBuffer[SAMPLES];
volatile int samplesRead = 0;
float accelX, accelY, accelZ;
volatile bool triggerRipple = false; 

// --- Forward declarations ---
void processAudio();
void processAccel();

// --- Callback function for BLEManager ---
void onBleCommandReceived(const String& command) {
    if (command.startsWith("0x")) {
        int hex_len = (command.length() - 2) / 2;
        uint8_t* hex_data = new uint8_t[hex_len];
        for(int i=0; i < hex_len; i++) {
            String byteString = command.substring(2 + i*2, 4 + i*2);
            hex_data[i] = strtol(byteString.c_str(), NULL, 16);
        }
        binaryCommandHandler.handleCommand(hex_data, hex_len);
        delete[] hex_data;
    } else {
        serialCommandHandler.handleCommand(command);
    }
}
void setup() {
    initSerial();
    initFS();

    String configJson = loadConfig();
    if (configJson.length() > 0) {
        StaticJsonDocument<256> doc; 
        DeserializationError error = deserializeJson(doc, configJson);
        if (error == DeserializationError::Ok) {
            LED_COUNT = doc["led_count"] | 150;
        }
    }

    initIMU();
    initAudio();
    initLEDs(); 

    if (configJson.length() > 0) {
        Serial.println("Restoring full configuration from saved state...");
        handleBatchConfigJson(configJson); 
    }
    
    bleManager.begin("RaveControllerV2", onBleCommandReceived);

    Serial.println("Setup complete. Entering main loop...");
}

void loop() {
    // --- FIX: This block now correctly handles both text and binary commands ---
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command.length() > 0) {
            // Replicate the logic from the BLE handler to route commands correctly
            if (command.startsWith("0x")) {
                int hex_len = (command.length() - 2) / 2;
                uint8_t* hex_data = new uint8_t[hex_len];
                for(int i=0; i < hex_len; i++) {
                    String byteString = command.substring(2 + i*2, 4 + i*2);
                    hex_data[i] = strtol(byteString.c_str(), NULL, 16);
                }
                binaryCommandHandler.handleCommand(hex_data, hex_len);
                delete[] hex_data;
            } else {
                serialCommandHandler.handleCommand(command);
            }
        }
    }
    // --- END OF FIX ---
    
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
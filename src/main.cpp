/**
 * @file main.cpp
 * @brief Main application logic for the Rave Controller.
 *
 * This version is updated to use the new simplified BLEManager and a pure
 * binary command handler, removing the old text-based serial handler.
 * It also re-introduces a serial command handler for debugging.
 *
 * @version 2.2 (Fixed Serial Binary State Handling)
 * @date 2025-07-16
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_LSM6DSOX.h>
#include "globals.h"
#include "Init.h"
#include "BLEManager.h"
#include "BinaryCommandHandler.h"
#include "SerialCommandHandler.h" // Include the serial handler
#include "ConfigManager.h"

// --- Global Object Instances ---
BLEManager &bleManager = BLEManager::getInstance();
BinaryCommandHandler binaryCommandHandler;
SerialCommandHandler serialCommandHandler; // Create an instance of the serial handler

// --- Global Variable Definitions ---
PixelStrip *strip = nullptr;
PixelStrip::Segment *seg = nullptr;
uint16_t LED_COUNT = 150; // Default value, will be overwritten by config
const char *STATE_FILE = "/littlefs/state.json";
LittleFS_MBED myFS;

AudioTrigger<SAMPLES> audioTrigger;
volatile int16_t sampleBuffer[SAMPLES];
volatile int samplesRead = 0;
float accelX, accelY, accelZ;
volatile bool triggerRipple = false;

// Initialize the new global flag
bool reAdvertisingMessagePrinted = false; //

// --- Forward declarations for hardware processing ---
void processAudio();
void processAccel();
void processSerial(); // Add forward declaration for serial processing

/**
 * @brief Callback function for the BLEManager.
 * This function is called by the BLEManager whenever a command is received from the app.
 * It simply passes the data to the binary command handler.
 * @param data Pointer to the received byte array.
 * @param len Length of the byte array.
 */
void onBleCommandReceived(const uint8_t *data, size_t len)
{
    binaryCommandHandler.handleCommand(data, len);
}

void setup()
{
    initSerial();
    initFS();

    // Load the configuration from LittleFS
    String configJson = loadConfig();
    if (configJson.length() > 0)
    {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, configJson);
        if (error == DeserializationError::Ok)
        {
            LED_COUNT = doc["led_count"] | 150; // Load LED count or use default
        }
    }

    // Initialize hardware and LED strip
    initIMU();
    initAudio();
    initLEDs();

    // If a full configuration was saved, apply it now
    if (configJson.length() > 0)
    {
        Serial.println("Restoring full configuration from saved state...");
        handleBatchConfigJson(configJson);
    }

    // Start the BLE manager with our command handler callback
    bleManager.begin("RaveControllerV2", onBleCommandReceived);

    Serial.println("Setup complete. Entering main loop...");
}

void loop()
{
    // Poll for BLE events
    bleManager.update();

    // Periodically check if BLE is connected. If not, ensure advertising is restarted.
    if (!bleManager.isConnected())
    {
        BLE.advertise();
        if (!reAdvertisingMessagePrinted)
        {
            Serial.println("BLE: Re-advertising due to detected disconnect.");
            reAdvertisingMessagePrinted = true;
        }
    }
    else
    {
        reAdvertisingMessagePrinted = false;
    }

    // Poll for Serial commands
    processSerial();

    // Process hardware inputs
    processAudio();
    processAccel();

    // Update all LED segments and show the result
    if (strip)
    {
        for (auto *s : strip->getSegments())
        {
            s->update();
        }
        strip->show();
    }
}

// --- Serial Command Processing ---
void processSerial()
{
    // Check if BinaryCommandHandler is in a state expecting multi-part data
    if (binaryCommandHandler.getIncomingBatchState() == IncomingBatchState::EXPECTING_ALL_SEGMENTS_COUNT ||
        binaryCommandHandler.getIncomingBatchState() == IncomingBatchState::EXPECTING_ALL_SEGMENTS_JSON ||
        binaryCommandHandler.getIncomingBatchState() == IncomingBatchState::EXPECTING_EFFECT_ACK)
    { // <-- ADDED THIS STATE

        // If in a batch state, read raw bytes and pass them directly to the binary handler
        if (Serial.available() > 0)
        {
            const size_t max_read_len = 256;
            uint8_t temp_buffer[max_read_len];
            size_t bytes_read = Serial.readBytes(temp_buffer, min((size_t)Serial.available(), max_read_len));

            if (bytes_read > 0)
            {
                Serial.print("Serial RX (Raw for Binary State): ");
                for (size_t i = 0; i < bytes_read; i++)
                {
                    Serial.print("0x");
                    Serial.print(temp_buffer[i], HEX);
                    Serial.print(" ");
                }
                Serial.println();

                // Pass raw bytes to the binary command handler
                binaryCommandHandler.handleCommand(temp_buffer, bytes_read);
            }
        }
    }
    else
    {
        // Otherwise, process as a regular text command
        if (Serial.available() > 0)
        {
            String command = Serial.readStringUntil('\n');
            command.trim();
            if (command.length() > 0)
            {
                // Only print the received command if it's NOT the getalleffects test command
                if (!command.equalsIgnoreCase("getalleffects")) {
                    Serial.print("Serial Command Received: '");
                    Serial.print(command);
                    Serial.println("'");
                }
                serialCommandHandler.handleCommand(command);
            }
        }
    }
}

// void processSerial()
// {
//     // Check if BinaryCommandHandler is in a state expecting multi-part data
//     if (binaryCommandHandler.getIncomingBatchState() == IncomingBatchState::EXPECTING_ALL_SEGMENTS_COUNT ||
//         binaryCommandHandler.getIncomingBatchState() == IncomingBatchState::EXPECTING_ALL_SEGMENTS_JSON ||
//         binaryCommandHandler.getIncomingBatchState() == IncomingBatchState::EXPECTING_EFFECT_ACK)
//     { // <-- ADDED THIS STATE

//         // If in a batch state, read raw bytes and pass them directly to the binary handler
//         if (Serial.available() > 0)
//         {
//             const size_t max_read_len = 256;
//             uint8_t temp_buffer[max_read_len];
//             size_t bytes_read = Serial.readBytes(temp_buffer, min((size_t)Serial.available(), max_read_len));

//             if (bytes_read > 0)
//             {
//                 Serial.print("Serial RX (Raw for Binary State): ");
//                 for (size_t i = 0; i < bytes_read; i++)
//                 {
//                     Serial.print("0x");
//                     Serial.print(temp_buffer[i], HEX);
//                     Serial.print(" ");
//                 }
//                 Serial.println();

//                 // Pass raw bytes to the binary command handler
//                 binaryCommandHandler.handleCommand(temp_buffer, bytes_read);
//             }
//         }
//     }
//     else
//     {
//         // Otherwise, process as a regular text command
//         if (Serial.available() > 0)
//         {
//             String command = Serial.readStringUntil('\n');
//             command.trim();
//             if (command.length() > 0)
//             {
//                 Serial.print("Serial Command Received: '");
//                 Serial.print(command);
//                 Serial.println("'");
//                 serialCommandHandler.handleCommand(command);
//             }
//         }
//     }
// }

// --- Hardware Processing Functions ---
void processAudio()
{
    if (samplesRead > 0)
    {
        audioTrigger.update(sampleBuffer);
        samplesRead = 0;
    }
}

void processAccel()
{
    if (IMU.accelerationAvailable())
    {
        IMU.readAcceleration(accelX, accelY, accelZ);
    }
}

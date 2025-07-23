/**
 * @file main.cpp
 * @brief Main application logic for the Rave Controller.
 *
 * @version 2.9 (Reliable Packet Protocol - Final)
 * @date 2025-07-22
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_LSM6DSOX.h>
#include "globals.h"
#include "Init.h"
#include "BLEManager.h"
#include "BinaryCommandHandler.h"
#include "SerialCommandHandler.h"
#include "ConfigManager.h"
#include "EffectLookup.h"

// --- Global Object Instances ---
BLEManager &bleManager = BLEManager::getInstance();
BinaryCommandHandler binaryCommandHandler;
SerialCommandHandler serialCommandHandler;

// --- Global Variable Definitions ---
PixelStrip *strip = nullptr;
PixelStrip::Segment *seg = nullptr;
uint16_t LED_COUNT = 585;
const char *STATE_FILE = "/littlefs/state.json";
LittleFS_MBED myFS;

unsigned long lastHeartbeatReceived = 0;
uint8_t effectScratchpad[EFFECT_SCRATCHPAD_SIZE];

AudioTrigger<SAMPLES> audioTrigger;
volatile int16_t sampleBuffer[SAMPLES];
volatile int samplesRead = 0;
float accelX, accelY, accelZ;
volatile bool triggerRipple = false;
bool reAdvertisingMessagePrinted = false;

// --- Forward declarations ---
void processAudio();
void processAccel();
void processSerial();

/**
 * @brief Callback function for when any data is received over BLE.
 */
void onBleCommandReceived(const uint8_t *data, size_t len)
{
    binaryCommandHandler.handleCommand(data, len);
}

void setup()
{
    initSerial();
    initFS();

    static char configBuffer[2048];
    size_t configSize = loadConfig(configBuffer, sizeof(configBuffer));

    if (configSize > 0)
    {
        StaticJsonDocument<2048> doc;
        DeserializationError error = deserializeJson(doc, configBuffer, configSize);

        if (error == DeserializationError::Ok)
        {
            LED_COUNT = doc["led_count"] | 585;
            initIMU();
            initAudio();
            initLEDs();

            if (strip && doc.containsKey("segments"))
            {
                strip->clearUserSegments();
                JsonArray segments = doc["segments"];
                for (JsonObject segData : segments)
                {
                    const char* name = segData["name"] | "";
                    uint16_t start = segData["startLed"];
                    uint16_t end = segData["endLed"];
                    uint8_t brightness = segData["brightness"] | 255;
                    const char* effectNameStr = segData["effect"] | "SolidColor";
                    uint8_t segmentId = segData["id"] | 0;

                    PixelStrip::Segment *targetSeg = nullptr;
                    for (auto* s : strip->getSegments()) {
                        if (s->getId() == segmentId) {
                            targetSeg = s;
                            break;
                        }
                    }

                    if (!targetSeg) {
                        strip->addSection(start, end, name);
                        targetSeg = strip->getSegments().back();
                    }
                    
                    if (targetSeg) {
                        targetSeg->setRange(start, end);
                        targetSeg->setBrightness(brightness);

                        if (targetSeg->activeEffect) {
                            if (strcmp(targetSeg->activeEffect->getName(), effectNameStr) != 0) {
                                delete targetSeg->activeEffect;
                                targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);
                            }
                        } else {
                            targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);
                        }

                        if (targetSeg->activeEffect) {
                            for (int i = 0; i < targetSeg->activeEffect->getParameterCount(); ++i) {
                                EffectParameter *p = targetSeg->activeEffect->getParameter(i);
                                if (segData.containsKey(p->name)) {
                                    switch (p->type) {
                                        case ParamType::INTEGER: targetSeg->activeEffect->setParameter(p->name, segData[p->name].as<int>()); break;
                                        case ParamType::FLOAT:   targetSeg->activeEffect->setParameter(p->name, segData[p->name].as<float>()); break;
                                        case ParamType::COLOR:   targetSeg->activeEffect->setParameter(p->name, segData[p->name].as<uint32_t>()); break;
                                        case ParamType::BOOLEAN: targetSeg->activeEffect->setParameter(p->name, segData[p->name].as<bool>()); break;
                                    }
                                }
                            }
                        }
                    }
                }
                strip->show();
            }
        }
        else
        {
            initIMU();
            initAudio();
            initLEDs();
        }
    }
    else
    {
        initIMU();
        initAudio();
        initLEDs();
    }

    bleManager.begin("RaveCape-V1", onBleCommandReceived);
    Serial.println("Setup complete. Entering main loop...");
}

void loop()
{
    bleManager.update();
    binaryCommandHandler.update();
    processSerial();
    processAudio();
    processAccel();

    if (strip)
    {
        for (auto *s : strip->getSegments())
        {
            s->update();
        }
        strip->show();
    }
}


void processSerial()
{
    // --- FIX: Simplified serial processing ---
    // The old logic for a "serial batch mode" is removed as it's no longer needed
    // with the new reliable packet system. All serial commands are now simple text commands.
    if (Serial.available() > 0)
    {
        static char command_buffer[256];
        int bytes_read = Serial.readBytesUntil('\n', command_buffer, sizeof(command_buffer) - 1);

        if (bytes_read > 0) {
            command_buffer[bytes_read] = '\0';
            char* command = command_buffer;
            while (isspace(*command)) { command++; } // Trim leading whitespace
            for (int i = strlen(command) - 1; i >= 0; i--) { // Trim trailing whitespace
                if (isspace(command[i])) { command[i] = '\0'; }
                else { break; }
            }
            if (strlen(command) > 0) {
                serialCommandHandler.handleCommand(command);
            }
        }
    }
}


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
/**
 * @file main.cpp
 * @brief Main application logic for the Rave Controller.
 *
 * @version 2.6 (Robust Startup Parsing)
 * @date 2025-07-17
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
#include "EffectLookup.h" // Needed for createEffectByName

// --- Global Object Instances ---
BLEManager &bleManager = BLEManager::getInstance();
BinaryCommandHandler binaryCommandHandler;
SerialCommandHandler serialCommandHandler;

// --- Global Variable Definitions ---
PixelStrip *strip = nullptr;
PixelStrip::Segment *seg = nullptr;
uint16_t LED_COUNT = 150; // Default value
const char *STATE_FILE = "/littlefs/state.json";
LittleFS_MBED myFS;

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

    // --- Robust Configuration Loading ---
    if (configSize > 0)
    {
        // 1. Parse the entire configuration file ONCE.
        StaticJsonDocument<2048> doc;
        DeserializationError error = deserializeJson(doc, configBuffer, configSize);

        if (error == DeserializationError::Ok)
        {
            // 2. Extract LED_COUNT from the parsed document.
            LED_COUNT = doc["led_count"] | 150;
            
            // 3. Initialize hardware that depends on config values.
            initIMU();
            initAudio();
            initLEDs();

            // 4. Apply the rest of the configuration directly from the parsed 'doc'.
            Serial.println("Restoring full configuration from saved state...");
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

                    PixelStrip::Segment *targetSeg;
                    if (strcmp(name, "all") == 0)
                    {
                        targetSeg = strip->getSegments()[0];
                        targetSeg->setRange(start, end);
                    }
                    else
                    {
                        strip->addSection(start, end, name);
                        targetSeg = strip->getSegments().back();
                    }

                    targetSeg->setBrightness(brightness);
                    if (targetSeg->activeEffect) delete targetSeg->activeEffect;
                    targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);

                    if (targetSeg->activeEffect)
                    {
                        // This part is identical to handleBatchConfigJson
                        for (int i = 0; i < targetSeg->activeEffect->getParameterCount(); ++i)
                        {
                            EffectParameter *p = targetSeg->activeEffect->getParameter(i);
                            if (segData.containsKey(p->name))
                            {
                                switch (p->type)
                                {
                                case ParamType::INTEGER:
                                    targetSeg->activeEffect->setParameter(p->name, segData[p->name].as<int>());
                                    break;
                                case ParamType::FLOAT:
                                    targetSeg->activeEffect->setParameter(p->name, segData[p->name].as<float>());
                                    break;
                                case ParamType::COLOR:
                                    targetSeg->activeEffect->setParameter(p->name, segData[p->name].as<uint32_t>());
                                    break;
                                case ParamType::BOOLEAN:
                                    targetSeg->activeEffect->setParameter(p->name, segData[p->name].as<bool>());
                                    break;
                                }
                            }
                        }
                    }
                }
                strip->show();
                Serial.println("OK: Startup configuration restored.");
            }
        }
        else
        {
            // If JSON is invalid, proceed with defaults
            Serial.print("ERR: Config file parse failed: ");
            Serial.println(error.c_str());
            initIMU();
            initAudio();
            initLEDs(); // Use default LED_COUNT
        }
    }
    else
    {
        // No config file found, initialize with defaults
        initIMU();
        initAudio();
        initLEDs();
    }

    bleManager.begin("RaveControllerV2", onBleCommandReceived);

    Serial.println("Setup complete. Entering main loop...");
}

void loop()
{
    static unsigned long lastBleCheck = 0;
    unsigned long currentMillis = millis();

    bleManager.update();

    if (currentMillis - lastBleCheck > 500)
    {
        lastBleCheck = currentMillis;
        if (!bleManager.isConnected())
        {
            BLE.stopAdvertise();
            BLE.advertise();
            
            if (!reAdvertisingMessagePrinted)
            {
                Serial.println("BLE Polling: Not connected. Attempting to re-advertise.");
                reAdvertisingMessagePrinted = true;
            }
        }
        else
        {
            reAdvertisingMessagePrinted = false;
        }
    }

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

// --- Serial Command Processing ---
void processSerial()
{
    if (binaryCommandHandler.isSerialBatchActive() &&
       (binaryCommandHandler.getIncomingBatchState() != IncomingBatchState::IDLE))
    {
        if (Serial.available() > 0)
        {
            const size_t max_read_len = 256;
            uint8_t temp_buffer[max_read_len];
            size_t bytes_read = Serial.readBytes(temp_buffer, min((size_t)Serial.available(), max_read_len));
            if (bytes_read > 0)
            {
                binaryCommandHandler.handleCommand(temp_buffer, bytes_read);
            }
        }
    }
    else
    {
        if (Serial.available() > 0)
        {
            static char command_buffer[256];
            int bytes_read = Serial.readBytesUntil('\n', command_buffer, sizeof(command_buffer) - 1);
            
            if (bytes_read > 0) {
                command_buffer[bytes_read] = '\0';
                
                char* command = command_buffer;
                while (isspace(*command)) {
                    command++;
                }

                for (int i = strlen(command) - 1; i >= 0; i--) {
                    if (isspace(command[i])) {
                        command[i] = '\0';
                    } else {
                        break;
                    }
                }
                
                if (strlen(command) > 0) {
                    serialCommandHandler.handleCommand(command);
                }
            }
        }
    }
}

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
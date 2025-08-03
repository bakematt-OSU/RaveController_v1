#include "ConfigManager.h"
#include "globals.h"
#include <ArduinoJson.h>
#include <LittleFS_Mbed_RP2040.h>
#include "EffectLookup.h"
#include "BLEManager.h"

// --- External globals defined in main.cpp ---
extern PixelStrip *strip;
extern uint16_t LED_COUNT;
extern const char *STATE_FILE;
extern BLEManager &bleManager;

// --- Saves the complete strip configuration ---
bool saveConfig()
{
    StaticJsonDocument<4096> doc; // Increased size
    doc["led_count"] = LED_COUNT;

    JsonArray segments = doc.createNestedArray("segments");
    if (strip)
    {
        for (auto *s : strip->getSegments())
        {
            JsonObject segObj = segments.createNestedObject();
            segObj["id"] = s->getId();
            segObj["name"] = s->getName();
            segObj["startLed"] = s->startIndex();
            segObj["endLed"] = s->endIndex();
            segObj["brightness"] = s->getBrightness();
            
            if (s->activeEffect) {
                segObj["effect"] = s->activeEffect->getName();
                
                // Create a nested "parameters" object for consistency
                JsonObject paramsObj = segObj.createNestedObject("parameters");
                for (int i = 0; i < s->activeEffect->getParameterCount(); ++i)
                {
                    EffectParameter *p = s->activeEffect->getParameter(i);
                    switch (p->type)
                    {
                    case ParamType::INTEGER:
                        paramsObj[p->name] = p->value.intValue;
                        break;
                    case ParamType::FLOAT:
                        paramsObj[p->name] = p->value.floatValue;
                        break;
                    case ParamType::COLOR:
                        paramsObj[p->name] = p->value.colorValue;
                        break;
                    case ParamType::BOOLEAN:
                        paramsObj[p->name] = p->value.boolValue;
                        break;
                    }
                }
            } else {
                segObj["effect"] = "None";
            }
        }
    }

    FILE *file = fopen(STATE_FILE, "w");
    if (file)
    {
        static char serializationBuffer[4096]; // Increased size
        size_t bytesWritten = serializeJson(doc, serializationBuffer, sizeof(serializationBuffer));
        
        if (bytesWritten > 0) {
            fprintf(file, "%s", serializationBuffer);
        }
        
        fclose(file);
        Serial.println("OK: Config saved.");
        return true;
    }
    else
    {
        Serial.println("ERR: Failed to open state file for writing.");
        return false;
    }
}

// --- Loads the configuration from the filesystem ---
size_t loadConfig(char* buffer, size_t bufferSize)
{
    FILE *file = fopen(STATE_FILE, "r");
    if (file)
    {
        size_t bytesRead = fread(buffer, 1, bufferSize - 1, file);
        fclose(file);
        buffer[bytesRead] = '\0';
        return bytesRead;
    }
    return 0;
}

// --- Sets a new LED count, saves it, and restarts the device ---
void setLedCount(uint16_t newSize)
{
    if (newSize > 0 && newSize <= 4000)
    {
        LED_COUNT = newSize;
        if (saveConfig())
        {
            Serial.print("LED count set to ");
            Serial.print(newSize);
            Serial.println(". Restarting to apply changes.");
            delay(200); 
            NVIC_SystemReset();
        }
        else
        {
            bleManager.sendMessage("{\"error\":\"SAVE_CONFIG_FAILED\"}");
        }
    }
    else
    {
        bleManager.sendMessage("{\"error\":\"INVALID_LED_COUNT\"}");
    }
}

// --- Processes a JSON string to configure segments and effects ---
void handleBatchConfigJson(const char* json)
{
    StaticJsonDocument<4096> doc; // Increased size
    DeserializationError error = deserializeJson(doc, json);

    if (error)
    {
        Serial.print("ERR: handleBatchConfig JSON parse error: ");
        Serial.println(error.c_str());
        bleManager.sendMessage("{\"error\":\"JSON_PARSE_ERROR\"}");
        return;
    }

    if (strip)
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
            if (targetSeg->activeEffect)
                delete targetSeg->activeEffect;
            targetSeg->activeEffect = createEffectByName(effectNameStr, targetSeg);

            if (targetSeg->activeEffect)
            {
                // --- THIS IS THE FIX ---
                // Check for the nested "parameters" object first.
                if (segData.containsKey("parameters")) {
                    JsonObject paramsObj = segData["parameters"];
                    
                    // Iterate through the effect's actual parameters and look for them
                    // inside the "parameters" object we just found.
                    for (int i = 0; i < targetSeg->activeEffect->getParameterCount(); ++i)
                    {
                        EffectParameter *p = targetSeg->activeEffect->getParameter(i);
                        if (paramsObj.containsKey(p->name))
                        {
                            switch (p->type)
                            {
                            case ParamType::INTEGER:
                                targetSeg->activeEffect->setParameter(p->name, paramsObj[p->name].as<int>());
                                break;
                            case ParamType::FLOAT:
                                targetSeg->activeEffect->setParameter(p->name, paramsObj[p->name].as<float>());
                                break;
                            case ParamType::COLOR:
                                targetSeg->activeEffect->setParameter(p->name, paramsObj[p->name].as<uint32_t>());
                                break;
                            case ParamType::BOOLEAN:
                                targetSeg->activeEffect->setParameter(p->name, paramsObj[p->name].as<bool>());
                                break;
                            }
                        }
                    }
                }
                // --- END OF FIX ---
            }
        }
        strip->show();
        Serial.println("OK: Batch configuration applied.");
        bleManager.sendMessage("{\"status\":\"OK\"}");
    }
}
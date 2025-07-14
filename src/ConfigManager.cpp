#include "ConfigManager.h"
#include "globals.h"
#include <ArduinoJson.h>
#include <LittleFS_Mbed_RP2040.h>
#include "EffectLookup.h"
#include "BLEManager.h" // <-- ADD THIS LINE

// --- External globals defined in main.cpp ---
extern PixelStrip *strip;
extern uint16_t LED_COUNT;
extern const char *STATE_FILE;
extern BLEManager &bleManager; // Now the compiler knows what this is

// --- FIX: Updated to save the complete strip configuration ---
bool saveConfig()
{
    StaticJsonDocument<1024> doc; // Increased size to hold more data
    doc["led_count"] = LED_COUNT;

    // Create a JSON array to hold the segment configurations
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
            // Store the name of the effect, or "None" if nothing is active
            segObj["effect"] = s->activeEffect ? s->activeEffect->getName() : "None";
        }
    }

    FILE *file = fopen(STATE_FILE, "w");
    if (file)
    {
        String output;
        serializeJson(doc, output);
        fprintf(file, "%s", output.c_str());
        fclose(file);
        Serial.println("Configuration saved.");
        return true;
    }
    else
    {
        Serial.println("Failed to open state file for writing.");
        return false;
    }
}

// Loads the configuration from the filesystem.
String loadConfig()
{
    FILE *file = fopen(STATE_FILE, "r");
    if (file)
    {
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *buf = new char[fileSize + 1];
        fread(buf, 1, fileSize, file);
        fclose(file);
        buf[fileSize] = '\0';

        String json(buf);
        delete[] buf;

        Serial.println("Configuration file loaded from FS.");
        return json;
    }
    else
    {
        Serial.println("Could not find state file, using default configuration.");
        return "";
    }
}

// Sets a new LED count, saves it, and restarts the device.
void setLedCount(uint16_t newSize) {
    if (newSize > 0 && newSize <= 4000) {
        LED_COUNT = newSize;
        if (saveConfig()) {
            Serial.print("LED count set to ");
            Serial.print(newSize);
            Serial.println(". Restarting to apply changes.");

            // --- FIX: Force the filesystem to write all cached data to flash ---
            Serial.println("Flushing filesystem to flash...");
            myFS.end();  // Un-mount the filesystem to force a cache flush
            myFS.init(); // Re-mount the filesystem for the next boot
            // --- End of Fix ---

            delay(100); // A brief delay for stability before reset
            NVIC_SystemReset();
        } else {
            bleManager.sendMessage("{\"error\":\"SAVE_CONFIG_FAILED\"}");
        }
    } else {
        bleManager->sendMessage("{\"error\":\"INVALID_LED_COUNT\"}");
    }
}


// Processes a JSON string to configure segments and effects.
void handleBatchConfigJson(const String &json)
{
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error)
    {
        Serial.print("handleBatchConfig JSON parse error: ");
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
            String name = segData["name"] | "";
            uint16_t start = segData["startLed"];
            uint16_t end = segData["endLed"];
            uint8_t brightness = segData["brightness"] | 255;
            String effectNameStr = segData["effect"] | "SolidColor";

            PixelStrip::Segment *newSeg;
            if (name.equalsIgnoreCase("all"))
            {
                newSeg = strip->getSegments()[0];
                newSeg->setRange(start, end);
            }
            else
            {
                strip->addSection(start, end, name);
                newSeg = strip->getSegments().back();
            }

            newSeg->setBrightness(brightness);
            if (newSeg->activeEffect)
                delete newSeg->activeEffect;
            newSeg->activeEffect = createEffectByName(effectNameStr, newSeg);
        }
        strip->show();
        Serial.println("Batch configuration applied.");
        bleManager.sendMessage("{\"status\":\"OK\"}");
    }
}
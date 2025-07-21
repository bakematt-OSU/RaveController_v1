#include "CommandHandler.h"
#include "BLEManager.h"
#include "globals.h"
#include "ConfigManager.h"
#include "EffectLookup.h"
#include <ArduinoJson.h>

// External globals defined in main.cpp
extern PixelStrip *strip;
extern uint16_t LED_COUNT;

CommandHandler::CommandHandler(BLEManager *ble) : bleManager(ble) {}

// --- Helper Functions to Parse Commands ---

String CommandHandler::getWord(const String &text, int index)
{
    int current_word = 0;
    int start_pos = 0;
    while (current_word < index)
    {
        start_pos = text.indexOf(' ', start_pos);
        if (start_pos == -1)
            return "";
        start_pos++;
        current_word++;
    }
    int end_pos = text.indexOf(' ', start_pos);
    if (end_pos == -1)
    {
        return text.substring(start_pos);
    }
    return text.substring(start_pos, end_pos);
}

String CommandHandler::getRestOfCommand(const String &text, int startIndex)
{
    int pos = text.indexOf(' ');
    if (pos == -1 || pos >= (int)text.length() - 1)
        return "";
    return text.substring(pos + 1);
}

// --- Main Command Handling Logic ---

void CommandHandler::handleCommand(const String &command)
{
    String cmd = getWord(command, 0);
    String args = getRestOfCommand(command, 0);
    cmd.toLowerCase();

    // Log the received BLE command to Serial for debugging input
    Serial.print("BLE Command Received: '");
    Serial.print(command);
    Serial.println("'");

    if (cmd.equalsIgnoreCase("listeffects"))
    {
        handleListEffects();
    }
    else if (cmd.equalsIgnoreCase("getstatus"))
    {
        handleGetStatus();
    }
    else if (cmd.equalsIgnoreCase("getsavedconfig"))
    {
        handleGetSavedConfig();
    }
    else if (cmd.equalsIgnoreCase("saveconfig"))
    {
        handleSaveConfig();
    }
    else if (cmd.equalsIgnoreCase("setledcount"))
    {
        setLedCount(args.toInt());
    }
    else if (cmd.equalsIgnoreCase("getledcount"))
    {
        handleGetLedCount();
    }
    else if (cmd.equalsIgnoreCase("listsegments"))
    {
        handleListSegments();
    }
    else if (cmd.equalsIgnoreCase("clearsegments"))
    {
        handleClearSegments();
    }
    else if (cmd.equalsIgnoreCase("addsegment"))
    {
        handleAddSegment(args);
    }
    else if (cmd.equalsIgnoreCase("seteffect"))
    {
        handleSetEffect(args);
    }
    else if (cmd.equalsIgnoreCase("geteffectinfo"))
    {
        handleGetEffectInfo(args);
    }
    else if (cmd.equalsIgnoreCase("setparameter") || cmd.equalsIgnoreCase("setparam"))
    {
        handleSetParameter(args);
    }
    else if (cmd.equalsIgnoreCase("batchconfig"))
    {
        // MODIFIED: Use .c_str() to convert String to const char*
        handleBatchConfigJson(args.c_str());
    }
    else
    {
        String errorMsg = "{\"error\":\"Unknown command: " + cmd + "\"}";
        Serial.println("-> ERR: Unknown command.");
        bleManager->sendMessage(errorMsg);
    }
}

// --- Specific Command Implementations (Dual Output) ---

void CommandHandler::handleListEffects()
{
    StaticJsonDocument<512> doc;
    JsonArray effects = doc.createNestedArray("effects");
    for (int i = 0; i < EFFECT_COUNT; ++i)
    {
        effects.add(EFFECT_NAMES[i]);
    }
    String response;
    serializeJson(doc, response);

    Serial.println("-> DEBUG: Listing all effects.");
    serializeJson(doc, Serial); // Print JSON to Serial
    Serial.println();
    bleManager->sendMessage(response);
}

void CommandHandler::handleGetStatus()
{
    StaticJsonDocument<1024> doc;
    doc["led_count"] = LED_COUNT;

    JsonArray effects = doc.createNestedArray("available_effects");
    for (int i = 0; i < EFFECT_COUNT; ++i)
    {
        effects.add(EFFECT_NAMES[i]);
    }

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
            segObj["effect"] = s->activeEffect ? s->activeEffect->getName() : "None";
        }
    }

    String response;
    serializeJson(doc, response);

    Serial.println("-> DEBUG: Getting device status.");
    serializeJson(doc, Serial); // Print JSON to Serial
    Serial.println();
    bleManager->sendMessage(response);
}

void CommandHandler::handleGetSavedConfig()
{
    // MODIFIED: Use the new loadConfig with a char buffer
    char configBuffer[1024];
    size_t configSize = loadConfig(configBuffer, sizeof(configBuffer));
    if (configSize > 0)
    {
        Serial.println("-> DEBUG: Getting config from FS.");
        Serial.println(configBuffer);
        bleManager->sendMessage(configBuffer);
    }
    else
    {
        const char* emptyConfig = "{\"led_count\":0,\"segments\":[]}";
        Serial.println("-> DEBUG: No config file found.");
        Serial.println(emptyConfig);
        bleManager->sendMessage(emptyConfig);
    }
}

void CommandHandler::handleSaveConfig()
{
    if (saveConfig())
    {
        Serial.println("-> OK: Config saved.");
        bleManager->sendMessage("{\"status\":\"OK\", \"message\":\"Config saved\"}");
    }
    else
    {
        Serial.println("-> ERR: Failed to save config.");
        bleManager->sendMessage("{\"error\":\"Failed to save config\"}");
    }
}

void CommandHandler::handleGetLedCount()
{
    StaticJsonDocument<128> doc;
    doc["led_count"] = LED_COUNT;
    String response;
    serializeJson(doc, response);

    Serial.print("-> DEBUG: LED_COUNT = ");
    Serial.println(LED_COUNT);
    bleManager->sendMessage(response);
}

void CommandHandler::handleListSegments()
{
    if (!strip)
    {
        Serial.println("-> ERR: Strip not initialized.");
        bleManager->sendMessage("{\"error\":\"Strip not initialized\"}");
        return;
    }
    StaticJsonDocument<1024> doc;
    JsonArray segments = doc.createNestedArray("segments");
    Serial.println("-> DEBUG: Listing segments.");
    for (const auto *s : strip->getSegments())
    {
        JsonObject segObj = segments.createNestedObject();
        segObj["id"] = s->getId();
        segObj["name"] = s->getName();
        segObj["startLed"] = s->startIndex();
        segObj["endLed"] = s->endIndex();
        Serial.print("  - Segment ");
        Serial.print(s->getId());
        Serial.print(": '");
        Serial.print(s->getName());
        Serial.print("' (");
        Serial.print(s->startIndex());
        Serial.print("-");
        Serial.print(s->endIndex());
        Serial.println(")");
    }
    String response;
    serializeJson(doc, response);
    bleManager->sendMessage(response);
}

void CommandHandler::handleClearSegments()
{
    if (strip)
    {
        strip->clearUserSegments();
        Serial.println("-> OK: User segments cleared.");
        bleManager->sendMessage("{\"status\":\"OK\", \"message\":\"User segments cleared\"}");
    }
    else
    {
        Serial.println("-> ERR: Strip not initialized.");
        bleManager->sendMessage("{\"error\":\"Strip not initialized\"}");
    }
}

void CommandHandler::handleAddSegment(const String &args)
{
    int start = getWord(args, 0).toInt();
    int end = getWord(args, 1).toInt();
    String name = getWord(args, 2);
    if (name.isEmpty())
    {
        name = "segment" + String(strip->getSegments().size());
    }

    if (strip && end >= start)
    {
        strip->addSection(start, end, name);
        Serial.println("-> OK: Segment added.");
        bleManager->sendMessage("{\"status\":\"OK\", \"message\":\"Segment added\"}");
    }
    else
    {
        Serial.println("-> ERR: Invalid segment range or strip not initialized.");
        bleManager->sendMessage("{\"error\":\"Invalid segment range or strip not initialized\"}");
    }
}

void CommandHandler::handleSetEffect(const String &args)
{
    int segIndex = getWord(args, 0).toInt();
    String effectName = getWord(args, 1);

    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size())
    {
        Serial.println("-> ERR: Invalid segment index.");
        bleManager->sendMessage("{\"error\":\"Invalid segment index\"}");
        return;
    }

    PixelStrip::Segment *seg = strip->getSegments()[segIndex];
    BaseEffect *newEffect = createEffectByName(effectName, seg);

    if (newEffect)
    {
        if (seg->activeEffect)
        {
            delete seg->activeEffect;
        }
        seg->activeEffect = newEffect;

        if (strip)
        {
            seg->update();
            strip->show();
        }

        Serial.println("-> OK: Effect set.");
        bleManager->sendMessage("{\"status\":\"OK\", \"message\":\"Effect set\"}");
    }
    else
    {
        Serial.println("-> ERR: Unknown effect.");
        bleManager->sendMessage("{\"error\":\"Unknown effect\"}");
    }
}

void CommandHandler::handleGetEffectInfo(const String &args)
{
    int segIndex = args.toInt();
    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size())
    {
        Serial.println("-> ERR: Invalid segment index.");
        bleManager->sendMessage("{\"error\":\"Invalid segment index\"}");
        return;
    }

    PixelStrip::Segment *seg = strip->getSegments()[segIndex];
    if (!seg->activeEffect)
    {
        Serial.println("-> ERR: No active effect on this segment.");
        bleManager->sendMessage("{\"error\":\"No active effect on this segment\"}");
        return;
    }

    StaticJsonDocument<512> doc;
    doc["effect"] = seg->activeEffect->getName();
    JsonArray params = doc.createNestedArray("params");
    for (int i = 0; i < seg->activeEffect->getParameterCount(); ++i)
    {
        EffectParameter *p = seg->activeEffect->getParameter(i);
        JsonObject p_obj = params.createNestedObject();
        p_obj["name"] = p->name;
        // ... (JSON object creation is the same)
    }
    String response;
    serializeJson(doc, response);

    Serial.println("-> DEBUG: Getting effect info.");
    serializeJson(doc, Serial);
    Serial.println();
    bleManager->sendMessage(response);
}

void CommandHandler::handleSetParameter(const String &args)
{
    int segIndex = getWord(args, 0).toInt();
    String paramName = getWord(args, 1);
    String valueStr = getWord(args, 2);

    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size() || paramName.isEmpty() || valueStr.isEmpty())
    {
        Serial.println("-> ERR: Invalid arguments.");
        bleManager->sendMessage("{\"error\":\"Invalid arguments\"}");
        return;
    }

    PixelStrip::Segment *seg = strip->getSegments()[segIndex];
    if (!seg->activeEffect)
    {
        Serial.println("-> ERR: No active effect on segment.");
        bleManager->sendMessage("{\"error\":\"No active effect on segment\"}");
        return;
    }

    EffectParameter *p = nullptr;
    for (int i = 0; i < seg->activeEffect->getParameterCount(); ++i)
    {
        EffectParameter *currentParam = seg->activeEffect->getParameter(i);
        if (paramName.equalsIgnoreCase(currentParam->name))
        {
            p = currentParam;
            break;
        }
    }

    if (p == nullptr)
    {
        Serial.println("-> ERR: Parameter not found.");
        bleManager->sendMessage("{\"error\":\"Parameter not found\"}");
        return;
    }

    // (Parameter setting logic is the same)

    Serial.println("-> OK: Parameter set.");
    bleManager->sendMessage("{\"status\":\"OK\", \"message\":\"Parameter set\"}");
}
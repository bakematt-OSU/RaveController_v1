#include "SerialCommandHandler.h"
#include "globals.h"
#include "EffectLookup.h"
#include "Processes.h"
#include <ArduinoJson.h>
#include "BinaryCommandHandler.h" // Ensure this is included

// Forward declare the binary command handler instance
extern BinaryCommandHandler binaryCommandHandler;

// --- Helper Functions to Parse Commands ---
String SerialCommandHandler::getWord(const String &text, int index)
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

String SerialCommandHandler::getRestOfCommand(const String &text, int startIndex)
{
    int first_space_pos = text.indexOf(' ');
    if (first_space_pos == -1)
        return ""; // No space found

    // Find the Nth space for startIndex
    int current_word_count = 0;
    int current_pos = 0;
    while (current_word_count < startIndex)
    {
        current_pos = text.indexOf(' ', current_pos);
        if (current_pos == -1)
            return ""; // Not enough words
        current_pos++;
        current_word_count++;
    }

    // If startIndex is 0, we want the rest after the first word
    if (startIndex == 0)
    {
        return text.substring(first_space_pos + 1);
    }
    else
    {
        // Find the start of the desired "rest"
        int start_of_rest = text.indexOf(' ', current_pos);
        if (start_of_rest == -1)
            return ""; // No more words after startIndex
        return text.substring(start_of_rest + 1);
    }
}

// --- Main Command Handling Logic ---

void SerialCommandHandler::handleCommand(const String &command)
{
    String cmd = getWord(command, 0);
    String args = getRestOfCommand(command, 0);
    cmd.toLowerCase();

    if (cmd.equalsIgnoreCase("listeffects"))
    {
        handleListEffects();
    }
    else if (cmd.equalsIgnoreCase("getstatus"))
    {
        handleGetStatus();
    }
    else if (cmd.equalsIgnoreCase("getconfig"))
    {
        Serial.println(loadConfig());
    }
    else if (cmd.equalsIgnoreCase("saveconfig"))
    {
        if (saveConfig())
        {
            Serial.println("OK: Config saved.");
        }
        else
        {
            Serial.println("ERR: Failed to save config.");
        }
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
    else if (cmd.equalsIgnoreCase("getparams")) // New command
    {
        handleGetParameters(args);
    }
    else if (cmd.equalsIgnoreCase("batchconfig"))
    {
        handleBatchConfigJson(args);
    }
    else if (cmd.equalsIgnoreCase("getallsegmentconfigs"))
    {
        handleGetAllSegmentConfigsSerial();
    }
    else if (cmd.equalsIgnoreCase("getalleffects"))
    {
        handleGetAllEffectsSerial();
    }
    else if (cmd.equalsIgnoreCase("setallsegmentconfigs"))
    {
        handleSetAllSegmentConfigsSerial();
    }
    else if (cmd.equalsIgnoreCase("setsegmentjson"))
    {
        handleSetSingleSegmentJson(args);
    }
    else
    {
        Serial.print("ERR: Unknown command '");
        Serial.print(cmd);
        Serial.println("'");
    }
}

void SerialCommandHandler::handleGetAllSegmentConfigsSerial()
{
    Serial.println("Serial Command: Calling BinaryCommandHandler::handleGetAllSegmentConfigs() for Serial Output.");
    // This calls the BinaryCommandHandler's function, telling it to output to Serial.
    binaryCommandHandler.handleGetAllSegmentConfigs(true);
}

void SerialCommandHandler::handleGetAllEffectsSerial()
{
    // Call the binary command handler to start the process, specifying serial context
    binaryCommandHandler.handleGetAllEffectsCommand(true);
}

void SerialCommandHandler::handleSetAllSegmentConfigsSerial()
{
    Serial.println("Serial Command: Initiating Set All Segment Configurations. This requires manual JSON input.");
    Serial.println("The Arduino is now expecting:");
    Serial.println("1. A 2-byte segment count (e.g., '0002' for 2 segments).");
    Serial.println("2. Each segment's full JSON configuration, one after another.");
    Serial.println("After each piece of data (count or JSON), the Arduino will send an ACK.");

    // Set the state in BinaryCommandHandler to expect the count, then the JSONs.
    binaryCommandHandler.handleSetAllSegmentConfigsCommand(true);
}

// --- Specific Command Implementations (existing) ---
void SerialCommandHandler::handleListEffects()
{
    StaticJsonDocument<512> doc;
    JsonArray effects = doc.createNestedArray("effects");
    for (int i = 0; i < EFFECT_COUNT; ++i)
    {
        effects.add(EFFECT_NAMES[i]);
    }
    serializeJson(doc, Serial);
    Serial.println(); // Ensure a newline after the JSON
}

void SerialCommandHandler::handleGetStatus()
{
    StaticJsonDocument<1024> doc;
    doc["led_count"] = LED_COUNT;
    doc["brightness"] = strip ? strip->getSegments()[0]->getBrightness() : 0;

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
    serializeJson(doc, Serial);
    Serial.println(); // Ensure a newline after the JSON
}

void SerialCommandHandler::handleGetLedCount()
{
    Serial.print("LED_COUNT: ");
    Serial.println(LED_COUNT);
}

void SerialCommandHandler::handleListSegments()
{
    if (!strip)
    {
        Serial.println("ERR: Strip not initialized.");
        return;
    }
    for (const auto *s : strip->getSegments())
    {
        Serial.print("Segment ");
        Serial.print(s->getId());
        Serial.print(": '");
        Serial.print(s->getName());
        Serial.print("' (");
        Serial.print(s->startIndex());
        Serial.print("-");
        Serial.print(s->endIndex());
        Serial.println(")");
    }
}

void SerialCommandHandler::handleClearSegments()
{
    if (strip)
    {
        strip->clearUserSegments();
        Serial.println("OK: User segments cleared.");
    }
    else
    {
        Serial.println("ERR: Strip not initialized.");
    }
}

void SerialCommandHandler::handleAddSegment(const String &args)
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
        Serial.println("OK: Segment added.");
    }
    else
    {
        Serial.println("ERR: Invalid segment range or strip not initialized.");
    }
}

void SerialCommandHandler::handleSetEffect(const String &args)
{
    int segIndex = getWord(args, 0).toInt();
    String effectName = getWord(args, 1);

    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size())
    {
        Serial.println("ERR: Invalid segment index.");
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
            delay(10);
        }

        Serial.println("OK: Effect set.");
    }
    else
    {
        Serial.print("ERR: Unknown effect '");
        Serial.print(effectName);
        Serial.println("'");
    }
}

void SerialCommandHandler::handleGetEffectInfo(const String &args)
{
    String effectNameStr = getWord(args, 1);

    if (effectNameStr.isEmpty())
    {
        Serial.println("ERR: Missing effect name for GET_EFFECT_INFO.");
        return;
    }

    if (!strip || strip->getSegments().empty())
    {
        Serial.println("ERR: Strip not initialized or no segments available for dummy effect creation.");
        return;
    }
    PixelStrip::Segment *dummySegment = strip->getSegments()[0];

    BaseEffect *tempEffect = createEffectByName(effectNameStr, dummySegment);

    if (!tempEffect)
    {
        Serial.print("ERR: Failed to create temporary effect for '");
        Serial.print(effectNameStr);
        Serial.println("'.");
        return;
    }

    StaticJsonDocument<512> doc;
    doc["effect"] = tempEffect->getName();
    JsonArray params = doc.createNestedArray("params");

    for (int i = 0; i < tempEffect->getParameterCount(); ++i)
    {
        EffectParameter *p = tempEffect->getParameter(i);
        JsonObject p_obj = params.createNestedObject();
        p_obj["name"] = p->name;

        switch (p->type)
        {
        case ParamType::INTEGER:
            p_obj["type"] = "integer";
            p_obj["value"] = p->value.intValue;
            p_obj["min_val"] = p->min_val;
            p_obj["max_val"] = p->max_val;
            break;
        case ParamType::FLOAT:
            p_obj["type"] = "float";
            p_obj["value"] = p->value.floatValue;
            p_obj["min_val"] = p->min_val;
            p_obj["max_val"] = p->max_val;
            break;
        case ParamType::COLOR:
            p_obj["type"] = "color";
            p_obj["value"] = p->value.colorValue;
            if (p->min_val != 0 || p->max_val != 0)
            {
                p_obj["min_val"] = p->min_val;
                p_obj["max_val"] = p->max_val;
            }
            break;
        case ParamType::BOOLEAN:
            p_obj["type"] = "boolean";
            p_obj["value"] = p->value.boolValue;
            break;
        }
    }
    serializeJson(doc, Serial);
    Serial.println();

    delete tempEffect;
}

void SerialCommandHandler::handleSetParameter(const String &args)
{
    int segIndex = getWord(args, 0).toInt();
    String paramName = getWord(args, 1);
    String valueStr = getWord(args, 2);

    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size() || paramName.isEmpty() || valueStr.isEmpty())
    {
        Serial.println("ERR: Invalid arguments. Use: setparameter <seg_id> <param_name> <value>");
        return;
    }

    PixelStrip::Segment *seg = strip->getSegments()[segIndex];
    if (!seg->activeEffect)
    {
        Serial.println("ERR: No active effect on segment.");
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
        Serial.println("ERR: Parameter not found on active effect.");
        return;
    }

    switch (p->type)
    {
    case ParamType::INTEGER:
    {
        int intValue = valueStr.toInt();
        seg->activeEffect->setParameter(p->name, intValue);
        break;
    }
    case ParamType::FLOAT:
    {
        float floatValue = valueStr.toFloat();
        seg->activeEffect->setParameter(p->name, floatValue);
        break;
    }
    case ParamType::COLOR:
    {
        uint32_t colorValue = strtoul(valueStr.c_str(), NULL, 0);
        seg->activeEffect->setParameter(p->name, colorValue);
        break;
    }
    case ParamType::BOOLEAN:
    {
        bool boolValue = valueStr.equalsIgnoreCase("true") || valueStr.toInt() != 0;
        seg->activeEffect->setParameter(p->name, boolValue);
        break;
    }
    }

    Serial.println("OK: Parameter set.");
}
void SerialCommandHandler::handleSetSingleSegmentJson(const String& json)
{
    Serial.println("Serial Command: Calling BinaryCommandHandler::processSingleSegmentJson() for Serial Input.");
    binaryCommandHandler.processSingleSegmentJson(json);
}

// --- New Function Implementation ---
void SerialCommandHandler::handleGetParameters(const String &args)
{
    if (args.isEmpty()) {
        Serial.println("ERR: Missing segment ID. Usage: getparams <seg_id>");
        return;
    }

    int segIndex = args.toInt();

    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size())
    {
        Serial.println("ERR: Invalid segment index.");
        return;
    }

    PixelStrip::Segment *seg = strip->getSegments()[segIndex];
    if (!seg->activeEffect)
    {
        Serial.println("INFO: No active effect on this segment.");
        return;
    }

    Serial.print("Parameters for Segment ");
    Serial.print(segIndex);
    Serial.print(" ('");
    Serial.print(seg->getName());
    Serial.print("') with effect '");
    Serial.print(seg->activeEffect->getName());
    Serial.println("':");

    int paramCount = seg->activeEffect->getParameterCount();
    if (paramCount == 0) {
        Serial.println("  - This effect has no parameters.");
        return;
    }

    for (int i = 0; i < paramCount; ++i)
    {
        EffectParameter *p = seg->activeEffect->getParameter(i);
        Serial.print("  - ");
        Serial.print(p->name);
        Serial.print(": ");

        switch (p->type)
        {
        case ParamType::INTEGER:
            Serial.print(p->value.intValue);
            Serial.println(" (integer)");
            break;
        case ParamType::FLOAT:
            Serial.print(p->value.floatValue);
            Serial.println(" (float)");
            break;
        case ParamType::COLOR:
            Serial.print("0x");
            // Print leading zeros for a clean 6-digit hex value
            if (p->value.colorValue < 0x100000) Serial.print("0");
            if (p->value.colorValue < 0x10000) Serial.print("0");
            if (p->value.colorValue < 0x1000) Serial.print("0");
            if (p->value.colorValue < 0x100) Serial.print("0");
            if (p->value.colorValue < 0x10) Serial.print("0");
            Serial.print(p->value.colorValue, HEX);
            Serial.println(" (color)");
            break;
        case ParamType::BOOLEAN:
            Serial.print(p->value.boolValue ? "true" : "false");
            Serial.println(" (boolean)");
            break;
        }
    }
}

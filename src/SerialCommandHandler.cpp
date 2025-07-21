#include "SerialCommandHandler.h"
#include "globals.h"
#include "EffectLookup.h"
#include "ConfigManager.h"
#include "BLEManager.h"
#include <ArduinoJson.h>
#include "BinaryCommandHandler.h"
#include <cstring>
#include <cstdlib>

// Forward declare the binary command handler instance
extern BinaryCommandHandler binaryCommandHandler;
extern BLEManager &bleManager;

// --- Main Command Handling Logic ---

void SerialCommandHandler::handleCommand(char *command)
{
    char *saveptr; // For strtok_r, which is re-entrant and safer than strtok
    char *cmd = strtok_r(command, " ", &saveptr);
    if (!cmd)
        return;

    // Convert command to lowercase in-place
    for (char *p = cmd; *p; ++p)
        *p = tolower(*p);

    // The rest of the original string is now in saveptr
    char *args = strtok_r(NULL, "", &saveptr);

    if (strcmp(cmd, "help") == 0)
        handleHelp();
    else if (strcmp(cmd, "listeffects") == 0)
        handleListEffects();
    else if (strcmp(cmd, "getstatus") == 0)
        handleGetStatus();
    else if (strcmp(cmd, "getsavedconfig") == 0)
        handleGetSavedConfig();
    else if (strcmp(cmd, "getcurrconfig") == 0)
        handleGetCurrConfig();
    else if (strcmp(cmd, "saveconfig") == 0)
        handleSaveConfig();
    else if (strcmp(cmd, "setledcount") == 0)
        handleSetLedCount(args);
    else if (strcmp(cmd, "getledcount") == 0)
        handleGetLedCount();
    else if (strcmp(cmd, "listsegments") == 0)
        handleListSegments();
    else if (strcmp(cmd, "clearsegments") == 0)
        handleClearSegments();
    else if (strcmp(cmd, "addsegment") == 0)
        handleAddSegment(args);
    else if (strcmp(cmd, "seteffect") == 0)
        handleSetEffect(args);
    else if (strcmp(cmd, "geteffectinfo") == 0)
        handleGetEffectInfo(args);
    else if (strcmp(cmd, "setparameter") == 0 || strcmp(cmd, "setparam") == 0)
        handleSetParameter(args);
    else if (strcmp(cmd, "getparams") == 0)
        handleGetParameters(args);
    else if (strcmp(cmd, "batchconfig") == 0)
        handleBatchConfig(args);
    else if (strcmp(cmd, "getallsegmentconfigs") == 0)
        handleGetAllSegmentConfigsSerial();
    else if (strcmp(cmd, "getalleffects") == 0)
        handleGetAllEffectsSerial();
    else if (strcmp(cmd, "setallsegmentconfigs") == 0)
        handleSetAllSegmentConfigsSerial();
    else if (strcmp(cmd, "setsegmentjson") == 0)
        handleSetSingleSegmentJson(args);
    else if (strcmp(cmd, "blestatus") == 0)
        handleBleStatus();
    else if (strcmp(cmd, "blereset") == 0)
        handleBleReset();
    else
    {
        Serial.print("ERR: Unknown command '");
        Serial.print(cmd);
        Serial.println("'. Type 'help' for a list of commands.");
    }
}

// --- Command Implementations using C-Strings ---
void SerialCommandHandler::handleGetCurrConfig()
{
    StaticJsonDocument<2048> doc;
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

            if (s->activeEffect)
            {
                segObj["effect"] = s->activeEffect->getName();

                // Add all parameters for the active effect
                for (int i = 0; i < s->activeEffect->getParameterCount(); ++i)
                {
                    EffectParameter *p = s->activeEffect->getParameter(i);
                    switch (p->type)
                    {
                    case ParamType::INTEGER:
                        segObj[p->name] = p->value.intValue;
                        break;
                    case ParamType::FLOAT:
                        segObj[p->name] = p->value.floatValue;
                        break;
                    case ParamType::COLOR:
                        segObj[p->name] = p->value.colorValue;
                        break;
                    case ParamType::BOOLEAN:
                        segObj[p->name] = p->value.boolValue;
                        break;
                    }
                }
            }
            else
            {
                segObj["effect"] = "None";
            }
        }
    }

    // Serialize the JSON document directly to the Serial port
    serializeJsonPretty(doc, Serial);
    Serial.println(); // Add a newline for clean output
}

void SerialCommandHandler::handleHelp()
{
    Serial.println("\n--- Serial Command Help ---");
    Serial.println("Commands are not case-sensitive. Arguments are separated by spaces.");
    Serial.println("\n[General Commands]");
    Serial.println("  help                         - Shows this help message.");
    Serial.println("  getstatus                    - Prints the current status of the device as JSON.");
    Serial.println("  getcurrconfig                - Prints the current configuration in memory");
    Serial.println("  getsavedconfig               - Prints the saved configuration from the filesystem.");
    Serial.println("  saveconfig                   - Saves the current configuration to the filesystem.");
    Serial.println("\n[LED Configuration]");
    Serial.println("  getledcount                  - Prints the current LED count.");
    Serial.println("  setledcount <count>          - Sets the total number of LEDs and restarts.");
    Serial.println("\n[Segment Management]");
    Serial.println("  listsegments                 - Lists all current segments.");
    Serial.println("  clearsegments                - Deletes all user-defined segments.");
    Serial.println("  addsegment <start> <end> [name]");
    Serial.println("                               - Adds a new segment.");
    Serial.println("  setsegmentjson <json>        - Configures a single segment using a JSON string.");
    Serial.println("\n[Effect & Parameter Control]");
    Serial.println("  listeffects                  - Lists all available effects.");
    Serial.println("  seteffect <seg_id> <effect>  - Sets an effect on a specific segment.");
    Serial.println("  geteffectinfo <seg_id> <effect>");
    Serial.println("                               - Gets parameter info for an effect.");
    Serial.println("  setparam <seg_id> <param> <value>");
    Serial.println("                               - Sets a parameter for the active effect on a segment.");
    Serial.println("  getparams <seg_id>           - Gets parameters for the active effect on a segment.");
    Serial.println("\n[Bluetooth Commands]");
    Serial.println("  blestatus                    - Checks the current Bluetooth connection status.");
    Serial.println("  blereset                     - Resets the Bluetooth module.");
    Serial.println("\n[Advanced/Batch Commands]");
    Serial.println("  batchconfig <json>           - Applies a full configuration from a JSON string.");
    Serial.println("  getallsegmentconfigs         - Gets the full configuration of all segments as JSON.");
    Serial.println("  getalleffects                - Gets detailed information for all effects as JSON.");
    Serial.println("  setallsegmentconfigs         - Initiates receiving segment configurations.");
    Serial.println("--- End of Help ---\n");
}

void SerialCommandHandler::handleListEffects()
{
    StaticJsonDocument<512> doc;
    JsonArray effects = doc.createNestedArray("effects");
    for (int i = 0; i < EFFECT_COUNT; ++i)
    {
        effects.add(EFFECT_NAMES[i]);
    }
    serializeJson(doc, Serial);
    Serial.println();
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
    Serial.println();
}

void SerialCommandHandler::handleGetSavedConfig()
{
    static char configBuffer[2048];
    if (loadConfig(configBuffer, sizeof(configBuffer)) > 0)
    {
        Serial.println(configBuffer);
    }
    else
    {
        Serial.println("{}"); // Print empty JSON if no config found
    }
}

void SerialCommandHandler::handleSaveConfig()
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

void SerialCommandHandler::handleSetLedCount(const char *args)
{
    if (!args)
    {
        Serial.println("ERR: Missing LED count.");
        return;
    }
    setLedCount(atoi(args));
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

void SerialCommandHandler::handleAddSegment(char *args)
{
    if (!args)
    {
        Serial.println("ERR: Missing arguments for addsegment.");
        return;
    }

    char *saveptr;
    char *startStr = strtok_r(args, " ", &saveptr);
    char *endStr = strtok_r(NULL, " ", &saveptr);
    char *nameStr = strtok_r(NULL, "", &saveptr);

    if (!startStr || !endStr)
    {
        Serial.println("ERR: Invalid segment range. Use: addsegment <start> <end> [name]");
        return;
    }

    int start = atoi(startStr);
    int end = atoi(endStr);
    String name;
    if (nameStr)
    {
        name = nameStr;
    }
    else
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

void SerialCommandHandler::handleSetEffect(char *args)
{
    if (!args)
    {
        Serial.println("ERR: Missing arguments for seteffect.");
        return;
    }

    char *saveptr;
    char *segIndexStr = strtok_r(args, " ", &saveptr);
    char *effectName = strtok_r(NULL, "", &saveptr);

    if (!segIndexStr || !effectName)
    {
        Serial.println("ERR: Invalid arguments. Use: seteffect <seg_id> <EffectName>");
        return;
    }

    int segIndex = atoi(segIndexStr);
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
            delete seg->activeEffect;
        seg->activeEffect = newEffect;
        if (strip)
        {
            seg->update();
            strip->show();
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

void SerialCommandHandler::handleGetEffectInfo(char *args)
{
    if (!args)
    {
        Serial.println("ERR: Missing arguments for geteffectinfo.");
        return;
    }

    char *saveptr;
    strtok_r(args, " ", &saveptr); // Skip segment index, it's a dummy
    char *effectNameStr = strtok_r(NULL, "", &saveptr);

    if (!effectNameStr)
    {
        Serial.println("ERR: Missing effect name for GET_EFFECT_INFO.");
        return;
    }

    if (!strip || strip->getSegments().empty())
    {
        Serial.println("ERR: Strip not initialized.");
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

        // *** START of ADDED/CORRECTED CODE ***
        switch (p->type)
        {
        case ParamType::INTEGER:
            p_obj["type"] = "integer";
            break;
        case ParamType::FLOAT:
            p_obj["type"] = "float";
            break;
        case ParamType::COLOR:
            p_obj["type"] = "color";
            break;
        case ParamType::BOOLEAN:
            p_obj["type"] = "boolean";
            break;
        }
        // Also include min/max for the Python script to use
        p_obj["min_val"] = p->min_val;
        p_obj["max_val"] = p->max_val;
        // *** END of ADDED/CORRECTED CODE ***
    }

    serializeJson(doc, Serial);
    Serial.println();
    delete tempEffect;
}
void SerialCommandHandler::handleSetParameter(char *args)
{
    if (!args)
    {
        Serial.println("ERR: Missing arguments for setparameter.");
        return;
    }

    char *saveptr;
    char *segIndexStr = strtok_r(args, " ", &saveptr);
    char *paramName = strtok_r(NULL, " ", &saveptr);
    char *valueStr = strtok_r(NULL, "", &saveptr);

    if (!segIndexStr || !paramName || !valueStr)
    {
        Serial.println("ERR: Invalid arguments. Use: setparameter <seg_id> <param_name> <value>");
        return;
    }

    int segIndex = atoi(segIndexStr);
    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size())
    {
        Serial.println("ERR: Invalid segment index.");
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
        if (strcasecmp(paramName, currentParam->name) == 0)
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
        seg->activeEffect->setParameter(p->name, (int)atol(valueStr));
        break;
    case ParamType::FLOAT:
        seg->activeEffect->setParameter(p->name, (float)atof(valueStr));
        break;
    case ParamType::COLOR:
        seg->activeEffect->setParameter(p->name, (uint32_t)strtoul(valueStr, NULL, 0));
        break;
    case ParamType::BOOLEAN:
        seg->activeEffect->setParameter(p->name, (bool)(strcmp(valueStr, "true") == 0 || atoi(valueStr) != 0));
        break;
    }
    Serial.println("OK: Parameter set.");
}

void SerialCommandHandler::handleGetParameters(const char *args)
{
    if (!args)
    {
        Serial.println("ERR: Missing segment ID. Usage: getparams <seg_id>");
        return;
    }
    int segIndex = atoi(args);
    // ... rest of the function remains the same
}

void SerialCommandHandler::handleBatchConfig(const char *json)
{
    handleBatchConfigJson(json);
}

void SerialCommandHandler::handleSetSingleSegmentJson(const char *json)
{
    binaryCommandHandler.processSingleSegmentJson(json);
}

void SerialCommandHandler::handleGetAllSegmentConfigsSerial()
{
    binaryCommandHandler.handleGetAllSegmentConfigs(true);
}

void SerialCommandHandler::handleGetAllEffectsSerial()
{
    binaryCommandHandler.handleGetAllEffectsCommand(true);
}

void SerialCommandHandler::handleSetAllSegmentConfigsSerial()
{
    binaryCommandHandler.handleSetAllSegmentConfigsCommand(true);
}

void SerialCommandHandler::handleBleReset()
{
    Serial.println("Initiating BLE reset from serial command...");
    bleManager.reset();
}

void SerialCommandHandler::handleBleStatus()
{
    Serial.print("BLE Status: ");
    Serial.println(bleManager.isConnected() ? "Connected" : "Disconnected");
}
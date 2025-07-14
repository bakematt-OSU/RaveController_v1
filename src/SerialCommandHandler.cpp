#include "SerialCommandHandler.h"
#include "globals.h"
#include "EffectLookup.h"
#include "Processes.h" // Note: This was in your original file list
#include <ArduinoJson.h>

// --- Helper Functions to Parse Commands ---

// Gets the nth word from a space-separated string
String SerialCommandHandler::getWord(const String& text, int index) {
    int current_word = 0;
    int start_pos = 0;
    while (current_word < index) {
        start_pos = text.indexOf(' ', start_pos);
        if (start_pos == -1) return "";
        start_pos++; // Move past the space
        current_word++;
    }
    int end_pos = text.indexOf(' ', start_pos);
    if (end_pos == -1) {
        return text.substring(start_pos);
    }
    return text.substring(start_pos, end_pos);
}

// Gets the rest of the command string after the command itself
String SerialCommandHandler::getRestOfCommand(const String& text, int startIndex) {
    int pos = text.indexOf(' ');
    if (pos == -1 || pos >= (int)text.length() - 1) return "";
    return text.substring(pos + 1);
}

// --- Main Command Handling Logic ---

void SerialCommandHandler::handleCommand(const String& command) {
    String cmd = getWord(command, 0);
    String args = getRestOfCommand(command, 0);
    cmd.toLowerCase();

    if (cmd.equalsIgnoreCase("listeffects")) {
        handleListEffects();
    } else if (cmd.equalsIgnoreCase("getstatus")) {
        handleGetStatus();
    } else if (cmd.equalsIgnoreCase("getconfig")) {
        // Now calls global function
        Serial.println(loadConfig());
    } else if (cmd.equalsIgnoreCase("saveconfig")) {
        // Now calls global function
        if (saveConfig()) {
            Serial.println("OK: Config saved.");
        } else {
            Serial.println("ERR: Failed to save config.");
        }
    } else if (cmd.equalsIgnoreCase("setledcount")) {
        // Now calls global function
        setLedCount(args.toInt());
    } else if (cmd.equalsIgnoreCase("getledcount")) {
        handleGetLedCount();
    } else if (cmd.equalsIgnoreCase("listsegments")) {
        handleListSegments();
    } else if (cmd.equalsIgnoreCase("clearsegments")) {
        handleClearSegments();
    } else if (cmd.equalsIgnoreCase("addsegment")) {
        handleAddSegment(args);
    } else if (cmd.equalsIgnoreCase("seteffect")) {
        handleSetEffect(args);
    } else if (cmd.equalsIgnoreCase("geteffectinfo")) {
        handleGetEffectInfo(args);
    } else if (cmd.equalsIgnoreCase("setparameter") || cmd.equalsIgnoreCase("setparam")) {
        handleSetParameter(args);
    } else if (cmd.equalsIgnoreCase("batchconfig")) {
        // Now calls global function
        handleBatchConfigJson(args);
    } else {
        Serial.print("ERR: Unknown command '");
        Serial.print(cmd);
        Serial.println("'");
    }
}

// --- Specific Command Implementations ---

void SerialCommandHandler::handleListEffects() {
    StaticJsonDocument<512> doc;
    JsonArray effects = doc.createNestedArray("effects");
    for (int i = 0; i < EFFECT_COUNT; ++i) {
        effects.add(EFFECT_NAMES[i]);
    }
    serializeJson(doc, Serial);
    Serial.println();
}

void SerialCommandHandler::handleGetStatus() {
    StaticJsonDocument<1024> doc;
    doc["led_count"] = LED_COUNT;
    doc["brightness"] = strip ? strip->getSegments()[0]->getBrightness() : 0;

    JsonArray effects = doc.createNestedArray("available_effects");
     for (int i = 0; i < EFFECT_COUNT; ++i) {
        effects.add(EFFECT_NAMES[i]);
    }

    JsonArray segments = doc.createNestedArray("segments");
    if (strip) {
        for (auto* s : strip->getSegments()) {
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

void SerialCommandHandler::handleGetLedCount() {
    Serial.print("LED_COUNT: ");
    Serial.println(LED_COUNT);
}

void SerialCommandHandler::handleListSegments() {
    if (!strip) {
        Serial.println("ERR: Strip not initialized.");
        return;
    }
    for (const auto* s : strip->getSegments()) {
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

void SerialCommandHandler::handleClearSegments() {
    if (strip) {
        strip->clearUserSegments();
        Serial.println("OK: User segments cleared.");
    } else {
        Serial.println("ERR: Strip not initialized.");
    }
}

void SerialCommandHandler::handleAddSegment(const String& args) {
    int start = getWord(args, 0).toInt();
    int end = getWord(args, 1).toInt();
    String name = getWord(args, 2);
    if (name.isEmpty()) {
        name = "segment" + String(strip->getSegments().size());
    }

    if (strip && end >= start) {
        strip->addSection(start, end, name);
        Serial.println("OK: Segment added.");
    } else {
        Serial.println("ERR: Invalid segment range or strip not initialized.");
    }
}

void SerialCommandHandler::handleSetEffect(const String& args) {
    int segIndex = getWord(args, 0).toInt();
    String effectName = getWord(args, 1);

    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size()) {
        Serial.println("ERR: Invalid segment index.");
        return;
    }

    PixelStrip::Segment* seg = strip->getSegments()[segIndex];
    BaseEffect* newEffect = createEffectByName(effectName, seg);

    if (newEffect) {
        if (seg->activeEffect) {
            delete seg->activeEffect;
        }
        seg->activeEffect = newEffect;

        // --- FIX: Force an immediate update and show ---
        // This renders the first frame of the new effect right away,
        // synchronizing the hardware with the test script.
        if (strip) {
            seg->update(); // Call the new effect's update function
            strip->show(); // Push the new colors to the LEDs
            delay(10);     // Add a tiny delay to ensure the bus is ready for the next command
        }
        // --- End of Fix ---

        Serial.println("OK: Effect set.");
    } else {
        Serial.print("ERR: Unknown effect '");
        Serial.print(effectName);
        Serial.println("'");
    }
}

void SerialCommandHandler::handleGetEffectInfo(const String& args) {
    int segIndex = args.toInt();
    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size()) {
        Serial.println("ERR: Invalid segment index.");
        return;
    }

    PixelStrip::Segment* seg = strip->getSegments()[segIndex];
    if (!seg->activeEffect) {
        Serial.println("ERR: No active effect on this segment.");
        return;
    }

    StaticJsonDocument<512> doc;
    doc["effect"] = seg->activeEffect->getName();
    JsonArray params = doc.createNestedArray("params");

    for (int i = 0; i < seg->activeEffect->getParameterCount(); ++i) {
        EffectParameter* p = seg->activeEffect->getParameter(i);
        JsonObject p_obj = params.createNestedObject();
        p_obj["name"] = p->name;
        
        switch (p->type) {
            case ParamType::INTEGER:
                p_obj["type"] = "integer";
                p_obj["value"] = p->value.intValue;
                break;
            case ParamType::FLOAT:
                p_obj["type"] = "float";
                p_obj["value"] = p->value.floatValue;
                break;
            case ParamType::COLOR:
                p_obj["type"] = "color";
                p_obj["value"] = p->value.colorValue;
                break;
            case ParamType::BOOLEAN:
                p_obj["type"] = "boolean";
                p_obj["value"] = p->value.boolValue;
                break;
        }
        p_obj["min_val"] = p->min_val;
        p_obj["max_val"] = p->max_val;
    }
    serializeJson(doc, Serial);
    Serial.println();
}

void SerialCommandHandler::handleSetParameter(const String& args) {
    int segIndex = getWord(args, 0).toInt();
    String paramName = getWord(args, 1);
    String valueStr = getWord(args, 2);

    if (!strip || segIndex < 0 || segIndex >= (int)strip->getSegments().size() || paramName.isEmpty() || valueStr.isEmpty()) {
        Serial.println("ERR: Invalid arguments. Use: setparameter <seg_id> <param_name> <value>");
        return;
    }

    PixelStrip::Segment* seg = strip->getSegments()[segIndex];
    if (!seg->activeEffect) {
        Serial.println("ERR: No active effect on segment.");
        return;
    }

    EffectParameter* p = nullptr;
    for (int i = 0; i < seg->activeEffect->getParameterCount(); ++i) {
        EffectParameter* currentParam = seg->activeEffect->getParameter(i);
        if (paramName.equalsIgnoreCase(currentParam->name)) {
            p = currentParam;
            break;
        }
    }

    if (p == nullptr) {
        Serial.println("ERR: Parameter not found on active effect.");
        return;
    }

    switch (p->type) {
        case ParamType::INTEGER: {
            int intValue = valueStr.toInt();
            seg->activeEffect->setParameter(p->name, intValue);
            break;
        }
        case ParamType::FLOAT: {
            float floatValue = valueStr.toFloat();
            seg->activeEffect->setParameter(p->name, floatValue);
            break;
        }
        case ParamType::COLOR: {
            uint32_t colorValue = strtoul(valueStr.c_str(), NULL, 0);
            seg->activeEffect->setParameter(p->name, colorValue);
            break;
        }
        case ParamType::BOOLEAN: {
            bool boolValue = valueStr.equalsIgnoreCase("true") || valueStr.toInt() != 0;
            seg->activeEffect->setParameter(p->name, boolValue);
            break;
        }
    }
    
    Serial.println("OK: Parameter set.");
}
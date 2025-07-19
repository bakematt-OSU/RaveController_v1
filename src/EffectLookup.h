#pragma once

#include <Arduino.h>
#include "Config.h" // <-- ADD THIS INCLUDE
#include "effects/Effects.h"
#include "PixelStrip.h"
#include "AllEffects.h"

// --- Build the EffectType enum from the combined EFFECT_LIST ---
#define ENUM_ENTRY(name, ns) name,
enum class EffectType {
    EFFECT_LIST(ENUM_ENTRY)
    UNKNOWN
};
#undef ENUM_ENTRY

// --- Build a parallel array of effect names for UI or serialization ---
#define STRING_ENTRY(name, ns) #name,
static const char *EFFECT_NAMES[] = {
    EFFECT_LIST(STRING_ENTRY)
};
#undef STRING_ENTRY

static constexpr uint8_t EFFECT_COUNT =
    sizeof(EFFECT_NAMES) / sizeof(EFFECT_NAMES[0]);

// --- Forward-declare the global buffer from main.cpp ---
extern uint8_t effectScratchpad[];

/**
 * @brief Creates an effect instance based on its string name.
 */
inline BaseEffect* createEffectByName(const String& name, PixelStrip::Segment* seg) {
    // This macro creates a check for standard effects (e.g., new SolidColor(seg))
    #define CREATE_STANDARD_EFFECT_IF_MATCH(effectName, className) \
        if (name.equalsIgnoreCase(#effectName)) { \
            return new className(seg); \
        }

    // MODIFIED: This macro now uses the constant EFFECT_SCRATCHPAD_SIZE from Config.h
    #define CREATE_BUFFERED_EFFECT_IF_MATCH(effectName, className) \
        if (name.equalsIgnoreCase(#effectName)) { \
            return new className(seg, effectScratchpad, EFFECT_SCRATCHPAD_SIZE); \
        }

    // Automatically generate the 'if-else if' chain for all standard effects
    STANDARD_EFFECT_LIST(CREATE_STANDARD_EFFECT_IF_MATCH)

    // Automatically generate the 'if-else if' chain for all buffered effects
    BUFFERED_EFFECT_LIST(CREATE_BUFFERED_EFFECT_IF_MATCH)

    // Undefine the helper macros to keep them local to this function
    #undef CREATE_STANDARD_EFFECT_IF_MATCH
    #undef CREATE_BUFFERED_EFFECT_IF_MATCH

    return nullptr; // Return null if no matching effect is found
}

// Helper to get effect name from enum value
inline const char *getEffectNameFromId(uint8_t id)
{
    if (id < EFFECT_COUNT)
    {
        return EFFECT_NAMES[id];
    }
    return nullptr;
}
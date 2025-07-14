#pragma once

#include <Arduino.h>
#include "effects/Effects.h"
#include "PixelStrip.h"
#include "AllEffects.h"

// --- Build the EffectType enum from EFFECT_LIST ---
#define ENUM_ENTRY(name, ns) name,
enum class EffectType {
    EFFECT_LIST(ENUM_ENTRY)
    UNKNOWN
};
#undef ENUM_ENTRY

// --- Parallel array of effect names for UI or serialization ---
#define STRING_ENTRY(name, ns) #name,
static const char *EFFECT_NAMES[] = {
    EFFECT_LIST(STRING_ENTRY)
};
#undef STRING_ENTRY

static constexpr uint8_t EFFECT_COUNT =
    sizeof(EFFECT_NAMES) / sizeof(EFFECT_NAMES[0]);

// --- FIX: Moved from Init.h to a more logical location ---
// This function creates an effect instance based on its string name.
inline BaseEffect* createEffectByName(const String& name, PixelStrip::Segment* seg) {
    #define CREATE_EFFECT_IF_MATCH(effectName, className) \
        if (name.equalsIgnoreCase(#effectName)) { \
            return new className(seg); \
        }
    
    EFFECT_LIST(CREATE_EFFECT_IF_MATCH)
    
    #undef CREATE_EFFECT_IF_MATCH
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

#pragma once

#include <Arduino.h>
#include "effects/Effects.h"
#include "PixelStrip.h"
#include "AllEffects.h"   // ← pulls in each effect’s .h (with its ns::start())
#include "config.h"       // for activeR, activeG, activeB globals

extern uint8_t activeR, activeG, activeB;

// ── Build the EffectType enum from EFFECT_LIST ──
#define ENUM_ENTRY(name, ns) name,
enum class EffectType {
    EFFECT_LIST(ENUM_ENTRY)
    UNKNOWN
};
#undef ENUM_ENTRY

// ── Map a (case‐insensitive) String to EffectType ──
inline EffectType effectFromString(const String &str) {
    #define CASE_ENTRY(name, ns)         \
        if (str.equalsIgnoreCase(#name)) \
            return EffectType::name;
    EFFECT_LIST(CASE_ENTRY)
    #undef CASE_ENTRY
    return EffectType::UNKNOWN;
}

// ── Apply the chosen effect to a segment ──
inline void applyEffectToSegment(PixelStrip::Segment *seg, EffectType effect) {
    // stop any running effect
    seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);

    // pack RGB into 0xRRGGBB
    uint32_t color1 = (uint32_t(activeR) << 16)
                    | (uint32_t(activeG) << 8)
                    |  uint32_t(activeB);
    uint32_t color2 = color1;  // default second color = same

    switch (effect) {
        #define EFFECT_CASE(name, ns)         \
            case EffectType::name:            \
                ns::start(seg, color1, color2); \
                break;
        EFFECT_LIST(EFFECT_CASE)
        #undef EFFECT_CASE

        default:
            Serial.println("Unknown effect.");
            break;
    }
}

// ── Parallel array of effect names for UI or serialization ──
#define STRING_ENTRY(name, ns) #name,
static const char *EFFECT_NAMES[] = {
    EFFECT_LIST(STRING_ENTRY)
};
#undef STRING_ENTRY

static constexpr uint8_t EFFECT_COUNT =
    sizeof(EFFECT_NAMES) / sizeof(EFFECT_NAMES[0]);

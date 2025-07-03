#pragma once
#include <Arduino.h>
#include "effects/Effects.h"
#include "PixelStrip.h"
#include "AllEffects.h" // ← includes all effect .h files
#include "config.h"     // for activeR/G/B globals
extern uint8_t activeR, activeG, activeB;

#define ENUM_ENTRY(name, ns) name,
enum class EffectType
{
    EFFECT_LIST(ENUM_ENTRY)
        UNKNOWN
};
#undef ENUM_ENTRY

inline EffectType effectFromString(const String &str)
{
#define CASE_ENTRY(name, ns)         \
    if (str.equalsIgnoreCase(#name)) \
        return EffectType::name;
    EFFECT_LIST(CASE_ENTRY)
#undef CASE_ENTRY
    return EffectType::UNKNOWN;
}

inline void applyEffectToSegment(PixelStrip::Segment *seg, EffectType effect)
{
    seg->startEffect(PixelStrip::Segment::SegmentEffect::NONE);

    // Convert to packed color
    uint32_t color = (activeR << 16) | (activeG << 8) | activeB;

    switch (effect)
    {
#define EFFECT_CASE(name, ns)         \
    case EffectType::name:            \
        ns::start(seg, color, color); \
        break;
        EFFECT_LIST(EFFECT_CASE)
#undef EFFECT_CASE

    default:
        Serial.println("Unknown effect.");
        break;
    }
}

// ── build a parallel array of names ──
#define STRING_ENTRY(name, ns) #name,

static const char *EFFECT_NAMES[] = {
    EFFECT_LIST(STRING_ENTRY)};
#undef STRING_ENTRY

static constexpr uint8_t EFFECT_COUNT = sizeof(EFFECT_NAMES) / sizeof(EFFECT_NAMES[0]);
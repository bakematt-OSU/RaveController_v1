// In a new file: src/effects/EffectParameter.h

#ifndef EFFECT_PARAMETER_H
#define EFFECT_PARAMETER_H

#include <stdint.h> // Required for uint32_t

// The different kinds of data a parameter can hold.
// This tells the app what kind of UI control to show.
enum class ParamType
{
    INTEGER, // For whole numbers -> show a slider
    FLOAT,   // For decimal numbers -> show a slider
    COLOR,   // For a 24-bit color -> show a color wheel button
    BOOLEAN  // For a true/false value -> show a checkbox
};

// The main structure to describe a single parameter.
struct EffectParameter
{
    // The machine-readable name used for communication.
    // Example: "ripple_speed"
    const char *name;

    // The data type from our enum above.
    ParamType type;

    // A union to hold the actual value.
    union
    {
        int intValue;
        float floatValue;
        uint32_t colorValue; // e.g., 0xFF00FF
        bool boolValue;
    } value;

    // Optional: Min/max values to give the app hints for sliders.
    float min_val;
    float max_val;
};

#endif // EFFECT_PARAMETER_H
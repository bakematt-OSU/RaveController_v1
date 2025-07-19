#ifndef EFFECTS_H
#define EFFECTS_H

#include "RainbowChase.h"
#include "SolidColor.h"
#include "FlashOnTrigger.h"
#include "RainbowCycle.h"
#include "TheaterChase.h"
#include "Fire.h"
#include "Flare.h"
#include "ColoredFire.h"
#include "AccelMeter.h"
#include "KineticRipple.h"

// List of effects that DO NOT require an external memory buffer.
// Format: X(EnumName, ClassName)
#define STANDARD_EFFECT_LIST(X)      \
    X(RainbowChase,   RainbowChase)   \
    X(SolidColor,     SolidColor)     \
    X(FlashOnTrigger, FlashOnTrigger) \
    X(RainbowCycle,   RainbowCycle)   \
    X(TheaterChase,   TheaterChase)   \
    X(AccelMeter,     AccelMeter)     \
    X(KineticRipple,  KineticRipple)

// List of effects that DO require the shared "scratchpad" memory buffer.
// Format: X(EnumName, ClassName)
#define BUFFERED_EFFECT_LIST(X) \
    X(Fire,        Fire)        \
    X(Flare,       Flare)       \
    X(ColoredFire, ColoredFire)

// The EFFECT_LIST macro now combines both lists automatically.
// This is used to generate the list of names for the app.
#define EFFECT_LIST(X)          \
    STANDARD_EFFECT_LIST(X)     \
    BUFFERED_EFFECT_LIST(X)

#endif // EFFECTS_H
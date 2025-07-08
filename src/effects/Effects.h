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
// Master list of segment effects.
// Format: X(EnumName, ClassName)
#define EFFECT_LIST(X)                      \
    X(RainbowChase,   RainbowChase)        \
    X(SolidColor,     SolidColor)          \
    X(FlashOnTrigger, FlashOnTrigger)      \
    X(RainbowCycle,   RainbowCycle)        \
    X(TheaterChase,   TheaterChase)        \
    X(Fire,           Fire)                \
    X(Flare,          Flare)               \
    X(ColoredFire,    ColoredFire)         \
    X(AccelMeter,     AccelMeter)          \
    X(KineticRipple,  KineticRipple)

#endif // EFFECTS_H

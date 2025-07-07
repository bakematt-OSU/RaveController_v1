#ifndef EFFECTS_H
#define EFFECTS_H

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

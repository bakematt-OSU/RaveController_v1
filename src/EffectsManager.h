#ifndef EFFECTSMANAGER_H
#define EFFECTSMANAGER_H

#include <Arduino.h>
#include <vector>
#include "PixelStrip.h"

// Function pointers for effect begin/step callbacks
using EffectBeginFunc = void (*)(PixelStrip::Segment&);
using EffectStepFunc  = void (*)(PixelStrip::Segment&);

struct EffectDefinition {
    const char*     name;
    EffectBeginFunc begin;
    EffectStepFunc  step;
    uint16_t        intervalMs;
};

class EffectsManager {
public:
    EffectsManager(PixelStrip& strip);

    // Register built-in effects
    void registerDefaultEffects();

    // Prepare any internal state (called in setup())
    void begin();

    // Start a named effect on the primary segment
    void startEffect(const String& effectName);

    // Kick off the first‐registered effect at boot
    void startDefaultEffect();

    // Parse & dispatch text commands (e.g. "EFFECT rainbow")
    void handleCommand(const String& cmd);

    // Called each loop() to advance running effects
    void updateAll();

private:
    PixelStrip&                   strip;
    std::vector<EffectDefinition> definitions;

    struct ActiveEffect {
        PixelStrip::Segment* segment;
        EffectStepFunc       step;
        uint32_t             lastRun;
        uint16_t             interval;
    };
    std::vector<ActiveEffect> active;
};

#endif // EFFECTSMANAGER_H

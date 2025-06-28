// EffectsManager.h
#ifndef EFFECTSMANAGER_H
#define EFFECTSMANAGER_H

#include <Arduino.h>
#include <vector>
#include "PixelStrip.h"  // for Segment & SegmentEffect enum

// ——— ONE TRUE DEFINITION of your trigger flag ———————————————
// volatile bool triggerRipple = false;  // extern’d in KineticRipple.h :contentReference[oaicite:0]{index=0}
extern volatile bool triggerRipple;

// ——— HERE are the definitions your AccelMeter effect needs —————————
// float accelX = 0.0f;  // extern float accelX; in AccelMeter.h
// float accelY = 0.0f;  // extern float accelY; in AccelMeter.h
// float accelZ = 0.0f;  // extern float accelZ; in AccelMeter.h

// Single declaration—no initializer here!
extern volatile bool triggerRipple;

class EffectsManager {
public:
    struct EffectDef {
        String                                   name;
        PixelStrip::Segment::SegmentEffect       effect;
        uint32_t                                 color1;
        uint32_t                                 color2;
    };

    explicit EffectsManager(PixelStrip& strip);

    void registerDefaultEffects();
    void begin();
    void startDefaultEffect();
    void handleCommand(const String& cmd);
    void updateAll();

private:
    void startEffect(const String& name);

    PixelStrip& strip_;
    std::vector<EffectDef> effects_;
    std::vector<PixelStrip::Segment*> activeSegments_;
};

#endif // EFFECTSMANAGER_H

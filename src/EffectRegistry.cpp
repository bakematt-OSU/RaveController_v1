// EffectRegistry.cpp

#include "EffectRegistry.h"

// pull in all of your concrete‐Effect subclasses:
#include "effects/RainbowChaseEffect.h"
#include "effects/SolidColorEffect.h"
// … add any additional effect headers here …

std::map<String,FactoryFn>& registry() {
    // the only `static` here is the _variable_ holding your map
    static std::map<String,FactoryFn> reg = {
        { "rainbow_chase", RainbowChaseEffect::factory },
        { "solid_color",   SolidColorEffect::factory },
        // …and any other effects, e.g.:
        // { "kinetic_ripple", KineticRippleEffect::factory },
        // { "fire",           FireEffect::factory },
    };
    return reg;
}

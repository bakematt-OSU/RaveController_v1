#include "EffectsManager.h"
#include "Config.h"

// Provide the one true definition of triggerRipple (declared extern in KineticRipple.h)
volatile bool triggerRipple = false;

// plus your accelX/Y/Z definitions...
float accelX = 0.0f;
float accelY = 0.0f;
float accelZ = 0.0f;

EffectsManager::EffectsManager(PixelStrip& strip)
  : strip_(strip)
{}

// Populate effects_ with the ones you’ve actually provided headers for
void EffectsManager::registerDefaultEffects() {
    effects_.clear();

    effects_.push_back({ "solid",          PixelStrip::Segment::SegmentEffect::SOLID,          0xFFFFFF, 0 });
    effects_.push_back({ "rainbow",        PixelStrip::Segment::SegmentEffect::RAINBOW,        0,        0 });
    effects_.push_back({ "flash_trigger",  PixelStrip::Segment::SegmentEffect::FLASH_TRIGGER,  0,        0 });
    effects_.push_back({ "rainbow_cycle",  PixelStrip::Segment::SegmentEffect::RAINBOW_CYCLE,  20,       0 });
    effects_.push_back({ "theater_chase",  PixelStrip::Segment::SegmentEffect::THEATER_CHASE,  50,       0 });
    effects_.push_back({ "fire",           PixelStrip::Segment::SegmentEffect::FIRE,           0,        0 });
    effects_.push_back({ "flare",          PixelStrip::Segment::SegmentEffect::FLARE,          50,       80 });
    effects_.push_back({ "colored_fire",   PixelStrip::Segment::SegmentEffect::COLORED_FIRE,   0,        0 });
    effects_.push_back({ "accel_meter",    PixelStrip::Segment::SegmentEffect::ACCEL_METER,    0,        0 });
    effects_.push_back({ "kinetic_ripple", PixelStrip::Segment::SegmentEffect::KINETIC_RIPPLE, 0,        0 });
}

void EffectsManager::begin() {
    // nothing to do here
}

void EffectsManager::startDefaultEffect() {
    if (!effects_.empty()) {
        startEffect(effects_[0].name);
    }
}

// Internal helper to fire up a named effect on segment 0
void EffectsManager::startEffect(const String& name) {
    activeSegments_.clear();
    for (auto& ed : effects_) {
        if (name.equalsIgnoreCase(ed.name)) {
            auto* seg = strip_.getSegments().at(0);
            seg->startEffect(ed.effect, ed.color1, ed.color2);
            activeSegments_.push_back(seg);
            return;
        }
    }
    // Optional: Serial.println("Unknown effect: " + name);
}

void EffectsManager::handleCommand(const String& cmd) {
    const String prefix = "EFFECT ";
    if (cmd.startsWith(prefix)) {
        String name = cmd.substring(prefix.length());
        name.trim();
        startEffect(name);
    }
}

void EffectsManager::updateAll() {
    for (auto* seg : activeSegments_) {
        seg->update();
    }
}

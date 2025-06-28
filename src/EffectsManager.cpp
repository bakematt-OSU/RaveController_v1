#include "EffectsManager.h"
#include "Config.h"

// ─── HELPER: generate a 0–255 rainbow wheel color ─────────────────────────────
static uint32_t wheel(PixelStrip::Segment& seg, uint8_t pos) {
    if (pos < 85) {
        return seg.ColorRGB(255 - pos * 3, pos * 3, 0);
    } else if (pos < 170) {
        pos -= 85;
        return seg.ColorRGB(0, 255 - pos * 3, pos * 3);
    } else {
        pos -= 170;
        return seg.ColorRGB(pos * 3, 0, 255 - pos * 3);
    }
}

EffectsManager::EffectsManager(PixelStrip& s)
    : strip(s) {}

void EffectsManager::registerDefaultEffects() {
    // Solid white
    definitions.push_back({
        "solid",
        [](PixelStrip::Segment& seg) {
            uint32_t c = seg.ColorRGB(255,255,255);
            for (uint16_t i=seg.startIndex(); i<=seg.endIndex(); ++i)
                seg.setPixelColor(i, c);
        },
        [](PixelStrip::Segment&){},
        0
    });

    // Rainbow cycle
    definitions.push_back({
        "rainbow",
        [](PixelStrip::Segment&){},
        [](PixelStrip::Segment& seg) {
            static uint8_t hue = 0;
            uint16_t len = seg.endIndex() - seg.startIndex() + 1;
            for (uint16_t i=0; i<len; ++i) {
                uint8_t pos = (i * 255 / len + hue) & 0xFF;
                seg.setPixelColor(seg.startIndex() + i, wheel(seg, pos));
            }
            hue++;
        },
        20
    });
}

void EffectsManager::begin() {
    // no-op
}

void EffectsManager::startEffect(const String& name) {
    active.clear();
    for (auto& def : definitions) {
        if (name.equalsIgnoreCase(def.name)) {
            ActiveEffect ae;
            ae.segment  = strip.getSegments().at(0);
            ae.step     = def.step;
            ae.interval = def.intervalMs;
            ae.lastRun  = millis();
            def.begin(*ae.segment);
            active.push_back(ae);
            return;
        }
    }
}

void EffectsManager::startDefaultEffect() {
    if (!definitions.empty())
        startEffect(definitions.front().name);
}

void EffectsManager::handleCommand(const String& cmd) {
    if (cmd.startsWith("EFFECT ")) {
        startEffect(cmd.substring(7));
    }
}

void EffectsManager::updateAll() {
    uint32_t now = millis();
    for (auto& ae : active) {
        if (ae.interval == 0 || now - ae.lastRun >= ae.interval) {
            ae.step(*ae.segment);
            ae.lastRun = now;
        }
    }
}

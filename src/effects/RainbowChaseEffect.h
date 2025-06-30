#pragma once

// Bring in the base Effect interface
#include "../Effect.h"

// Pull in your PixelStrip definition so we can refer to PixelStrip::Segment
#include "../PixelStrip.h"

// Include the real implementation of the chase
#include "RainbowChase.h"
#include "../PixelStrip.h"

struct RainbowChaseEffect : public Effect {
    // Two colors for the chase
    uint32_t color1 = 0xFFFFFF;
    uint32_t color2 = 0xFFFFFF;

    // Unique key used by JSON, CLI, etc.
    const char* effectName() const override {
        return "rainbow";
    }

    // Save our parameters into JSON under "params"
    void serialize(JsonObject &obj) const override {
        obj["color1"] = color1;
        obj["color2"] = color2;
    }

    // Load them back (if present)
    void deserialize(const JsonObject &obj) override {
        if (obj.containsKey("color1"))
            color1 = obj["color1"].as<uint32_t>();
        if (obj.containsKey("color2"))
            color2 = obj["color2"].as<uint32_t>();
    }

    // Actually kick off the effect on a segment
    void apply(Segment *seg) override {
        RainbowChase::start(seg, color1, color2);
    }

    // Factory function, for the registry
    static Effect* factory() {
        return new RainbowChaseEffect();
    }
};

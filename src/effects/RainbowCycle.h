#ifndef RAINBOWCYCLE_H
#define RAINBOWCYCLE_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

class RainbowCycle : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[1];

    unsigned long rainbowFirstPixelHue;
    unsigned long lastUpdate;

public:
    RainbowCycle(PixelStrip::Segment* seg) : segment(seg) {
        params[0].name = "speed";
        params[0].type = ParamType::INTEGER;
        params[0].value.intValue = 20;
        params[0].min_val = 5;
        params[0].max_val = 100;
        // No COLOR or BOOLEAN params, so no min_val/max_val initialization needed here.
        rainbowFirstPixelHue = 0;
        lastUpdate = millis();
    }

    void update() override {
        int interval = params[0].value.intValue;
        if (millis() - lastUpdate < interval) return;
        lastUpdate = millis();

        uint16_t start = segment->startIndex();
        uint16_t end   = segment->endIndex();
        uint16_t length = end - start + 1;

        for (uint16_t i = start; i <= end; ++i) {
            uint16_t offset = i - start;
            uint16_t hue = rainbowFirstPixelHue + (uint32_t(offset) * 65536UL / length);

            uint32_t raw = segment->getParent().ColorHSV(hue);
            uint32_t scaled = PixelStrip::scaleColor(raw, segment->getBrightness());
            segment->getParent().setPixel(i, scaled);
        }

        rainbowFirstPixelHue += 256;
        if (rainbowFirstPixelHue >= 5 * 65536UL) {
            rainbowFirstPixelHue = 0;
        }
    }

    const char* getName() const override { return "RainbowCycle"; }
    int getParameterCount() const override { return 1; }
    EffectParameter* getParameter(int index) override {
        if (index == 0) return &params[0];
        return nullptr;
    }

    void setParameter(const char* name, int value) override {
        if (strcmp(params[0].name, name) == 0 && params[0].type == ParamType::INTEGER) {
            params[0].value.intValue = value;
        }
    }
};

#endif // RAINBOWCYCLE_H
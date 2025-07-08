#ifndef THEATERCHASE_H
#define THEATERCHASE_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

class TheaterChaseEffect : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[1];

    unsigned long lastUpdate;
    unsigned long rainbowFirstPixelHue;
    uint8_t chaseOffset;

public:
    TheaterChaseEffect(PixelStrip::Segment* seg) : segment(seg) {
        params[0].name = "speed";
        params[0].type = ParamType::INTEGER;
        params[0].value.intValue = 50; // Default to a 50ms interval
        params[0].min_val = 10;
        params[0].max_val = 150;
        lastUpdate = millis();
        rainbowFirstPixelHue = 0;
        chaseOffset = 0;
    }

    void update() override {
        int interval = params[0].value.intValue;
        if (millis() - lastUpdate < interval) return;
        lastUpdate = millis();

        segment->allOff();

        uint16_t start = segment->startIndex();
        uint16_t end   = segment->endIndex();
        uint16_t len   = end - start + 1;

        for (uint16_t i = start + chaseOffset; i <= end; i += 3) {
            uint16_t hue = rainbowFirstPixelHue + (uint32_t(i - start) * 65536UL / len);
            uint32_t rawColor = segment->getParent().ColorHSV(hue);
            segment->getParent().getStrip().SetPixelColor(i, rawColor);
        }

        chaseOffset = (chaseOffset + 1) % 3;
        rainbowFirstPixelHue += (65536UL / 90);
    }

    const char* getName() const override { return "TheaterChase"; }
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

#endif // THEATERCHASE_H

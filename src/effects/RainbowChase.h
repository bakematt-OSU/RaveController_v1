#ifndef RAINBOWCHASE_H
#define RAINBOWCHASE_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

class RainbowChaseEffect : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[1];

    unsigned long rainbowFirstPixelHue;
    unsigned long lastUpdate;

public:
    RainbowChaseEffect(PixelStrip::Segment* seg) : segment(seg) {
        params[0].name = "speed";
        params[0].type = ParamType::INTEGER;
        params[0].value.intValue = 30;   // 30ms interval
        params[0].min_val = 5;
        params[0].max_val = 100;
        rainbowFirstPixelHue = 0;
        lastUpdate = millis();
    }

    void update() override {
        int interval = params[0].value.intValue;
        if (millis() - lastUpdate < interval) return;
        lastUpdate = millis();

        for (int i = segment->startIndex(); i <= segment->endIndex(); ++i) {
            int hue = rainbowFirstPixelHue + ((i - segment->startIndex()) * 65536L / (segment->endIndex() - segment->startIndex() + 1));
            uint32_t rawColor = segment->getParent().ColorHSV(hue);
            uint32_t scaledColor = PixelStrip::scaleColor(rawColor, segment->getBrightness());
            segment->getParent().setPixel(i, scaledColor);
        }
        rainbowFirstPixelHue += 256;
    }

    const char* getName() const override { return "RainbowChase"; }
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

#endif // RAINBOWCHASE_H

#ifndef SOLIDCOLOR_H
#define SOLIDCOLOR_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

class SolidColor : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[1];

public:
    SolidColor(PixelStrip::Segment* seg) : segment(seg) {
        params[0].name = "color";
        params[0].type = ParamType::COLOR;
        params[0].value.colorValue = 0x800080; // Default: Purple
    }

    void update() override {
        uint32_t colorValue = params[0].value.colorValue;
        uint32_t scaledColor = PixelStrip::scaleColor(colorValue, segment->getBrightness());

        for (uint16_t i = segment->startIndex(); i <= segment->endIndex(); ++i) {
            segment->getParent().setPixel(i, scaledColor);
        }
    }

    const char* getName() const override { return "SolidColor"; }
    int getParameterCount() const override { return 1; }
    EffectParameter* getParameter(int index) override {
        if (index == 0) return &params[0];
        return nullptr;
    }

    void setParameter(const char* name, uint32_t value) override {
        if (strcmp(params[0].name, name) == 0 && params[0].type == ParamType::COLOR) {
            params[0].value.colorValue = value;
        }
    }
};

#endif // SOLIDCOLOR_H

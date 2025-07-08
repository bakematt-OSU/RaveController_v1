#ifndef FLASHONTRIGGER_H
#define FLASHONTRIGGER_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

class FlashOnTrigger : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[1];

public:
    FlashOnTrigger(PixelStrip::Segment* seg) : segment(seg) {
        params[0].name = "flash_color";
        params[0].type = ParamType::COLOR;
        params[0].value.colorValue = 0xFFFFFF; // Default: white
    }

    void update() override {
        if (segment->triggerIsActive) {
            uint32_t flashColorValue = params[0].value.colorValue;

            RgbColor finalColor(
                (flashColorValue >> 16) & 0xFF,
                (flashColorValue >> 8)  & 0xFF,
                flashColorValue         & 0xFF
            );
            finalColor.Dim(segment->triggerBrightness);
            finalColor.Dim(segment->getBrightness());

            uint32_t rawColor = segment->getParent().Color(finalColor.R, finalColor.G, finalColor.B);
            for (uint16_t i = segment->startIndex(); i <= segment->endIndex(); ++i) {
                segment->getParent().setPixel(i, rawColor);
            }
        } else {
            segment->allOff();
        }
    }

    const char* getName() const override { return "FlashOnTrigger"; }
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

#endif // FLASHONTRIGGER_H

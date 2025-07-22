#ifndef THEATERCHASE_H
#define THEATERCHASE_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

class TheaterChase : public BaseEffect
{
private:
    PixelStrip::Segment *segment;
    EffectParameter params[2]; // Now has 2 parameters: speed and color

    unsigned long lastUpdate;
    uint8_t chaseOffset;

public:
    TheaterChase(PixelStrip::Segment *seg) : segment(seg)
    {
        params[0].name = "speed";
        params[0].type = ParamType::INTEGER;
        params[0].value.intValue = 50; // Default to a 50ms interval
        params[0].min_val = 10;
        params[0].max_val = 150;

        params[1].name = "color";
        params[1].type = ParamType::COLOR;
        params[1].value.colorValue = 0xFF0000; // Default to Red
        params[1].min_val = 0.0;
        params[1].max_val = 0.0;

        lastUpdate = millis();
        chaseOffset = 0;
    }

    void update() override
    {
        int interval = params[0].value.intValue;
        if (millis() - lastUpdate < interval)
            return;
        lastUpdate = millis();

        segment->allOff();

        uint32_t colorValue = params[1].value.colorValue;
        uint32_t scaledColor = PixelStrip::scaleColor(colorValue, segment->getBrightness());

        uint16_t start = segment->startIndex();
        uint16_t end = segment->endIndex();

        for (uint16_t i = start; i <= end; i++)
        {
            if (((i - start) % 3) == chaseOffset)
            {
                segment->getParent().setPixel(i, scaledColor);
            }
        }

        chaseOffset = (chaseOffset + 1) % 3;
    }

    const char *getName() const override { return "TheaterChase"; }
    int getParameterCount() const override { return 2; }
    EffectParameter *getParameter(int index) override
    {
        if (index >= 0 && index < 2)
            return &params[index];
        return nullptr;
    }

    void setParameter(const char *name, int value) override
    {
        if (strcmp(params[0].name, name) == 0 && params[0].type == ParamType::INTEGER)
        {
            params[0].value.intValue = value;
        }
    }

    void setParameter(const char *name, uint32_t value) override
    {
        // Add this override to handle the new color parameter
        if (strcmp(params[1].name, name) == 0 && params[1].type == ParamType::COLOR)
        {
            params[1].value.colorValue = value;
        }
    }
};

#endif // THEATERCHASE_H
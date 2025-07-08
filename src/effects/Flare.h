#ifndef FLARE_H
#define FLARE_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

class Flare : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[2];
    byte* heat = nullptr;
    int heatSize = 0;

    byte qadd8(byte a, byte b) {
        unsigned int s = a + b;
        return s > 255 ? 255 : byte(s);
    }
    byte qsub8(byte a, byte b) {
        return b > a ? 0 : a - b;
    }
    RgbColor FlareHeatColor(byte temperature) {
        byte t192 = round((temperature / 255.0) * 191);
        byte heatramp = (t192 & 0x3F) << 2;

        if (t192 > 0x80) return RgbColor(255, 255, heatramp);
        else if (t192 > 0x40) return RgbColor(255, heatramp, 0);
        else return RgbColor(heatramp, 0, 0);
    }

public:
    Flare(PixelStrip::Segment* seg) : segment(seg) {
        params[0].name = "sparking";
        params[0].type = ParamType::INTEGER;
        params[0].value.intValue = 50;
        params[0].min_val = 0;
        params[0].max_val = 255;

        params[1].name = "cooling";
        params[1].type = ParamType::INTEGER;
        params[1].value.intValue = 80;
        params[1].min_val = 0;
        params[1].max_val = 100;

        if (segment != nullptr) {
            // CORRECTED: Called the correct function 'PixelCount()'
            heatSize = segment->getParent().getStrip().PixelCount();
            heat = new byte[heatSize];
            memset(heat, 0, heatSize);
        }
    }

    ~Flare() override {
        delete[] heat;
    }

    void update() override {
        if (!heat) return;

        int sparking = params[0].value.intValue;
        int cooling  = params[1].value.intValue;
        int startPixel = segment->startIndex();
        int endPixel   = segment->endIndex();
        int len        = endPixel - startPixel + 1;

        if (endPixel >= heatSize) { endPixel = heatSize - 1; }

        for (int i = startPixel; i <= endPixel; ++i) {
            heat[i] = qsub8(heat[i], random(0, ((cooling * 10) / len) + 2));
        }
        for (int k = endPixel; k >= startPixel + 2; --k) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        
        byte chance = segment->triggerIsActive
                      ? map(segment->triggerBrightness, 0, 255, 150, 255)
                      : sparking;

        if (random(255) < chance) {
            int idx = startPixel + random(7);
            if(idx < heatSize) {
               heat[idx] = qadd8(heat[idx], random(160, 255));
            }
        }

        for (int j = startPixel; j <= endPixel; ++j) {
            RgbColor finalColor = FlareHeatColor(heat[j]);
            finalColor.Dim(segment->getBrightness());
            segment->getParent().getStrip().SetPixelColor(j, finalColor);
        }
    }

    const char* getName() const override { return "Flare"; }
    int getParameterCount() const override { return 2; }
    EffectParameter* getParameter(int index) override {
        if (index >= 0 && index < 2) return &params[index];
        return nullptr;
    }

    void setParameter(const char* name, int value) override {
        for (int i = 0; i < 2; ++i) {
            if (strcmp(params[i].name, name) == 0 && params[i].type == ParamType::INTEGER) {
                params[i].value.intValue = value;
                return;
            }
        }
    }
}; // <-- CORRECTED: Added missing closing brace

#endif // FLARE_H
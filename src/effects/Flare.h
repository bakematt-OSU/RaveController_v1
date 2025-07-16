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
    byte* heat      = nullptr;
    int   heatSize  = 0;

    byte qadd8(byte a, byte b) {
        unsigned int s = a + b;
        return s > 255 ? 255 : byte(s);
    }
    byte qsub8(byte a, byte b) {
        return b > a ? 0 : a - b;
    }
    RgbColor FlareHeatColor(byte temperature) {
        byte t192    = round((temperature / 255.0) * 191);
        byte heatr  = (t192 & 0x3F) << 2;
        if (t192 > 0x80)        return RgbColor(255, 255, heatr);
        else if (t192 > 0x40)   return RgbColor(255, heatr, 0);
        else                    return RgbColor(heatr, 0, 0);
    }

public:
    Flare(PixelStrip::Segment* seg)
      : segment(seg)
    {
        // sparking
        params[0].name           = "sparking";
        params[0].type           = ParamType::INTEGER;
        params[0].value.intValue = 50;
        params[0].min_val        = 0;
        params[0].max_val        = 255;
        // cooling
        params[1].name           = "cooling";
        params[1].type           = ParamType::INTEGER;
        params[1].value.intValue = 80;
        params[1].min_val        = 0;
        params[1].max_val        = 100;
        // No COLOR or BOOLEAN params, so no min_val/max_val initialization needed here.

        if (segment) {
            int start = segment->startIndex();
            int end   = segment->endIndex();
            heatSize  = end - start + 1;
            heat      = new byte[heatSize];
            memset(heat, 0, heatSize);
        }
    }

    ~Flare() override {
        delete[] heat;
    }

    void update() override {
        if (!heat) return;

        int sparking   = params[0].value.intValue;
        int cooling    = params[1].value.intValue;
        int len        = heatSize;
        int startPix   = segment->startIndex();

        // cool
        for (int i = 0; i < len; ++i) {
            heat[i] = qsub8(heat[i], random(0, ((cooling * 10) / len) + 2));
        }
        // drift
        for (int k = len - 1; k >= 2; --k) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        // spark (or trigger‑based boost)
        byte chance = segment->triggerIsActive
            ? map(segment->triggerBrightness, 0, 255, 150, 255)
            : sparking;
        if (random(255) < chance) {
            int idx = random(min(7, len));
            heat[idx] = qadd8(heat[idx], random(160, 255));
        }
        // render
        for (int i = 0; i < len; ++i) {
            RgbColor col = FlareHeatColor(heat[i]);
            col.Dim(segment->getBrightness());
            segment->getParent().getStrip().SetPixelColor(startPix + i, col);
        }
    }

    const char* getName() const override { return "Flare"; }
    int getParameterCount() const override { return 2; }
    EffectParameter* getParameter(int idx) override {
        return (idx >= 0 && idx < 2) ? &params[idx] : nullptr;
    }
    void setParameter(const char* name, int val) override {
        for (int i = 0; i < 2; ++i) {
            if (strcmp(params[i].name, name) == 0) {
                params[i].value.intValue = val;
                return;
            }
        }
    }
};

#endif // FLARE_H
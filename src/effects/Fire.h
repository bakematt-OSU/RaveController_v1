#ifndef FIRE_H
#define FIRE_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

class Fire : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[2];
    byte* heat       = nullptr;  // heat buffer for this segment only
    int   heatSize   = 0;        // number of pixels in this segment

    // Fast add/sub helpers
    byte qadd8(byte a, byte b) {
        unsigned int s = a + b;
        return s > 255 ? 255 : byte(s);
    }
    byte qsub8(byte a, byte b) {
        return b > a ? 0 : a - b;
    }

    // Map heat to color
    RgbColor HeatColor(byte temperature) {
        byte t192 = round((temperature / 255.0) * 191);
        byte heatramp = t192 & 0x3F;           // 0..63
        heatramp <<= 2;                        // scale up to 0..252
        if (t192 > 0x80) {
            return RgbColor(255, 255, heatramp);            // white → yellow
        } else if (t192 > 0x40) {
            return RgbColor(255, heatramp, 0);              // yellow → red
        } else {
            return RgbColor(heatramp, 0, 0);                // red → black
        }
    }

public:
    Fire(PixelStrip::Segment* seg)
      : segment(seg)
    {
        // parameter setup
        params[0].name        = "sparking";
        params[0].type        = ParamType::INTEGER;
        params[0].value.intValue = 120;
        params[0].min_val     = 20;
        params[0].max_val     = 200;

        params[1].name        = "cooling";
        params[1].type        = ParamType::INTEGER;
        params[1].value.intValue = 55;
        params[1].min_val     = 20;
        params[1].max_val     = 85;
        // No COLOR or BOOLEAN params, so no min_val/max_val initialization needed here.

        // allocate heat[] just for this segment
        if (segment) {
            int start = segment->startIndex();
            int end   = segment->endIndex();
            heatSize  = end - start + 1;
            heat      = new byte[heatSize];
            memset(heat, 0, heatSize);
        }
    }

    ~Fire() override {
        delete[] heat;
    }

    void update() override {
        if (!heat) return;

        int sparking = params[0].value.intValue;
        int cooling  = params[1].value.intValue;
        int startPix = segment->startIndex();

        // Step 1: cool down every cell
        for (int i = 0; i < heatSize; ++i) {
            heat[i] = qsub8(heat[i], random(0, ((cooling * 10) / heatSize) + 2));
        }
        // Step 2: heat drifts up
        for (int k = heatSize - 1; k >= 2; --k) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        // Step 3: ignite new sparks
        if (random(255) < sparking) {
            int idx = random(min(7, heatSize));
            heat[idx] = qadd8(heat[idx], random(160, 255));
        }
        // Step 4: map heat to LED colors
        for (int i = 0; i < heatSize; ++i) {
            RgbColor c = HeatColor(heat[i]);
            c.Dim(segment->getBrightness());
            segment->getParent().getStrip().SetPixelColor(startPix + i, c);
        }
    }

    const char* getName() const override { return "Fire"; }
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
};

#endif // FIRE_H
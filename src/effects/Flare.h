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

    byte qadd8(byte a, byte b) { unsigned int s = a + b; return s > 255 ? 255 : byte(s); }
    byte qsub8(byte a, byte b) { return b > a ? 0 : a - b; }
    RgbColor FlareHeatColor(byte temperature) {
        byte t192 = round((temperature / 255.0) * 191);
        byte heatr = (t192 & 0x3F) << 2;
        if (t192 > 0x80) return RgbColor(255, 255, heatr);
        else if (t192 > 0x40) return RgbColor(255, heatr, 0);
        else return RgbColor(heatr, 0, 0);
    }

public:
    // MODIFIED CONSTRUCTOR
    Flare(PixelStrip::Segment* seg, uint8_t* externalBuffer, int bufferSize)
      : segment(seg)
    {
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

        if (segment) {
            heatSize  = segment->endIndex() - segment->startIndex() + 1;
            if (heatSize <= bufferSize) {
                heat = externalBuffer; // Point to global buffer
                memset(heat, 0, heatSize);
            }
            // NO 'new byte[]'
        }
    }

    // MODIFIED DESTRUCTOR
    ~Flare() override {
        // NO 'delete[] heat'
    }

    void update() override {
        if (!heat) return;

        int sparking   = params[0].value.intValue;
        int cooling    = params[1].value.intValue;
        int startPix   = segment->startIndex();

        for (int i = 0; i < heatSize; ++i) {
            heat[i] = qsub8(heat[i], random(0, ((cooling * 10) / heatSize) + 2));
        }
        for (int k = heatSize - 1; k >= 2; --k) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        byte chance = segment->triggerIsActive ? map(segment->triggerBrightness, 0, 255, 150, 255) : sparking;
        if (random(255) < chance) {
            int idx = random(min(7, heatSize));
            heat[idx] = qadd8(heat[idx], random(160, 255));
        }
        for (int i = 0; i < heatSize; ++i) {
            RgbColor col = FlareHeatColor(heat[i]);
            uint32_t rawColor = segment->getParent().Color(col.R, col.G, col.B);
            uint32_t scaledColor = PixelStrip::scaleColor(rawColor, segment->getBrightness());
            segment->getParent().setPixel(startPix + i, scaledColor);
        }
    }

    const char* getName() const override { return "Flare"; }
    int getParameterCount() const override { return 2; }
    EffectParameter* getParameter(int idx) override { return (idx >= 0 && idx < 2) ? &params[idx] : nullptr; }
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
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
    byte* heat       = nullptr;
    int   heatSize   = 0;

    byte qadd8(byte a, byte b) { unsigned int s = a + b; return s > 255 ? 255 : byte(s); }
    byte qsub8(byte a, byte b) { return b > a ? 0 : a - b; }

    RgbColor HeatColor(byte temperature) {
        byte t192 = round((temperature / 255.0) * 191);
        byte heatramp = t192 & 0x3F;
        heatramp <<= 2;
        if (t192 > 0x80) return RgbColor(255, 255, heatramp);
        else if (t192 > 0x40) return RgbColor(255, heatramp, 0);
        else return RgbColor(heatramp, 0, 0);
    }

public:
    // MODIFIED CONSTRUCTOR: Accepts an external buffer
    Fire(PixelStrip::Segment* seg, uint8_t* externalBuffer, int bufferSize)
      : segment(seg)
    {
        params[0].name = "sparking";
        params[0].type = ParamType::INTEGER;
        params[0].value.intValue = 120;
        params[0].min_val = 20;
        params[0].max_val = 200;

        params[1].name = "cooling";
        params[1].type = ParamType::INTEGER;
        params[1].value.intValue = 55;
        params[1].min_val = 20;
        params[1].max_val = 85;

        if (segment) {
            heatSize = segment->endIndex() - segment->startIndex() + 1;
            // Use the provided buffer if it's large enough
            if (heatSize <= bufferSize) {
                heat = externalBuffer; // Point to the global buffer
                memset(heat, 0, heatSize);
            }
            // NO 'new byte[]' HERE
        }
    }

    // MODIFIED DESTRUCTOR: No longer deletes the buffer
    ~Fire() override {
        // NO 'delete[] heat' HERE
    }

    void update() override {
        if (!heat) return; // Important safety check

        int sparking = params[0].value.intValue;
        int cooling  = params[1].value.intValue;
        int startPix = segment->startIndex();

        for (int i = 0; i < heatSize; ++i) {
            heat[i] = qsub8(heat[i], random(0, ((cooling * 10) / heatSize) + 2));
        }
        for (int k = heatSize - 1; k >= 2; --k) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        if (random(255) < sparking) {
            int idx = random(min(7, heatSize));
            heat[idx] = qadd8(heat[idx], random(160, 255));
        }
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
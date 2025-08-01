#ifndef COLOREDFIRE_H
#define COLOREDFIRE_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

class ColoredFire : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[5];
    byte* heat      = nullptr;
    int   heatSize  = 0;

    byte qadd8(byte a, byte b) { unsigned int s = a + b; return s > 255 ? 255 : byte(s); }
    byte qsub8(byte a, byte b) { return b > a ? 0 : a - b; }
    byte lerp8(byte a, byte b, byte t) { return a + (long(b - a) * t) / 255; }
    RgbColor ThreeColorHeatColor(byte h, RgbColor c1, RgbColor c2, RgbColor c3) {
        if (h <= 127) {
            byte t = h * 2;
            return RgbColor(lerp8(c1.R, c2.R, t), lerp8(c1.G, c2.G, t), lerp8(c1.B, c2.B, t));
        }
        byte t = (h - 128) * 2;
        return RgbColor(lerp8(c2.R, c3.R, t), lerp8(c2.G, c3.G, t), lerp8(c2.B, c3.B, t));
    }

public:
    // MODIFIED CONSTRUCTOR
    ColoredFire(PixelStrip::Segment* seg, uint8_t* externalBuffer, int bufferSize)
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

        params[2].name = "color1";
        params[2].type = ParamType::COLOR;
        params[2].value.colorValue = 0xFF0000;

        params[3].name = "color2";
        params[3].type = ParamType::COLOR;
        params[3].value.colorValue = 0xFFFF00;

        params[4].name = "color3";
        params[4].type = ParamType::COLOR;
        params[4].value.colorValue = 0xFFFFFF;

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
    ~ColoredFire() override {
        // NO 'delete[] heat'
    }

    void update() override {
        if (!heat) return;

        int sparking = params[0].value.intValue;
        int cooling = params[1].value.intValue;
        uint32_t v1 = params[2].value.colorValue;
        uint32_t v2 = params[3].value.colorValue;
        uint32_t v3 = params[4].value.colorValue;
        RgbColor c1((v1 >> 16) & 0xFF, (v1 >> 8) & 0xFF, v1 & 0xFF);
        RgbColor c2((v2 >> 16) & 0xFF, (v2 >> 8) & 0xFF, v2 & 0xFF);
        RgbColor c3((v3 >> 16) & 0xFF, (v3 >> 8) & 0xFF, v3 & 0xFF);

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
            RgbColor col = ThreeColorHeatColor(heat[i], c1, c2, c3);
            uint32_t rawColor = segment->getParent().Color(col.R, col.G, col.B);
            uint32_t scaledColor = PixelStrip::scaleColor(rawColor, segment->getBrightness());
            segment->getParent().setPixel(startPix + i, scaledColor);
        }
    }

    const char* getName() const override { return "ColoredFire"; }
    int getParameterCount() const override { return 5; }
    EffectParameter* getParameter(int idx) override { return (idx >= 0 && idx < 5) ? &params[idx] : nullptr; }
    void setParameter(const char* name, int val) override {
        for (int i = 0; i < 5; ++i) {
            if (strcmp(params[i].name, name) == 0) {
                if (params[i].type == ParamType::INTEGER) params[i].value.intValue = val;
                if (params[i].type == ParamType::COLOR) params[i].value.colorValue = val;
                return;
            }
        }
    }
};

#endif // COLOREDFIRE_H
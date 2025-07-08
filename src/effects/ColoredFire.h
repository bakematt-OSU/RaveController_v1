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
    byte* heat = nullptr;
    int heatSize = 0;

    byte qadd8(byte a, byte b) {
        unsigned int s = a + b;
        return s > 255 ? 255 : byte(s);
    }
    byte qsub8(byte a, byte b) {
        return b > a ? 0 : a - b;
    }
    byte lerp8(byte a, byte b, byte t) {
        return a + (long(b - a) * t) / 255;
    }
    RgbColor ThreeColorHeatColor(byte h, RgbColor c1, RgbColor c2, RgbColor c3) {
        if (h <= 127) {
            byte t = h * 2;
            return RgbColor(lerp8(c1.R, c2.R, t), lerp8(c1.G, c2.G, t), lerp8(c1.B, c2.B, t));
        }
        byte t = (h - 128) * 2;
        return RgbColor(lerp8(c2.R, c3.R, t), lerp8(c2.G, c3.G, t), lerp8(c2.B, c3.B, t));
    }

public:
    ColoredFire(PixelStrip::Segment* seg) : segment(seg) {
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
        params[2].value.colorValue = 0x000000;

        params[3].name = "color2";
        params[3].type = ParamType::COLOR;
        params[3].value.colorValue = 0xFF0000;

        params[4].name = "color3";
        params[4].type = ParamType::COLOR;
        params[4].value.colorValue = 0xFFFF00;
        
        // This check prevents a crash if the effect is created
        // without being assigned to a segment first.
        if (segment != nullptr) {
            heatSize = segment->getParent().getStrip().PixelCount();
            heat = new byte[heatSize];
            memset(heat, 0, heatSize);
        }
    }

    ~ColoredFire() override {
        delete[] heat;
    }

    void update() override {
        if (!heat) return;

        int sparking = params[0].value.intValue;
        int cooling  = params[1].value.intValue;
        uint32_t c1_val = params[2].value.colorValue;
        uint32_t c2_val = params[3].value.colorValue;
        uint32_t c3_val = params[4].value.colorValue;

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
        if (random(255) < sparking) {
            int idx = startPixel + random(7);
            if (idx < heatSize) {
                heat[idx] = qadd8(heat[idx], random(160, 255));
            }
        }

        RgbColor c1((c1_val >> 16) & 0xFF, (c1_val >> 8) & 0xFF, c1_val & 0xFF);
        RgbColor c2((c2_val >> 16) & 0xFF, (c2_val >> 8) & 0xFF, c2_val & 0xFF);
        RgbColor c3((c3_val >> 16) & 0xFF, (c3_val >> 8) & 0xFF, c3_val & 0xFF);

        for (int j = startPixel; j <= endPixel; ++j) {
            RgbColor finalColor = ThreeColorHeatColor(heat[j], c1, c2, c3);
            finalColor.Dim(segment->getBrightness());
            segment->getParent().getStrip().SetPixelColor(j, finalColor);
        }
    }

    const char* getName() const override { return "ColoredFire"; }
    int getParameterCount() const override { return 5; }
    EffectParameter* getParameter(int index) override {
        if (index >= 0 && index < 5) return &params[index];
        return nullptr;
    }

    void setParameter(const char* name, int value) override {
        for (int i = 0; i < 5; ++i) {
            if (strcmp(params[i].name, name) == 0 && params[i].type == ParamType::INTEGER) {
                params[i].value.intValue = value;
                return;
            }
        }
    }
    void setParameter(const char* name, uint32_t value) override {
        for (int i = 0; i < 5; ++i) {
            if (strcmp(params[i].name, name) == 0 && params[i].type == ParamType::COLOR) {
                params[i].value.colorValue = value;
                return;
            }
        }
    }
};

#endif // COLOREDFIRE_H
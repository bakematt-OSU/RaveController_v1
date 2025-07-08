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
    byte* heat = nullptr; // The heat array is now a pointer
    int heatSize = 0;     // Store the size of the heat array

    byte qadd8(byte a, byte b) {
        unsigned int s = a + b;
        return s > 255 ? 255 : byte(s);
    }
    byte qsub8(byte a, byte b) {
        return b > a ? 0 : a - b;
    }
    RgbColor HeatColor(byte temperature) {
        byte t192 = round((temperature / 255.0) * 191);
        byte heatramp = (t192 & 0x3F) << 2;

        if (t192 > 0x80) return RgbColor(255, 255, heatramp);
        else if (t192 > 0x40) return RgbColor(255, heatramp, 0);
        else return RgbColor(heatramp, 0, 0);
    }

public:
    // Constructor
    Fire(PixelStrip::Segment* seg) : segment(seg) {
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
        
        // CORRECTED: Dynamically allocate the heat array to the correct size
        if (segment != nullptr) {
            // Get the total number of pixels in the parent strip for a safe buffer size
            heatSize = segment->getParent().getStrip().PixelCount();
            heat = new byte[heatSize];
            memset(heat, 0, heatSize);
        }
    }

    // Destructor to free the allocated memory
    ~Fire() override {
        delete[] heat;
    }

    void update() override {
        if (!heat) return; // Safety check

        int sparking = params[0].value.intValue;
        int cooling  = params[1].value.intValue;

        int startPixel = segment->startIndex();
        int endPixel   = segment->endIndex();
        int len        = endPixel - startPixel + 1;

        // Safety check to prevent writing past the end of the heat array
        if(endPixel >= heatSize) {
            endPixel = heatSize - 1;
        }

        for (int i = startPixel; i <= endPixel; ++i) {
            heat[i] = qsub8(heat[i], random(0, ((cooling * 10) / len) + 2));
        }
        for (int k = endPixel; k >= startPixel + 2; --k) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        if (random(255) < sparking) {
            int idx = startPixel + random(7);
            if(idx < heatSize) { // More safety
               heat[idx] = qadd8(heat[idx], random(160, 255));
            }
        }

        for (int j = startPixel; j <= endPixel; ++j) {
            RgbColor finalColor = HeatColor(heat[j]);
            finalColor.Dim(segment->getBrightness());
            segment->getParent().getStrip().SetPixelColor(j, finalColor);
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
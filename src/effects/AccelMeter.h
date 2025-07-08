#ifndef ACCELMETER_H
#define ACCELMETER_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"    // <<-- Add this line
#include <Arduino.h>

// Provided by your main.cpp (live accel data)
extern float accelX;

class AccelMeter : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[2];

public:
    AccelMeter(PixelStrip::Segment* seg) : segment(seg) {
        // Parameter 1: Bubble Color
        params[0].name = "color";
        params[0].type = ParamType::COLOR;
        params[0].value.colorValue = 0x00FF00; // Green

        // Parameter 2: Bubble Size
        params[1].name = "bubble_size";
        params[1].type = ParamType::INTEGER;
        params[1].value.intValue = 5;
        params[1].min_val = 1;
        params[1].max_val = 25;
    }

    void update() override {
        uint32_t bubbleColorValue = params[0].value.colorValue;
        int      bubbleSize       = params[1].value.intValue;

        int startPixel = segment->startIndex();
        int numPixels  = segment->endIndex() - startPixel + 1;
        float mapped_position = (accelX + 1.0f) * (numPixels - bubbleSize) / 2.0f;
        int centerPixel = constrain((int)mapped_position, 0, numPixels - bubbleSize) + startPixel;

        RgbColor finalBubbleColor(
            (bubbleColorValue >> 16) & 0xFF,
            (bubbleColorValue >> 8)  & 0xFF,
            bubbleColorValue         & 0xFF
        );
        finalBubbleColor.Dim(segment->getBrightness());

        segment->allOff();
        for (int i = 0; i < bubbleSize; ++i) {
            segment->getParent().getStrip().SetPixelColor(centerPixel + i, finalBubbleColor);
        }
    }

    // --- Polymorphic API ---

    const char* getName() const override { return "AccelMeter"; }
    int getParameterCount() const override { return 2; }
    EffectParameter* getParameter(int index) override {
        if (index >= 0 && index < 2) return &params[index];
        return nullptr;
    }

    // Optional: Allow set by name/type
    void setParameter(const char* name, int value) override {
        for (int i = 0; i < 2; ++i) {
            if (strcmp(params[i].name, name) == 0 && params[i].type == ParamType::INTEGER) {
                params[i].value.intValue = value;
                return;
            }
        }
    }
    void setParameter(const char* name, uint32_t value) override {
        for (int i = 0; i < 2; ++i) {
            if (strcmp(params[i].name, name) == 0 && params[i].type == ParamType::COLOR) {
                params[i].value.colorValue = value;
                return;
            }
        }
    }
};

#endif // ACCELMETER_H

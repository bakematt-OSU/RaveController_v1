#ifndef KINETICRIPPLE_H
#define KINETICRIPPLE_H

#include "../PixelStrip.h"
#include "EffectParameter.h"
#include "BaseEffect.h"
#include <Arduino.h>

// This global variable signals when a step/movement has been detected.
extern volatile bool triggerRipple;

class KineticRipple : public BaseEffect {
private:
    PixelStrip::Segment* segment;
    EffectParameter params[3];

    bool rippleActive = false;
    unsigned long rippleStartTime = 0;
    RgbColor rippleColor;

public:
    KineticRipple(PixelStrip::Segment* seg) : segment(seg) {
        params[0].name = "color";
        params[0].type = ParamType::COLOR;
        params[0].value.colorValue = 0x8A2BE2; // BlueViolet

        params[1].name = "speed";
        params[1].type = ParamType::FLOAT;
        params[1].value.floatValue = 0.2f;
        params[1].min_val = 0.05f;
        params[1].max_val = 1.0f;

        params[2].name = "width";
        params[2].type = ParamType::INTEGER;
        params[2].value.intValue = 3;
        params[2].min_val = 1;
        params[2].max_val = 11;
    }

    void update() override {
        if (triggerRipple && !rippleActive) {
            rippleActive = true;
            rippleStartTime = millis();
            uint32_t rippleColorValue = params[0].value.colorValue;
            rippleColor = RgbColor((rippleColorValue >> 16) & 0xFF, (rippleColorValue >> 8) & 0xFF, rippleColorValue & 0xFF);
            triggerRipple = false; // Consume the trigger
        }

        segment->allOff();

        if (rippleActive) {
            float speed = params[1].value.floatValue;
            int width   = params[2].value.intValue;

            float elapsed = millis() - rippleStartTime;
            int radius = int(elapsed * speed);
            int s = segment->startIndex();
            int e = segment->endIndex();
            int center = s + (e - s) / 2;
            int halfLen = (e - s) / 2;
            if (halfLen == 0) halfLen = 1;

            int brightness_fade = 255 - (radius * 255 / halfLen);
            brightness_fade = constrain(brightness_fade, 0, 255);

            RgbColor finalColor = rippleColor;
            finalColor.Dim(brightness_fade);
            finalColor.Dim(segment->getBrightness());

            int halfWidth = width / 2;
            bool pixelsDrawn = false;

            for (int i = 0; i < width; ++i) {
                int p1 = center - radius - halfWidth + i;
                if (p1 >= s && p1 <= e) {
                    segment->getParent().getStrip().SetPixelColor(p1, finalColor);
                    pixelsDrawn = true;
                }

                int p2 = center + radius - halfWidth + i;
                if (p2 != p1 && p2 >= s && p2 <= e) {
                    segment->getParent().getStrip().SetPixelColor(p2, finalColor);
                    pixelsDrawn = true;
                }
            }

            if (!pixelsDrawn && elapsed > 100) {
                rippleActive = false;
            }
        }
    }

    const char* getName() const override { return "KineticRipple"; }
    int getParameterCount() const override { return 3; }
    EffectParameter* getParameter(int index) override {
        if (index >= 0 && index < 3) return &params[index];
        return nullptr;
    }

    void setParameter(const char* name, uint32_t value) override {
        if (strcmp(params[0].name, name) == 0 && params[0].type == ParamType::COLOR) {
            params[0].value.colorValue = value;
        }
    }
    void setParameter(const char* name, float value) override {
        if (strcmp(params[1].name, name) == 0 && params[1].type == ParamType::FLOAT) {
            params[1].value.floatValue = value;
        }
    }
    void setParameter(const char* name, int value) override {
        if (strcmp(params[2].name, name) == 0 && params[2].type == ParamType::INTEGER) {
            params[2].value.intValue = value;
        }
    }
};

#endif // KINETICRIPPLE_H

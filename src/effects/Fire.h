#ifndef FIRE_H
#define FIRE_H

#include "../PixelStrip.h"

namespace Fire {
    // --- Helper functions to replace missing library functions ---
    inline byte qadd8(byte a, byte b) {
        unsigned int s = a + b;
        return s > 255 ? 255 : byte(s);
    }
    inline byte qsub8(byte a, byte b) { return b > a ? 0 : a - b; }

    // --- Effect-specific constants ---
    const int MAX_LEDS = 300;
    static byte heat[MAX_LEDS];

    // Map a heat value (0-255) to a fire-like RGB color
    inline RgbColor HeatColor(byte t) {
        byte t192 = round((t / 255.0) * 191);
        byte ramp = (t192 & 0x3F) << 2;
        if (t192 > 0x80)
            return RgbColor(255, 255, ramp);
        else if (t192 > 0x40)
            return RgbColor(255, ramp, 0);
        else
            return RgbColor(ramp, 0, 0);
    }

    // Start the fire effect: ignore color parameters for interval, use defaults
    inline void start(PixelStrip::Segment* seg, uint32_t c1, uint32_t c2) {
        seg->setEffect(PixelStrip::Segment::SegmentEffect::FIRE);
        seg->active = true;
        // Always run updates every ~15ms
        seg->interval = 15;
        // Allow overriding spark and cooling if values are small
        if (c1 > 0 && c1 <= 255) seg->fireSparking = (uint8_t)c1;
        if (c2 > 0 && c2 <= 100) seg->fireCooling = (uint8_t)c2;
    }

    // Update the fire effect: cool, diffuse, spark, and map to LEDs
    inline void update(PixelStrip::Segment* seg) {
        if (!seg->active || (millis() - seg->lastUpdate < seg->interval)) return;
        seg->lastUpdate = millis();
        int s = seg->startIndex(), e = seg->endIndex(), len = e - s + 1;

        // Step 1. Cool down every cell a little
        for (int i = s; i <= e; ++i) {
            heat[i] = qsub8(heat[i], random(0, ((seg->fireCooling * 10) / len) + 2));
        }
        // Step 2. Heat diffusion
        for (int k = e; k >= s + 2; --k) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        // Step 3. Sparking
        if (random(255) < seg->fireSparking) {
            int y = s + random(7);
            heat[y] = qadd8(heat[y], random(160, 255));
        }
        // Step 4. Map from heat to LED colors
        for (int j = s; j <= e; ++j) {
            RgbColor col = HeatColor(heat[j]);
            uint32_t raw = seg->getParent().Color(col.R, col.G, col.B);
            seg->getParent().setPixel(j, raw);
        }
    }
}

#endif // FIRE_H

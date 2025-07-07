#ifndef COLOREDFIRE_H
#define COLOREDFIRE_H

#include "../PixelStrip.h"
#include <Arduino.h>

namespace ColoredFire {
    const int MAX_LEDS_FIRE = 300;
    static byte heat[MAX_LEDS_FIRE];

    inline byte qadd8(byte a, byte b) {
        unsigned int s = a + b;
        return s > 255 ? 255 : byte(s);
    }

    inline byte qsub8(byte a, byte b) {
        return b > a ? 0 : a - b;
    }

    inline byte lerp8(byte a, byte b, byte t) {
        return a + (long(b - a) * t) / 255;
    }

    inline RgbColor ThreeColorHeatColor(byte h, RgbColor c1, RgbColor c2, RgbColor c3) {
        if (h <= 127) {
            byte t = h * 2;
            return RgbColor(
                lerp8(c1.R, c2.R, t),
                lerp8(c1.G, c2.G, t),
                lerp8(c1.B, c2.B, t)
            );
        }
        byte t = (h - 128) * 2;
        return RgbColor(
            lerp8(c2.R, c3.R, t),
            lerp8(c2.G, c3.G, t),
            lerp8(c2.B, c3.B, t)
        );
    }

    inline void start(PixelStrip::Segment* seg, uint32_t c1, uint32_t c2) {
        seg->setEffect(PixelStrip::Segment::SegmentEffect::ColoredFire);
        seg->active = true;
        seg->interval = 15;
    }

    inline void update(PixelStrip::Segment* seg) {
        if (!seg->active || (millis() - seg->lastUpdate < seg->interval)) return;
        seg->lastUpdate = millis();

        int s = seg->startIndex();
        int e = seg->endIndex();
        int len = e - s + 1;

        for (int i = s; i <= e; ++i) {
            heat[i] = qsub8(heat[i], random(0, ((seg->fireCooling * 10) / len) + 2));
        }
        for (int k = e; k >= s + 2; --k) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        if (random(255) < seg->fireSparking) {
            int idx = s + random(7);
            heat[idx] = qadd8(heat[idx], random(160, 255));
        }

        RgbColor fc1((seg->fireColor1 >> 16) & 0xFF,
                     (seg->fireColor1 >> 8)  & 0xFF,
                     seg->fireColor1         & 0xFF);
        RgbColor fc2((seg->fireColor2 >> 16) & 0xFF,
                     (seg->fireColor2 >> 8)  & 0xFF,
                     seg->fireColor2         & 0xFF);
        RgbColor fc3((seg->fireColor3 >> 16) & 0xFF,
                     (seg->fireColor3 >> 8)  & 0xFF,
                     seg->fireColor3         & 0xFF);

        for (int j = s; j <= e; ++j) {
            RgbColor col = ThreeColorHeatColor(heat[j], fc1, fc2, fc3);
            col.Dim(seg->getBrightness());
            seg->getParent().getStrip().SetPixelColor(j, col);
        }
    }
}

#endif // COLOREDFIRE_H

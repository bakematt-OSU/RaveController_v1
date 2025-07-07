#ifndef FLARE_H
#define FLARE_H

#include "../PixelStrip.h"

namespace Flare {
    const int MAX_LEDS = 300;
    static byte flare_heat[MAX_LEDS];

    inline RgbColor FlareHeatColor(byte t) {
        byte t192 = round((t / 255.0) * 191);
        byte ramp = (t192 & 0x3F) << 2;
        if (t192 > 0x80) {
            return RgbColor(255, 255, ramp);
        } else if (t192 > 0x40) {
            return RgbColor(255, ramp, 0);
        } else {
            return RgbColor(ramp, 0, 0);
        }
    }

    inline byte qadd8(byte a, byte b) {
        unsigned int s = a + b;
        return s > 255 ? 255 : byte(s);
    }

    inline byte qsub8(byte a, byte b) {
        return b > a ? 0 : a - b;
    }

    inline void start(PixelStrip::Segment* seg, uint32_t c1, uint32_t c2) {
        seg->setEffect(PixelStrip::Segment::SegmentEffect::Flare);
        seg->active = true;
        seg->interval = 15;
        seg->fireSparking = (c1 > 0 && c1 <= 255) ? c1 : 50;
        seg->fireCooling  = (c2 > 0 && c2 <= 100) ? c2 : 80;
    }

    inline void update(PixelStrip::Segment* seg) {
        if (!seg->active || (millis() - seg->lastUpdate < seg->interval)) return;
        seg->lastUpdate = millis();
        int s   = seg->startIndex();
        int e   = seg->endIndex();
        int len = e - s + 1;

        for (int i = s; i <= e; ++i) {
            flare_heat[i] = qsub8(flare_heat[i],
                                 random(0, ((seg->fireCooling * 10) / len) + 2));
        }
        for (int k = e; k >= s + 2; --k) {
            flare_heat[k] = (flare_heat[k - 1] + flare_heat[k - 2] + flare_heat[k - 2]) / 3;
        }
        byte chance = seg->triggerIsActive
                      ? map(seg->triggerBrightness, 0, 255, 150, 255)
                      : seg->fireSparking;
        if (random(255) < chance) {
            int idx = s + random(7);
            flare_heat[idx] = qadd8(flare_heat[idx], random(160, 255));
        }
        for (int j = s; j <= e; ++j) {
            RgbColor col = FlareHeatColor(flare_heat[j]);
            col.Dim(seg->getBrightness());
            seg->getParent().getStrip().SetPixelColor(j, col);
        }
    }
}

#endif // FLARE_H

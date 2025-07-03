#ifndef THEATERCHASE_H
#define THEATERCHASE_H

#include "../PixelStrip.h"

namespace TheaterChase {
    // Start the theater chase effect: default 50ms interval or override via c1 (≤1000ms)
    inline void start(PixelStrip::Segment* seg, uint32_t c1, uint32_t /*c2*/) {
        seg->setEffect(PixelStrip::Segment::SegmentEffect::THEATER_CHASE);
        seg->active = true;
        seg->interval = (c1 > 0 && c1 <= 1000) ? (uint16_t)c1 : 50;
        seg->lastUpdate = millis();
        seg->rainbowFirstPixelHue = 0;
        seg->chaseOffset = 0;
    }

    // Update frame: clear segment, light every 3rd pixel with rotating hue
    inline void update(PixelStrip::Segment* seg) {
        if (!seg->active || (millis() - seg->lastUpdate < seg->interval)) return;
        seg->lastUpdate = millis();

        seg->clear();
        uint16_t start = seg->startIndex();
        uint16_t end = seg->endIndex();
        uint16_t len = end - start + 1;

        for (uint16_t i = start + seg->chaseOffset; i <= end; i += 3) {
            uint16_t hue = seg->rainbowFirstPixelHue + (uint32_t(i - start) * 65536UL / len);
            uint32_t rawColor = seg->getParent().ColorHSV(hue);
            // Use raw color so it's dimmed only once by overall strip brightness
            seg->getParent().setPixel(i, rawColor);
        }

        seg->chaseOffset = (seg->chaseOffset + 1) % 3;
        seg->rainbowFirstPixelHue += (65536UL / 90);
    }
}

#endif // THEATERCHASE_H
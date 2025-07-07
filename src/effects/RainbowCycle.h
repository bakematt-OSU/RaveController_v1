#ifndef RAINBOWCYCLE_H
#define RAINBOWCYCLE_H

#include "../PixelStrip.h"

namespace RainbowCycle {
    // Initialize the rainbow cycle effect
    inline void start(PixelStrip::Segment* seg, uint32_t c1, uint32_t /*c2*/) {
        seg->setEffect(PixelStrip::Segment::SegmentEffect::RainbowCycle);
        seg->active = true;
        // Use passed interval if reasonable, otherwise default to 20ms
        seg->interval = (c1 > 0 && c1 <= 1000) ? (uint16_t)c1 : 20;
        seg->lastUpdate = millis();
        seg->rainbowFirstPixelHue = 0;
    }

    // Update one frame of the rainbow cycle
    inline void update(PixelStrip::Segment* seg) {
        if (!seg->active || (millis() - seg->lastUpdate < seg->interval)) return;
        seg->lastUpdate = millis();

        uint16_t start = seg->startIndex();
        uint16_t end   = seg->endIndex();
        uint16_t length = end - start + 1;

        // Map each pixel in the segment to a hue along the cycle
        for (uint16_t i = start; i <= end; ++i) {
            uint16_t offset = i - start;
            uint16_t hue = seg->rainbowFirstPixelHue + (uint32_t(offset) * 65536UL / length);
            // Generate raw HSV color, then scale by brightness
            uint32_t raw = seg->getParent().ColorHSV(hue);
            uint32_t scaled = PixelStrip::scaleColor(raw, seg->getBrightness());
            seg->getParent().setPixel(i, scaled);
        }

        // Advance the hue for the next frame
        seg->rainbowFirstPixelHue += 256;
        if (seg->rainbowFirstPixelHue >= 5 * 65536UL) {
            seg->rainbowFirstPixelHue = 0;
        }
    }
}

#endif // RAINBOWCYCLE_H

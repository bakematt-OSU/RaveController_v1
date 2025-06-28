#ifndef RAINBOWCYCLE_H
#define RAINBOWCYCLE_H

#include "../PixelStrip.h"

namespace RainbowCycle {

/**
 * @brief Initializes the RainbowCycle effect.
 * @param seg The segment to apply the effect to.
 * @param color1 The 'wait' time in milliseconds for the delay. Defaults to 20ms.
 * @param color2 Unused for this effect.
 */
inline void start(PixelStrip::Segment* seg, uint32_t color1, uint32_t color2) {
    seg->setEffect(PixelStrip::Segment::SegmentEffect::RAINBOW_CYCLE);
    seg->active = true;
    seg->interval = (color1 > 0) ? color1 : 20; // Use color1 as the 'wait' parameter
    seg->lastUpdate = millis();
    seg->rainbowFirstPixelHue = 0; 
}

/**
 * @brief Updates the RainbowCycle effect on each loop, non-blocking.
 */
inline void update(PixelStrip::Segment* seg) {
    if (!seg->active) return;

    if (millis() - seg->lastUpdate < seg->interval) return;
    seg->lastUpdate = millis();

    for (uint16_t i = seg->startIndex(); i <= seg->endIndex(); i++) {
        uint16_t pixelHue = seg->rainbowFirstPixelHue + 
                           ((i - seg->startIndex()) * 65536L / (seg->endIndex() - seg->startIndex() + 1));
        
        uint32_t color = seg->getParent().ColorHSV(pixelHue);

        // --- CORRECTED LINE ---
        // Manually break the color into R, G, B components before creating the RgbColor object.
        RgbColor rgbColor((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
        seg->getParent().getStrip().SetPixelColor(i, rgbColor);
    }

    seg->rainbowFirstPixelHue += 256;

    if (seg->rainbowFirstPixelHue >= 5 * 65536) {
        seg->rainbowFirstPixelHue = 0;
    }
}

}

#endif
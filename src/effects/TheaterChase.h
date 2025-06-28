#ifndef THEATERCHASE_H
#define THEATERCHASE_H

#include "../PixelStrip.h"

namespace TheaterChase {

/**
 * @brief Initializes the TheaterChase effect.
 * @param seg The segment to apply the effect to.
 * @param color1 The 'wait' time in milliseconds. Defaults to 50ms.
 * @param color2 Unused.
 */
inline void start(PixelStrip::Segment* seg, uint32_t color1, uint32_t color2) {
    seg->setEffect(PixelStrip::Segment::SegmentEffect::THEATER_CHASE);
    seg->active = true;
    seg->interval = (color1 > 0) ? color1 : 50; // Use color1 as the 'wait' parameter
    seg->lastUpdate = millis();
    seg->rainbowFirstPixelHue = 0; // Re-using this for the hue state
    seg->chaseOffset = 0;          // Start the chase from the first pixel
}

/**
 * @brief Updates the TheaterChase effect on each loop, non-blocking.
 */
inline void update(PixelStrip::Segment* seg) {
    if (!seg->active) return;

    // Check if enough time has passed to draw the next frame
    if (millis() - seg->lastUpdate < seg->interval) return;
    seg->lastUpdate = millis();

    seg->clear(); // Clear the segment for this frame

    // This loop lights up every third pixel, starting from the current offset
    for (uint16_t i = seg->startIndex() + seg->chaseOffset; i <= seg->endIndex(); i += 3) {
        // Calculate the hue for this pixel
        uint16_t hue = seg->rainbowFirstPixelHue + 
                       (i - seg->startIndex()) * 65536L / (seg->endIndex() - seg->startIndex() + 1);
        
        uint32_t color = seg->getParent().ColorHSV(hue);
        seg->getParent().setPixel(i, color); // Use setPixel to apply brightness
    }
    
    // --- Update state for the NEXT frame ---
    
    // Advance the chase offset (0, 1, 2, 0, 1, 2, ...)
    seg->chaseOffset = (seg->chaseOffset + 1) % 3;

    // Advance the hue slightly
    seg->rainbowFirstPixelHue += 65536 / 90;
}

}

#endif
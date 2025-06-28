#ifndef FLASH_ON_TRIGGER_H
#define FLASH_ON_TRIGGER_H

#include "../PixelStrip.h"

namespace FlashOnTrigger {

inline void start(PixelStrip::Segment* seg, uint32_t color1, uint32_t color2) {
    if (!seg) return;
    seg->setEffect(PixelStrip::Segment::SegmentEffect::FLASH_TRIGGER);
    seg->active = true;
    seg->baseColor = color1;
}

// UPDATED: The signature is now standard. It gets the state from the segment object.
inline void update(PixelStrip::Segment* seg) {
    if (!seg->active) return;
    
    PixelStrip& strip = seg->getParent();

    // Check the trigger state that is now stored inside the segment
    if (seg->triggerIsActive) {
        uint32_t baseColor = seg->baseColor;
        uint8_t r = (baseColor >> 16) & 0xFF;
        uint8_t g = (baseColor >> 8) & 0xFF;
        uint8_t b = baseColor & 0xFF;
        RgbColor finalColor(r, g, b);

        // Use the brightness now stored inside the segment
        finalColor.Dim(seg->triggerBrightness);
        
        for (uint16_t i = seg->startIndex(); i <= seg->endIndex(); ++i) {
            strip.getStrip().SetPixelColor(i, finalColor);
        }
    } else {
        seg->allOff();
    }
}

} 

#endif
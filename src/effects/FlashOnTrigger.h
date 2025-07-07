#ifndef FLASHONTRIGGER_H
#define FLASHONTRIGGER_H

#include "../PixelStrip.h"

namespace FlashOnTrigger {
    // Start the flash-on-trigger effect
    inline void start(PixelStrip::Segment* seg, uint32_t c1, uint32_t /*c2*/) {
        if (!seg) return;
        seg->setEffect(PixelStrip::Segment::SegmentEffect::FlashOnTrigger);
        seg->active    = true;
        seg->baseColor = c1;
    }

    // Update one frame: light the segment while trigger is active, else clear it
    inline void update(PixelStrip::Segment* seg) {
        if (!seg || !seg->active) return;

        PixelStrip &strip = seg->getParent();

        if (seg->triggerIsActive) {
            // Build and dim the color
            uint32_t bc = seg->baseColor;
            RgbColor fc(
                (bc >> 16) & 0xFF,
                (bc >> 8)  & 0xFF,
                bc         & 0xFF
            );
            fc.Dim(seg->triggerBrightness);
            fc.Dim(seg->getBrightness());

            // Convert to packed color and set each LED
            uint32_t raw = strip.Color(fc.R, fc.G, fc.B);
            for (uint16_t i = seg->startIndex(); i <= seg->endIndex(); ++i) {
                strip.setPixel(i, raw);
            }
        }
        else {
            // Turn the segment off
            seg->allOff();
        }
    }
}

#endif // FLASHONTRIGGER_H

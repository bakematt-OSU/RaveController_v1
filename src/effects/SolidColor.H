#ifndef SOLIDCOLOR_H
#define SOLIDCOLOR_H

#include "../PixelStrip.h"

namespace SolidColor {

// UPDATED: Uses the generic state variables from the Segment class.
inline void start(PixelStrip::Segment* seg, uint32_t color1, uint32_t color2) {
    seg->setEffect(PixelStrip::Segment::SegmentEffect::SOLID);
    seg->setBrightness(255);
    seg->active = true;
    seg->baseColor = color1;
}

// UPDATED: Checks for the generic 'active' flag and uses 'baseColor'.
inline void update(PixelStrip::Segment* seg) {
    if (!seg->active) return;
    
    for (uint16_t i = seg->startIndex(); i <= seg->endIndex(); ++i) {
        seg->getParent().setPixel(i, seg->baseColor);
    }
}

}

#endif
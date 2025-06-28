#ifndef RAINBOWCHASE_H
#define RAINBOWCHASE_H

#include "../PixelStrip.h"

namespace RainbowChase
{

    // UPDATED: Uses the generic state variables from the Segment class.
    inline void start(PixelStrip::Segment *seg, uint32_t color1, uint32_t color2)
    {
        seg->setEffect(PixelStrip::Segment::SegmentEffect::RAINBOW);
        seg->active = true;
        seg->interval = 30; // Use generic 'interval' for the delay
        seg->lastUpdate = millis();
        seg->rainbowFirstPixelHue = 0; // This state is unique to the rainbow effect
        seg->setBrightness(50);
    }

    // UPDATED: Checks for the generic state variables.
    inline void update(PixelStrip::Segment *seg)
    {
        if (!seg->active)
            return;

        unsigned long now = millis();
        if (now - seg->lastUpdate < seg->interval)
            return;

        seg->lastUpdate = now;

        for (int i = seg->startIndex(); i <= seg->endIndex(); ++i)
        {
            int pixelHue = seg->rainbowFirstPixelHue + ((i - seg->startIndex()) * 65536L / (seg->endIndex() - seg->startIndex() + 1));
            uint32_t color = seg->getParent().ColorHSV(pixelHue);
            seg->getParent().setPixel(i, color);
        }
        seg->rainbowFirstPixelHue += 256;
    }

}

#endif
#ifndef ACCELMETER_H
#define ACCELMETER_H

#include "../PixelStrip.h"
#include <Arduino.h>

// MODIFIED: "volatile" removed to match main.cpp
extern float accelX, accelY, accelZ;

namespace AccelMeter
{

    const int BUBBLE_SIZE = 5;

    inline void start(PixelStrip::Segment *seg, uint32_t color1, uint32_t color2)
    {
        seg->setEffect(PixelStrip::Segment::SegmentEffect::ACCEL_METER);
        seg->active = true;
        seg->interval = 10;
        seg->baseColor = color1;
    }

    inline void update(PixelStrip::Segment *seg)
    {
        if (!seg->active || (millis() - seg->lastUpdate < seg->interval))
        {
            return;
        }
        seg->lastUpdate = millis();

        int startPixel = seg->startIndex();
        int endPixel = seg->endIndex();
        int numPixels = endPixel - startPixel + 1;

        float mappedPosition = (accelX + 1.0f) * (float)(numPixels - BUBBLE_SIZE) / 2.0f;
        int bubbleCenter = constrain((int)mappedPosition, 0, numPixels - BUBBLE_SIZE) + startPixel;

        RgbColor bubbleColor((seg->baseColor >> 16) & 0xFF, (seg->baseColor >> 8) & 0xFF, seg->baseColor & 0xFF);
        seg->allOff();

        for (int i = 0; i < BUBBLE_SIZE; i++)
        {
            seg->getParent().getStrip().SetPixelColor(bubbleCenter + i, bubbleColor);
        }
    }

} // namespace AccelMeter

#endif // ACCELMETER_H
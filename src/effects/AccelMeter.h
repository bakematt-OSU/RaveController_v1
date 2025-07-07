#ifndef ACCELMETER_H
#define ACCELMETER_H

#include "../PixelStrip.h"
#include <Arduino.h>

extern float accelX, accelY, accelZ;

namespace AccelMeter {
    const int BUBBLE_SIZE = 5;

    // Initialize the accelerometer meter effect
    inline void start(PixelStrip::Segment* seg, uint32_t color1, uint32_t /*color2*/) {
        seg->setEffect(PixelStrip::Segment::SegmentEffect::AccelMeter);
        seg->active    = true;
        seg->interval  = 10;
        seg->baseColor = color1;
    }

    // Update one frame of the accelerometer meter
    inline void update(PixelStrip::Segment* seg) {
        if (!seg->active || (millis() - seg->lastUpdate < seg->interval)) return;
        seg->lastUpdate = millis();

        int startPixel = seg->startIndex();
        int numPixels  = seg->endIndex() - startPixel + 1;

        // Map accelX from [-1,1] to [0, numPixels - BUBBLE_SIZE]
        float mapped = (accelX + 1.0f) * (numPixels - BUBBLE_SIZE) / 2.0f;
        int center = constrain((int)mapped, 0, numPixels - BUBBLE_SIZE) + startPixel;

        // Build the bubble color
        RgbColor bubbleColor(
            (seg->baseColor >> 16) & 0xFF,
            (seg->baseColor >> 8)  & 0xFF,
            seg->baseColor         & 0xFF
        );
        bubbleColor.Dim(seg->getBrightness());

        // Clear and draw the bubble
        seg->allOff();
        for (int i = 0; i < BUBBLE_SIZE; ++i) {
            seg->getParent().getStrip().SetPixelColor(center + i, bubbleColor);
        }
    }
}

#endif // ACCELMETER_H

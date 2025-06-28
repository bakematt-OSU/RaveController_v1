#ifndef KINETICRIPPLE_H
#define KINETICRIPPLE_H

#include "../PixelStrip.h"
#include <Arduino.h>

extern volatile bool triggerRipple;

namespace KineticRipple {

bool rippleActive = false;
unsigned long rippleStartTime = 0;
RgbColor rippleColor;
// REMOVED: The hardcoded rippleWidth and rippleSpeed variables are now in PixelStrip.h

inline void start(PixelStrip::Segment* seg, uint32_t color1, uint32_t color2) {
    seg->setEffect(PixelStrip::Segment::SegmentEffect::KINETIC_RIPPLE);
    seg->active = true;
    seg->interval = 5;
    seg->baseColor = color1;
    rippleActive = false;
}

inline void update(PixelStrip::Segment* seg) {
    if (!seg->active) return;
    
    if (triggerRipple && !rippleActive) {
        rippleActive = true;
        rippleStartTime = millis();
        rippleColor = RgbColor((seg->baseColor >> 16) & 0xFF, (seg->baseColor >> 8) & 0xFF, seg->baseColor & 0xFF);
        triggerRipple = false;
    }

    seg->allOff();

    if (rippleActive) {
        float elapsed = millis() - rippleStartTime;
        // MODIFIED: Read speed from the segment's properties
        int radius = (int)(elapsed * seg->rippleSpeed);

        int startPixel = seg->startIndex();
        int endPixel = seg->endIndex();
        int centerPixel = startPixel + (endPixel - startPixel) / 2;
        int halfLength = (endPixel - startPixel) / 2;

        if (halfLength == 0) halfLength = 1;
        int brightness = 255 - (radius * 255 / halfLength);
        brightness = constrain(brightness, 0, 255);
        RgbColor fadedColor = rippleColor;
        fadedColor.Dim(brightness);

        int pixel1_center = centerPixel + radius;
        int pixel2_center = centerPixel - radius;
        // MODIFIED: Read width from the segment's properties
        int halfWidth = seg->rippleWidth / 2;
        bool rippleDrawn = false;

        // Draw the first ripple bar (moving right)
        for (int i = 0; i < seg->rippleWidth; i++) {
            int currentPixel = pixel1_center - halfWidth + i;
            if (currentPixel >= startPixel && currentPixel <= endPixel) {
                seg->getParent().getStrip().SetPixelColor(currentPixel, fadedColor);
                rippleDrawn = true;
            }
        }

        // Draw the second ripple bar (moving left)
        if (pixel1_center != pixel2_center) {
            for (int i = 0; i < seg->rippleWidth; i++) {
                int currentPixel = pixel2_center - halfWidth + i;
                if (currentPixel >= startPixel && currentPixel <= endPixel) {
                    seg->getParent().getStrip().SetPixelColor(currentPixel, fadedColor);
                    rippleDrawn = true;
                }
            }
        }

        if (!rippleDrawn && elapsed > 100) {
            rippleActive = false;
        }
    }
}

} // namespace KineticRipple

#endif // KINETICRIPPLE_H
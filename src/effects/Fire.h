#ifndef FIRE_H
#define FIRE_H

#include "../PixelStrip.h"

namespace Fire {

// --- ADDED: Helper functions to replace missing library functions ---

// qadd8: Adds two bytes with saturation at 255.
inline byte qadd8(byte a, byte b) {
    unsigned int sum = a + b;
    if (sum > 255) {
        return 255;
    }
    return static_cast<byte>(sum);
}

// qsub8: Subtracts one byte from another with saturation at 0.
inline byte qsub8(byte a, byte b) {
    if (b > a) {
        return 0;
    }
    return a - b;
}


// --- Effect-specific constants you can tweak ---
const int MAX_LEDS = 300; // Must be at least as large as your LED_COUNT

// Internal state array for the heat of each pixel
inline static byte heat[MAX_LEDS];

// This helper function maps a heat temperature (0-255) to a fire-like color
inline RgbColor HeatColor(byte temperature) {
    byte t192 = round((temperature / 255.0) * 191);
    byte heatramp = t192 & 0x3F; // 0..63
    heatramp <<= 2; // scale to 0..252
    if( t192 > 0x80) { // 128
        return RgbColor(255, 255, heatramp);
    } else if( t192 > 0x40 ) { // 64
        return RgbColor(255, heatramp, 0);
    } else { // 0..63
        return RgbColor(heatramp, 0, 0);
    }
}

inline void start(PixelStrip::Segment* seg, uint32_t color1, uint32_t color2) {
    seg->setEffect(PixelStrip::Segment::SegmentEffect::FIRE);
    seg->active = true;
    seg->interval = (color1 > 0) ? color1 : 15; // Default to 15ms delay

    // If a value for Sparking was passed, use it. Otherwise, keep the default.
    if (color1 > 0 && color1 <= 255) {
        seg->fireSparking = color1;
    }
    // If a value for Cooling was passed, use it. Otherwise, keep the default.
    if (color2 > 0 && color2 <= 100) {
        seg->fireCooling = color2;
    }
}

inline void update(PixelStrip::Segment* seg) {
    if (!seg->active) return;

    if (millis() - seg->lastUpdate < seg->interval) return;
    seg->lastUpdate = millis();

    int start = seg->startIndex();
    int end = seg->endIndex();
    int len = (end - start + 1);

    // Step 1. Cool down every cell a little
    for (int i = start; i <= end; i++) {
      heat[i] = qsub8(heat[i], random(0, ((seg->fireCooling * 10) / len) + 2));
    }
  
    // Step 2. Heat from each cell drifts 'up' and diffuses a little
    for (int k = end; k >= start + 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    
    // Step 3. Randomly ignite new 'sparks' of heat at the bottom
    if (random(255) < seg->fireSparking) {
      int y = start + random(7);
      heat[y] = qadd8(heat[y], random(160, 255));
    }

    // Step 4. Map from heat cells to LED colors
    for (int j = start; j <= end; j++) {
      RgbColor color = HeatColor(heat[j]);
      seg->getParent().getStrip().SetPixelColor(j, color);
    }
}

}

#endif
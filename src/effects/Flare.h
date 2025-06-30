#ifndef FLARE_H
#define FLARE_H

#include "../PixelStrip.h"

namespace Flare {

// --- Effect-specific constants ---
const int MAX_LEDS = 300; 

// This effect uses the same static heat array as the Fire effect.
// This is okay, as only one can be active on a given segment at a time,
// but for larger projects, you might give each its own array.
static byte flare_heat[MAX_LEDS];

// --- Helper Functions ---
// (These are the same as the Fire effect)

inline RgbColor FlareHeatColor(byte temperature) {
    byte t192 = round((temperature / 255.0) * 191);
    byte heatramp = t192 & 0x3F; 
    heatramp <<= 2;
    if( t192 > 0x80) {
        return RgbColor(255, 255, heatramp);
    } else if( t192 > 0x40 ) {
        return RgbColor(255, heatramp, 0);
    } else {
        return RgbColor(heatramp, 0, 0);
    }
}

inline byte qadd8(byte a, byte b) {
    unsigned int sum = a + b;
    if (sum > 255) return 255;
    return static_cast<byte>(sum);
}
inline byte qsub8(byte a, byte b) {
    if (b > a) return 0;
    return a - b;
}

// --- Main Effect Functions ---

inline void start(PixelStrip::Segment* seg, uint32_t color1, uint32_t color2) {
    seg->setEffect(PixelStrip::Segment::SegmentEffect::FLARE);
    seg->active = true;
    seg->interval = 15; // Fixed speed

    // Set a very low baseline for sparking for an "embers" effect.
    // Use color1 if provided, otherwise default to a low value like 50.
    seg->fireSparking = (color1 > 0 && color1 <= 255) ? color1 : 50;
    // Use a high cooling value for shorter, smoldering flames.
    // Use color2 if provided, otherwise default to a high value like 80.
    seg->fireCooling = (color2 > 0 && color2 <= 100) ? color2 : 80;
}

inline void update(PixelStrip::Segment* seg) {
    if (!seg->active) return;

    if (millis() - seg->lastUpdate < seg->interval) return;
    seg->lastUpdate = millis();

    int start = seg->startIndex();
    int end = seg->endIndex();
    int len = (end - start + 1);

    // Step 1. Cool down every cell
    for (int i = start; i <= end; i++) {
      flare_heat[i] = qsub8(flare_heat[i], random(0, ((seg->fireCooling * 10) / len) + 2));
    }
  
    // Step 2. Heat drifts 'up'
    for (int k = end; k >= start + 2; k--) {
      flare_heat[k] = (flare_heat[k - 1] + flare_heat[k - 2] + flare_heat[k - 2]) / 3;
    }
    
    // --- THIS IS THE NEW LOGIC ---
    // Step 3. Determine sparking chance based on audio trigger
    byte currentSparkingChance = seg->fireSparking; // Start with the low baseline

    if (seg->triggerIsActive) {
        // If a beat is detected, dramatically increase the chance of sparks
        // based on the trigger's brightness (intensity).
        currentSparkingChance = map(seg->triggerBrightness, 0, 255, 150, 255);
    }

    if (random(255) < currentSparkingChance) {
      int y = start + random(7);
      flare_heat[y] = qadd8(flare_heat[y], random(160, 255));
    }

    // Step 4. Map from heat to LED colors
    for (int j = start; j <= end; j++) {
      RgbColor color = FlareHeatColor(flare_heat[j]);
      seg->getParent().getStrip().SetPixelColor(j, color);
    }
}

}

#endif
#pragma once

#include "Effect.h"
#include "PixelStrip.h"        // for PixelStrip::Segment
#include "effects/SolidColor.h" // for SolidColor::start()

class SolidColorEffect : public Effect {
public:
  // Factory used by your registry
  static Effect* factory() { 
    return new SolidColorEffect(); 
  }

  // Apply the effect to a segment
  void apply(Segment* seg) override {
    // The incoming Segment is actually a PixelStrip::Segment
    auto pSeg = static_cast<PixelStrip::Segment*>(seg);
    SolidColor::start(pSeg, color, 0);
  }
};

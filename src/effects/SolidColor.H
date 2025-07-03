// 9. SolidColor.h
#ifndef SOLIDCOLOR_H
#define SOLIDCOLOR_H
#include "../PixelStrip.h"
namespace SolidColor {
    inline void start(PixelStrip::Segment* seg,uint32_t c1,uint32_t c2){ seg->setEffect(PixelStrip::Segment::SegmentEffect::SOLID); seg->setBrightness(seg->getBrightness()); seg->active=true; seg->baseColor=c1; }
    inline void update(PixelStrip::Segment* seg){ if(!seg->active) return; uint32_t sc=PixelStrip::scaleColor(seg->baseColor,seg->getBrightness());
        for(uint16_t i=seg->startIndex();i<=seg->endIndex();++i) seg->getParent().setPixel(i,sc);
    }
}
#endif
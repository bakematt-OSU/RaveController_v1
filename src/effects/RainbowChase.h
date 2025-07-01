#ifndef RAINBOWCHASE_H
#define RAINBOWCHASE_H
#include "../PixelStrip.h"
namespace RainbowChase {
    inline void start(PixelStrip::Segment *seg,uint32_t c1,uint32_t c2){ seg->setEffect(PixelStrip::Segment::SegmentEffect::RAINBOW); seg->active=true; seg->interval=30; seg->lastUpdate=millis(); seg->rainbowFirstPixelHue=0; seg->setBrightness(seg->getBrightness()); }
    inline void update(PixelStrip::Segment *seg){ if(!seg->active||(millis()-seg->lastUpdate<seg->interval)) return; seg->lastUpdate=millis();
        for(int i=seg->startIndex();i<=seg->endIndex();++i){
            int hue=seg->rainbowFirstPixelHue+((i-seg->startIndex())*65536L/(seg->endIndex()-seg->startIndex()+1));
            uint32_t c=seg->getParent().ColorHSV(hue);
            uint32_t sc=PixelStrip::scaleColor(c,seg->getBrightness());
            seg->getParent().setPixel(i,sc);
        }
        seg->rainbowFirstPixelHue+=256;
    }
}
#endif

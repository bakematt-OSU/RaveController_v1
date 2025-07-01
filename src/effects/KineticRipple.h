// 6. KineticRipple.h
#ifndef KINETICRIPPLE_H
#define KINETICRIPPLE_H
#include "../PixelStrip.h"
#include <Arduino.h>
extern volatile bool triggerRipple;
namespace KineticRipple {
    inline bool rippleActive=false; inline unsigned long rippleStartTime=0; inline RgbColor rippleColor;
    inline void start(PixelStrip::Segment* seg,uint32_t c1,uint32_t c2){ seg->setEffect(PixelStrip::Segment::SegmentEffect::KINETIC_RIPPLE); seg->active=true; seg->interval=5; seg->baseColor=c1; rippleActive=false; }
    inline void update(PixelStrip::Segment* seg){ if(!seg->active) return;
        if(triggerRipple && !rippleActive){ rippleActive=true; rippleStartTime=millis(); rippleColor=RgbColor((seg->baseColor>>16)&0xFF,(seg->baseColor>>8)&0xFF,seg->baseColor&0xFF); triggerRipple=false; }
        seg->allOff(); if(rippleActive){ float elapsed=millis()-rippleStartTime;
            int radius=int(elapsed * seg->rippleSpeed);
            int s=seg->startIndex(), e=seg->endIndex(), center=s+(e-s)/2, halfLen=(e-s)/2;
            if(halfLen==0) halfLen=1;
            int b255=255 - (radius*255/halfLen); b255=constrain(b255,0,255);
            RgbColor fcol=rippleColor; fcol.Dim(b255); fcol.Dim(seg->getBrightness());
            int hw=seg->rippleWidth/2;
            bool drawn=false;
            for(int i=0;i<seg->rippleWidth;++i){ int p1=center-radius-hw+i; if(p1>=s&&p1<=e){ seg->getParent().getStrip().SetPixelColor(p1,fcol); drawn=true; }}
            int p2=center+radius-hw; if(p2!=center){ for(int i=0;i<seg->rippleWidth;++i){ int p=p2+i; if(p>=s&&p<=e){ seg->getParent().getStrip().SetPixelColor(p,fcol); drawn=true; } }}
            if(!drawn && elapsed>100) rippleActive=false;
        }
    }
}
#endif
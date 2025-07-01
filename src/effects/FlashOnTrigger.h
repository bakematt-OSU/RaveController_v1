// 5. FlashOnTrigger.h
#ifndef FLASH_ON_TRIGGER_H
#define FLASH_ON_TRIGGER_H
#include "../PixelStrip.h"
namespace FlashOnTrigger
{
    inline void start(PixelStrip::Segment *seg, uint32_t c1, uint32_t c2)
    {
        if (!seg)
            return;
        seg->setEffect(PixelStrip::Segment::SegmentEffect::FLASH_TRIGGER);
        seg->active = true;
        seg->baseColor = c1;
    }
    inline void update(PixelStrip::Segment *seg)
    {
        if (!seg->active)
            return;
        PixelStrip &strip = seg->getParent();
        if (seg->triggerIsActive)
        {
            uint32_t bc = seg->baseColor;
            RgbColor fc((bc >> 16) & 0xFF, (bc >> 8) & 0xFF, bc & 0xFF);
            fc.Dim(seg->triggerBrightness);
            fc.Dim(seg->getBrightness());
            for (uint16_t i = seg->startIndex(); i <= seg->endIndex(); ++i)
                strip.getStrip().SetPixelColor(i, fc);
        }
        else
            seg->allOff();
    }
}
#endif

#include "PixelStrip.h"
#include "effects/RainbowChase.h"
#include "effects/SolidColor.h"
#include "effects/FlashOnTrigger.h"
#include "effects/RainbowCycle.h"
#include "effects/TheaterChase.h"
#include "effects/Fire.h"
#include "effects/Flare.h"
#include "effects/ColoredFire.h"
#include "effects/AccelMeter.h"
#include "effects/KineticRipple.h"

//================================================================================
// PixelStrip Class Methods
//================================================================================

PixelStrip::PixelStrip(uint8_t pin, uint16_t ledCount, uint8_t brightness, uint8_t numSections)
    : strip(ledCount, pin)
{
    segments_.push_back(new Segment(*this, 0, ledCount - 1, String("all"), 0));
    segments_[0]->setBrightness(brightness);

    if (numSections > 0)
    {
        uint16_t per = ledCount / numSections;
        for (uint8_t s = 0; s < numSections; ++s)
        {
            uint16_t start = s * per;
            uint16_t end = (s == numSections - 1) ? (ledCount - 1) : (start + per - 1);
            segments_.push_back(new Segment(*this, start, end, String("seg") + String(s + 1), s + 1));
        }
    }
}

void PixelStrip::addSection(uint16_t start, uint16_t end, const String &name)
{
    uint8_t newId = segments_.size();
    segments_.push_back(new Segment(*this, start, end, name, newId));
}

void PixelStrip::begin() { strip.Begin(); }
void PixelStrip::show()
{
    if (strip.CanShow())
    {
        strip.Show();
    }
}
void PixelStrip::clear() { strip.ClearTo(RgbColor(0)); }

uint32_t PixelStrip::Color(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void PixelStrip::setPixel(uint16_t i, uint32_t col)
{
    RgbColor color((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF);
    color.Dim(activeBrightness_);
    strip.SetPixelColor(i, color);
}

void PixelStrip::clearPixel(uint16_t i)
{
    strip.SetPixelColor(i, RgbColor(0));
}

uint32_t PixelStrip::ColorHSV(uint16_t hue, uint8_t sat, uint8_t val)
{
    HsbColor hsb(hue / 65535.0f, sat / 255.0f, val / 255.0f);
    RgbColor rgb = hsb;
    return Color(rgb.R, rgb.G, rgb.B);
}

// --- REQUIRED: Implementations for missing functions ---
void PixelStrip::setActiveBrightness(uint8_t b)
{
    activeBrightness_ = b;
}
const std::vector<PixelStrip::Segment *> &PixelStrip::getSegments() const
{
    return segments_;
}
PixelBus &PixelStrip::getStrip()
{
    return strip;
}

//================================================================================
// PixelStrip::Segment Class Methods
//================================================================================

PixelStrip::Segment::Segment(PixelStrip &p, uint16_t s, uint16_t e, const String &n, uint8_t i)
    : parent(p), startIdx(s), endIdx(e), name(n), id(i), brightness(255) {}

// --- REQUIRED: Implementations for missing functions ---
uint16_t PixelStrip::Segment::startIndex() const { return startIdx; }
uint16_t PixelStrip::Segment::endIndex() const { return endIdx; }
String PixelStrip::Segment::getName() const { return name; }
uint8_t PixelStrip::Segment::getId() const { return id; }

void PixelStrip::Segment::begin() { clear(); }
// void PixelStrip::Segment::setBrightness(uint8_t b) { brightness = b; }
// uint8_t PixelStrip::Segment::getBrightness() const { return brightness; }

void PixelStrip::Segment::allOff()
{
    for (uint16_t i = startIndex(); i <= endIndex(); ++i)
    {
        parent.getStrip().SetPixelColor(i, RgbColor(0));
    }
}

void PixelStrip::Segment::setEffect(SegmentEffect effect)
{
    active = false;
    clear();
    activeEffect = effect;
}

void PixelStrip::Segment::setTriggerState(bool isActive, uint8_t brightness)
{
    triggerIsActive = isActive;
    triggerBrightness = brightness;
}

void PixelStrip::Segment::startEffect(SegmentEffect effect, uint32_t color1, uint32_t color2)
{
    setEffect(effect);
    switch (effect)
    {
#define EFFECT_START_CASE(name, className)      \
    case SegmentEffect::name:                   \
        className::start(this, color1, color2); \
        break;
        EFFECT_LIST(EFFECT_START_CASE)
#undef EFFECT_START_CASE
    case SegmentEffect::NONE:
    default:
        clear();
        break;
    }
}

void PixelStrip::Segment::update()
{
    parent.setActiveBrightness(brightness);

    switch (activeEffect)
    {
#define EFFECT_UPDATE_CASE(name, className) \
    case SegmentEffect::name:               \
        className::update(this);            \
        break;
        EFFECT_LIST(EFFECT_UPDATE_CASE)
#undef EFFECT_UPDATE_CASE

    case SegmentEffect::NONE:
    default:
        break;
    }
}

// In PixelStrip.cpp
void PixelStrip::clearUserSegments() {
    // We start at index 1 to preserve the default "all" segment at index 0
    if (segments_.size() <= 1) return;

    for (size_t i = 1; i < segments_.size(); ++i) {
        delete segments_[i]; // Free the memory for each segment
    }
    segments_.resize(1); // Shrink the vector back to only contain the "all" segment
}

void PixelStrip::propagateTriggerState(bool isActive, uint8_t brightness)
{
    // Loop through all segments
    for (auto* s : segments_) {
        // If a segment is running a trigger effect, update its state.
        // This can be expanded with || for other future trigger effects.
        if (s->activeEffect == Segment::SegmentEffect::FLASH_TRIGGER) {
            s->setTriggerState(isActive, brightness);
        }
    }
}

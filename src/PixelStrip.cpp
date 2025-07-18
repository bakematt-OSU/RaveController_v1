#include "PixelStrip.h"

// Destructor to clean up segments
PixelStrip::~PixelStrip()
{
    for (auto* s : segments_)
    {
        delete s;
    }
    segments_.clear();
}


//================================================================================
// PixelStrip Class Methods
//================================================================================

PixelStrip::PixelStrip(uint8_t pin, uint16_t ledCount, uint8_t brightness, uint8_t numSections)
    : strip(ledCount, pin), ledCount_(ledCount) // Initialize ledCount_
{
    segments_.push_back(new Segment(*this, 0, ledCount - 1, "all", 0));
    segments_[0]->setBrightness(brightness);

    if (numSections > 0)
    {
        uint16_t per = ledCount / numSections;
        for (uint8_t s = 0; s < numSections; ++s)
        {
            uint16_t start = s * per;
            uint16_t end = (s == numSections - 1) ? (ledCount - 1) : (start + per - 1);
            addSection(start, end, "seg" + String(s + 1));
        }
    }
}
uint16_t PixelStrip::getLedCount() const
{
    return ledCount_;
}

void PixelStrip::addSection(uint16_t start, uint16_t end, const String &name)
{
    uint8_t newId = segments_.size();
    segments_.push_back(new Segment(*this, start, end, name, newId));
}

void PixelStrip::clearUserSegments()
{
    for (size_t i = 1; i < segments_.size(); ++i)
    {
        delete segments_[i];
    }
    segments_.resize(1);
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

uint32_t PixelStrip::ColorHSV(uint16_t hue, uint8_t sat, uint8_t val)
{
    HsbColor hsb(hue / 65535.0f, sat / 255.0f, val / 255.0f);
    RgbColor rgb = hsb;
    return Color(rgb.R, rgb.G, rgb.B);
}

uint32_t PixelStrip::scaleColor(uint32_t color, uint8_t brightness)
{
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    r = (uint16_t(r) * brightness) / 255;
    g = (uint16_t(g) * brightness) / 255;
    b = (uint16_t(b) * brightness) / 255;
    return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

void PixelStrip::setPixel(uint16_t i, uint32_t col)
{
    RgbColor color((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF);
    strip.SetPixelColor(i, color);
}

void PixelStrip::clearPixel(uint16_t i)
{
    strip.SetPixelColor(i, RgbColor(0));
}

const std::vector<PixelStrip::Segment *> &PixelStrip::getSegments() const
{
    return segments_;
}

PixelBus &PixelStrip::getStrip()
{
    return strip;
}

void PixelStrip::propagateTriggerState(bool isActive, uint8_t brightness)
{
    for (auto *s : segments_)
    {
        s->triggerIsActive = isActive;
        s->triggerBrightness = brightness;
    }
}

//================================================================================
// PixelStrip::Segment Class Methods
//================================================================================

// MODIFIED: Constructor now uses strncpy for safe copying and no longer initializes `name` in the member initializer list.
PixelStrip::Segment::Segment(PixelStrip &p, uint16_t s, uint16_t e, const String &n, uint8_t i)
    : parent(p), startIdx(s), endIdx(e), id(i)
{
    // Safely copy the incoming name into the fixed-size char array
    strncpy(name, n.c_str(), sizeof(name) - 1);
    // Ensure the array is always null-terminated, even if the source string was too long
    name[sizeof(name) - 1] = '\0';
}

PixelStrip::Segment::~Segment()
{
    if (activeEffect)
    {
        delete activeEffect;
    }
}

void PixelStrip::Segment::update()
{
    if (activeEffect)
    {
        activeEffect->update();
    }
    else
    {
        allOff();
    }
}

void PixelStrip::Segment::allOff()
{
    for (uint16_t i = startIdx; i <= endIdx; ++i)
    {
        parent.getStrip().SetPixelColor(i, RgbColor(0));
    }
}

void PixelStrip::Segment::setRange(uint16_t newStart, uint16_t newEnd)
{
    if (newEnd >= newStart)
    {
        startIdx = newStart;
        endIdx = newEnd;
    }
}

void PixelStrip::Segment::setColor(uint8_t r, uint8_t g, uint8_t b)
{
    baseColor = parent.Color(r, g, b);
}

uint16_t PixelStrip::Segment::startIndex() const { return startIdx; }
uint16_t PixelStrip::Segment::endIndex() const { return endIdx; }

// MODIFIED: This function now returns a const char* to match the change in the header file.
const char* PixelStrip::Segment::getName() const { return name; }

uint8_t PixelStrip::Segment::getId() const { return id; }
PixelStrip &PixelStrip::Segment::getParent() { return parent; }
void PixelStrip::Segment::setBrightness(uint8_t b) { brightness = b; }
uint8_t PixelStrip::Segment::getBrightness() const { return brightness; }

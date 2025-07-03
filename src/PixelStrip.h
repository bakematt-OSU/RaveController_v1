#ifndef PIXELSTRIP_H
#define PIXELSTRIP_H

#include <Arduino.h>
#include <NeoPixelBus.h>
#include <vector>
#include "effects/Effects.h"

using PixelBus = NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>;

class PixelStrip
{
public:
    void clearUserSegments();
    void propagateTriggerState(bool isActive, uint8_t brightness);

    class Segment
    {
    public:
        uint8_t brightness = 255; // 0–255, default full
        void setBrightness(uint8_t b) { brightness = b; }
        uint8_t getBrightness() const { return brightness; }

        enum class SegmentEffect
        {
            NONE,
#define EFFECT_ENUM_ENTRY(name, className) name,
            EFFECT_LIST(EFFECT_ENUM_ENTRY)
#undef EFFECT_ENUM_ENTRY
                EFFECT_COUNT
        };

        Segment(PixelStrip &parent, uint16_t startIdx, uint16_t endIdx, const String &name, uint8_t id);

        uint16_t startIndex() const;
        uint16_t endIndex() const;
        String getName() const;
        uint8_t getId() const;
        SegmentEffect activeEffect = SegmentEffect::NONE;

        void begin();
        void update();
        void allOff();
        inline void clear() { allOff(); }

        void setEffect(SegmentEffect effect);
        void startEffect(SegmentEffect effect, uint32_t color1 = 0, uint32_t color2 = 0);

        void setTriggerState(bool isActive, uint8_t brightness);

        // void setBrightness(uint8_t b);
        // uint8_t getBrightness() const;

        PixelStrip &getParent() { return parent; }

        // --- UNIFIED STATE VARIABLES ---
        bool active = false;
        uint32_t baseColor = 0;
        unsigned long lastUpdate = 0;
        unsigned long interval = 0;

        // State for Trigger-based effects
        bool triggerIsActive = false;
        uint8_t triggerBrightness = 0;

        // --- State unique to specific effects ---
        unsigned long rainbowFirstPixelHue = 0;
        uint8_t chaseOffset = 0;

        // Configurable parameters for the Fire effect
        uint8_t fireSparking = 120;
        uint8_t fireCooling = 55;

        // Configurable parameters for the colored fire effect
        uint32_t fireColor1 = 0x000000;
        uint32_t fireColor2 = 0xFF0000;
        uint32_t fireColor3 = 0xFFFF00;

        // --- ADDED: Configurable parameters for the Kinetic Ripple effect ---
        int rippleWidth = 3;      // The width of the ripple in pixels
        float rippleSpeed = 0.2f; // The speed/fade duration of the ripple

        /// Change this segment’s start/end indices
        void setRange(uint16_t newStart, uint16_t newEnd)
        {
            if (newEnd < newStart)
                return;          // guard
            startIdx = newStart; // private members of this class
            endIdx = newEnd;
        }

    private:
        PixelStrip &parent;
        uint16_t startIdx, endIdx;
        String name;
        uint8_t id;
        // uint8_t brightness;
    };

    PixelStrip(uint8_t pin, uint16_t ledCount, uint8_t brightness = 50, uint8_t numSections = 0);
    void begin();
    void show();
    void clear();
    uint32_t Color(uint8_t r, uint8_t g, uint8_t b);
    uint32_t ColorHSV(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);
    static uint32_t scaleColor(uint32_t color, uint8_t brightness)
    {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        r = (uint16_t(r) * brightness) / 255;
        g = (uint16_t(g) * brightness) / 255;
        b = (uint16_t(b) * brightness) / 255;
        return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
    }

    void setPixel(uint16_t idx, uint32_t color);
    void clearPixel(uint16_t idx);
    void setActiveBrightness(uint8_t b);
    const std::vector<Segment *> &getSegments() const;
    PixelBus &getStrip();
    void addSection(uint16_t start, uint16_t end, const String &name);

private:
    PixelBus strip;
    std::vector<Segment *> segments_;
    uint8_t activeBrightness_ = 128;
};

#endif // PIXELSTRIP_H
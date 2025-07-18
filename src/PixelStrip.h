#ifndef PIXELSTRIP_H
#define PIXELSTRIP_H

#include <Arduino.h>
#include <NeoPixelBus.h>
#include <vector>
#include "effects/BaseEffect.h" // Use the BaseEffect abstract class

using PixelBus = NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>;

class PixelStrip
{
public:
    class Segment; // Forward declaration
    uint16_t getLedCount() const;

    PixelStrip(uint8_t pin, uint16_t ledCount, uint8_t brightness = 50, uint8_t numSections = 0);
    void begin();
    void show();
    void clear();
    void addSection(uint16_t start, uint16_t end, const String &name);
    void clearUserSegments();
    void propagateTriggerState(bool isActive, uint8_t brightness);

    uint32_t Color(uint8_t r, uint8_t g, uint8_t b);
    uint32_t ColorHSV(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);
    static uint32_t scaleColor(uint32_t color, uint8_t brightness);

    void setPixel(uint16_t idx, uint32_t color);
    void clearPixel(uint16_t idx);
    
    const std::vector<Segment *> &getSegments() const;
    PixelBus &getStrip();

    class Segment
    {
    public:
        Segment(PixelStrip &parent, uint16_t startIdx, uint16_t endIdx, const String &name, uint8_t id);
        ~Segment();

        // --- Core Methods ---
        void update();
        void allOff();
        void setRange(uint16_t newStart, uint16_t newEnd);

        // --- Getters & Setters ---
        uint16_t startIndex() const;
        uint16_t endIndex() const;
        const char* getName() const; // MODIFIED: Return type is now const char*
        uint8_t getId() const;
        PixelStrip &getParent();
        void setBrightness(uint8_t b);
        uint8_t getBrightness() const;
        void setColor(uint8_t r, uint8_t g, uint8_t b);

        // --- State Variables ---
        BaseEffect* activeEffect = nullptr; // Pointer to the current effect object
        uint32_t baseColor = 0;
        
        // State for Trigger-based effects, checked by the effects themselves
        bool triggerIsActive = false;
        uint8_t triggerBrightness = 0;

    private:
        PixelStrip &parent;
        uint16_t startIdx, endIdx;
        char name[32]; // MODIFIED: Changed from String to fixed-size char array
        uint8_t id;
        uint8_t brightness = 255;
    };

private:
    PixelBus strip;
    std::vector<Segment *> segments_;
    uint16_t ledCount_; // Store the count internally

};

#endif // PIXELSTRIP_H
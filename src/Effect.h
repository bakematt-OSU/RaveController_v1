#pragma once

// Include ArduinoJson to let effects (de)serialize their own parameters
#include <ArduinoJson.h>

// Forward‐declare our Segment type
class Segment;

/**
 * Base class for *all* LED effects.
 * Each subclass must implement:
 *  • effectName()      → unique string key  
 *  • serialize()       → write its params to JSON  
 *  • deserialize()     → read params from JSON  
 *  • apply()           → actually start the effect on a Segment
 * And finally we provide a static create() to instantiate by name.
 */
struct Effect {
    // Virtual destructor so subclass cleanup runs properly
    virtual ~Effect() {}

    // — returns the unique name used for JSON and CLI (“rainbow”, “solid”, etc.)
    virtual const char* effectName() const = 0;

    // — called when saving: writes this effect’s parameters into obj
    virtual void serialize(JsonObject &obj) const = 0;

    // — called when loading: reads parameters back from obj
    virtual void deserialize(const JsonObject &obj) = 0;

    // — invoked to actually start or apply this effect on the given segment
    virtual void apply(Segment *seg) = 0;

    /**
     * Factory: given an effectName(), returns a new instance of the right subclass,
     * or nullptr if name is unrecognized. 
     */
    static Effect* create(const char *name);
};

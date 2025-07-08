#pragma once
#include "EffectParameter.h"

// Abstract base class for all effects
class BaseEffect {
public:
    virtual ~BaseEffect() {}

    // Must implement effect logic
    virtual void update() = 0;

    // Must provide the effect's name
    virtual const char* getName() const = 0;

    // Parameter discovery interface
    virtual int getParameterCount() const = 0;
    virtual EffectParameter* getParameter(int idx) = 0;

    // Convenience: set by name (overload for type)
    virtual void setParameter(const char* name, float value) {
        for (int i = 0; i < getParameterCount(); ++i) {
            EffectParameter* p = getParameter(i);
            if (strcmp(p->name, name) == 0 && p->type == ParamType::FLOAT) {
                p->value.floatValue = value;
                return;
            }
        }
    }
    virtual void setParameter(const char* name, int value) {
        for (int i = 0; i < getParameterCount(); ++i) {
            EffectParameter* p = getParameter(i);
            if (strcmp(p->name, name) == 0 && p->type == ParamType::INTEGER) {
                p->value.intValue = value;
                return;
            }
        }
    }
    virtual void setParameter(const char* name, bool value) {
        for (int i = 0; i < getParameterCount(); ++i) {
            EffectParameter* p = getParameter(i);
            if (strcmp(p->name, name) == 0 && p->type == ParamType::BOOLEAN) {
                p->value.boolValue = value;
                return;
            }
        }
    }
    virtual void setParameter(const char* name, uint32_t value) {
        for (int i = 0; i < getParameterCount(); ++i) {
            EffectParameter* p = getParameter(i);
            if (strcmp(p->name, name) == 0 && p->type == ParamType::COLOR) {
                p->value.colorValue = value;
                return;
            }
        }
    }
};

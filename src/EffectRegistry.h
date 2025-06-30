// EffectRegistry.h
#pragma once

#include <Arduino.h>    // for String
#include <map>
#include "Effect.h"     // declares `class Effect` and `using FactoryFn = Effect* (*)();`

// Factory function alias: create one instance of an Effect
using FactoryFn = Effect* (*)();

/**
 *  Returns the global map of effect‐names → factory functions.
 *  You can look up by name and call:
 *     auto e = registry()[name]();
 */
std::map<String,FactoryFn>& registry();

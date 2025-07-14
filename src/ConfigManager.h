#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

// Declares the functions that can be used by any part of the program
// to manage the device's configuration state.

void setLedCount(uint16_t newSize);
bool saveConfig();
String loadConfig();
void handleBatchConfigJson(const String& json);


#endif // CONFIG_MANAGER_H
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

// Declares the functions that can be used by any part of the program
// to manage the device's configuration state.

void setLedCount(uint16_t newSize);
bool saveConfig();

// Corrected Declaration: Takes a buffer and returns the size.
size_t loadConfig(char* buffer, size_t bufferSize);

// Corrected Declaration: Takes a C-style string.
void handleBatchConfigJson(const char* json);


#endif // CONFIG_MANAGER_H
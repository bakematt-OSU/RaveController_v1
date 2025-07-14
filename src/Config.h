// config.h
// Centralized configuration constants and types

#pragma once
#include <stdint.h>
#include <LittleFS_Mbed_RP2040.h>

// —— Pin Definitions ——
constexpr uint8_t LEDR_PIN = 25;
constexpr uint8_t LEDG_PIN = 26;
constexpr uint8_t LEDB_PIN = 27;
constexpr uint8_t LED_PIN  = 4;

// —— LED Strip Configuration ——
extern uint16_t LED_COUNT;
constexpr uint8_t  BRIGHTNESS    = 10;
constexpr uint8_t  SEGMENT_COUNT = 0;

// —— Accelerometer & Step Detection ——
constexpr float        STEP_THRESHOLD      = 2.5f;
constexpr unsigned long STEP_COOLDOWN_MS    = 300;

// —— Audio Input ——
constexpr int SAMPLES         = 256;
constexpr int SAMPLING_FREQ   = 16000;

// NOTE: The definition for STATE_FILE has been removed from here.
// It is now defined in main.cpp and declared extern in globals.h

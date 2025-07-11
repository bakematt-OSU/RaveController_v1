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
// constexpr uint16_t LED_COUNT     = 585; // This is now a variable
extern uint16_t LED_COUNT;               // Declare it as an external variable
constexpr uint8_t  BRIGHTNESS    = 10;
constexpr uint8_t  SEGMENT_COUNT = 0;

// —— Accelerometer & Step Detection ——
constexpr float        STEP_THRESHOLD      = 2.5f;
constexpr unsigned long STEP_COOLDOWN_MS    = 300;

// —— Audio Input ——
constexpr int SAMPLES         = 256;
constexpr int SAMPLING_FREQ   = 16000;

// —— Heartbeat Effect ——
enum class HeartbeatColor { RED, GREEN, BLUE };
constexpr unsigned long HB_INTERVAL_MS = 2000;


// where we store the overall state
static constexpr char STATE_FILE[] = "/littlefs/state.json";


/// --- Heartbeat Effect State Variables ---
enum HeartbeatColorState
{
    HEARTBEAT_RED,
    HEARTBEAT_GREEN,
    HEARTBEAT_BLUE
};

// --- THIS IS THE FIX ---
// Declare the variables here, but don't define them
extern HeartbeatColorState heartbeatColorState;
extern unsigned long lastHeartbeatColorChange;

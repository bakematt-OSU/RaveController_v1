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
constexpr uint16_t LED_COUNT     = 300;
constexpr uint8_t  BRIGHTNESS    = 25;
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
static constexpr char STATE_FILE[] = "/state.json";

// call this once in setup(), before loadConfig()
inline void initStateFS() {
  static LittleFS_MBED myFS;
  if (!myFS.init()) {
    Serial.println("⚠️ LittleFS mount failed");
  }
}

// // load JSON from flash into a document; returns false if missing/bad
// inline bool loadConfig(JsonDocument &doc) {
//   FILE *f = fopen(STATE_FILE, "r");
//   if (!f) return false;
//   DeserializationError err = deserializeJson(doc, *f);
//   fclose(f);
//   return !err;
// }

// // write the given JSON doc back to flash
// inline bool saveConfig(const JsonDocument &doc) {
//   FILE *f = fopen(STATE_FILE, "w");
//   if (!f) return false;
//   serializeJson(doc, *f);
//   fclose(f);
//   return true;
// }


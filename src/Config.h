// config.h
// Centralized configuration constants and types

#pragma once
#include <stdint.h>
#include <LittleFS_Mbed_RP2040.h>
#include <ArduinoJson.h>

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

// load JSON from flash into a document; returns false if missing/bad
inline bool loadConfig(JsonDocument &doc) {
  FILE *f = fopen(STATE_FILE, "r");
  if (!f) return false;
  DeserializationError err = deserializeJson(doc, *f);
  fclose(f);
  return !err;
}

// write the given JSON doc back to flash
inline bool saveConfig(const JsonDocument &doc) {
  FILE *f = fopen(STATE_FILE, "w");
  if (!f) return false;
  serializeJson(doc, *f);
  fclose(f);
  return true;
}

// walk the JSON “segments” array and re-create them in your PixelStrip
// (you’ll need to adapt calls to match your API)
inline void applyConfig(const JsonDocument &doc) {
  if (!doc.containsKey("segments")) return;
  for (JsonObject seg : doc["segments"].as<JsonArray>()) {
    uint8_t  id    = seg["id"];
    uint16_t start = seg["start"];
    uint16_t end   = seg["end"];
    const char *name = seg["name"];
    const char *effect = seg["effect"];
    // 1) re-create segment in your strip manager:
    auto *s = pixelStrip.addSegment(id, start, end, String(name));
    // 2) restore effect with parameters:
    if (strcmp(effect, "rainbow")==0) {
      uint16_t speed = seg["speed"] | 50;
      RainbowChase::start(s, speed);
    }
    else if (strcmp(effect, "solid")==0) {
      auto arr = seg["color"].as<JsonArray>();
      RgbColor c(arr[0], arr[1], arr[2]);
      SolidColor::start(s, c);
    }
    // …add your other effects here…
  }
}
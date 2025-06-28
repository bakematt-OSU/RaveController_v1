#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ─── LED STRIP SETTINGS ────────────────────────────────────────────────────────
// Data pin connected to NeoPixel DIN
#define DATA_PIN            4

// Total number of LEDs in your strip
#define LED_COUNT           300

// Default global brightness (0–255)
#define DEFAULT_BRIGHTNESS  175

// How many segments you’d like to divide the strip into
#define NUM_SEGMENTS        3

// ─── WI-FI SETTINGS ────────────────────────────────────────────────────────────
// Replace with your network SSID/password
static const char* WIFI_SSID     = "RAVECONTROLLER";
static const char* WIFI_PASSWORD = "ravecontroller";

// Port for the built-in command server (HTTP or simple TCP)
#define WIFI_SERVER_PORT    80

// ─── BLUETOOTH SETTINGS ──────────────────────────────────────────────────────
// Device name shown to clients
#define BLE_DEVICE_NAME         "RP2040-LED"

// UUIDs for your BLE command service & characteristic
#define BLE_SERVICE_UUID        "12345678-1234-5678-1234-56789ABCDEF0"
#define BLE_CHAR_CMD_UUID       "ABCDEFAB-1234-5678-1234-56789ABCDEF0"

// ─── SERIAL SETTINGS ──────────────────────────────────────────────────────────
// Serial baud rate for debugging & direct commands
#define SERIAL_BAUD_RATE    115200

#endif // CONFIG_H

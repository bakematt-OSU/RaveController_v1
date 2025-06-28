#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ─── LIBRARIES ──────────────────────────────────────────────────────────────────
#include <WiFiNINA.h>   // Wi-Fi using the NINA module
#include <ArduinoBLE.h> // Bluetooth Low Energy

// ─── SERIAL SETTINGS ──────────────────────────────────────────────────────────
// Baud rate for Serial debug & commands
#define SERIAL_BAUD_RATE    115200

// ─── LED STRIP SETTINGS ────────────────────────────────────────────────────────
// Data pin connected to NeoPixel DIN
#define DATA_PIN            4
// Total number of LEDs in your strip
#define LED_COUNT           300
// Default global brightness (0–255)
#define DEFAULT_BRIGHTNESS  175
// Number of logical segments on the strip
#define NUM_SEGMENTS        3

// ─── WI-FI (STATION) SETTINGS ─────────────────────────────────────────────────
// Replace with your network SSID/password
static const char* __attribute__((unused)) WIFI_SSID     = "YOUR_SSID";
static const char* __attribute__((unused)) WIFI_PASSWORD = "YOUR_PASSWORD";
// TCP port for incoming text‐command connections
#define WIFI_SERVER_PORT    80

// ─── WI-FI (ACCESS POINT) SETTINGS ─────────────────────────────────────────────
// If you’d rather host your own AP instead of joining an existing network:
// SSID & password for your hotspot
static const char* AP_SSID = "RAVECONTROLLER";
static const char* AP_PASS = "ravecontroller";

// ─── BLUETOOTH SETTINGS ────────────────────────────────────────────────────────
// BLE device name shown to scanners
#define BLE_DEVICE_NAME         "RP2040-LED"
// UUID for your BLE command service & characteristic
#define BLE_SERVICE_UUID        "12345678-1234-5678-1234-56789ABCDEF0"
#define BLE_CHAR_CMD_UUID       "ABCDEFAB-1234-5678-1234-56789ABCDEF0"

#endif // CONFIG_H

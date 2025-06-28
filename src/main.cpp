#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include "Config.h"
#include "PixelStrip.h"
#include "EffectsManager.h"
#include "InputManager.h"
#include "Triggers.h"

// ─── YOUR ACCESS-POINT CREDENTIALS ───────────────────────────────────────────────
static const char* AP_SSID = "RAVECONTROLLER";
static const char* AP_PASS = "ravecontroller";

// ─── TCP SERVER (over the AP) ─────────────────────────────────────────────────────
WiFiServer server(WIFI_SERVER_PORT);

// ─── CORE OBJECTS ─────────────────────────────────────────────────────────────────
PixelStrip    strip(DATA_PIN, LED_COUNT, DEFAULT_BRIGHTNESS, NUM_SEGMENTS);
EffectsManager effectsManager(strip);
InputManager   inputManager;

// ─── BLE COMMAND SERVICE & CHARACTERISTIC ────────────────────────────────────────
BLEService       cmdService(BLE_SERVICE_UUID);
BLECharacteristic cmdChar( BLE_CHAR_CMD_UUID,
                           BLEWrite,   // clients can write text into this
                           256          // up to 256 bytes/command
);

void setup() {
  // —— Serial for debug & direct commands
  Serial.begin(SERIAL_BAUD_RATE);
  while (!Serial);

  // —— Start Wi-Fi Access Point
  Serial.print("Starting AP \""); Serial.print(AP_SSID); Serial.print("\" …");
  int status = WiFi.beginAP(AP_SSID, AP_PASS);
  if (status != WL_AP_LISTENING) {
    Serial.println(" failed!");
  } else {
    Serial.println(" OK");
    IPAddress ip = WiFi.softAPIP();
    Serial.print("AP IP: "); Serial.println(ip);
    server.begin();
  }

  // —— Start BLE
  if (BLE.begin()) {
    Serial.println("BLE initialized");
    BLE.setLocalName(BLE_DEVICE_NAME);
    BLE.setAdvertisedService(cmdService);
    cmdService.addCharacteristic(cmdChar);
    BLE.addService(cmdService);
    cmdChar.setWriteProperty(true);
    BLE.advertise();
    Serial.println("BLE advertising");
  } else {
    Serial.println("BLE init failed");
  }

  // —— Initialize your strip & effects
  strip.begin();
  effectsManager.registerDefaultEffects();
  effectsManager.begin();
  effectsManager.startDefaultEffect();

  // —— Wire up text commands → effects
  inputManager.setCommandCallback([&](const String &cmd){
    effectsManager.handleCommand(cmd);
  });

  // —— Initialize your existing Triggers hooks
  Triggers::begin();
}

void loop() {
  // —— Poll Serial
  inputManager.loop();

  // —— Poll Wi-Fi TCP clients
  WiFiClient client = server.available();
  if (client && client.connected()) {
    String cmd = client.readStringUntil('\n');
    cmd.trim();
    if (cmd.length()) inputManager.receive(cmd);
  }

  // —— Poll BLE writes
  BLEDevice central = BLE.central();
  if (central && central.connected() && cmdChar.written()) {
    String cmd = cmdChar.value();
    inputManager.receive(cmd);
  }

  // —— Run your sensor-based triggers
  Triggers::update();

  // —— Step all active effects & push pixels
  effectsManager.updateAll();
  strip.show();
}

#include <Arduino.h>
#include <PDM.h>               // for on-board PDM mic
#include <WiFiNINA.h>          // Wi-Fi
#include <ArduinoBLE.h>        // BLE
#include "Config.h"
#include "PixelStrip.h"
#include "EffectsManager.h"
#include "InputManager.h"
#include "Triggers.h"

// ─── FFT / AUDIO SETTINGS ───────────────────────────────────────────────────────
#define SAMPLES              64
#define SAMPLING_FREQUENCY   16000

// Audio buffer (filled in interrupt)
volatile int16_t sampleBuffer[SAMPLES];

// Create your trigger instance
AudioTrigger<SAMPLES> audioTrigger;

// ─── TCP SERVER (over the AP) ─────────────────────────────────────────────────────
WiFiServer server(WIFI_SERVER_PORT);

// ─── CORE OBJECTS ─────────────────────────────────────────────────────────────────
PixelStrip     strip(DATA_PIN, LED_COUNT, DEFAULT_BRIGHTNESS, NUM_SEGMENTS);
EffectsManager effectsManager(strip);
InputManager   inputManager;

// ─── BLE COMMAND SERVICE & CHARACTERISTIC ────────────────────────────────────────
BLEService       cmdService(BLE_SERVICE_UUID);
BLECharacteristic cmdChar( BLE_CHAR_CMD_UUID, BLEWrite, 256 );


// ─── PDM DATA CALLBACK ────────────────────────────────────────────────────────────
// Cast away 'volatile' to satisfy PDM.read(void*, int)
void onPDMData() {
    int avail = PDM.available();
    if (avail > SAMPLES) avail = SAMPLES;
    PDM.read((void*)sampleBuffer, avail);  // :contentReference[oaicite:0]{index=0}
    for(int i = avail; i < SAMPLES; ++i) sampleBuffer[i] = 0;
}


// ─── AUDIO TRIGGER CALLBACK ──────────────────────────────────────────────────────
void audioTriggerCallback(bool isActive, uint8_t brightness) {
    if (isActive) {
        // Use the correct PixelStrip API (not setBrightness)
        strip.setActiveBrightness(brightness);  // :contentReference[oaicite:1]{index=1}
    }
}


void setup() {
    // —— Serial for debug & direct commands
    Serial.begin(SERIAL_BAUD_RATE);
    while (!Serial);

    // —— Start Wi-Fi Access Point (use AP_PASS, not AP_PASSWORD)
    Serial.print("Starting AP \""); Serial.print(AP_SSID); Serial.print("\" …");
    if (WiFi.beginAP(AP_SSID, AP_PASS) != WL_AP_LISTENING) {
        Serial.println(" failed!");
    } else {
        Serial.println(" OK");
        // WiFi.localIP() works for both station & soft-AP modes
        IPAddress ip = WiFi.localIP();            // :contentReference[oaicite:2]{index=2}
        Serial.print("AP IP: "); Serial.println(ip);
        server.begin();
    }

    // —— Start BLE (no setWriteProperty() in ArduinoBLE)
    if (BLE.begin()) {
        Serial.println("BLE initialized");
        BLE.setLocalName(BLE_DEVICE_NAME);
        BLE.setAdvertisedService(cmdService);
        cmdService.addCharacteristic(cmdChar);
        BLE.addService(cmdService);
        BLE.advertise();
        Serial.println("BLE advertising");
    } else {
        Serial.println("BLE init failed");
    }

    // —— Initialize strip & effects
    strip.begin();
    effectsManager.registerDefaultEffects();
    effectsManager.begin();
    effectsManager.startDefaultEffect();

    // —— Route text commands → EffectsManager
    inputManager.setCommandCallback([&](const String &cmd){
        effectsManager.handleCommand(cmd);
    });

    // —— Hook up audio trigger
    audioTrigger.onTrigger(audioTriggerCallback);

    // —— PDM mic input
    PDM.onReceive(onPDMData);
    if (!PDM.begin(1, SAMPLING_FREQUENCY)) {
        Serial.println("PDM begin() failed");
    }
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

    // —— Poll BLE writes (cast the raw bytes to a C-string)
    BLEDevice central = BLE.central();
    if (central && central.connected() && cmdChar.written()) {
        // cmdChar.value() returns const uint8_t*; cast to char* for String
        String cmd = String((char*)cmdChar.value());
        inputManager.receive(cmd);
    }

    // —— Feed samples into your AudioTrigger
    audioTrigger.update(sampleBuffer);

    // —— Run effects & push pixels
    effectsManager.updateAll();
    strip.show();
}

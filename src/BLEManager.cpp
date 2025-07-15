/**
 * @file BLEManager.cpp
 * @brief Implementation of the simplified BLE manager.
 *
 * @version 2.0
 * @date 2025-07-15
 */
#include "BLEManager.h"

// --- UUIDs for the BLE Service and Characteristics ---
// These MUST match the UUIDs in the Android app's BluetoothService.kt
const char* SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214";
// TX is for transmitting FROM the Arduino TO the App (Notifications)
const char* TX_CHAR_UUID = "19B10001-E8F2-537E-4F6C-D104768A1214";
// RX is for receiving data FROM the App (Writes)
const char* RX_CHAR_UUID = "19B10002-E8F2-537E-4F6C-D104768A1214";

// --- Static C-Style Callback Functions ---
// The BLE library requires simple C-style function pointers for callbacks.
// These static functions will simply call the public methods on our singleton instance,
// bridging the gap between the C-style API and our C++ class.
static void staticOnWrite(BLEDevice central, BLECharacteristic characteristic) {
    BLEManager::getInstance().handleWrite(central, characteristic);
}
static void staticOnConnect(BLEDevice central) {
    BLEManager::getInstance().handleConnect(central);
}
static void staticOnDisconnect(BLEDevice central) {
    BLEManager::getInstance().handleDisconnect(central);
}


// --- Constructor ---
BLEManager::BLEManager() :
    bleService(SERVICE_UUID),
    // TX characteristic is set to NOTIFY, so the Arduino can send data anytime.
    txCharacteristic(TX_CHAR_UUID, BLENotify, 512), // Use a large size for notifications
    // RX characteristic is set to WRITE, so the app can send commands.
    rxCharacteristic(RX_CHAR_UUID, BLEWrite, 512), // Allow large writes for batch configs
    commandHandlerCallback(nullptr)
{}

// --- Public Methods ---

void BLEManager::begin(const char* deviceName, CommandCallback callback) {
    if (!BLE.begin()) {
        Serial.println("FATAL: Starting BLE failed!");
        while (1); // Halt execution
    }

    // Set the device name and advertise the service
    BLE.setLocalName(deviceName);
    BLE.setAdvertisedService(bleService);

    // Add characteristics to the service
    bleService.addCharacteristic(txCharacteristic);
    bleService.addCharacteristic(rxCharacteristic);

    // Add the service to the BLE stack
    BLE.addService(bleService);

    // Assign our static functions as the event handlers
    rxCharacteristic.setEventHandler(BLEWritten, staticOnWrite);
    BLE.setEventHandler(BLEConnected, staticOnConnect);
    BLE.setEventHandler(BLEDisconnected, staticOnDisconnect);

    // Store the callback function that will handle incoming commands
    commandHandlerCallback = callback;

    // Start advertising
    BLE.advertise();
    Serial.print("BLE Manager initialized. Advertising as '");
    Serial.print(deviceName);
    Serial.println("'");
}

void BLEManager::update() {
    // This is the only function that needs to be called in the main loop()
    // It processes all incoming BLE events.
    BLE.poll();
}

void BLEManager::sendMessage(const String& message) {
    // This is a convenience wrapper to send a String.
    // It calls the byte array version.
    sendMessage((const uint8_t*)message.c_str(), message.length());
}

void BLEManager::sendMessage(const uint8_t* data, size_t len) {
    if (!isConnected()) return;

    // BLE can only send a limited number of bytes at a time (the MTU, max transmission unit).
    // This loop breaks the data into chunks and sends them sequentially.
    // The Android side will be responsible for reassembling them.
    size_t offset = 0;
    while (offset < len) {
        size_t chunkSize = min((size_t)20, len - offset); // Use a safe, standard chunk size of 20
        txCharacteristic.writeValue(data + offset, chunkSize);
        offset += chunkSize;
    }
}

bool BLEManager::isConnected() {
    // BLE.connected() returns true if a central device is connected.
    return BLE.connected();
}

// --- Event Handlers ---

void BLEManager::handleConnect(BLEDevice central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());
}

void BLEManager::handleDisconnect(BLEDevice central) {
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
    // After disconnecting, start advertising again to allow new connections.
    BLE.advertise();
    Serial.println("Advertising restarted.");
}

void BLEManager::handleWrite(BLEDevice central, BLECharacteristic characteristic) {
    // This function is called whenever the app writes data to our RX characteristic.
    Serial.print("Received ");
    Serial.print(characteristic.valueLength());
    Serial.println(" bytes.");

    // If a command handler callback is registered, call it with the received data.
    if (commandHandlerCallback) {
        commandHandlerCallback(characteristic.value(), characteristic.valueLength());
    }
}

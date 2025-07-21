/**
 * @file BLEManager.cpp
 * @brief Implementation of the simplified BLE manager.
 *
 * @version 2.0
 * @date 2025-07-15
 */
#include "BLEManager.h"
#include "BinaryCommandHandler.h" // ADDED: Include to access BleCommand enum

// --- UUIDs for the BLE Service and Characteristics ---
// These MUST match the UUIDs in the Android app's BluetoothService.kt
const char *SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214";
// TX is for transmitting FROM the Arduino TO the App (Notifications)
const char *TX_CHAR_UUID = "19B10001-E8F2-537E-4F6C-D104768A1214";
// RX is for receiving data FROM the App (Writes)
const char *RX_CHAR_UUID = "19B10002-E8F2-537E-4F6C-D104768A1214";

// --- Static C-Style Callback Functions ---
// The BLE library requires simple C-style function pointers for callbacks.
// These static functions will simply call the public methods on our singleton instance,
// bridging the gap between the C-style API and our C++ class.
static void staticOnWrite(BLEDevice central, BLECharacteristic characteristic)
{
    BLEManager::getInstance().handleWrite(central, characteristic);
}
static void staticOnConnect(BLEDevice central)
{
    BLEManager::getInstance().handleConnect(central);
}
static void staticOnDisconnect(BLEDevice central)
{
    BLEManager::getInstance().handleDisconnect(central);
}

// --- Constructor ---
BLEManager::BLEManager() : bleService(SERVICE_UUID),
                           // TX characteristic is set to NOTIFY, so the Arduino can send data anytime.
                           txCharacteristic(TX_CHAR_UUID, BLENotify, 512), // Use a large size for notifications
                           // RX characteristic is set to WRITE, so the app can send commands.
                           rxCharacteristic(RX_CHAR_UUID, BLEWrite, 512), // Allow large writes for batch configs
                           deviceName_(nullptr),
                           commandHandlerCallback(nullptr)
{
}

// --- Public Methods ---

void BLEManager::begin(const char *deviceName, CommandCallback callback)
{
    Serial.println("BLE: Initializing BLE Manager...");
    deviceName_ = deviceName;
    if (!BLE.begin())
    {
        Serial.println("FATAL: Starting BLE failed!");
        while (1)
            ; // Halt execution
    }
    Serial.println("BLE: BLE stack started successfully");

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
    Serial.print("' with service UUID: ");
    Serial.println(SERVICE_UUID);
    Serial.println("BLE: Ready for connections");
}

void BLEManager::update()
{
    // This is the only function that needs to be called in the main loop()
    // It processes all incoming BLE events.
    BLE.poll();
}

void BLEManager::reset()
{
    Serial.println("BLE: Resetting BLE stack...");
    BLE.stopAdvertise();
    BLE.end();
    delay(200);
    begin(deviceName_, commandHandlerCallback);
    Serial.println("BLE: Reset complete.");
}

void BLEManager::sendMessage(const String &message)
{
    // This is a convenience wrapper to send a String.
    // It calls the byte array version.
    Serial.print("BLE TX (String): '");
    Serial.print(message);
    Serial.print("' (");
    Serial.print(message.length());
    Serial.println(" bytes)");
    sendMessage((const uint8_t *)message.c_str(), message.length());
}

void BLEManager::sendMessage(const uint8_t *data, size_t len)
{
    if (!isConnected())
    {
        Serial.println("BLE TX Failed: Not connected");
        return;
    }

    Serial.print("BLE TX (Raw): ");
    Serial.print(len);
    Serial.print(" bytes - ");
    for (size_t i = 0; i < min(len, (size_t)32); i++)
    {
        if (data[i] >= 32 && data[i] <= 126)
        {
            Serial.print((char)data[i]);
        }
        else
        {
            Serial.print("[0x");
            Serial.print(data[i], HEX);
            Serial.print("]");
        }
    }
    if (len > 32)
        Serial.print("...");
    Serial.println();

    // BLE can only send a limited number of bytes at a time (the MTU, max transmission unit).
    // This loop breaks the data into chunks and sends them sequentially.
    // The Android side will be responsible for reassembling them.
    size_t offset = 0;
    int chunkCount = 0;
    // MODIFIED: Changed chunk size to 20 bytes as requested by the user.
    const size_t BLE_MAX_CHUNK_SIZE = 20; 
    while (offset < len)
    {
        size_t chunkSize = min(BLE_MAX_CHUNK_SIZE, len - offset); 
        Serial.print("  Chunk ");
        Serial.print(++chunkCount);
        Serial.print(": ");
        Serial.print(chunkSize);
        Serial.println(" bytes");
        txCharacteristic.writeValue(data + offset, chunkSize);
        offset += chunkSize;
    }
    Serial.print("BLE TX Complete: ");
    Serial.print(chunkCount);
    Serial.println(" chunks sent");
}

bool BLEManager::isConnected()
{
    // BLE.connected() returns true if a central device is connected.
    bool connected = BLE.connected();
    // Uncomment the line below for very verbose connection status debugging
    // Serial.print("BLE Connection Status: "); Serial.println(connected ? "CONNECTED" : "DISCONNECTED");
    return connected;
}

// --- Event Handlers ---

void BLEManager::handleConnect(BLEDevice central)
{
    Serial.print("BLE CONNECT: Device connected - ");
    Serial.print(central.address());
    Serial.print(" (Name: ");
    Serial.print(central.localName());
    Serial.println(")");
}

void BLEManager::handleDisconnect(BLEDevice central)
{
    Serial.print("BLE DISCONNECT: Device disconnected - ");
    Serial.print(central.address());
    Serial.print(" (Name: ");
    Serial.print(central.localName());
    Serial.println(")");
    // After disconnecting, start advertising again to allow new connections.
    BLE.advertise();
    Serial.println("BLE: Advertising restarted after disconnect");
}

void BLEManager::handleWrite(BLEDevice central, BLECharacteristic characteristic)
{
    // This function is called whenever the app writes data to our RX characteristic.
    Serial.print("BLE RX: Received ");
    Serial.print(characteristic.valueLength());
    Serial.print(" bytes from ");
    Serial.print(central.address());
    Serial.print(" - ");

    // Print the received data in a readable format
    const uint8_t *data = characteristic.value();
    size_t len = characteristic.valueLength();
    for (size_t i = 0; i < min(len, (size_t)32); i++)
    {
        if (data[i] >= 32 && data[i] <= 126)
        {
            Serial.print((char)data[i]);
        }
        else
        {
            Serial.print("[0x");
            Serial.print(data[i], HEX);
            Serial.print("]");
        }
    }
    if (len > 32)
        Serial.print("...");
    Serial.println();

    // If a command handler callback is registered, call it with the received data.
    if (commandHandlerCallback)
    {
        Serial.print("BLE RX: Command: ");
        // Check if data is long enough to safely cast to BleCommand
        if (len > 0) {
            Serial.print((BleCommand)data[0]); // Print the command type
        } else {
            Serial.print("Empty command");
        }
        Serial.println(" bytes)");
    
        commandHandlerCallback(characteristic.value(), characteristic.valueLength());
        Serial.println("BLE RX: Command handler completed");
    }
    else
    {
        Serial.println("BLE RX: No command handler registered!");
    }
}

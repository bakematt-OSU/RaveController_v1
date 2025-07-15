/**
 * @file BLEManager.h
 * @brief A simplified BLE manager for the Rave Controller.
 *
 * This version removes the custom packet protocol (SOH, EOT, sequence numbers)
 * in favor of a simpler, direct communication method. It relies on the inherent
 * reliability of BLE for small packet delivery and uses a simple ACK system
 * managed by the BinaryCommandHandler.
 *
 * @version 2.0
 * @date 2025-07-15
 */
#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <ArduinoBLE.h>

// Define a function pointer type for the command callback.
// It will be given the raw data and its length.
typedef void (*CommandCallback)(const uint8_t* data, size_t len);

class BLEManager {
public:
    /**
     * @brief Get the singleton instance of the BLEManager.
     * This ensures there's only one BLE manager active.
     * @return Reference to the singleton BLEManager instance.
     */
    static BLEManager& getInstance() {
        static BLEManager instance;
        return instance;
    }

    /**
     * @brief Initializes the BLE service, characteristics, and starts advertising.
     * @param deviceName The name the device will advertise.
     * @param callback The function to call when a command is received.
     */
    void begin(const char* deviceName, CommandCallback callback);

    /**
     * @brief Polls for BLE events. This should be called in the main loop.
     */
    void update();

    /**
     * @brief Sends a message to the connected central device (the app).
     * This function handles chunking the message if it's too large for a single BLE packet.
     * @param message The String message to send.
     */
    void sendMessage(const String& message);

    /**
     * @brief Sends a raw byte array to the connected central device.
     * @param data A pointer to the byte array.
     * @param len The length of the byte array.
     */
    void sendMessage(const uint8_t* data, size_t len);

    /**
     * @brief Checks if a central device is currently connected.
     * @return True if connected, false otherwise.
     */
    bool isConnected();

    // --- Public handlers for the static C-style callback functions ---
    // These are called by the BLE library and route events to our instance.
    void handleConnect(BLEDevice central);
    void handleDisconnect(BLEDevice central);
    void handleWrite(BLEDevice central, BLECharacteristic characteristic);

private:
    // --- Private Constructor for Singleton Pattern ---
    BLEManager();
    // Delete copy constructor and assignment operator to prevent copies
    BLEManager(const BLEManager&) = delete;
    void operator=(const BLEManager&) = delete;

    // --- Member Variables ---
    BLEService        bleService;
    BLECharacteristic txCharacteristic; // For sending data TO the app (Arduino -> App)
    BLECharacteristic rxCharacteristic; // For receiving data FROM the app (App -> Arduino)

    CommandCallback   commandHandlerCallback; // Function to call with incoming data
};

#endif // BLE_MANAGER_H

/**
 * @file BLEManager.h
 * @author Matthew Baker
 * @brief A simplified BLE manager for the Rave Controller.
 *
 * @details This version removes the custom packet protocol (SOH, EOT, sequence numbers)
 * in favor of a simpler, direct communication method. It relies on the inherent
 * reliability of BLE for small packet delivery and uses a simple ACK system
 * managed by the BinaryCommandHandler.
 *
 * This class is implemented as a singleton to ensure only one instance
 * manages the device's BLE hardware.
 *
 * @version 2.0
 * @date 2025-07-16
 * @copyright Copyright (c) 2025
 */
#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <ArduinoBLE.h>
#include "globals.h"

/**
 * @brief Defines a function pointer type for the command callback.
 * @param data A const pointer to the received raw byte data.
 * @param len The length of the data array.
 */
typedef void (*CommandCallback)(const uint8_t *data, size_t len);

/**
 * @class BLEManager
 * @brief Manages BLE communication using a singleton pattern.
 *
 * @details This class handles the initialization of BLE services, advertising,
 * connection management, and data transmission (TX/RX). It uses a callback
 * function to pass incoming data to the main application logic.
 */
class BLEManager
{
public:
    /**
     * @brief Get the singleton instance of the BLEManager.
     * @details This ensures there's only one BLE manager active.
     * @return Reference to the singleton BLEManager instance.
     */
    static BLEManager &getInstance()
    {
        static BLEManager instance;
        return instance;
    }

    /**
     * @brief Initializes the BLE service, characteristics, and starts advertising.
     * @param deviceName The name the device will advertise.
     * @param callback The function to call when a command is received.
     */
    void begin(const char *deviceName, CommandCallback callback);

    /**
     * @brief Polls for BLE events. This should be called in the main loop.
     */
    void update();

    /**
     * @brief Sends a String message to the connected central device (the app).
     * @details This function handles chunking the message if it's too large for a single BLE packet.
     * @param message The String message to send.
     */
    void sendMessage(const String &message);

    /**
     * @brief Sends a raw byte array to the connected central device.
     * @param data A pointer to the byte array.
     * @param len The length of the byte array.
     */
    void sendMessage(const uint8_t *data, size_t len);

    /**
     * @brief Checks if a central device is currently connected.
     * @return True if connected, false otherwise.
     */
    bool isConnected();

    // --- Public handlers for the static C-style callback functions ---
    // These are called by the BLE library and route events to our instance.

    /** @brief Public handler for connection events. */
    void handleConnect(BLEDevice central);
    /** @brief Public handler for disconnection events. */
    void handleDisconnect(BLEDevice central);
    /** @brief Public handler for characteristic write events. */
    void handleWrite(BLEDevice central, BLECharacteristic characteristic);

private:
    // --- Private Constructor for Singleton Pattern ---
    BLEManager();
    // Delete copy constructor and assignment operator to prevent copies
    BLEManager(const BLEManager &) = delete;
    void operator=(const BLEManager &) = delete;

    // --- Member Variables ---
    BLEService bleService;              ///< The main BLE service for the controller.
    BLECharacteristic txCharacteristic; ///< For sending data TO the app (Arduino -> App).
    BLECharacteristic rxCharacteristic; ///< For receiving data FROM the app (App -> Arduino).

    CommandCallback commandHandlerCallback; ///< Function pointer to the command handler.
};

#endif // BLE_MANAGER_H

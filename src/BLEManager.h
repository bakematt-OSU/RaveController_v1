#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <ArduinoBLE.h>
#include <queue>

typedef void (*CommandCallback)(const String& command);

class BLEManager {
public:
    // --- Singleton Accessor ---
    static BLEManager& getInstance() {
        static BLEManager instance;
        return instance;
    }

    void begin(const char* deviceName, CommandCallback callback);
    void update();
    void sendMessage(const String& message);
    bool isConnected();

    // --- FIX: Public handlers for the static callback functions ---
    void handleConnect(BLEDevice central);
    void handleDisconnect(BLEDevice central);
    void handleWrite(BLEDevice central, BLECharacteristic characteristic);

private:
    // Private constructor for Singleton pattern
    BLEManager();
    
    // Deleted copy constructor and assignment operator
    BLEManager(const BLEManager&) = delete;
    void operator=(const BLEManager&) = delete;

    // Protocol Logic
    void processSendQueue();
    void processReceivedData(const uint8_t* data, int len);
    void sendPacket(uint8_t seq, uint8_t ack, const uint8_t* payload, uint8_t len);
    void sendAck(uint8_t ackNum);

    // Member Variables
    BLEService        bleService;
    BLECharacteristic txCharacteristic;
    BLECharacteristic rxCharacteristic;
    CommandCallback   commandHandlerCallback;

    std::queue<String> sendQueue;
    String             currentMessageToSend;
    int                sendOffset;
    uint8_t            sendSequenceNumber;
    unsigned long      lastPacketSentTime;
    bool               waitingForAck;

    String             receiveBuffer;
    uint8_t            lastReceivedSequenceNumber;
    unsigned long      lastHeartbeatTime; // <-- FIX: Added missing member variable
};

#endif // BLE_MANAGER_H

#include "BLEManager.h"

// --- Protocol Constants ---
#define SOH 0x01
#define EOT 0x04
#define MAX_PAYLOAD_SIZE 15
#define PACKET_TIMEOUT 1000
#define HEARTBEAT_INTERVAL 2000

// --- UUIDs ---
const char* SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214";
const char* TX_CHAR_UUID = "19B10001-E8F2-537E-4F6C-D104768A1214";
const char* RX_CHAR_UUID = "19B10002-E8F2-537E-4F6C-D104768A1214";

// --- FIX: Static C-Style Callback Functions ---
// These simple functions just call the public methods on our singleton instance.
// This resolves the lambda-to-function-pointer conversion errors.
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
    txCharacteristic(TX_CHAR_UUID, BLENotify, 20),
    rxCharacteristic(RX_CHAR_UUID, BLEWrite, 20),
    commandHandlerCallback(nullptr),
    sendOffset(0),
    sendSequenceNumber(0),
    lastPacketSentTime(0),
    waitingForAck(false),
    lastReceivedSequenceNumber(255),
    lastHeartbeatTime(0)
{}

void BLEManager::begin(const char* deviceName, CommandCallback callback) {
    if (!BLE.begin()) {
        Serial.println("Starting BLE failed!");
        while (1);
    }

    BLE.setLocalName(deviceName);
    BLE.setAdvertisedService(bleService);
    bleService.addCharacteristic(txCharacteristic);
    bleService.addCharacteristic(rxCharacteristic);
    BLE.addService(bleService);

    // FIX: Assign the static functions as event handlers
    rxCharacteristic.setEventHandler(BLEWritten, staticOnWrite);
    BLE.setEventHandler(BLEConnected, staticOnConnect);
    BLE.setEventHandler(BLEDisconnected, staticOnDisconnect);

    commandHandlerCallback = callback;

    BLE.advertise();
    Serial.print("BLE Manager initialized. Advertising as '");
    Serial.print(deviceName);
    Serial.println("'");
}

// The rest of BLEManager.cpp remains the same as the previous correct version...
// (update, sendMessage, isConnected, handleConnect, handleDisconnect, handleWrite, etc.)
void BLEManager::update() {
    BLE.poll();
    if (isConnected()) {
        processSendQueue();
        if (millis() - lastHeartbeatTime > HEARTBEAT_INTERVAL) {
            sendAck(lastReceivedSequenceNumber);
            lastHeartbeatTime = millis();
        }
    }
}

void BLEManager::sendMessage(const String& message) {
    if(message.length() == 0) return;
    sendQueue.push(message);
}

bool BLEManager::isConnected() {
    return BLE.connected();
}

void BLEManager::handleConnect(BLEDevice central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());
    lastHeartbeatTime = millis();
}

void BLEManager::handleDisconnect(BLEDevice central) {
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
    waitingForAck = false;
    sendOffset = 0;
    while(!sendQueue.empty()) sendQueue.pop();
    currentMessageToSend = "";
    receiveBuffer = "";
    lastReceivedSequenceNumber = 255;
    BLE.advertise();
    Serial.println("Advertising restarted.");
}

void BLEManager::handleWrite(BLEDevice central, BLECharacteristic characteristic) {
    const uint8_t* data = characteristic.value();
    int len = characteristic.valueLength();
    processReceivedData(data, len);
}

void BLEManager::processSendQueue() {
    if (waitingForAck) {
        if (millis() - lastPacketSentTime > PACKET_TIMEOUT) {
            Serial.println("Packet ACK timeout. Retrying...");
            lastPacketSentTime = millis(); 
            int remaining = currentMessageToSend.length() - sendOffset;
            int payloadLen = min(remaining, MAX_PAYLOAD_SIZE);
            sendPacket(sendSequenceNumber, lastReceivedSequenceNumber, (const uint8_t*)currentMessageToSend.c_str() + sendOffset, payloadLen);
        }
        return;
    }

    if (currentMessageToSend.length() == 0 && !sendQueue.empty()) {
        currentMessageToSend = sendQueue.front();
        sendQueue.pop();
        sendOffset = 0;
        sendSequenceNumber = (sendSequenceNumber + 1) % 256;
    }

    if (currentMessageToSend.length() > 0) {
        int remaining = currentMessageToSend.length() - sendOffset;
        if (remaining > 0) {
            int payloadLen = min(remaining, MAX_PAYLOAD_SIZE);
            sendPacket(sendSequenceNumber, lastReceivedSequenceNumber, (const uint8_t*)currentMessageToSend.c_str() + sendOffset, payloadLen);
            waitingForAck = true;
        } else {
            currentMessageToSend = "";
            sendOffset = 0;
        }
    }
}

void BLEManager::processReceivedData(const uint8_t* data, int len) {
    if (len < 5 || data[0] != SOH || data[len - 1] != EOT) {
        Serial.println("Received malformed packet.");
        return;
    }

    uint8_t seq = data[1];
    uint8_t ack = data[2];
    uint8_t payloadLen = data[3];
    const uint8_t* payload = data + 4;

    if (ack == sendSequenceNumber) {
        waitingForAck = false;
        sendOffset += MAX_PAYLOAD_SIZE;
        Serial.print("ACK received for my packet #");
        Serial.println(ack);
    }

    if (seq != lastReceivedSequenceNumber && payloadLen > 0) {
        lastReceivedSequenceNumber = seq;
        receiveBuffer.concat((const char*)payload, payloadLen);
        
        if (payloadLen < MAX_PAYLOAD_SIZE) {
            if (commandHandlerCallback) {
                commandHandlerCallback(receiveBuffer);
            }
            receiveBuffer = "";
        }
    }
    
    sendAck(seq);
}

void BLEManager::sendPacket(uint8_t seq, uint8_t ack, const uint8_t* payload, uint8_t len) {
    if (!isConnected()) return;

    uint8_t packet[20];
    packet[0] = SOH;
    packet[1] = seq;
    packet[2] = ack;
    packet[3] = len;
    if (payload && len > 0) {
        memcpy(packet + 4, payload, len);
    }
    packet[4 + len] = EOT;

    txCharacteristic.writeValue(packet, 5 + len);
    lastPacketSentTime = millis();
    
    Serial.print("Sent Packet | SEQ: ");
    Serial.print(seq);
    Serial.print(" | ACK: ");
    Serial.print(ack);
    Serial.print(" | LEN: ");
    Serial.println(len);
}

void BLEManager::sendAck(uint8_t ackNum) {
    uint8_t newSeq = (sendSequenceNumber + 1) % 256;
    sendPacket(newSeq, ackNum, nullptr, 0);
}

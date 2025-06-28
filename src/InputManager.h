#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <Arduino.h>
#include <functional>

// Routes text commands from Serial, BLE, or Wi-Fi to your callback
class InputManager {
public:
    using CmdCallback = std::function<void(const String&)>;

    InputManager();

    // Set this once in setup() to point at EffectsManager::handleCommand
    void setCommandCallback(CmdCallback cb);

    // Call every loop() to poll Serial
    void loop();

    // Call from BluetoothManager or WiFiManager when a line arrives
    void receive(const String& cmd);

private:
    CmdCallback callback;
};

#endif // INPUTMANAGER_H

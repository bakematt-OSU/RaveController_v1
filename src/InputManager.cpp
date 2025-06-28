#include "InputManager.h"

InputManager::InputManager() : callback(nullptr) {}

void InputManager::setCommandCallback(CmdCallback cb) {
    callback = cb;
}

void InputManager::loop() {
    while (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() && callback) {
            callback(cmd);
        }
    }
}

void InputManager::receive(const String& cmd) {
    if (callback) callback(cmd);
}

#!/usr/bin/env python3
"""
Comprehensive test harness for RaveController_v1 serial commands.
This script has been updated to match the BaseEffect-based firmware.
"""
import argparse
import serial
import time
import json
import re
import sys
import os

# Command IDs matching Processes.h
CMD_SET_COLOR        = 0x01
CMD_SET_EFFECT       = 0x02
CMD_SET_BRIGHTNESS   = 0x03
CMD_SET_SEG_BRIGHT   = 0x04
CMD_SELECT_SEGMENT   = 0x05
CMD_CLEAR_SEGMENTS   = 0x06
CMD_SET_SEG_RANGE    = 0x07
CMD_GET_STATUS       = 0x08
CMD_BATCH_CONFIG     = 0x09
CMD_NUM_PIXELS       = 0x0A
CMD_GET_EFFECT_INFO  = 0x0B

BAUD  = 115200
DELAY = 0.2  # seconds between commands

# Updated ASCII commands to match the new firmware API
ASCII_COMMANDS = [
    "clearsegments",
    "addsegment 0 50",
    "addsegment 51 100",
    "listsegments",
    "setsegrange 1 60 90",
    "seteffect 1 SolidColor",
    "select 1", # Select segment 1 to set its color
    "setcolor 128 64 32",
    "select 0", # Switch back to segment 0
    "seteffect 0 Fire",
    "setsegbrightness 0 150",
    "getstatus",
    "geteffectinfo 0",
    # Corrected batchconfig JSON with proper keys
    "batchconfig {\"segments\":[{\"startLed\":0,\"endLed\":20,\"name\":\"segA\",\"brightness\":100,\"effect\":\"SolidColor\"}]}"
]

# Simplified binary commands to only those with explicit handlers in C++
BINARY_COMMANDS = [
    bytearray([CMD_CLEAR_SEGMENTS]),
    bytearray([CMD_GET_STATUS]),
]

# Effect names are now hardcoded as there's no command to list them all.
# This list is derived from the createEffectByName function in Processes.cpp.
AVAILABLE_EFFECTS = [
    "Fire", "SolidColor", "RainbowChase", "RainbowCycle", "AccelMeter",
    "Flare", "FlashOnTrigger", "KineticRipple", "TheaterChase", "ColoredFire"
]

def load_expected_pixels(config_path=None):
    """
    Locate src/Config.h and extract LED_COUNT.
    """
    if config_path is None:
        base_dir = os.path.dirname(os.path.abspath(__file__))
        # Corrected filename to Config.h
        config_path = os.path.join(base_dir, '..', 'src', 'Config.h')
    config_path = os.path.normpath(os.path.abspath(config_path))

    if not os.path.exists(config_path):
        raise FileNotFoundError(f"Config file not found at {config_path}")

    with open(config_path, 'r') as f:
        text = f.read()

    # Regex to find LED_COUNT, works for #define and constexpr
    match = re.search(r'(?:#define|constexpr\s+\w+\s+)\bLED_COUNT\s+[=]?\s*(\d+)', text)
    if not match:
        raise RuntimeError(f"Could not find LED_COUNT in {config_path}")
    return int(match.group(1))

EXPECTED_PIXELS = load_expected_pixels()

def read_all(ser):
    """Read all available data from the serial port with a timeout."""
    data = b""
    start_time = time.time()
    while time.time() - start_time < DELAY:
        if ser.in_waiting > 0:
            data += ser.read(ser.in_waiting)
        else:
            time.sleep(0.01) # Avoid busy-waiting
    
    try:
        return data.decode("utf-8", errors="ignore").strip()
    except UnicodeDecodeError:
        return str(data)

def send_ascii(ser, cmd):
    """Send an ASCII command and print the response, with validation."""
    print(f">>> ASCII: {cmd}")
    ser.write((cmd + "\n").encode())
    time.sleep(DELAY)
    resp = read_all(ser)
    print(resp or "<no response>")

    # --- JSON Validations ---
    if cmd == "getstatus":
        try:
            obj = json.loads(resp)
            assert "segments" in obj and isinstance(obj["segments"], list)
            print("  -> getstatus JSON OK")
        except (json.JSONDecodeError, AssertionError) as e:
            print(f"  !! getstatus JSON error: {e}")
            
    if cmd.startswith("geteffectinfo"):
        try:
            obj = json.loads(resp)
            assert "effect" in obj and "params" in obj
            assert isinstance(obj["params"], list)
            print("  -> geteffectinfo JSON OK")
        except (json.JSONDecodeError, AssertionError) as e:
            print(f"  !! geteffectinfo JSON error: {e}")

    return resp

def send_binary(ser, packet):
    """Send a binary command packet and print the response."""
    print(f">>> BINARY: {[hex(b) for b in packet]}")
    ser.write(packet)
    time.sleep(DELAY)
    resp = read_all(ser)
    print(resp or "<no response>")

    # --- JSON Validation for Binary GET_STATUS ---
    if packet[0] == CMD_GET_STATUS:
        try:
            obj = json.loads(resp)
            assert "segments" in obj
            print("  -> binary GET_STATUS JSON OK")
        except (json.JSONDecodeError, AssertionError) as e:
            print(f"  !! binary GET_STATUS JSON error: {e}")

    return resp

def interactive_effects_check(ser):
    """Set each available effect and ask the user to verify visually."""
    print("\n--- INTERACTIVE EFFECTS CHECK ---")
    print("This test will cycle through all known effects.")
    
    results = {}
    for effect_name in AVAILABLE_EFFECTS:
        # Set the effect on segment 0
        send_ascii(ser, f"seteffect 0 {effect_name}")
        ans = input(f"  [Effect: {effect_name}] Display OK? (y/n): ").strip().lower()
        results[effect_name] = (ans == "y")

    print("\n--- Effect Check Summary ---")
    for eff, passed in results.items():
        status = 'PASS' if passed else 'FAIL'
        print(f"  {eff:20s} {status}")

def run_tests(port, baud):
    """Main function to run all automated and interactive tests."""
    print(f"Opening serial port {port} @ {baud} baud")
    print(f"Found LED_COUNT = {EXPECTED_PIXELS} in Config.h")
    try:
        with serial.Serial(port, baud, timeout=DELAY) as ser:
            time.sleep(1)  # Wait for Arduino to reset
            ser.reset_input_buffer()

            print("\n--- ASCII COMMANDS ---")
            for cmd in ASCII_COMMANDS:
                send_ascii(ser, cmd)
                print("-" * 20)

            print("\n--- BINARY COMMANDS ---")
            for packet in BINARY_COMMANDS:
                send_binary(ser, packet)
                print("-" * 20)
                
            if input("\nRun interactive effects-check? (y/N): ").strip().lower() == "y":
                interactive_effects_check(ser)

            print("\nAll tests completed.")
            
    except serial.SerialException as e:
        print(f"FATAL: Error opening serial port {port}: {e}")
        sys.exit(1)
    except FileNotFoundError as e:
        print(f"FATAL: {e}")
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test harness for RaveController_v1 firmware.")
    parser.add_argument("--port", required=True, help="Serial port name (e.g., COM7 or /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=BAUD, help=f"Baud rate (default: {BAUD})")
    args = parser.parse_args()
    run_tests(args.port, args.baud)
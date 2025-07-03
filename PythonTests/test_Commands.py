#!/usr/bin/env python3
"""
Comprehensive test harness for RaveController_v1 serial commands
Sends ASCII and binary commands and validates basic responses, including
an interactive mode to verify each effect visually.
"""
import argparse
import serial
import time
import json
import re
import sys
import os

# Command IDs matching Processes.h
CMD_SET_COLOR      = 0x01
CMD_SET_EFFECT     = 0x02
CMD_SET_BRIGHTNESS = 0x03
CMD_SET_SEG_BRIGHT = 0x04
CMD_SELECT_SEGMENT = 0x05
CMD_CLEAR_SEGMENTS = 0x06
CMD_SET_SEG_RANGE  = 0x07
CMD_GET_STATUS     = 0x08
CMD_BATCH_CONFIG   = 0x09
CMD_NUM_PIXELS     = 0x0A  # newly added

BAUD = 115200
DELAY = 0.2  # seconds between commands

ASCII_COMMANDS = [
    "clearsegments",
    "addsegment 0 50",
    "addsegment 51 100",
    "listsegments",
    "setsegrange 1 60 90",
    "setsegeffect 1 SolidColor",
    "setsegname 1 middle",
    "setcolor 128 64 32",
    "listeffects",
    "listeffectsjson",
    "seteffect Fire",
    "setbrightness 200",
    "setsegbrightness 0 150",
    "getstatus",
    "numpixels",  # test the new command
    "batchconfig {\"segments\":[{\"start\":0,\"end\":20,\"name\":\"segA\",\"brightness\":100,\"effect\":\"SolidColor\"}],\"brightness\":180,\"color\":[10,20,30]}"
]

BINARY_COMMANDS = [
    bytearray([CMD_CLEAR_SEGMENTS]),
    bytearray([CMD_SELECT_SEGMENT, 1]),
    bytearray([CMD_SET_SEG_RANGE, 1, 0x00, 0x0A, 0x00, 0x1E]),
    bytearray([CMD_SET_SEG_BRIGHT, 1, 128]),
    bytearray([CMD_SET_COLOR, 255, 0, 128]),
    bytearray([CMD_SET_EFFECT, 3]),
    bytearray([CMD_SET_BRIGHTNESS, 100]),
    bytearray([CMD_GET_STATUS]),
    bytearray([CMD_NUM_PIXELS])
]

def load_expected_pixels(config_path=None):
    """
    Locate src/config.h (not Config.h) and extract LED_COUNT,
    whether it's defined via #define or static const.
    """
    # Compute default path if none given
    if config_path is None:
        base_dir = os.path.dirname(os.path.abspath(__file__))
        config_path = os.path.join(base_dir, '..', 'src', 'config.h')

    config_path = os.path.normpath(os.path.abspath(config_path))

    if not os.path.exists(config_path):
        raise FileNotFoundError(f"Config file not found at {config_path}")

    text = open(config_path, 'r').read()

    # Try #define style first
    m = re.search(r'#define\s+LED_COUNT\s+(\d+)', text)
    if not m:
        # Try static const style:
        m = re.search(r'\bLED_COUNT\s*=\s*(\d+)', text)
    if not m:
        raise RuntimeError(f"LED_COUNT not defined in {config_path}")

    return int(m.group(1))

EXPECTED_PIXELS = load_expected_pixels()

def read_all(ser):
    data = b""
    start = time.time()
    while time.time() - start < DELAY:
        chunk = ser.read(ser.in_waiting or 1)
        if not chunk:
            break
        data += chunk
    try:
        return data.decode("utf-8", errors="ignore").strip()
    except:
        return str(data)

def send_ascii(ser, cmd):
    ser.write((cmd + "\n").encode())
    print(f">>> ASCII: {cmd}")
    time.sleep(DELAY)
    resp = read_all(ser)
    print(resp or "<no response>")

    # JSON validations
    if cmd == "listeffectsjson":
        try:
            obj = json.loads(resp)
            assert "effects" in obj and isinstance(obj["effects"], list)
            print("  -> listeffectsjson OK")
        except Exception as e:
            print(f"  !! listeffectsjson JSON error: {e}")

    if cmd == "getstatus":
        try:
            obj = json.loads(resp)
            assert "effects" in obj and "segments" in obj
            print("  -> getstatus JSON OK")
        except Exception as e:
            print(f"  !! getstatus JSON error: {e}")

    if cmd == "numpixels":
        try:
            obj = json.loads(resp)
            assert obj.get("numpixels") == EXPECTED_PIXELS, \
                   f"expected {EXPECTED_PIXELS}, got {obj.get('numpixels')}"
            print(f"  -> numpixels OK ({obj['numpixels']})")
        except Exception as e:
            print(f"  !! numpixels JSON error: {e}")

    return resp

def send_binary(ser, packet):
    ser.write(packet)
    print(f">>> BINARY: {[hex(b) for b in packet]}")
    time.sleep(DELAY)
    resp = read_all(ser)
    print(resp or "<no response>")

    if packet[0] in (CMD_GET_STATUS, CMD_NUM_PIXELS):
        key = "segments" if packet[0] == CMD_GET_STATUS else "numpixels"
        try:
            obj = json.loads(resp)
            assert key in obj
            if key == "numpixels":
                assert obj[key] == EXPECTED_PIXELS
            print(f"  -> binary {key} JSON OK")
        except Exception as e:
            print(f"  !! binary {key} JSON error: {e}")

    return resp

def interactive_effects_check(ser):
    """Fetch list of effects, set each one, and ask user to verify."""
    print("\n--- INTERACTIVE EFFECTS CHECK ---")
    # get the list
    raw = send_ascii(ser, "listeffectsjson")
    try:
        effects = json.loads(raw)["effects"]
    except Exception:
        print("Failed to fetch effect list; skipping interactive check.")
        return

    results = {}
    for effect in effects:
        send_ascii(ser, f"seteffect {effect}")
        ans = input(f"[Effect: {effect}] Did this display correctly? (y/n): ").strip().lower()
        results[effect] = (ans == "y")

    # summary
    print("\nEffect Check Summary:")
    for eff, passed in results.items():
        status = "PASS" if passed else "FAIL"
        print(f"  {eff:20s} {status}")

def run_tests(port, baud):
    print(f"Opening serial port {port} @ {baud} baud")
    try:
        with serial.Serial(port, baud, timeout=DELAY) as ser:
            time.sleep(1)
            ser.reset_input_buffer()

            # Ask user if they want to do the interactive effects check
            choice = input("Do you want to run the interactive effects-check? (y/N): ").strip().lower()
            if choice == "y":
                interactive_effects_check(ser)

            print("\n--- ASCII COMMANDS ---")
            for cmd in ASCII_COMMANDS:
                send_ascii(ser, cmd)

            print("\n--- BINARY COMMANDS ---")
            for packet in BINARY_COMMANDS:
                send_binary(ser, packet)

            print("\nAll tests completed.")

    except serial.SerialException as e:
        print(f"Error opening port {port}: {e}")
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test RaveController commands")
    parser.add_argument("--port", required=True, help="Serial port (e.g. COM7 or /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=BAUD, help="Baud rate")
    args = parser.parse_args()
    run_tests(args.port, args.baud)

#!/usr/bin/env python3
"""
A comprehensive and interactive test harness for the RaveController device.

This script validates all major functionality, including the full suite of
ASCII (serial) and binary (hex) commands, configuration persistence, and
dynamic parameter testing for all lighting effects.
"""

import argparse
import json
import os
import serial
import sys
import time
from typing import Any, Dict, Optional

# --- Constants ---
BAUD_RATE = 115200
SERIAL_TIMEOUT = 0.5
RESTART_DELAY = 6
DEFAULT_LED_COUNT = 45
DEVICE_READY_MSG = "Setup complete. Entering main loop..."

# --- Binary Command Definitions ---
CMD_SET_COLOR = 0x01
CMD_SET_EFFECT = 0x02
CMD_SET_BRIGHTNESS = 0x03
CMD_SET_SEG_BRIGHT = 0x04
CMD_SELECT_SEGMENT = 0x05
CMD_CLEAR_SEGMENTS = 0x06
CMD_SET_SEG_RANGE = 0x07
CMD_GET_STATUS = 0x08
CMD_BATCH_CONFIG = 0x09
# CMD_NUM_PIXELS = 0x0A  # This command is no longer implemented in firmware
CMD_GET_EFFECT_INFO = 0x0B
CMD_ACK = 0xA0
CMD_SET_LED_COUNT = 0x0C
CMD_GET_LED_COUNT = 0x0D


class TestError(Exception):
    """Custom exception for test failures."""

    pass


# --- Helper & Communication Functions ---


def parse_json_from_response(response: str) -> Dict[str, Any]:
    """Finds and parses a JSON object from a serial response string."""
    start_brace = response.find("{")
    start_bracket = response.find("[")

    if start_brace == -1 and start_bracket == -1:
        raise TestError(f"No JSON object or array found in response: '{response}'")

    start_pos = min(p for p in [start_brace, start_bracket] if p != -1)

    try:
        return json.loads(response[start_pos:])
    except json.JSONDecodeError as e:
        raise TestError(
            f"Failed to parse JSON from response: '{response[start_pos:]}'. Error: {e}"
        )


def read_all(ser: serial.Serial) -> str:
    """Reads all available data from the serial port with a timeout."""
    data = b""
    start_time = time.time()
    while time.time() - start_time < SERIAL_TIMEOUT:
        if ser.in_waiting > 0:
            data += ser.read(ser.in_waiting)
        time.sleep(0.01)
    return data.decode("utf-8", errors="ignore").strip()


def send_command(
    ser: serial.Serial, command: str, quiet: bool = False, expect_response: bool = True
) -> str:
    """
    Sends a command to the device and returns the response.
    If expect_response is False, it returns immediately after sending.
    """
    prefix = "BINARY" if command.startswith("0x") else "ASCII"
    if not quiet:
        print(
            f">>> SEND ({prefix}): {command[:100]}{'...' if len(command) > 100 else ''}"
        )

    ser.write((command + "\n").encode())

    if not expect_response:
        return ""

    time.sleep(SERIAL_TIMEOUT)
    response = read_all(ser)

    if not quiet:
        print(f"<<< RECV: {response or '<no response>'}\n" + "-" * 20)

    if "ERR" in response:
        raise TestError(f"Command '{command}' returned an error: {response}")

    return response


# --- Test Utilities ---


def wait_for_device_ready(ser: serial.Serial, timeout: int = 15) -> None:
    """Waits for the device to signal it's ready after a restart."""
    print("      Waiting for device to be ready...")
    start_time = time.time()
    buffer = ""
    while time.time() - start_time < timeout:
        if ser.in_waiting > 0:
            new_data = ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
            buffer += new_data
            print(f"      Debug: Received: {repr(new_data)}")  # Debug output
            if DEVICE_READY_MSG in buffer:
                print("      Device is ready.")
                ser.reset_input_buffer()
                return
            # Alternative ready signals
            if "BLE Manager initialized" in buffer or "Advertising" in buffer:
                print("      Device appears to be ready (BLE initialized).")
                time.sleep(1)  # Give it a moment to fully initialize
                ser.reset_input_buffer()
                return
        time.sleep(0.1)

    print(
        f"      Warning: Timeout waiting for ready signal. Buffer contents: {repr(buffer)}"
    )
    # Try to proceed anyway - device might be ready but not sending expected message
    ser.reset_input_buffer()
    print("      Proceeding anyway - attempting basic communication test...")

    # Test if device is responsive
    try:
        test_response = send_command(ser, "getledcount", quiet=True)
        if "LED_COUNT:" in test_response:
            print("      Device appears to be responsive.")
            return
    except:
        pass

    raise TestError(
        "Timeout waiting for device ready signal and device not responsive to basic commands."
    )


def restart_and_reconnect(ser: serial.Serial) -> None:
    """Closes, waits, and reopens the serial port to reconnect after a device restart."""
    port = ser.port
    baudrate = ser.baudrate

    if ser.is_open:
        try:
            ser.close()
        except serial.SerialException as e:
            # This can happen if the device disappears abruptly, which is expected.
            print(f"      Ignoring error while closing stale port: {e}")

    print(
        f"      Device restarting. Waiting {RESTART_DELAY}s for serial port to reappear..."
    )
    time.sleep(RESTART_DELAY)

    # Try multiple times to reconnect
    max_retries = 3
    for attempt in range(max_retries):
        try:
            print(f"      Reconnection attempt {attempt + 1}/{max_retries}")
            ser.port = port
            ser.baudrate = baudrate
            ser.open()
            time.sleep(2.0)  # Longer delay after opening to let the port stabilize
            if ser.is_open:
                print("      Serial port reconnected.")
                wait_for_device_ready(ser)
                return
        except serial.SerialException as e:
            print(f"      Attempt {attempt + 1} failed: {e}")
            if attempt < max_retries - 1:
                print("      Waiting before retry...")
                time.sleep(2)
            else:
                raise TestError(
                    f"Failed to reconnect to the serial port after {max_retries} attempts. Final error: {e}"
                )


def run_test(test_function, *args):
    """A wrapper to run a test function and print its pass/fail status."""
    test_name = test_function.__name__.replace("_", " ").title()
    print(f"\n--- Testing: {test_name} ---")
    try:
        test_function(*args)
        print(f"✅ PASS: {test_name}")
        return True
    except (TestError, AssertionError) as e:
        print(f"❌ FAIL: {test_name}\n     Reason: {e}")
        return False
    except Exception as e:
        print(f"💥 ERROR: An unexpected error occurred in {test_name}: {e}")
        return False


# --- Test Suites ---


def test_all_serial_commands(ser: serial.Serial, json_config_path: str):
    """Provides full test coverage for the ASCII serial command interface."""
    # 1. Effect Discovery
    response = send_command(ser, "listeffects")
    effect_list = parse_json_from_response(response).get("effects", [])
    assert len(effect_list) > 5, "listeffects should return a list of effects."

    # 2. Segment Management
    send_command(ser, "clearsegments")
    response = parse_json_from_response(send_command(ser, "getstatus"))
    assert (
        len(response["segments"]) == 1
    ), "Should only have the 'all' segment after clearing."

    send_command(ser, "addsegment 10 20 seg_one")
    send_command(ser, "addsegment 21 30 seg_two")
    response = parse_json_from_response(send_command(ser, "getstatus"))
    assert len(response["segments"]) == 3, "Failed to add new segments."

    # 3. Effect and Parameter Management
    test_effect = "SolidColor"
    test_seg_id = 1
    send_command(ser, f"seteffect {test_seg_id} {test_effect}")
    status = parse_json_from_response(send_command(ser, "getstatus"))
    assert status["segments"][test_seg_id]["effect"] == test_effect, "seteffect failed."

    info = parse_json_from_response(send_command(ser, f"geteffectinfo {test_seg_id}"))
    assert info["effect"] == test_effect and "params" in info, "geteffectinfo failed."

    test_color = "0x112233"
    param_name = info["params"][0]["name"]
    send_command(ser, f"setparam {test_seg_id} {param_name} {test_color}")
    info_after = parse_json_from_response(
        send_command(ser, f"geteffectinfo {test_seg_id}")
    )
    assert info_after["params"][0]["value"] == int(test_color, 16), "setparam failed."

    # 4. Configuration Management
    send_command(ser, "saveconfig")
    config_str = send_command(ser, "getconfig")
    assert "segments" in parse_json_from_response(
        config_str
    ), "getconfig failed to return valid JSON."

    with open(json_config_path, "r") as f:
        compact_json = json.dumps(json.load(f), separators=(",", ":"))
    send_command(ser, f"batchconfig {compact_json}")
    status = parse_json_from_response(send_command(ser, "getstatus"))
    assert len(status["segments"]) >= 3, "batchconfig did not apply new segments."

    # 5. LED Count Management (ASCII)
    current_led_count = status.get("led_count")
    response = send_command(ser, "getledcount")
    assert (
        f"LED_COUNT: {current_led_count}" in response
    ), f"getledcount failed. Expected {current_led_count}."


def test_all_hex_commands(ser: serial.Serial):
    """Tests the full suite of hexadecimal commands."""
    # Setup: one main segment and one user segment
    send_command(ser, "clearsegments", quiet=True)
    send_command(ser, "addsegment 10 20 user_seg", quiet=True)
    status_before = parse_json_from_response(send_command(ser, "getstatus", quiet=True))
    current_led_count = status_before.get("led_count")

    # Test CMD_SET_BRIGHTNESS
    brightness = 150
    cmd_bytes = bytes([CMD_SET_BRIGHTNESS, brightness])
    response = send_command(ser, "0x" + cmd_bytes.hex())
    assert (
        f"OK: Brightness set to {brightness}" in response
    ), "CMD_SET_BRIGHTNESS failed."

    # Test CMD_SET_COLOR (on main segment)
    r, g, b = 255, 0, 255  # Magenta
    cmd_bytes = bytes([CMD_SET_COLOR, r, g, b])
    assert "OK: Color set" in send_command(
        ser, "0x" + cmd_bytes.hex()
    ), "CMD_SET_COLOR failed."

    # Test CMD_SET_EFFECT (Fire, ID=5, on main segment)
    effect_id_fire = 5
    cmd_bytes = bytes([CMD_SET_EFFECT, effect_id_fire])
    assert "OK: Effect set to Fire" in send_command(
        ser, "0x" + cmd_bytes.hex()
    ), "CMD_SET_EFFECT failed."

    # Test CMD_GET_EFFECT_INFO for the effect we just set
    cmd_bytes = bytes([CMD_GET_EFFECT_INFO, 0])  # Check segment 0
    response = send_command(ser, "0x" + cmd_bytes.hex())
    info = parse_json_from_response(response)
    assert (
        info.get("effect") == "Fire" and "params" in info
    ), "CMD_GET_EFFECT_INFO failed."

    # Test CMD_SET_SEG_BRIGHT
    seg_id, seg_brightness = 1, 200
    cmd_bytes = bytes([CMD_SET_SEG_BRIGHT, seg_id, seg_brightness])
    response = send_command(ser, "0x" + cmd_bytes.hex())
    assert (
        f"OK: Segment {seg_id} brightness set to {seg_brightness}" in response
    ), "CMD_SET_SEG_BRIGHT failed."

    # Test CMD_SELECT_SEGMENT (no-op, just check for OK)
    assert "OK: Segment selected" in send_command(
        ser, "0x" + bytes([CMD_SELECT_SEGMENT, 1]).hex()
    ), "CMD_SELECT_SEGMENT failed."

    # Test CMD_SET_SEG_RANGE
    seg_id, start, end = 1, 15, 25
    cmd_bytes = bytes(
        [
            CMD_SET_SEG_RANGE,
            seg_id,
            (start >> 8) & 0xFF,
            start & 0xFF,
            (end >> 8) & 0xFF,
            end & 0xFF,
        ]
    )
    response = send_command(ser, "0x" + cmd_bytes.hex())
    assert (
        f"OK: Segment {seg_id} range set to {start}-{end}" in response
    ), "CMD_SET_SEG_RANGE failed."

    # Test CMD_GET_STATUS (Note: Binary command sends JSON via BLE, confirm via Serial log)
    response = send_command(ser, "0x" + bytes([CMD_GET_STATUS]).hex())
    assert (
        "-> Sent Status JSON" in response
    ), "CMD_GET_STATUS did not send JSON via BLE."

    # Test CMD_CLEAR_SEGMENTS
    assert "OK: User segments cleared" in send_command(
        ser, "0x" + bytes([CMD_CLEAR_SEGMENTS]).hex()
    ), "CMD_CLEAR_SEGMENTS failed."
    # Verify segments were cleared using ASCII command (which returns JSON via Serial)
    status = parse_json_from_response(send_command(ser, "getstatus"))
    assert len(status["segments"]) == 1, "Segments were not cleared correctly."

    # Test CMD_BATCH_CONFIG (verifies acknowledgment, as function is not implemented in FW)
    assert "OK: Batch config (not implemented)" in send_command(
        ser, "0x" + bytes([CMD_BATCH_CONFIG]).hex()
    ), "CMD_BATCH_CONFIG failed."

    # Test CMD_GET_LED_COUNT (Note: This sends binary response via BLE, but we can verify via Serial log)
    response = send_command(ser, "0x" + bytes([CMD_GET_LED_COUNT]).hex())
    # The binary command sends data via BLE, but prints confirmation to Serial
    assert (
        f"-> Sent LED Count: {current_led_count}" in response
    ), f"CMD_GET_LED_COUNT failed. Expected confirmation of {current_led_count}."

    # Test CMD_ACK
    assert "OK: ACK" in send_command(
        ser, "0x" + bytes([CMD_ACK]).hex()
    ), "CMD_ACK failed."


def test_led_count_and_persistence(ser: serial.Serial):
    """Tests setting LED_COUNT, which triggers a restart and tests config persistence."""
    # 1. Set a unique configuration to test persistence
    send_command(ser, "clearsegments", quiet=True)
    send_command(ser, "seteffect 0 ColoredFire", quiet=True)
    send_command(ser, "saveconfig", quiet=True)

    # 2. Set new LED count via binary command, which forces a restart
    initial_count = parse_json_from_response(
        send_command(ser, "getstatus", quiet=True)
    ).get("led_count")
    new_count = 30
    cmd_bytes = bytes([CMD_SET_LED_COUNT, (new_count >> 8) & 0xFF, new_count & 0xFF])
    send_command(ser, "0x" + cmd_bytes.hex(), expect_response=False)
    restart_and_reconnect(ser)

    # 3. Verify the new LED count
    response = send_command(ser, "getledcount", quiet=True)
    verified_count = int(response.split(":")[1].strip())
    assert (
        verified_count == new_count
    ), f"Expected new count of {new_count}, but got {verified_count}."

    # 4. Verify that the configuration was restored after the restart
    status = parse_json_from_response(send_command(ser, "getstatus", quiet=True))
    assert (
        status["segments"][0]["effect"] == "ColoredFire"
    ), "Configuration did not persist across restart."

    # 5. Restore original count via ASCII command for safety and restart again
    send_command(ser, f"setledcount {initial_count}", expect_response=False)
    restart_and_reconnect(ser)
    response = send_command(ser, "getledcount", quiet=True)
    final_count = int(response.split(":")[1].strip())
    assert final_count == initial_count, "Could not restore LED count to default."


def test_all_parameters_for_all_effects(ser: serial.Serial, mode: str):
    """
    Discovers all effects, then discovers, adjusts, and verifies every parameter
    of each effect, either automatically or with manual confirmation.
    """
    print(f"      Mode: {mode.upper()}")
    # 1. Discover all effects
    response = send_command(ser, "listeffects")
    effects_to_test = parse_json_from_response(response)["effects"]
    print(f"      Found {len(effects_to_test)} effects to test.")

    # 2. Prepare a clean segment for testing
    send_command(ser, "clearsegments", quiet=True)
    current_led_count = parse_json_from_response(
        send_command(ser, "getstatus", quiet=True)
    ).get("led_count")
    send_command(ser, f"addsegment 0 {current_led_count - 1} test_segment", quiet=True)
    segment_id = 1

    for effect_name in effects_to_test:
        print(f"\n--- Testing Effect: {effect_name} ---")
        send_command(ser, f"seteffect {segment_id} {effect_name}", quiet=True)
        params_response = send_command(ser, f"geteffectinfo {segment_id}", quiet=True)
        params_list = parse_json_from_response(params_response).get("params", [])

        if not params_list:
            print("      No parameters to test for this effect.")
            continue

        for param in params_list:
            param_name = param["name"]
            param_type = param["type"]

            # Choose a test value based on type
            if param_type == "color":
                test_value = 0x123456
                cmd_value = f"0x{test_value:06X}"
            elif param_type == "boolean":
                test_value = True
                cmd_value = "true"
            else:  # integer or float
                test_value = param.get("max_val", 255)
                cmd_value = test_value

            print(f"      Adjusting param '{param_name}' to '{cmd_value}'...")
            send_command(
                ser, f"setparam {segment_id} {param_name} {cmd_value}", quiet=True
            )

            # Automatic verification
            info_after = parse_json_from_response(
                send_command(ser, f"geteffectinfo {segment_id}", quiet=True)
            )
            param_after = next(
                (p for p in info_after.get("params", []) if p["name"] == param_name),
                None,
            )

            assert (
                param_after is not None
            ), f"Parameter '{param_name}' not found after setting."
            assert (
                param_after["value"] == test_value
            ), f"Verification failed for '{param_name}'. Sent {cmd_value}, got {param_after['value']}."

            # Manual (visual) verification if in manual mode
            if mode == "manual":
                ans = (
                    input(
                        f"      --> VISUAL CHECK: Did '{param_name}' change correctly? (y/n): "
                    )
                    .strip()
                    .lower()
                )
                if ans != "y":
                    raise TestError(f"Visual confirmation failed for '{param_name}'.")


# --- Main Application ---


def main():
    """Parses arguments and runs the full test suite."""
    parser = argparse.ArgumentParser(
        description="Comprehensive test suite for RaveController."
    )
    parser.add_argument(
        "--port", required=True, help="Serial port (e.g., COM7 or /dev/ttyACM0)"
    )
    parser.add_argument(
        "--baud", type=int, default=BAUD_RATE, help=f"Baud rate (default: {BAUD_RATE})"
    )
    parser.add_argument(
        "--config",
        default="test_config.json",
        help="Path to the JSON config file for upload test.",
    )
    parser.add_argument(
        "--mode",
        choices=["automatic", "manual"],
        default="automatic",
        help="Set parameter testing mode: 'automatic' for silent verification, 'manual' for visual confirmation.",
    )
    parser.add_argument(
        "--skip-init",
        action="store_true",
        help="Skip initial device restart and LED count reset. Use if device is already running.",
    )
    args = parser.parse_args()

    ser = None
    try:
        print(f"Opening serial port {args.port} @ {args.baud} baud")
        # Directly open the serial port, handling potential immediate errors.
        ser = serial.Serial(args.port, args.baud, timeout=SERIAL_TIMEOUT)
        print(f"Serial port {args.port} opened successfully.")

        # Initialize device state
        print("\n--- Initializing Device State ---")
        if args.skip_init:
            print("Skipping device restart (--skip-init flag used)")
            # Just test if device is responsive
            try:
                response = send_command(ser, "getledcount", quiet=True)
                if "LED_COUNT:" in response:
                    print("Device appears to be ready and responsive.")
                else:
                    print("Warning: Device may not be fully ready.")
            except Exception as e:
                print(f"Warning: Device communication test failed: {e}")
        else:
            # Force LED count to default and restart device
            try:
                send_command(
                    ser, f"setledcount {DEFAULT_LED_COUNT}", expect_response=False
                )
                restart_and_reconnect(ser)
                print("Device state initialized.")
            except Exception as e:
                print(
                    f"Warning: Initial restart failed ({e}). Trying to proceed anyway..."
                )
                # Try basic communication test
                try:
                    response = send_command(ser, "getledcount", quiet=True)
                    if "LED_COUNT:" in response:
                        print("Device appears to be responsive despite restart issues.")
                    else:
                        raise TestError("Device not responding to basic commands.")
                except Exception as comm_e:
                    raise TestError(
                        f"Device initialization failed and not responsive: {comm_e}"
                    )

        results = {
            "Full Serial Command Suite": run_test(
                test_all_serial_commands, ser, args.config
            ),
            "Full Hex Command Suite": run_test(test_all_hex_commands, ser),
            "LED Count & Persistence": run_test(test_led_count_and_persistence, ser),
            "Effect Parameter Validation": run_test(
                test_all_parameters_for_all_effects, ser, args.mode
            ),
        }

        print("\n\n--- FINAL TEST SUMMARY ---")
        for name, passed in results.items():
            status = "✅ PASS" if passed else "❌ FAIL"
            print(f"  {name:28} {status}")

        if all(results.values()):
            print("\n🎉 All tests passed successfully! 🎉")
        else:
            print("\n❗️ Some tests failed. Please review the log. ❗️")
            sys.exit(1)

    except serial.SerialException as e:
        print(
            f"\n💥 FATAL: Could not open or interact with serial port {args.port}: {e}"
        )
        sys.exit(1)
    except (TestError, Exception) as e:
        print(f"\n💥 FATAL: An unexpected error occurred: {e}")
        sys.exit(1)
    finally:
        if ser and ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()

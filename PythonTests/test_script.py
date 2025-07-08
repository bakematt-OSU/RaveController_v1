#!/usr/bin/env python3
"""
The definitive, comprehensive, and interactive test harness for RaveController_v1.
This script validates all major functionality including:
1. Standard text-based serial commands.
2. LED count getting and setting, including device restarts.
3. Batch configuration upload from a JSON file.
4. Automated discovery and adjustment of every parameter for every effect.
5. Interactive visual confirmation for each parameter change.
"""
import argparse
import serial
import time
import json
import sys
import os

BAUD = 115200
DELAY = 0.35  # Increased delay slightly for visual confirmation
DEFAULT_LED_COUNT = 45  # The default value in the firmware

# A list of basic commands to test the core text API
ASCII_COMMANDS_TO_TEST = [
    "clearsegments",
    "addsegment 0 20",
    "addsegment 21 45",
    "listsegments",
    "getstatus",
]


def read_all(ser):
    """Reads all available data from the serial port with a timeout."""
    data = b""
    start_time = time.time()
    while time.time() - start_time < DELAY:
        if ser.in_waiting > 0:
            data += ser.read(ser.in_waiting)
        else:
            time.sleep(0.01)
    return data.decode("utf-8", errors="ignore").strip()


def send_command(ser, command, quiet=False):
    """Sends a command (string or dict) and returns the response."""
    if isinstance(command, dict):
        payload = json.dumps(command)
    else:
        payload = command

    if not quiet:
        print(f">>> SEND: {payload[:100]}{'...' if len(payload) > 100 else ''}")

    ser.write((payload + "\n").encode())
    time.sleep(DELAY)
    resp = read_all(ser)

    if not quiet:
        print(f"<<< RECV: {resp or '<no response>'}")
        print("-" * 20)

    return resp


def test_standard_commands(ser):
    """Runs through a basic set of plain-text commands."""
    print("## Testing Standard ASCII Commands... ##")
    all_ok = True
    for cmd in ASCII_COMMANDS_TO_TEST:
        response = send_command(ser, cmd)
        if "ERR" in response:
            all_ok = False
            print(f"  !! FAIL: Command '{cmd}' returned an error.")
    if all_ok:
        print("  -> PASS: All standard commands executed without errors.")
    return all_ok


def test_led_count_commands(ser):
    """Tests the get/set functionality for LED_COUNT, including restarts."""
    print("\n## Testing LED Count Get/Set... ##")

    # 1. Get the initial count
    print("  1. Getting initial LED count...")
    response = send_command(ser, "getledcount")
    try:
        initial_count = int(response.split(":")[1].strip())
        if initial_count != DEFAULT_LED_COUNT:
            print(
                f"  !! WARN: Initial LED count is {initial_count}, not the default {DEFAULT_LED_COUNT}."
            )
        else:
            print(f"  -> PASS: Initial LED count is correct ({initial_count}).")
    except (IndexError, ValueError) as e:
        print(
            f"  !! FAIL: Could not parse initial LED count. Response: '{response}'. Error: {e}"
        )
        return False

    # 2. Set a new count, which will trigger a restart
    new_count = 45
    print(f"\n  2. Setting LED count to {new_count} (device will restart)...")
    send_command(ser, f"setledcount {new_count}")

    # --- FIX: Close and reopen the serial port to handle the device reset ---
    print("      Device restarting, closing serial port...")
    ser.close()
    time.sleep(8)  # Wait for the device to reboot and initialize
    print("      Re-opening serial port...")
    ser.open()
    ser.reset_input_buffer()
    print("      Device reconnected.")
    # --- END FIX ---

    # 3. Verify the new count after restart
    print("\n  3. Verifying new LED count after restart...")
    response = send_command(ser, "getledcount")
    try:
        verified_count = int(response.split(":")[1].strip())
        if verified_count == new_count:
            print(f"  -> PASS: LED count successfully updated to {verified_count}.")
        else:
            print(f"  !! FAIL: Expected {new_count}, but got {verified_count}.")
            return False
    except (IndexError, ValueError) as e:
        print(
            f"  !! FAIL: Could not parse verified LED count. Response: '{response}'. Error: {e}"
        )
        return False

    # 4. Restore the original count
    print(
        f"\n  4. Restoring LED count to {DEFAULT_LED_COUNT} (device will restart again)..."
    )
    send_command(ser, f"setledcount {DEFAULT_LED_COUNT}")

    # --- FIX: Close and reopen again ---
    print("      Device restarting, closing serial port...")
    ser.close()
    time.sleep(4)
    print("      Re-opening serial port...")
    ser.open()
    ser.reset_input_buffer()
    print("      Device reconnected.")
    # --- END FIX ---

    print("\n  5. Final verification...")
    response = send_command(ser, "getledcount", quiet=True)
    try:
        final_count = int(response.split(":")[1].strip())
        assert final_count == DEFAULT_LED_COUNT
        print("  -> PASS: LED count restored successfully.")
        return True
    except (AssertionError, IndexError, ValueError):
        print("  !! FAIL: Could not restore LED count to default.")
        return False


def test_json_upload(ser, json_file_path):
    """Tests the 'batchconfig' command by uploading a JSON file."""
    print(f"\n## Testing JSON Batch Upload from '{json_file_path}'... ##")
    if not os.path.exists(json_file_path):
        print(f"  !! FAIL: JSON file not found at '{json_file_path}'")
        return False

    try:
        with open(json_file_path, "r") as f:
            data_to_upload = json.load(f)
        compact_json = json.dumps(data_to_upload, separators=(",", ":"))
    except json.JSONDecodeError as e:
        print(f"  !! FAIL: Invalid JSON in '{json_file_path}': {e}")
        return False

    command = f"batchconfig {compact_json}"
    send_command(ser, command)

    print("  Verifying upload by checking status...")
    response = send_command(ser, "getstatus")

    try:
        status_data = json.loads(response)
        num_segments_expected = len(data_to_upload.get("segments", []))
        num_segments_actual = len(status_data.get("segments", []))

        assert num_segments_actual == num_segments_expected
        print("  -> PASS: 'batchconfig' seems to have worked. Segment count matches.")
        return True
    except (json.JSONDecodeError, AssertionError) as e:
        print(f"  !! FAIL: Verification failed. Error: {e}")
        return False


def test_all_parameters_for_all_effects(ser):
    """
    Discovers all effects, then discovers, adjusts, and asks for visual
    confirmation for every parameter of each effect.
    """
    print("\n## Running Full Automated and Interactive Parameter Validation... ##")

    # 1. Discover all effects
    print("\n1. Discovering effects with 'listeffects'...")
    response = send_command(ser, "listeffects")
    try:
        effects_to_test = json.loads(response)["effects"]
        print(f"   Found {len(effects_to_test)} effects to test.")
    except (json.JSONDecodeError, KeyError):
        print("  !! FATAL: Could not parse effect list. Aborting.")
        return False

    # 2. Loop and validate each effect's parameters
    print("\n2. Discovering and testing parameters for each effect...")
    overall_results = {}

    for effect_name in effects_to_test:
        print(f"\n--- Testing Effect: {effect_name} ---")

        # Activate the effect on segment 0 so we can set its parameters
        send_command(ser, f"seteffect 0 {effect_name}", quiet=True)

        # Get the list of parameters for this effect
        get_params_cmd = {"get_parameters": effect_name}
        params_response = send_command(ser, get_params_cmd, quiet=True)

        try:
            params_list = json.loads(params_response)["params"]
        except (json.JSONDecodeError, KeyError):
            print(f"  !! FAIL: Could not get parameters for '{effect_name}'.")
            overall_results[effect_name] = False
            continue

        print(f"  Found {len(params_list)} parameters to test for '{effect_name}'.")
        effect_passed = True

        # Loop through each parameter and test setting it
        for param in params_list:
            param_name = param["name"]
            param_type = param["type"]

            test_value = None
            if param_type == "integer":
                test_value = int(param.get("max", 255))
            elif param_type == "float":
                test_value = float(param.get("max", 1.0))
            elif param_type == "color":
                test_value = "0x123456"
            elif param_type == "boolean":
                test_value = True

            if test_value is not None:
                print(
                    f"      Adjusting param '{param_name}' with value '{test_value}'..."
                )
                set_param_cmd = {
                    "set_parameter": {
                        "segment_id": 0,
                        "effect": effect_name,
                        "name": param_name,
                        "value": test_value,
                    }
                }
                response = send_command(ser, set_param_cmd, quiet=True)

                if "OK" not in response:
                    effect_passed = False
                    print(
                        f"      !! FAIL: Command to set '{param_name}' was not acknowledged. Response: {response}"
                    )
                else:
                    ans = (
                        input(
                            f"      --> Please visually confirm: Did the '{param_name}' parameter change? (y/n): "
                        )
                        .strip()
                        .lower()
                    )
                    if ans != "y":
                        effect_passed = False
                        print(
                            f"      !! FAIL: Visual confirmation failed for '{param_name}'."
                        )

        overall_results[effect_name] = effect_passed

    # Final summary
    print("\n\n--- Automated Parameter Test Summary ---")
    all_passed = True
    for effect, passed in overall_results.items():
        print(f"  {effect:20s} {'PASS' if passed else 'FAIL'}")
        if not passed:
            all_passed = False

    return all_passed


def run_all_tests(port, baud, json_config):
    """Main function to run the full test suite."""
    print(f"Opening serial port {port} @ {baud} baud")
    try:
        with serial.Serial(port, baud, timeout=DELAY) as ser:
            print("Waiting for device to initialize...")
            time.sleep(2)  # Increased wait time for initial connection
            ser.reset_input_buffer()

            # Run all major test functions
            std_ok = test_standard_commands(ser)
            led_count_ok = test_led_count_commands(ser)
            # Re-read the buffer after potential restarts from the LED count test
            read_all(ser)
            json_ok = test_json_upload(ser, json_config)
            params_ok = test_all_parameters_for_all_effects(ser)

            print("\n\n--- FINAL TEST SUMMARY ---")
            print(f"  Standard Commands:      {'PASS' if std_ok else 'FAIL'}")
            print(f"  LED Count Management:   {'PASS' if led_count_ok else 'FAIL'}")
            print(f"  JSON Upload:            {'PASS' if json_ok else 'FAIL'}")
            print(f"  Parameter Validation:   {'PASS' if params_ok else 'FAIL'}")

            if std_ok and led_count_ok and json_ok and params_ok:
                print("\n🎉 All tests passed successfully! 🎉")
            else:
                print("\n❌ Some tests failed. Please review the log. ❌")

    except serial.SerialException as e:
        print(f"\nFATAL: Error opening serial port {port}: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nAn unexpected error occurred: {e}")
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Comprehensive and interactive test suite for RaveController_v1."
    )
    parser.add_argument(
        "--port", required=True, help="Serial port (e.g., COM7 or /dev/ttyACM0)"
    )
    parser.add_argument(
        "--baud", type=int, default=BAUD, help=f"Baud rate (default: {BAUD})"
    )
    parser.add_argument(
        "--json-config",
        default="test_config.json",
        help="Path to the JSON config file to upload.",
    )
    args = parser.parse_args()
    run_all_tests(args.port, args.baud, args.json_config)

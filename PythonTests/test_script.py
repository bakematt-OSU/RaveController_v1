#!/usr/bin/env python3
"""
The definitive, comprehensive, and interactive test harness for RaveController_v1.
This script validates all major functionality including:
1. Standard text-based serial commands.
2. LED count getting and setting, including device restarts.
3. Configuration saving and persistence across restarts.
4. Batch configuration upload from a JSON file.
5. Automated and Manual (visual) validation of every parameter for every effect.
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
    "saveconfig",
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

def wait_for_device_ready(ser, timeout=10):
    """Reads from serial until the 'setup complete' message is seen or timeout occurs."""
    print("      Waiting for device to be ready...")
    start_time = time.time()
    buffer = ""
    while time.time() - start_time < timeout:
        if ser.in_waiting > 0:
            buffer += ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
            if "Setup complete. Entering main loop..." in buffer:
                print("      Device is ready.")
                ser.reset_input_buffer()
                return True
        time.sleep(0.1)
    print("      !! TIMEOUT: Did not receive ready signal from device.")
    return False

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
        print(f"  -> PASS: Initial LED count is ({initial_count}).")
    except (IndexError, ValueError) as e:
        print(
            f"  !! FAIL: Could not parse initial LED count. Response: '{response}'. Error: {e}"
        )
        return False

    # 2. Set a new count, which will trigger a restart
    new_count = 300
    print(f"\n  2. Setting LED count to {new_count} (device will restart)...")
    send_command(ser, f"setledcount {new_count}")

    ser.close()
    time.sleep(4)
    ser.open()
    if not wait_for_device_ready(ser): return False

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

    ser.close()
    time.sleep(4)
    ser.open()
    if not wait_for_device_ready(ser): return False
    
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

def test_config_persistence(ser):
    """Tests if a saved configuration persists across a restart."""
    print("\n## Testing Configuration Persistence... ##")
    
    # 1. Set a unique, verifiable configuration
    test_effect = "ColoredFire"
    test_segment_id = 1
    print(f"  1. Setting a unique configuration (Effect: {test_effect} on Segment: {test_segment_id})...")
    send_command(ser, "clearsegments", quiet=True)
    send_command(ser, "addsegment 0 100", quiet=True)
    send_command(ser, f"seteffect {test_segment_id} {test_effect}")

    # 2. Save the configuration and force a restart
    print("\n  2. Saving configuration and restarting device...")
    send_command(ser, "saveconfig")
    send_command(ser, f"setledcount {DEFAULT_LED_COUNT}", quiet=True)
    
    ser.close()
    time.sleep(4)
    ser.open()
    if not wait_for_device_ready(ser): return False

    # 3. Verify the configuration was loaded on startup
    print("\n  3. Verifying configuration after restart...")
    response = send_command(ser, "getstatus")
    try:
        status = json.loads(response)
        loaded_effect = status["segments"][test_segment_id]["effect"]
        if loaded_effect == test_effect:
            print(f"  -> PASS: Configuration persisted. Found '{loaded_effect}' on segment {test_segment_id}.")
            return True
        else:
            print(f"  !! FAIL: Configuration did not persist. Expected '{test_effect}', but found '{loaded_effect}'.")
            return False
    except (json.JSONDecodeError, IndexError, KeyError) as e:
        print(f"  !! FAIL: Could not parse or verify status after restart. Error: {e}")
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


def test_all_parameters_for_all_effects(ser, mode):
    """
    Discovers all effects, then discovers, adjusts, and verifies every parameter 
    of each effect, either automatically or with manual confirmation.
    """
    print(f"\n## Running Parameter Validation (Mode: {mode.upper()})... ##")

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

        send_command(ser, f"seteffect 0 {effect_name}", quiet=True)

        for param in params_list:
            param_name = param["name"]
            param_type = param["type"]
            
            test_value = None
            # Use the max value provided by the device for integer and float params
            if param_type == "integer": test_value = int(param.get("max_val", 255))
            elif param_type == "float": test_value = float(param.get("max_val", 1.0))
            elif param_type == "color": test_value = 0x123456 # Use integer for color
            elif param_type == "boolean": test_value = True

            if test_value is not None:
                # Convert to hex string for printing, if it's a color
                print_val = f"0x{test_value:X}" if param_type == 'color' else test_value
                print(
                    f"      Adjusting param '{param_name}' to '{print_val}'..."
                )
                
                # Use string version of color for the JSON command
                set_param_value = f"0x{test_value:X}" if param_type == 'color' else test_value
                set_param_cmd = {
                    "set_parameter": {
                        "segment_id": 0,
                        "effect": effect_name,
                        "name": param_name,
                        "value": set_param_value,
                    }
                }
                response = send_command(ser, set_param_cmd, quiet=True)

                if "OK" not in response:
                    effect_passed = False
                    print(
                        f"      !! AUTO-FAIL: Command to set '{param_name}' was not acknowledged. Response: {response}"
                    )
                    continue

                # Automatic verification
                info_resp = send_command(ser, f"geteffectinfo 0", quiet=True)
                try:
                    info = json.loads(info_resp)
                    param_verified = False
                    for p_info in info.get("params", []):
                        if p_info["name"] == param_name:
                            if p_info["value"] == test_value:
                                param_verified = True
                                break
                    if not param_verified:
                        effect_passed = False
                        print(f"      !! AUTO-FAIL: Verification for '{param_name}' failed. Sent {test_value}, but device reports another value.")
                except (json.JSONDecodeError, KeyError) as e:
                    effect_passed = False
                    print(f"      !! AUTO-FAIL: Could not parse geteffectinfo. Error: {e}")

                # Manual (visual) verification if in manual mode and auto check passed
                if mode == 'manual' and effect_passed:
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
                            f"      !! MANUAL-FAIL: Visual confirmation failed for '{param_name}'."
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


def run_all_tests(port, baud, json_config, mode):
    """Main function to run the full test suite."""
    print(f"Opening serial port {port} @ {baud} baud")
    try:
        ser = serial.Serial(port, baud, timeout=DELAY)
        
        if not wait_for_device_ready(ser):
             sys.exit(1)

        # Run all major test functions
        std_ok = test_standard_commands(ser)
        led_count_ok = test_led_count_commands(ser)
        config_ok = test_config_persistence(ser)
        json_ok = test_json_upload(ser, json_config)
        params_ok = test_all_parameters_for_all_effects(ser, mode)

        print("\n\n--- FINAL TEST SUMMARY ---")
        print(f"  Standard Commands:       {'PASS' if std_ok else 'FAIL'}")
        print(f"  LED Count Management:    {'PASS' if led_count_ok else 'FAIL'}")
        print(f"  Config Persistence:      {'PASS' if config_ok else 'FAIL'}")
        print(f"  JSON Upload:             {'PASS' if json_ok else 'FAIL'}")
        print(f"  Parameter Validation:    {'PASS' if params_ok else 'FAIL'}")

        if std_ok and led_count_ok and config_ok and json_ok and params_ok:
            print("\n🎉 All tests passed successfully! 🎉")
        else:
            print("\n❌ Some tests failed. Please review the log. ❌")

    except serial.SerialException as e:
        print(f"\nFATAL: Error opening serial port {port}: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nAn unexpected error occurred: {e}")
        sys.exit(1)
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()


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
    parser.add_argument(
        "--mode",
        choices=['automatic', 'manual'],
        default='manual',
        help="Set parameter testing mode: 'automatic' for silent verification, 'manual' for visual confirmation."
    )
    args = parser.parse_args()
    run_all_tests(args.port, args.baud, args.json_config, args.mode)
import serial
import json
import time
import sys
import argparse
import struct  # For converting integer to bytes
import random  # For generating random-ish values for test data

# --- Helper Functions for Serial Communication ---


def send_command(ser, command_str):
    """Sends a command string to Arduino and prints debug info."""
    print(f"\n[SEND] Command: '{command_str.strip()}'")
    ser.write(command_str.encode("utf-8"))
    print(
        f"[SENT] ASCII: '{command_str.strip()}' | Bytes: {command_str.encode('utf-8').hex()}"
    )


def read_line_with_timeout(ser, timeout_s=5, expected_prefix=None):
    """Reads a line from serial with a timeout, optionally checking for a prefix."""
    start_time = time.time()
    buffer = ""
    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            try:
                char = ser.read(1).decode("utf-8", errors="ignore")
                buffer += char
                if "\n" in buffer:
                    line = buffer.split("\n")[0].strip()
                    print(f"[RECV] Line: '{line}'")
                    if expected_prefix and not line.startswith(expected_prefix):
                        # If we're expecting a specific prefix and don't get it,
                        # it might be a debug message, so we continue reading.
                        buffer = buffer.split("\n", 1)[1] if "\n" in buffer else ""
                        continue
                    return line
            except UnicodeDecodeError:
                # Handle cases where a byte might not be a valid UTF-8 character
                print("[RECV] UnicodeDecodeError: Skipping byte.")
                buffer = ""  # Clear buffer on error to avoid accumulating bad data
        time.sleep(0.001)  # Small delay to prevent busy-waiting
    print(
        f"[RECV] Timeout after {timeout_s}s. No line received or expected prefix '{expected_prefix}' not found."
    )
    return None


def wait_for_ack(ser, timeout_s=5):
    """Waits for an ACK message from the Arduino."""
    print("[RECV] Waiting for ACK...")
    start_time = time.time()
    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            print(f"[RECV] Line: '{line}'")
            # The Arduino sends "OK: Segment X (Name) config applied." for each segment
            # or "-> Sent ACK" for the initial command.
            if (
                "-> Sent ACK" in line
                or "OK: Segment" in line
                or "OK: All segment configurations received" in line
            ):
                print("[RECV] ACK received.")
                return True
            elif "ERR:" in line or "error" in line:
                print(f"[RECV] Error detected: {line}")
                return False
        time.sleep(0.001)
    print("[RECV] Timeout: No ACK received.")
    return False


# --- Arduino Data Fetching Functions ---


def read_json_response(ser, timeout_s=15):
    """
    Reads lines from serial until a complete, valid JSON object is received or timeout.
    Intelligently attempts to parse the buffer as JSON and discards non-JSON debug lines.
    """
    json_str_buffer = ""
    raw_char_buffer = ""  # To accumulate all characters for line-by-line printing
    start_time = time.time()

    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            char = ser.read(1).decode("utf-8", errors="ignore")
            raw_char_buffer += char

            # If a newline is received, process the line for printing and potential JSON content
            if "\n" in raw_char_buffer:
                current_line = raw_char_buffer.split("\n")[0].strip()
                print(f"[RECV] Line: '{current_line}'")
                raw_char_buffer = "\n".join(
                    raw_char_buffer.split("\n")[1:]
                )  # Keep remaining part

                # Heuristic to identify potential JSON lines and filter out common debug messages
                if current_line.startswith("{") or current_line.startswith("["):
                    json_str_buffer = (
                        current_line  # Start new buffer if new JSON object begins
                    )
                elif (
                    not current_line.startswith("Serial RX (Raw):")
                    and not current_line.startswith("Serial Command Received:")
                    and not current_line.startswith("Serial Command:")
                    and not current_line.startswith("CMD:")
                    and not current_line.startswith("-> Sending")
                ):
                    # Append only lines that seem to be part of JSON and not debug messages
                    json_str_buffer += current_line

                # Try to parse the current buffer as JSON
                if json_str_buffer:  # Only try if there's something in the buffer
                    try:
                        parsed_json = json.loads(json_str_buffer)
                        return parsed_json  # Successfully parsed, return it!
                    except json.JSONDecodeError:
                        # Not a complete or valid JSON yet, keep buffering
                        pass

            # Reset timeout if any data is coming in (even debug messages)
            start_time = time.time()
        time.sleep(0.001)  # Small delay

    print(
        f"Timeout: Did not receive full JSON within {timeout_s} seconds. Current buffer: '{json_str_buffer}'"
    )
    return None


def get_available_effects(ser):
    """Fetches the list of available effects from the Arduino."""
    send_command(ser, "listeffects\n")
    print("Waiting for available effects list from Arduino...")

    response_json = read_json_response(ser)

    if response_json:
        effects = response_json.get("effects", [])
        print(f"Available effects fetched: {effects}")
        return effects
    else:
        print("Failed to receive valid effects list JSON.")
        return []


def get_effect_info(ser, effect_name):
    """Fetches parameter info for a specific effect from the Arduino."""
    command = f"geteffectinfo 0 {effect_name}\n"  # Use segment 0 as a dummy
    send_command(ser, command)
    print(f"Waiting for effect info for '{effect_name}' from Arduino...")

    response_json = read_json_response(ser)

    if response_json:
        # Verify that the 'effect' key matches the requested effect_name
        if response_json.get("effect") != effect_name:
            print(
                f"Warning: Received effect info for '{response_json.get('effect')}' but expected '{effect_name}'. Skipping."
            )
            return []

        params = response_json.get("params", [])
        return params
    else:
        print(f"Failed to receive valid effect info JSON for '{effect_name}'.")
        return []


# --- Test Functions ---


def get_all_segment_configs(ser):
    """Sends 'getallsegmentconfigs' and receives all segment JSONs."""
    all_segments_data = []

    send_command(ser, "getallsegmentconfigs\n")

    print("Waiting for segment configurations JSON from Arduino...")
    response_json = read_json_response(ser)

    if response_json:
        all_segments_data = response_json.get("segments", [])
        print(f"Received total segments: {len(all_segments_data)}")
        return all_segments_data
    else:
        print("Timeout or error: Did not receive full JSON for segment configurations.")
        return None


def set_all_segment_configs(ser, segments_to_send):
    """Sends 'setallsegmentconfigs' and then the segment data."""
    print("\n--- Initiating Set All Segment Configurations ---")

    # 1. Send the initial command (text command to SerialCommandHandler)
    send_command(ser, "setallsegmentconfigs\n")
    if not wait_for_ack(ser):
        print("Failed to receive ACK for setallsegmentconfigs command initiation.")
        return False

    # 2. Send the 2-byte segment count (binary data)
    num_segments = len(segments_to_send)
    count_bytes = struct.pack(
        ">H", num_segments
    )  # >H means Big-endian, unsigned short (2 bytes)
    print(f"[SEND] Segment Count: {num_segments} | Bytes: {count_bytes.hex()}")
    ser.write(count_bytes)

    if not wait_for_ack(ser):
        print("Failed to receive ACK for segment count.")
        return False

    # 3. Send each segment's JSON configuration (text data)
    print(f"\nSending {num_segments} segment configurations...")
    for i, segment_data in enumerate(segments_to_send):
        json_segment = (
            json.dumps(segment_data) + "\n"
        )  # Add newline for readline on Arduino
        print(f"[SEND] Segment {i+1}/{num_segments} JSON: '{json_segment.strip()}'")
        ser.write(json_segment.encode("utf-8"))

        # Wait for ACK for each segment
        if not wait_for_ack(ser):
            print(f"Failed to receive ACK for segment {i+1}. Aborting.")
            return False

        print(f"Successfully sent segment {i+1}.")
        time.sleep(0.05)  # Small delay between segments

    print("\n--- All segment configurations sent successfully! ---")
    return True


def generate_test_segments(ser, led_count=45):  # Default changed to 45
    """
    Generates a set of test segments dynamically:
    - Fetches available effects from Arduino.
    - Creates specific segments as requested.
    - Assigns a random effect and random parameters to each.
    """
    segments = []

    # Fetch available effects
    available_effects = get_available_effects(ser)
    if not available_effects:
        print("No effects available from Arduino. Cannot generate dynamic segments.")
        return []

    # Get detailed parameter info for each effect
    effect_param_info = {}
    for effect_name in available_effects:
        effect_param_info[effect_name] = get_effect_info(ser, effect_name)

    # Filter out "None" and "SolidColor" for dynamic assignment to avoid static effects
    dynamic_effects = [e for e in available_effects if e not in ["None", "SolidColor"]]
    if not dynamic_effects:
        print(
            "Warning: No dynamic effects available for segment assignment. Using all available effects."
        )
        dynamic_effects = available_effects  # Fallback to all if no "dynamic" ones

    num_dynamic_effects = len(dynamic_effects)
    if num_dynamic_effects == 0:
        print("Error: No effects available to assign to segments.")
        return []

    # Segment 0: The "all" segment (always present, usually covers the whole strip)
    segments.append(
        {
            "id": 0,
            "name": "all",
            "startLed": 0,
            "endLed": led_count - 1,
            "brightness": 150,
            "effect": "SolidColor",  # Default for 'all' segment
            "color": 0xFF00FF,  # Purple
        }
    )

    segment_id_counter = 1  # Start IDs for new segments from 1

    # Calculate segment length for even distribution
    # Ensure each segment has at least 1 LED
    segment_length = max(1, led_count // num_dynamic_effects)

    current_led_start = 0
    effect_index = 0

    while current_led_start < led_count and effect_index < num_dynamic_effects:
        start_led = current_led_start
        end_led = min(current_led_start + segment_length - 1, led_count - 1)

        # If it's the last segment, ensure it goes to the end of the strip
        if effect_index == num_dynamic_effects - 1:
            end_led = led_count - 1

        if start_led > end_led:  # Skip if segment is invalid
            break

        segment = {
            "id": segment_id_counter,
            "name": f"effect_seg_{segment_id_counter}",
            "startLed": start_led,
            "endLed": end_led,
            "brightness": random.randint(100, 255),  # Random brightness
        }

        chosen_effect = dynamic_effects[effect_index]
        segment["effect"] = chosen_effect

        # Add parameters for the chosen effect
        if chosen_effect in effect_param_info:
            # print(f"DEBUG: Parameters for {chosen_effect} before assignment: {effect_param_info[chosen_effect]}") # Debug line
            for param in effect_param_info[chosen_effect]:
                param_name = param["name"]
                param_type = param["type"]
                min_val = param.get("min_val")
                max_val = param.get("max_val")

                if param_type == "integer":
                    segment[param_name] = random.randint(
                        int(min_val if min_val is not None else 0),
                        int(max_val if max_val is not None else 255),
                    )
                elif param_type == "float":
                    segment[param_name] = round(
                        random.uniform(
                            min_val if min_val is not None else 0.0,
                            max_val if max_val is not None else 1.0,
                        ),
                        2,
                    )
                elif param_type == "color":
                    segment[param_name] = random.randint(0, 0xFFFFFF)
                elif param_type == "boolean":
                    segment[param_name] = random.choice([True, False])
        else:
            print(
                f"Warning: No parameter info found for effect: {chosen_effect}. Skipping parameter assignment."
            )

        segments.append(segment)
        segment_id_counter += 1
        current_led_start = end_led + 1
        effect_index += 1

    print(f"\nGenerated {len(segments)} test segments.")
    return segments


# --- Main Script Execution ---


def main():
    parser = argparse.ArgumentParser(description="Arduino Segment Config Serial Tester")
    parser.add_argument(
        "--port",
        type=str,
        required=True,
        help="The serial port connected to the Arduino (e.g., COM3, /dev/ttyACM0)",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="The baud rate for the serial connection (default: 115200)",
    )
    parser.add_argument(
        "--mode",
        type=str,
        choices=["get", "set", "both"],
        default="get",
        help='Operation mode: "get" (read configs), "set" (send sample configs), or "both"',
    )
    parser.add_argument(
        "--led_count",
        type=int,
        default=45,  # Changed default to 45
        help="The total number of LEDs on the strip. Used for generating test segment ranges.",
    )
    args = parser.parse_args()

    # Initialize test results
    get_test_status = "NOT RUN"
    get_test_reason = ""
    set_test_status = "NOT RUN"
    set_test_reason = ""

    print(f"Connecting to {args.port} at {args.baud} baud...")
    ser = None  # Initialize ser to None
    try:
        ser = serial.Serial(args.port, args.baud, timeout=5)
        time.sleep(3)  # Increased initial delay for Arduino boot-up
        ser.flushInput()  # Clear any residual data in the input buffer
        print("Connection established and buffer flushed.")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        print("Please check if the Arduino is connected and the port is correct.")
        sys.exit(1)  # Exit if connection fails

    try:
        if args.mode == "get" or args.mode == "both":
            print("\n--- Running GET Test ---")
            try:
                segments_received = get_all_segment_configs(ser)
                if segments_received is not None:
                    get_test_status = "PASSED"
                else:
                    get_test_status = "FAILED"
                    get_test_reason = (
                        "Timeout or JSON parsing error during segment config retrieval."
                    )
            except Exception as e:
                get_test_status = "FAILED"
                get_test_reason = f"Exception during GET test: {e}"
            time.sleep(1)  # Give some time before the next operation

        if args.mode == "set" or args.mode == "both":
            print("\n--- Running SET Test ---")
            try:
                test_segments = generate_test_segments(ser, args.led_count)
                if (
                    test_segments
                ):  # Only proceed if segments were successfully generated
                    if set_all_segment_configs(ser, test_segments):
                        set_test_status = "PASSED"
                    else:
                        set_test_status = "FAILED"
                        set_test_reason = "ACK not received or communication error during segment sending."
                else:
                    set_test_status = "SKIPPED"
                    set_test_reason = "No segments generated (e.g., no effects available from Arduino)."
            except Exception as e:
                set_test_status = "FAILED"
                set_test_reason = f"Exception during SET test: {e}"
            time.sleep(1)  # Give some time after sending

            # This is the requested verification step
            if (
                set_test_status == "PASSED"
            ):  # Only verify if the SET operation itself passed
                print("\n--- Verifying SET by running GET again ---")
                verify_segments = get_all_segment_configs(ser)
                if verify_segments is None:
                    print(
                        "Verification failed: Could not retrieve segments after setting them."
                    )
                else:
                    print(
                        "Verification successful: Segments retrieved after SET operation."
                    )

    except KeyboardInterrupt:
        print("\nTesting interrupted by user.")
    except Exception as e:
        print(f"An unexpected error occurred during test execution: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()
            print("Serial port closed.")

        print("\n" + "=" * 30)
        print("      TEST SUMMARY       ")
        print("=" * 30)
        print(f"GET Test Status: {get_test_status}")
        if get_test_status == "FAILED":
            print(f"  Reason: {get_test_reason}")
        print(f"SET Test Status: {set_test_status}")
        if set_test_status == "FAILED" or set_test_status == "SKIPPED":
            print(f"  Reason: {set_test_reason}")
        print("=" * 30)


if __name__ == "__main__":
    main()

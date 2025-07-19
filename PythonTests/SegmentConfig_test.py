import serial
import json
import time
import sys
import argparse
import struct  # For converting integer to bytes
import random  # For generating random-ish values for test data

# --- Constants for binary commands (from BinaryCommandHandler.h) ---
CMD_GET_ALL_EFFECTS = 0x10
CMD_ACK_GENERIC = 0xA0  # This corresponds to CMD_ACK_GENERIC in the firmware

# --- Helper Functions for Serial Communication ---


def send_command(ser, command_str):
    """Sends a text command string to Arduino and prints debug info."""
    print(f"\n[SEND] Command: '{command_str.strip()}'")
    ser.write(command_str.encode("utf-8"))
    print(
        f"[SENT] ASCII: '{command_str.strip()}' | Bytes: {command_str.encode('utf-8').hex()}"
    )


def send_binary_command(ser, command_byte, payload=b""):
    """Sends a binary command byte followed by an optional payload."""
    full_command = bytes([command_byte]) + payload
    print(
        f"\n[SEND BINARY] Command: 0x{command_byte:02X} | Payload: {payload.hex()} | Full Bytes: {full_command.hex()}"
    )
    ser.write(full_command)


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


def read_exact_bytes(ser, num_bytes, timeout_s=5):
    """Reads exactly num_bytes from serial with a timeout."""
    start_time = time.time()
    buffer = b""
    while time.time() - start_time < timeout_s and len(buffer) < num_bytes:
        if ser.in_waiting > 0:
            bytes_to_read = min(ser.in_waiting, num_bytes - len(buffer))
            buffer += ser.read(bytes_to_read)
        time.sleep(0.001)
    if len(buffer) == num_bytes:
        print(f"[RECV BINARY] Received {num_bytes} bytes: {buffer.hex()}")
        return buffer
    else:
        print(
            f"[RECV BINARY] Timeout or insufficient bytes received. Expected {num_bytes}, Got {len(buffer)}: {buffer.hex()}"
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
                or "OK: All effects sent."
                in line  # Added for the getalleffects final ACK
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
    This version is more robust by strictly filtering for JSON start characters.
    """
    json_str_buffer = ""
    start_time = time.time()

    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            print(f"[RECV] Line: '{line}'")  # Always print the raw line for debugging

            # Reset timeout if any data is coming in
            start_time = time.time()

            # Only consider lines that start with '{' or '[' as potential JSON.
            if line.startswith("{") or line.startswith("["):
                json_str_buffer += line
                try:
                    parsed_json = json.loads(json_str_buffer)
                    return parsed_json  # Successfully parsed, return it!
                except json.JSONDecodeError:
                    # Not a complete or valid JSON yet, keep buffering
                    pass
            else:
                # If it's not a JSON start, discard this line.
                # If we had partial JSON, and this is NOT JSON, it implies corruption.
                if json_str_buffer:
                    print(
                        f"[RECV] Discarding non-JSON line after partial JSON. Buffer reset."
                    )
                    json_str_buffer = ""  # Discard corrupted buffer.
                # Otherwise, it's just a debug line before any JSON started, so simply discard.

        time.sleep(0.001)  # Small delay

    print(
        f"Timeout: Did not receive full JSON within {timeout_s} seconds. Current buffer: '{json_str_buffer}'"
    )
    return None


def get_available_effects(ser):
    """
    Fetches the list of available effects from the Arduino using the new binary protocol.

    """
    available_effects = []

    print("\n--- Fetching All Effects (Binary Protocol) ---")
    ser.flushInput()  # Clear buffer before sending command

    # 1. Send the text command to SerialCommandHandler to initiate the binary transfer
    send_command(ser, "getalleffects\n")

    # 2. Read the 3-byte binary effect count (CMD_GET_ALL_EFFECTS + 2 bytes for count)
    # The Arduino sends this immediately after processing the text command.
    print("[RECV] Waiting for binary effect count header...")
    response_bytes = read_exact_bytes(ser, 3)
    if (
        not response_bytes or response_bytes[0] != CMD_GET_ALL_EFFECTS
    ):  # Check command byte
        print(
            f"Failed to receive valid binary effect count header. Expected command {CMD_GET_ALL_EFFECTS:02X}, got {response_bytes[0]:02X if response_bytes else 'None'}"
        )
        return []

    effect_count = struct.unpack(">H", response_bytes[1:])[
        0
    ]  # Unpack big-endian unsigned short
    print(f"Expected {effect_count} effects to receive.")

    # 3. Send ACK_GENERIC after receiving the count header. The Arduino expects this ACK
    # before sending the *first* effect's JSON.
    send_binary_command(ser, CMD_ACK_GENERIC)
    time.sleep(
        0.05
    )  # Small delay to ensure Arduino processes ACK before sending next data

    # 4. Loop to receive each effect's JSON and send an ACK
    print("\n[RECV] Waiting for individual effect JSONs...")
    for i in range(effect_count):
        # We expect a JSON string, which is text, so use read_json_response
        effect_json_response = read_json_response(ser)
        if effect_json_response:
            effect_name = effect_json_response.get("effect")
            if effect_name:
                available_effects.append(effect_name)
                print(f"Received effect {i+1}: '{effect_name}'")

                # Send ACK_GENERIC for each received effect JSON.
                # The Arduino waits for this ACK before sending the *next* effect.
                send_binary_command(ser, CMD_ACK_GENERIC)
                time.sleep(0.05)  # Small delay to ensure ACK is processed by Arduino
            else:
                print(
                    f"Warning: Received effect JSON without 'effect' key for item {i+1}. JSON: {effect_json_response}"
                )
                send_binary_command(
                    ser, CMD_ACK_GENERIC
                )  # Still send ACK to not block the Arduino
        else:
            print(f"Failed to receive JSON for effect {i+1}. Aborting effect fetching.")
            # Even if we fail to read, try to send an ACK to avoid blocking the Arduino indefinitely
            send_binary_command(ser, CMD_ACK_GENERIC)
            return available_effects  # Return what we have so far

    # After all effects are sent and ACKed, the Arduino sends a final "OK: All effects sent." message.
    # We wait for this to confirm the sequence is complete.
    if not wait_for_ack(ser):
        print("Failed to receive final 'All effects sent' ACK.")

    print(f"\nSuccessfully fetched {len(available_effects)} effects.")
    return available_effects


def get_effect_info(ser, effect_name):
    """Fetches parameter info for a specific effect from the Arduino."""
    ser.flushInput()  # Clear buffer before sending command
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
def set_single_segment_config(ser, segment_data):
    """Sends 'setsegmentjson' with the given segment data."""
    print(
        f"\n--- Sending Single Segment Configuration for ID: {segment_data['id']} ---"
    )
    json_payload = json.dumps(segment_data)
    command = f"setsegmentjson {json_payload}\n"
    send_command(ser, command)
    # The firmware does not send a specific ACK for this command,
    # but we can check for the "OK: Segment..." debug message.
    # A short delay is often sufficient.
    time.sleep(0.2)
    return True


def get_all_segment_configs(ser):
    """Sends 'getallsegmentconfigs' and receives all segment JSONs."""
    all_segments_data = []

    ser.flushInput()  # Clear buffer before sending command
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
    ser.flushInput()  # Clear buffer before sending command
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
        ser.flushInput()  # Clear buffer before sending each segment JSON
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


def generate_test_segments(ser, led_count=45):
    """
    Generates a set of test segments, ensuring one segment is created for
    each available dynamic effect to guarantee complete testing.
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

    # Filter out static effects to get the list of effects to test
    dynamic_effects = [e for e in available_effects if e not in ["None", "SolidColor"]]
    if not dynamic_effects:
        print("Error: No dynamic effects available to assign to segments.")
        return []

    # Shuffle the list to randomize the order of effects
    random.shuffle(dynamic_effects)

    # Segment 0: The "all" segment (always present)
    segments.append(
        {
            "id": 0,
            "name": "all",
            "startLed": 0,
            "endLed": led_count - 1,
            "brightness": 150,
            "effect": "SolidColor",
            "color": 0xFF00FF,
        }
    )

    # Determine segment length based on the number of effects to test
    num_effects_to_test = len(dynamic_effects)
    segment_length = max(1, led_count // num_effects_to_test)
    current_led_start = 0

    # Loop through the shuffled list of effects
    for i, effect_name in enumerate(dynamic_effects):
        start_led = current_led_start
        end_led = min(start_led + segment_length - 1, led_count - 1)

        # Ensure the last segment covers the rest of the strip
        if i == num_effects_to_test - 1:
            end_led = led_count - 1

        if start_led >= led_count:
            break

        segment = {
            "id": i + 1,
            "name": f"eff_seg_{i+1}",
            "startLed": start_led,
            "endLed": end_led,
            "brightness": random.randint(100, 255),
            "effect": effect_name,  # Assign the effect from the shuffled list
        }

        # Add random parameters for the assigned effect
        if effect_name in effect_param_info:
            for param in effect_param_info[effect_name]:
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

        segments.append(segment)
        current_led_start = end_led + 1

    print(
        f"\nGenerated {len(segments)} segments to test all {num_effects_to_test} dynamic effects."
    )
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
        choices=["get", "set", "set_single", "both", "all"],
        default="all",
        help='Operation mode. "all" (the default) runs get, set, and set_single.',
    )
    parser.add_argument(
        "--led_count",
        type=int,
        default=45,
        help="The total number of LEDs on the strip. Used for generating test segment ranges.",
    )
    args = parser.parse_args()

    # Initialize test results
    get_test_status = "NOT RUN"
    get_test_reason = ""
    set_test_status = "NOT RUN"
    set_test_reason = ""
    set_single_test_status = "NOT RUN"
    set_single_test_reason = ""
    verify_test_status = "NOT RUN"
    verify_test_reason = ""

    print(f"Connecting to {args.port} at {args.baud} baud...")
    ser = None
    try:
        ser = serial.Serial(args.port, args.baud, timeout=5)
        time.sleep(3)
        ser.flushInput()
        print("Connection established and buffer flushed.")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        print("Please check if the Arduino is connected and the port is correct.")
        sys.exit(1)

    try:
        # Determine which tests to run
        modes_to_run = []
        if args.mode == "all":
            modes_to_run = ["get", "set", "set_single"]
        elif args.mode == "both":
            modes_to_run = ["get", "set"]
        else:
            modes_to_run = [args.mode]

        # --- Execute Tests ---
        for mode in modes_to_run:
            if mode == "get":
                print("\n--- Running GET Test ---")
                try:
                    segments_received = get_all_segment_configs(ser)
                    if segments_received is not None:
                        get_test_status = "PASSED"
                    else:
                        get_test_status = "FAILED"
                        get_test_reason = (
                            "Timeout or JSON parsing error during retrieval."
                        )
                except Exception as e:
                    get_test_status = "FAILED"
                    get_test_reason = f"Exception during GET test: {e}"
                time.sleep(1)

            elif mode == "set":
                print("\n--- Running SET (All) Test ---")
                try:
                    test_segments = generate_test_segments(ser, args.led_count)
                    if test_segments:
                        if set_all_segment_configs(ser, test_segments):
                            set_test_status = "PASSED"
                        else:
                            set_test_status = "FAILED"
                            set_test_reason = "ACK not received or communication error."
                    else:
                        set_test_status = "SKIPPED"
                        set_test_reason = "No segments generated."
                except Exception as e:
                    set_test_status = "FAILED"
                    set_test_reason = f"Exception during SET test: {e}"
                time.sleep(1)

            elif mode == "set_single":
                print("\n--- Running SET (Single) Test ---")
                try:
                    # Generate a new set of random segments for this test
                    test_segments = generate_test_segments(ser, args.led_count)
                    if test_segments:
                        all_sent_ok = True
                        # Iterate through all generated segments including 'all'
                        for segment in test_segments:
                            if not set_single_segment_config(ser, segment):
                                all_sent_ok = False
                                set_single_test_reason = (
                                    f"Failed on segment ID {segment['id']}"
                                )
                                break
                            time.sleep(1)  # Longer pause to visually see each effect
                        if all_sent_ok:
                            set_single_test_status = "PASSED"
                        else:
                            set_single_test_status = "FAILED"
                    else:
                        set_single_test_status = "SKIPPED"
                        set_single_test_reason = "No segments generated."
                except Exception as e:
                    set_single_test_status = "FAILED"
                    set_single_test_reason = f"Exception during SET_SINGLE test: {e}"
                time.sleep(1)

        # --- Verification Step ---
        if set_test_status == "PASSED" or set_single_test_status == "PASSED":
            print("\n--- Verifying SET by running GET again ---")
            try:
                verify_segments = get_all_segment_configs(ser)
                if verify_segments is None:
                    verify_test_status = "FAILED"
                    verify_test_reason = (
                        "Could not retrieve segments after setting them."
                    )
                else:
                    verify_test_status = "PASSED"
                    verify_test_reason = "Segments retrieved successfully."
            except Exception as e:
                verify_test_status = "FAILED"
                verify_test_reason = f"Exception during verification GET: {e}"

    except KeyboardInterrupt:
        print("\nTesting interrupted by user.")
    except Exception as e:
        print(f"An unexpected error occurred during test execution: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()
            print("\nSerial port closed.")

        # --- Final Summary ---
        print("\n" + "=" * 30)
        print("       TEST SUMMARY       ")
        print("=" * 30)
        if get_test_status != "NOT RUN":
            print(f"GET Test Status: {get_test_status}")
            if get_test_status != "PASSED":
                print(f"  Reason: {get_test_reason}")
        if set_test_status != "NOT RUN":
            print(f"SET (All) Test Status: {set_test_status}")
            if set_test_status != "PASSED":
                print(f"  Reason: {set_test_reason}")
        if set_single_test_status != "NOT RUN":
            print(f"SET (Single) Test Status: {set_single_test_status}")
            if set_single_test_status != "PASSED":
                print(f"  Reason: {set_single_test_reason}")
        if verify_test_status != "NOT RUN":
            print(f"Verification Test Status: {verify_test_status}")
            if verify_test_status != "PASSED":
                print(f"  Reason: {verify_test_reason}")
        print("=" * 30)


if __name__ == "__main__":
    main()

import serial
import json
import time
import sys
import argparse
import struct
import random

# --- Serial Communication Helpers ---


def send_command(ser, command_str):
    """Sends a command string to the Arduino and prints debug info."""
    print(f"\n[SEND] Command: '{command_str.strip()}'")
    ser.write(command_str.encode("utf-8"))
    print(
        f"[SENT] ASCII: '{command_str.strip()}' | Bytes: {command_str.encode('utf-8').hex()}"
    )


def read_line(ser, timeout_s=5):
    """Reads a single line from serial with a timeout."""
    start_time = time.time()
    buffer = b""
    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            byte = ser.read(1)
            buffer += byte
            if byte == b"\n":
                break
    line = buffer.decode("utf-8", errors="ignore").strip()
    if line:
        print(f"[RECV] Line: '{line}'")
    return line


def read_json_response(ser, timeout_s=15):
    """
    Reads from serial until a complete JSON object is received or timeout.
    This function ignores common debug messages from the Arduino.
    """
    json_str_buffer = ""
    start_time = time.time()
    debug_prefixes = ["CMD:", "->", "OK:", "ERR:", "Serial ready"]

    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            print(f"[RECV] Line: '{line}'")

            if any(line.startswith(prefix) for prefix in debug_prefixes) or not line:
                continue

            json_str_buffer += line
            try:
                # Attempt to parse the buffer on each new line
                return json.loads(json_str_buffer)
            except json.JSONDecodeError:
                # Incomplete JSON, continue buffering
                pass
        time.sleep(0.01)

    print(f"Timeout: No valid JSON received. Buffer: '{json_str_buffer}'")
    return None


def wait_for_ack(ser, timeout_s=5):
    """Waits for an ACK or relevant 'OK' message from the Arduino."""
    print("[RECV] Waiting for ACK...")
    start_time = time.time()
    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            print(f"[RECV] Line: '{line}'")
            if "-> Sent ACK" in line or "OK:" in line:
                print("[RECV] ACK received.")
                return True
            elif "ERR:" in line:
                print(f"[RECV] Error detected: {line}")
                return False
        time.sleep(0.01)
    print("[RECV] Timeout: No ACK received.")
    return False


# --- Phase 1: Set LED Count ---
def set_led_count(port, baud, led_count):
    """
    Connects to the device, sets the new LED count, and waits for it to restart.
    """
    print("\n--- PHASE 1: SETTING LED COUNT ---")
    print(f"Setting LED count to {led_count} and waiting for device to restart.")

    ser = None
    try:
        ser = serial.Serial(port, baud, timeout=5)
        time.sleep(2)  # Wait for initial connection
        ser.flushInput()

        command = f"setledcount {led_count}\n"
        send_command(ser, command)

        print("Command sent. The device will now restart. This script will pause.")

    finally:
        if ser and ser.is_open:
            ser.close()
            print("Serial port closed for restart.")

    time.sleep(8)
    print("Device should have restarted. Proceeding to Phase 2.")


# --- Phase 2: Configuration Generation and Upload ---


def get_available_effects(ser):
    """Fetches the list of available effects from the Arduino."""
    ser.flushInput()
    send_command(ser, "listeffects\n")
    print("Waiting for available effects list...")
    response_json = read_json_response(ser)
    if response_json and "effects" in response_json:
        effects = response_json["effects"]
        print(f"Available effects: {effects}")
        return effects
    print("Failed to get effects list.")
    return []


def generate_bench_test_config(available_effects, leds_per_segment=45):
    """
    Generates a simple configuration for a single test segment.
    """
    if not available_effects:
        print("Cannot generate config without a list of effects.")
        return []

    segments = []
    total_leds = leds_per_segment

    # Segment 0: The 'all' segment, covering the entire strip
    segments.append(
        {
            "id": 0,
            "name": "all",
            "startLed": 0,
            "endLed": total_leds - 1,
            "brightness": 150,
            "effect": "SolidColor",
            "color": 0x000000,  # Off by default
        }
    )

    # Segment 1: The single segment for bench testing
    dynamic_effects = [e for e in available_effects if e not in ["None", "SolidColor"]]
    effect_name = random.choice(dynamic_effects) if dynamic_effects else "RainbowChase"

    segments.append(
        {
            "id": 1,
            "name": "bench_test_segment",
            "startLed": 0,
            "endLed": total_leds - 1,
            "brightness": random.randint(150, 255),
            "effect": effect_name,
            "speed": 50,
            "color": random.randint(0, 0xFFFFFF),
        }
    )

    print(
        f"\nGenerated {len(segments)} total segments for a single strip of {total_leds} LEDs."
    )
    return segments


def upload_configuration(ser, segments_to_send):
    """Uploads the full segment configuration to the device."""
    print("\n--- PHASE 2: UPLOADING SEGMENT CONFIGURATION ---")

    ser.flushInput()
    send_command(ser, "setallsegmentconfigs\n")
    if not wait_for_ack(ser):
        print("Error: Did not receive initial ACK. Aborting.")
        return False

    num_segments = len(segments_to_send)
    count_bytes = struct.pack(">H", num_segments)
    print(f"[SEND] Segment Count: {num_segments} | Bytes: {count_bytes.hex()}")
    ser.write(count_bytes)
    if not wait_for_ack(ser):
        print("Error: Did not receive ACK for segment count. Aborting.")
        return False

    print(f"\nSending {num_segments} segment configurations...")
    for i, segment_data in enumerate(segments_to_send):
        json_payload = json.dumps(segment_data) + "\n"
        print(f"[SEND] Segment {i+1}/{num_segments}: {json_payload.strip()}")
        ser.write(json_payload.encode("utf-8"))
        if not wait_for_ack(ser):
            print(f"Error: Failed to receive ACK for segment {i+1}. Aborting.")
            return False
        print(f"Successfully sent segment {i+1}.")
        time.sleep(0.1)

    print("\n--- Configuration successfully uploaded! ---")
    return True


# --- Phase 3: Verification ---


def verify_led_count(ser, expected_count):
    """Connects and sends 'getledcount' to verify the setting."""
    print("\n--- Verifying LED Count ---")
    ser.flushInput()
    send_command(ser, "getledcount\n")

    while True:
        line = read_line(ser)
        if not line:
            print("Verification FAILED: No response for getledcount.")
            return False
        if "LED_COUNT:" in line:
            try:
                actual_count = int(line.split(":")[1].strip())
                print(f"Expected: {expected_count}, Actual: {actual_count}")
                if actual_count == expected_count:
                    print("Verification PASSED: LED count is correct.")
                    return True
                else:
                    print("Verification FAILED: LED count is incorrect.")
                    return False
            except (ValueError, IndexError):
                print(f"Verification FAILED: Could not parse LED count from '{line}'.")
                return False


def verify_configuration(ser, sent_config):
    """Sends 'getallsegmentconfigs' and compares the result."""
    print("\n--- Verifying Segment Configuration ---")
    ser.flushInput()
    send_command(ser, "getallsegmentconfigs\n")

    received_config = read_json_response(ser)
    if not received_config or "segments" not in received_config:
        print("Verification FAILED: Did not receive valid segment config JSON.")
        return False

    received_segments = received_config["segments"]
    print(
        f"Expected {len(sent_config)} segments, Received {len(received_segments)} segments."
    )
    if len(sent_config) != len(received_segments):
        print("Verification FAILED: Segment count mismatch.")
        return False

    for sent_seg in sent_config:
        received_seg = next(
            (s for s in received_segments if s["id"] == sent_seg["id"]), None
        )
        if not received_seg:
            print(
                f"Verification FAILED: Sent segment ID {sent_seg['id']} not found in received config."
            )
            return False

        if (
            sent_seg["name"] != received_seg["name"]
            or sent_seg["startLed"] != received_seg["startLed"]
            or sent_seg["endLed"] != received_seg["endLed"]
        ):
            print(
                f"Verification FAILED: Mismatch found for segment ID {sent_seg['id']}."
            )
            print(f"  Sent:     {sent_seg}")
            print(f"  Received: {received_seg}")
            return False

    print("Verification PASSED: Segment names and ranges are correct.")
    return True


# --- Main Script Execution ---


def main():
    parser = argparse.ArgumentParser(
        description="Arduino Bench Test Configuration Loader"
    )
    parser.add_argument(
        "--port", type=str, required=True, help="Serial port of the Arduino"
    )
    parser.add_argument(
        "--baud", type=int, default=115200, help="Baud rate for serial connection"
    )
    parser.add_argument(
        "--leds",
        type=int,
        default=45,
        help="Number of LEDs for the single test segment.",
    )
    args = parser.parse_args()

    ser = None
    all_tests_passed = True
    try:
        # --- PHASE 1 ---
        total_leds = args.leds
        set_led_count(args.port, args.baud, total_leds)

        # --- PHASE 2 ---
        print(f"Re-connecting to {args.port} to upload configuration...")
        ser = serial.Serial(args.port, args.baud, timeout=5)
        time.sleep(3)
        ser.flushInput()
        print("Connection re-established.")

        effects = get_available_effects(ser)
        if not effects:
            print("Could not retrieve effects. Aborting.")
            sys.exit(1)

        bench_config = generate_bench_test_config(effects, args.leds)
        if not upload_configuration(ser, bench_config):
            print("Upload failed. Aborting verification.")
            sys.exit(1)

        # --- PHASE 3 ---
        if not verify_led_count(ser, total_leds):
            all_tests_passed = False
        if not verify_configuration(ser, bench_config):
            all_tests_passed = False

    except serial.SerialException as e:
        print(f"Error: Could not open serial port {args.port}. {e}")
        all_tests_passed = False
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")
        all_tests_passed = False
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        all_tests_passed = False
    finally:
        if ser and ser.is_open:
            ser.close()
            print("\nSerial port closed.")

        print("\n" + "=" * 30)
        if all_tests_passed:
            print("✅ BENCH TEST CONFIGURATION COMPLETE ✅")
        else:
            print("❌ CONFIGURATION FAILED OR VERIFICATION MISMATCH ❌")
        print("=" * 30)


if __name__ == "__main__":
    main()
